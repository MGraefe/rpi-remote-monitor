
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

#include "readstat.h"
#include "array.h"


#define MAX_CLIENTS 64


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


int send_output()
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
                char msg[] = "Hello\n";
                //TODO: do some real stuff here
                if (send(sock, msg, strlen(msg), MSG_NOSIGNAL) <= 0)
                    disconnect_client(i--);
            }
        }
    }
    
    return 0;
}


int handle_clients()
{
    while (1)
    {
        //always look if we have a new client
        check_new_clients();
        send_output();
        usleep(500000U);
    }
}


int main()
{
    start_server(29100, INET4, false);
    handle_clients();
    
    return 0;
    
    struct jiffyinfo_t lastInfo, nowInfo;
    get_jiffy_info(&lastInfo);
    
    while (1)
    {
        sleep(1);
        get_jiffy_info(&nowInfo);
        int cpuUtil = get_cpu_util(&lastInfo, &nowInfo);
        lastInfo = nowInfo;
        printf("CPU-Util: %i (per mil)\n", cpuUtil);
    }
}


