

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netdb.h>
    #define closesocket(a) close(a)
    #define SOCKET_ERROR -1
    #define INVALID_SOCKET -1
    #define SOCKET int
#endif

#include "../server/measurement.h"
#include "../server/msgbuf.h"
#include "include.h"


#define RPRM_PACKET_HEADER_SIZE 4
#define RPRM_PROTO_VERSION 1
#define RECV_BUF_SIZE 512


#ifdef _WIN32
    int wsa_initialized = 0;

    int _wsa_initialize()
    {
        WSADATA wsa_data;
        int result;
        result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != NO_ERROR)
            return 1;
        wsa_initialized = 1;
        return 0;
    }
#endif


LIBCLIENT_PRIVATE
int _get_family(enum rprm_connection_type type)
{
    switch (type)
    {
    case UNSPEC:
        return AF_UNSPEC;
    case IPV4:
        return AF_INET;
    case IPV6:
        return AF_INET6;
    }
}


LIBCLIENT_PUBLIC
enum rprm_error rprm_connect(struct rprm_client *client,
        const char *address, int port, enum rprm_connection_type type)
{
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    SOCKET sock;
    char port_str[6];

#ifdef _WIN32
    if (!wsa_initialized)
        _wsa_initialize();
#endif

    if (port < 1 || port > 65535)
        return RPRM_ERROR_INVALID_PORT;

    sprintf(port_str, "%i", port);

    memset(&hints, 0, sizeof(hints));

    hints.ai_family = _get_family(type);
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(address, port_str, &hints, &result) != 0)
        return RPRM_ERROR_RESOLVE_ADRESS;

    /*
     * getaddrinfo() returns a list of address structures.
     * Try each address until we successfully connect(2).
     * If socket(2) (or connect(2)) fails, we (close the socket
     * and) try the next address.
     */
    for (rp = result; rp != NULL; rp = rp->ai_next)
    {
        sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock == INVALID_SOCKET)
            continue;
        if (connect(sock, rp->ai_addr, rp->ai_addrlen) == 0)
            break; /* Success */

        closesocket(sock);
        sock = INVALID_SOCKET;
    }

    freeaddrinfo(result);

    if (rp == NULL) /* no connection ? */
        return RPRM_ERROR_COULD_NOT_CONNECT; /* TODO: handle errors from connect() */

    client->_socket = sock;
    client->_packet_offset = 0;
    client->num_measurements = 0;
    client->measurements = 0;
    msgbuf_init(&client->_recvbuf);

    return RPRM_ERROR_NONE;
}


LIBCLIENT_PRIVATE
int _strlen_safe(const char *data, size_t max_length)
{
    const char *dataptr = data;
    while (*dataptr != '\0')
        if (++dataptr - data >= max_length)
            return -1;
    return dataptr - data;
}


LIBCLIENT_PRIVATE
int _get_measurement_data_size(struct measurement *m, size_t max_size)
{
    switch(m->type)
    {
    case MT_INT8:
    case MT_UINT8:
        return max_size < 1 ? -1 : 1;
    case MT_INT16:
    case MT_UINT16:
        return max_size < 2 ? -1 : 2;
    case MT_INT32:
    case MT_UINT32:
    case MT_FLOAT:
        return max_size < 4 ? -1 : 4;
    case MT_STRING:
        return _strlen_safe(m->data, max_size);
    }
}


LIBCLIENT_PRIVATE
int _packet_unpack(const char *data, size_t data_size, struct rprm_client *client)
{
    const char *dataptr = data + RPRM_PACKET_HEADER_SIZE;
    uint8_t nm = *(uint8_t*)(data + 3);
    size_t i = 0;
    int mds, slen;

    if (client->num_measurements != nm || client->measurements == NULL)
    {
        if (client->measurements != NULL)
            free(client->measurements);
        client->num_measurements = nm;
        client->measurements = malloc(sizeof(struct measurement) * nm);
    }


    while (dataptr - data < data_size)
    {
        client->measurements[i].type = *dataptr++;
        client->measurements[i].name = dataptr;

        /* advance for string length safe */
        slen = _strlen_safe(dataptr, data_size - (dataptr - data));
        if (slen < 0)
            break;
        dataptr += slen;

        /* read data */
        client->measurements[i].data = dataptr;
        mds = _get_data_size(client->measurements[i].type, data_size - (dataptr - data));
        if (mds < 0)
            break;
        dataptr += mds;

        if (dataptr - data == data_size)
            return 0; /* sucessfully finished */
        i++;
    }

    return 1;
}


LIBCLIENT_PUBLIC
enum rprm_error rprm_receive(struct rprm_client *client)
{
    char recv_buf[RECV_BUF_SIZE];
    int recvd;
    size_t packet_size = 0;

    /* where there remaining unhandled data from the last call? */
    if (client->_packet_offset > 0)
    {
        memmove(client->_recvbuf.data,
                client->_recvbuf.data + client->_packet_offset,
                client->_recvbuf.size - client->_packet_offset);
        client->_recvbuf.size -= client->_packet_offset;
        client->_packet_offset = 0;
    }
    else
        msgbuf_clear(&client->_recvbuf);

    while (1)
    {
        recvd = recv(client->_socket, recv_buf, RECV_BUF_SIZE, MSG_NOSIGNAL);
        if (recvd <= 0)
            return RPRM_ERROR_DISCONNECTED;
        msgbuf_app(&client->_recvbuf, recv_buf, recvd);

        if (packet_size == 0) /* packet size not yet known? */
        {
            if (*client->_recvbuf.data != RPRM_PROTO_VERSION)
                return RPRM_ERROR_PROTOCOL_VERSION;
            if (client->_recvbuf.size < RPRM_PACKET_HEADER_SIZE)
                continue;
            packet_size = ntohs(*(uint16_t*)(client->_recvbuf.data + 1));
        }

        /* packet complete? */
        if (client->_recvbuf.size >= packet_size)
        {
            if (_packet_unpack(client->_recvbuf.data, packet_size, client) != 0)
                return RPRM_ERROR_INVALID_PACKET;

            /* received more than one packet? */
            if (client->_recvbuf.size > packet_size)
                client->_packet_offset = client->_recvbuf.size;
            break;
        }
    }

    return RPRM_ERROR_NONE;
}
