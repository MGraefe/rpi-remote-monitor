

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

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

#include "shared/measurement.h"
#include "shared/msgbuf.h"
#include "libclient/include.h"

struct conn_int
{
    int socket;
    struct msgbuf recvbuf;
    size_t packet_offset;
    size_t packet_size;
};


#define RPRM_PACKET_HEADER_SIZE 4
#define RPRM_PROTO_VERSION 1
#define RECV_BUF_SIZE 512


size_t _get_proto_version(const char *data)
{
    return *((uint8_t*)(data));
}


size_t _get_message_size(const char *data)
{
    return ntohs(*((uint16_t*)(data + 1)));
}


size_t _get_num_measurements(const char *data)
{
    return *((uint8_t*)(data + 3));
}


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
enum rprm_error rprm_connect(struct rprm_connection *conn,
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

    struct conn_int *internals = malloc(sizeof(struct conn_int));
    internals->socket = sock;
    internals->packet_offset = 0;
    msgbuf_init(&internals->recvbuf);
    conn->internals = internals;

    return RPRM_ERROR_NONE;
}


int _strlen_safe(const char *data, size_t max_length)
{
    const char *dataptr = data;
    while (*dataptr != '\0')
        if (++dataptr - data >= max_length)
            return -1;
    return dataptr - data;
}


int _get_measurement_data_size(struct measurement *m, size_t max_size)
{
    int slen;

    switch(m->type)
    {
    case MT_INT8:
    case MT_UINT8:
        return 1;
    case MT_INT16:
    case MT_UINT16:
        return 2;
    case MT_INT32:
    case MT_UINT32:
    case MT_FLOAT:
        return 4;
    case MT_STRING:
        slen = _strlen_safe(m->data, max_size);
        return slen < 0 ? slen : slen + 1;
    }
}


void _data_to_host(struct measurement *m)
{
    switch(m->type)
    {
    case MT_INT16:
    case MT_UINT16:
        *(uint16_t*)(m->data) = ntohs(*(uint16_t*)m->data);
        break;
    case MT_INT32:
    case MT_UINT32:
        *(uint32_t*)(m->data) = ntohl(*(uint32_t*)m->data);
        break;
    }
}


int _packet_unpack(const char *data, size_t data_size, struct measurement *measurements)
{
    const char *dataptr = data + RPRM_PACKET_HEADER_SIZE;
    size_t i = 0;
    int mds, slen;

    while (dataptr - data < data_size)
    {
        measurements[i].type = *dataptr++;
        measurements[i].name = dataptr;

        /* advance for string length safe */
        slen = _strlen_safe(dataptr, data_size - (dataptr - data));
        if (slen < 0)
            break;
        dataptr += slen + 1;

        /* read data */
        measurements[i].data = dataptr;
        mds = _get_measurement_data_size(&measurements[i],
                data_size - (dataptr - data));
        if (mds < 0 || mds > data_size - (dataptr - data))
            break;
        _data_to_host(&measurements[i]);
        dataptr += mds;

        if (dataptr - data == data_size)
            return 0; /* sucessfully finished */
        i++;
    }

    return 1;
}


LIBCLIENT_PUBLIC
enum rprm_error rprm_receive(const struct rprm_connection *conn, size_t *num_measurements)
{
    char recv_buf[RECV_BUF_SIZE];
    int recvd;
    struct conn_int *in = conn->internals;

    in->packet_size = 0;

    /* where there remaining unhandled data from the last call? */
    if (in->packet_offset > 0)
    {
        memmove(in->recvbuf.data,
                in->recvbuf.data + in->packet_offset,
                in->recvbuf.size - in->packet_offset);
        in->recvbuf.size -= in->packet_offset;
        in->packet_offset = 0;
    }
    else
        msgbuf_clear(&in->recvbuf);

    while (1)
    {
        recvd = recv(in->socket, recv_buf, RECV_BUF_SIZE, MSG_NOSIGNAL);
        if (recvd <= 0)
            return RPRM_ERROR_DISCONNECTED;
        printf("received %i bytes\n", recvd);
        msgbuf_app(&in->recvbuf, recv_buf, recvd);

        /* packet size not yet known? */
        if (in->packet_size == 0)
        {
            if (_get_proto_version(in->recvbuf.data) != RPRM_PROTO_VERSION)
                return RPRM_ERROR_PROTOCOL_VERSION;
            if (in->recvbuf.size < RPRM_PACKET_HEADER_SIZE)
                continue;
            in->packet_size = _get_message_size(in->recvbuf.data);
        }

        /* packet complete? */
        if (in->recvbuf.size >= in->packet_size)
        {
            *num_measurements = _get_num_measurements(in->recvbuf.data);
            if (in->recvbuf.size > in->packet_size)
                in->packet_offset = in->packet_size;
            break;
        }
    }

    return RPRM_ERROR_NONE;
}


LIBCLIENT_PUBLIC
enum rprm_error rprm_get_data(struct rprm_connection *conn,
        struct measurement *measurements)
{
    struct conn_int *in = conn->internals;

    if (_packet_unpack(in->recvbuf.data, in->packet_size, measurements) != 0)
        return RPRM_ERROR_INVALID_PACKET;

    return RPRM_ERROR_NONE;
}
