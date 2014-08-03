
#define _POSIX_SOURCE
#define _BSD_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <stdbool.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>

#include "measurement.h"
#include "readstat.h"
#include "msgbuf.h"


#define MAX_CLIENTS 64
#define PROTOCOL_VERSION 1

enum servertype_e
{
    INET4 = AF_INET,
    INET6 = AF_INET6
};

int g_listenSock = -1;
enum servertype_e g_listenSockType;


struct clientinfo
{
    int sock;
    struct msgbuf sendbuf;
    size_t sendbuf_pos;
};

size_t g_numClients = 0;
struct clientinfo g_clientInfos[MAX_CLIENTS];

struct measure_callback g_callbacks[32];
struct measurement *g_measurements[32];
size_t g_callbacks_num = 0;
size_t g_measurements_num = 0;


void clientinfo_init(struct clientinfo *info, int sock)
{
    info->sock = sock;
    msgbuf_init(&info->sendbuf);
    info->sendbuf_pos = 0;
}


void clientinfo_destruct(struct clientinfo *info)
{
    info->sock = -1;
    msgbuf_dealloc(&info->sendbuf);
    info->sendbuf_pos = 0;
}


void register_callbacks()
{
    size_t i;

    g_callbacks[g_callbacks_num++] = cpu_util_callback;
    
    for(i = 0; i < g_callbacks_num; i++)
    {
        g_measurements_num += g_callbacks[i].num_measures;
        g_measurements[i] = malloc(g_callbacks[i].num_measures * sizeof(struct measurement));
    }
}


void init_measurements()
{
    size_t i;
    for(i = 0; i < g_callbacks_num; i++)
        g_callbacks[i].init_func();
}


void append_measurement(struct msgbuf *buf, struct measurement *m)
{
    msgbuf_app_byte(buf, m->type);
    msgbuf_app_string(buf, m->name);

    switch(m->type)
    {
    case MT_INT8:
    case MT_UINT8:
        msgbuf_app_byte(buf, *(uint8_t*)m->data);
        break;
    case MT_INT16:
    case MT_UINT16:
        msgbuf_app_short(buf, *(uint16_t*)m->data);
        break;
    case MT_INT32:
    case MT_UINT32:
        msgbuf_app_int(buf, *(uint32_t*)m->data);
        break;
    case MT_FLOAT:
        msgbuf_app_float(buf, *(float*)m->data);
        break;
    case MT_STRING:
        msgbuf_app_string(buf, (const char*)m->data);
        break;
    default:
        printf("Unhandled switch in append_measurement\n");
        abort();
    }
}


void take_measurements(struct msgbuf *buf)
{
    size_t i;
    int result, j;

    msgbuf_clear(buf);
    msgbuf_app_byte(buf, PROTOCOL_VERSION);
    msgbuf_app_short(buf, 0); //reserve header size to fill later
    msgbuf_app_byte(buf, g_measurements_num);

    for(i = 0; i < g_callbacks_num; i++)
    {
        result = g_callbacks[i].measure_func(g_measurements[i]);
        for(j = 0; j < result; j++)
            append_measurement(buf, &g_measurements[i][j]);
    }

    //Set size in header
    uint16_t size = buf->size;
    memcpy(buf->data + 1, &size, 2);
}


int start_server(int port, enum servertype_e type /* = AF_INET6 */, bool local /* = false */)
{
    g_listenSockType = type;
    g_listenSock = socket((int)type, SOCK_STREAM, 0);
    if (g_listenSock == -1)
    {
        printf("Error opening socket");
        return 1;
    }
    
    switch(type)
    {
        case INET4:
        {
            struct sockaddr_in sockAddrIn;
            memset(&sockAddrIn, 0, sizeof(sockAddrIn));
            sockAddrIn.sin_port = htons(port);
            sockAddrIn.sin_family = AF_INET;
            sockAddrIn.sin_addr.s_addr = local ? INADDR_LOOPBACK : INADDR_ANY;
            if (bind(g_listenSock, (struct sockaddr*)&sockAddrIn, sizeof(sockAddrIn)) != 0)
            {
                printf("Error binding server to port %i on IPv4, already bound?\n", port);
                return 1;
            }
            break;
        }
        case INET6:
        {
            struct sockaddr_in6 sockAddrIn;
            memset(&sockAddrIn, 0, sizeof(sockAddrIn));
            sockAddrIn.sin6_port = htons(port);
            sockAddrIn.sin6_family = AF_INET6;
            sockAddrIn.sin6_addr = local ? in6addr_loopback : in6addr_any;
            if (bind(g_listenSock, (struct sockaddr*)&sockAddrIn, sizeof(sockAddrIn)) != 0)
            {
                printf("Error binding server to port %i on IPv6, already bound?\n", port);
                return 1;
            }
            break;
        }
        default:
            printf("Unhandled switch\n");
            return 1;
    }
    
    if (listen(g_listenSock, 5) != 0)
    {
        printf("Call to listen() failed\n");
        return 1;
    }
        
    return 0;
}


