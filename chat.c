#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
// #include <time.h>

#define MAXLINE 256
#ifndef MSG_CONFIRM
#define MSG_CONFIRM 15091996
#endif
/*
 *  Here is the starting point for your netster part.1 definitions. Add the
 *  appropriate comment header as defined in the code formatting guidelines
 */

/* Add function definitions */

void answer(char *buffer)
{
    char answer[256] = "";
    if (strcmp(buffer, "goodbye\n") == 0)
    {
        strcat(answer, "farewell\n");
    }
    else if (strcmp(buffer, "hello\n") == 0)
    {
        strcat(answer, "world\n");
    }
    else if (strcmp(buffer, "exit\n") == 0)
    {
        strcat(answer, "ok\n");
    }
    else
    {
        strcat(answer, buffer);
    }
    bzero(buffer, 1024);
    strcpy(buffer, answer);
}

void tcpListenProcess(int server_sock, long port, int n_bind)
{
    int client_sock;
    socklen_t addr_size;
    char buffer[1024];
    struct sockaddr_in client_addr;
    listen(server_sock, 5);
    // printf("Listening...\n");

    int client_connection_count = 0;
    while (1)
    {
        addr_size = sizeof(client_addr);
        client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_size);
        // printf("[+]Client connected. %d\n", client_sock);

        char host[4096];
        void *raw_addr = &(client_addr.sin_addr); // Extract the address from the container
        inet_ntop(AF_INET, raw_addr, host, 4096);

        // printf("IPv4 %s\n", host);
        printf("connection %d from ('%s, %ld)\n", client_connection_count, host, port);
        client_connection_count++;

        int exit = 0;
        while (1)
        {
            int close = 0;
            bzero(buffer, 1024);
            recv(client_sock, buffer, sizeof(buffer), 0);
            printf("got message from ('%s', %ld)\n", host, port);
            // printf("Client: %s", buffer);
            if (strcmp(buffer, "goodbye\n") == 0)
            {
                close = 1;
            }
            if (strcmp(buffer, "farewell\n") == 0)
            {
                close = 1;
            }
            if (strcmp(buffer, "ok\n") == 0)
            {
                close = 1;
            }
            if (strcmp(buffer, "exit\n") == 0)
            {
                close = 1;
                exit = 1;
            }

            answer(buffer);

            // printf("Server: %s", buffer);
            send(client_sock, buffer, strlen(buffer), 0);
            if (close)
            {
                break;
            }
        }
        close(client_sock);
        // printf("[+]Client disconnected.\n\n");
        if (exit)
        {
            break;
        }
    }
    close(n_bind);
}

void tcpServer(char *iface, long port)
{

    int server_sock;
    struct sockaddr_in server_addr;
    int n;

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0)
    {
        perror("[-]Socket error");
        exit(1);
    }
    // printf("[+]TCP server socket created.\n");

    memset(&server_addr, '\0', sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = port;
    server_addr.sin_addr.s_addr = inet_addr(iface);

    n = bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
    while (n < 0)
    {
        n = bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
        sleep(1);
        // printf("cb lg %ld\n", time(NULL));
    }
    // printf("[+]Bind to the port number: %ld\n", port);
    int status = fork();
    if (status > 0)
    {
        tcpListenProcess(server_sock, port, n);
    }
    else
    {
        tcpListenProcess(server_sock, port, n);
    }
}

