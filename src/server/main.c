
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


struct clientinfo_t
{
    int sock;
};

size_t g_numClients = 0;
struct clientinfo_t g_clientInfos[MAX_CLIENTS];

struct measure_callback g_callbacks[32];
struct measurement *g_measurements[32];
size_t g_callbacks_num = 0;
size_t g_measurements_num = 0;

void register_callbacks()
{
    g_callbacks[g_callbacks_num++] = cpu_util_callback;
    
    for(size_t i = 0; i < g_callbacks_num; i++)
    {
        g_measurements_num += g_callbacks[i].num_measures;
        g_measurements[i] = malloc(g_callbacks[i].num_measures * sizeof(struct measurement));
    }
}


void init_measurements()
{
    for(size_t i = 0; i < g_callbacks_num; i++)
        g_callbacks[i].init_func();
}

//size_t get_measurement_data_size(struct measurement *m)
//{
//    switch(m->type)
//    {
//    case MT_INT8:
//    case MT_UINT8:
//        return 1;
//    case MT_INT16:
//    case MT_UINT16:
//        return 2;
//    case MT_INT32:
//    case MT_UINT32:
//    case MT_FLOAT:
//        return 4;
//    case MT_STRING:
//        return strlen(m->data);
//    }
//}

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
    msgbuf_clear(buf);
    msgbuf_app_byte(buf, PROTOCOL_VERSION);
    msgbuf_app_short(buf, 0); //reserve header size to fill later

    for(size_t i = 0; i < g_callbacks_num; i++)
    {
        int result = g_callbacks[i].measure_func(g_measurements[i]);

        for(int j = 0; j < result; j++)
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
        
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 0;
        
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
        if (clsock == -1)
            return;
        
        print_connect_info(addr, addrsize);
        
        g_clientInfos[g_numClients].sock = clsock;
        g_numClients++;
    }
}


void disconnect_client(int index)
{
    printf("A client disconnected\n");
    close(g_clientInfos[index].sock);
    memmove(&g_clientInfos[index],
            &g_clientInfos[index + 1],
            (g_numClients - index - 1) * sizeof(struct clientinfo_t));
    g_numClients--;
}


int send_output(char *msg, size_t size)
{
    int fdmax = -1;
    fd_set fs_write;
    FD_ZERO(&fs_write);
    for (size_t i = 0; i < g_numClients; i++)
    {
        int fd = g_clientInfos[i].sock;
        FD_SET(fd, &fs_write);
        fdmax = fd > fdmax ? fd : fdmax;
    }
    
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;
    
    int numfds = select(fdmax + 1, NULL, &fs_write, NULL, &timeout);
    if (numfds > 0)
    {
        //write to all clients
        for (size_t i = 0; i < g_numClients; i++)
        {
            int sock = g_clientInfos[i].sock;
            if (FD_ISSET(sock, &fs_write))
            {
                //TODO: do some real stuff here
                if (send(sock, msg, size, MSG_NOSIGNAL) <= 0)
                    disconnect_client(i--);
            }
        }
    }
    
    return 0;
}


void handle_clients()
{
    struct msgbuf buf;
    msgbuf_init(&buf);

    while (1)
    {
        //always look if we have a new client
        check_new_clients();
        msgbuf_clear(&buf);
        take_measurements(&buf);
        send_output(buf.data, buf.size);
        usleep(500000U);
    }
}


int main()
{
    register_callbacks();
    init_measurements();
    start_server(29100, INET4, false);
    handle_clients();
    
    return 0;
}