void print_connect_info(const struct sockaddr *addr, socklen_t addrlen)
{
    char host[256];
    if (getnameinfo(addr, addrlen, host, 256, NULL, 0, NI_NUMERICHOST) != 0)
        printf("Connection from unknown IP\n");
    else
        printf("Connection from %s\n", host);    
}


void check_new_clients()
{
    while (g_numClients < MAX_CLIENTS)
    {
        fd_set fs_read;
        FD_ZERO(&fs_read);
        FD_SET(g_listenSock, &fs_read);

        struct timeval timeout = {0, 0};
        int fss = select(g_listenSock + 1, &fs_read, NULL, NULL, &timeout);
        if (fss <= 0)
            return;
        
        struct sockaddr *addr = NULL;
        socklen_t addrsize = 0;
        switch (g_listenSockType)
        {
            case INET4:
                addr = malloc(sizeof(struct sockaddr_in));
                memset(addr, 0, sizeof(struct sockaddr_in));
                addrsize = sizeof(struct sockaddr_in);
                break;
            case INET6:
                addr = malloc(sizeof(struct sockaddr_in6));
                memset(addr, 0, sizeof(struct sockaddr_in6));
                addrsize = sizeof(struct sockaddr_in6);
                break;
        }
        
        int clsock = accept(g_listenSock, addr, &addrsize);
        if (clsock != -1)
        {
            print_connect_info(addr, addrsize);

            clientinfo_init(&g_clientInfos[g_numClients], clsock);
            g_numClients++;
        }
        free(addr);
    }
}


void disconnect_client(int index)
{
    printf("A client disconnected\n");
    close(g_clientInfos[index].sock);
    clientinfo_destruct(&g_clientInfos[index]);
    memmove(&g_clientInfos[index],
            &g_clientInfos[index + 1],
            (g_numClients - index - 1) * sizeof(struct clientinfo));
    g_numClients--;
}


int _send(int sock, const char *data, size_t size)
{
    return send(sock, data, size, MSG_NOSIGNAL);
}


//returns the number of bytes sent
int send_to_client(struct clientinfo *client)
{
    if (client->sendbuf.size == 0)
        return 0;
    int sent = _send(client->sock,
            client->sendbuf.data + client->sendbuf_pos,
            client->sendbuf.size - client->sendbuf_pos);
    if (sent <= 0)
        return sent;
    client->sendbuf_pos += sent;
    if (client->sendbuf_pos == client->sendbuf.size) //all sent?
    {
        msgbuf_clear(&client->sendbuf);
        client->sendbuf_pos = 0;
    }
    return sent;
}


int send_output()
{
    int repeat;
    size_t i;
    do {
        int fdmax = -1;
        fd_set fs_write;
        FD_ZERO(&fs_write);
        for (i = 0; i < g_numClients; i++)
        {
            if (g_clientInfos[i].sendbuf.size > 0)
            {
                int fd = g_clientInfos[i].sock;
                FD_SET(fd, &fs_write);
                fdmax = fd > fdmax ? fd : fdmax;
            }
        }

        struct timeval timeout = {0, 0};
        int numfds = select(fdmax + 1, NULL, &fs_write, NULL, &timeout);
        if (numfds == 0)
            break;
    
        repeat = 0;
        for (i = 0; i < g_numClients; i++)
        {
            if (FD_ISSET(g_clientInfos[i].sock, &fs_write))
            {
                int sent = send_to_client(&g_clientInfos[i]);
                if (sent <= 0)
                    disconnect_client(i--);
                else if (g_clientInfos[i].sendbuf.size > 0)
                    repeat = 1; //Still something left? repeat
            }
        }
    } while (repeat);
    
    return 0;
}


void main_loop()
{
    size_t i;
    struct msgbuf buf;

    msgbuf_init(&buf);

    while (1)
    {
        //always look if we have a new client
        check_new_clients();

        //Clear our own messagebuffer and take the measurement
        msgbuf_clear(&buf);
        take_measurements(&buf);

        // Copy the measurement into the send-buffer of all clients IF their buffer is empty
        // If their buffer is not empty they have some part of previous messages pending and
        // simply are discarded from this update to prevent cluttering the server.
        for (i = 0; i < g_numClients; i++)
            if (g_clientInfos[i].sendbuf.size == 0)
                msgbuf_app(&g_clientInfos[i].sendbuf, buf.data, buf.size);

        //Finally send the output to all clients and sleep
        send_output();
        usleep(500000U);
    }
}


int main()
{
    register_callbacks();
    init_measurements();
    start_server(29100, INET4, false);
    main_loop();
    
    return 0;
}