void udpServer(char *iface, long port)
{

    int sockfd;
    char buffer[MAXLINE];
    // char *hello = "Hello from server";
    struct sockaddr_in servaddr, cliaddr;

    // Creating socket file descriptor
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));

    // Filling server information
    servaddr.sin_family = AF_INET; // IPv4
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(port);

    // Bind the socket with the server address
    if (bind(sockfd, (const struct sockaddr *)&servaddr,
             sizeof(servaddr)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    while (1)
    {
        int n;
        socklen_t len;

        len = sizeof(cliaddr); // len is value/result

        n = recvfrom(sockfd, (char *)buffer, MAXLINE,
                     MSG_WAITALL, (struct sockaddr *)&cliaddr,
                     &len);
        buffer[n] = '\0';
        char host[4096];
        void *raw_addr = &(cliaddr.sin_addr); // Extract the address from the container
        inet_ntop(AF_INET, raw_addr, host, 4096);

        printf("got message from ('%s', %ld)\n", host, port);
        // printf("Client : %s\n", buffer);

        int exit = 0;
        if (strcmp(buffer, "exit\n") == 0)
        {
            exit = 1;
        }
        answer(buffer);
        // sendto(sockfd, (const char *)buffer, strlen(buffer),
        //        MSG_CONFIRM, (const struct sockaddr *)&cliaddr,
        //        len);

        if (exit)
        {
            break;
        }

        // printf("Hello message sent.\n");
    }
}

void chat_server(char *iface, long port, int use_udp)
{
    if (port < 1024)
    {
        printf("port is used");
        exit(EXIT_FAILURE);
    }

    struct addrinfo hints;
    struct addrinfo *res;

    // memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    char strPort[256];
    sprintf(strPort, "%ld", port);

    int ret = getaddrinfo(iface, strPort, &hints, &res);
    if (ret != 0)
    {
        // fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
        // exit(EXIT_FAILURE);
    }

    if (use_udp)
    {
        udpServer(iface, port);
    }
    else
    {
        tcpServer(iface, port);
    }
}

void tcpClient(char *host, long port)
{
    // char *ip = "127.0.0.1";

    int sock;
    struct sockaddr_in addr;
    //   socklen_t addr_size;
    char buffer[1024];
    //   int n;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("[-]Socket error");
        exit(1);
    }
    // printf("[+]TCP server socket created.\n");

    memset(&addr, '\0', sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = port;
    addr.sin_addr.s_addr = inet_addr(host);

    connect(sock, (struct sockaddr *)&addr, sizeof(addr));
    // printf("Connected to the server.\n");

    while (1)
    {
        // printf("message: ");
        char *message = NULL;
        size_t len = 0;
        ssize_t lineSize = 0;
        lineSize = getline(&message, &len, stdin);
        if (lineSize)
        {
        }
        bzero(buffer, 1024);
        strcpy(buffer, message);
        // printf("Client: %s\n", buffer);
        send(sock, buffer, strlen(buffer), 0);

        bzero(buffer, 1024);
        recv(sock, buffer, sizeof(buffer), 0);
        printf("%s", buffer);
        if (strcmp(buffer, "farewell\n") == 0)
        {
            break;
        }
        if (strcmp(buffer, "ok\n") == 0)
        {
            break;
        }
    }

    close(sock);
    // printf("Disconnected from the server.\n");
}

void udpClient(char *host, long port)
{
    int sockfd;
    char buffer[MAXLINE];
    char *hello = NULL;
    ;
    struct sockaddr_in servaddr;

    // Creating socket file descriptor
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));

    // Filling server information
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = INADDR_ANY;

    while (1)
    {
        int n;
        socklen_t len;
        // char *message = NULL;
        size_t lens = 0;
        ssize_t lineSize = 0;
        lineSize = getline(&hello, &lens, stdin);
        if (lineSize)
        {
        }

        // sendto(sockfd, (const char *)hello, strlen(hello),
        //        MSG_CONFIRM, (const struct sockaddr *)&servaddr,
        //        sizeof(servaddr));
        // printf("Hello message sent.\n");

        n = recvfrom(sockfd, (char *)buffer, MAXLINE,
                     MSG_WAITALL, (struct sockaddr *)&servaddr,
                     &len);
        buffer[n] = '\0';
        printf("%s", buffer);
        if (strcmp(buffer, "farewell\n") == 0)
        {
            break;
        }
        if (strcmp(buffer, "ok\n") == 0)
        {
            break;
        }
    }
    close(sockfd);
}

void chat_client(char *host, long port, int use_udp)
{
    if (port < 1024)
    {
        printf("port is used");
        exit(EXIT_FAILURE);
    }

    struct addrinfo hints;
    struct addrinfo *res;

    // memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    char strPort[256];
    sprintf(strPort, "%ld", port);

    int ret = getaddrinfo(host, strPort, &hints, &res);
    if (ret != 0)
    {
        // fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
        // exit(EXIT_FAILURE);
    }
    if (use_udp)
    {
        udpClient(host, port);
    }
    else
    {
        tcpClient(host, port);
    }
}
