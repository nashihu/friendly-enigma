#include "file.h"
#include <stdio.h>
#include <string.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define IP_PROTOCOL 0
#define PORT_NO 15050
#define IP_ADDRESS "127.0.0.1" // localhost
#define NET_BUF_SIZE 32
#define cipherKey 'S'
#define sendrecvflag 0
#define nofile "File Not Found!"

// function to clear buffer
void clearBuf(char *b)
{
    int i;
    for (i = 0; i < NET_BUF_SIZE; i++)
        b[i] = '\0';
}

int min(int a, int b) {
    if (a < b) return a;
    return b;
}

// function to encrypt
char Cipher(char ch)
{
    return ch;
}

// function sending file
int sendFile(FILE *fp, char *buf, int s)
{
    int i, len;
    if (fp == NULL)
    {
        strcpy(buf, nofile);
        len = strlen(nofile);
        buf[len] = EOF;
        for (i = 0; i <= len; i++)
            buf[i] = Cipher(buf[i]);
        return 1;
    }

    char ch, ch2;
    for (i = 0; i < s; i++)
    {
        ch = fgetc(fp);
        ch2 = Cipher(ch);
        buf[i] = ch2;
        if (ch == EOF)
            return 1;
    }
    return 0;
}

int recvFile(FILE *fp, char *buf, int s)
{
    int i;
    char ch;
    for (i = 0; i < s; i++)
    {
        ch = buf[i];
        ch = Cipher(ch);
        if (ch == EOF)
        {
            printf("\n");
            return 1;
        }
        else
        {
            fprintf(fp, "%c", ch);
            printf("%c", ch);
        }
    }
    return 0;
}

int udpServerFile(char *iface, long port, int use_udp, FILE *fp)
{
    int sockfd, nBytes;
    struct sockaddr_in addr_con;
    socklen_t addrlen = sizeof(addr_con);
    addr_con.sin_family = AF_INET;
    addr_con.sin_port = htons(PORT_NO);
    addr_con.sin_addr.s_addr = INADDR_ANY;
    char net_buf[NET_BUF_SIZE];

    // socket()
    sockfd = socket(AF_INET, SOCK_DGRAM, IP_PROTOCOL);

    if (sockfd < 0)
    {
        // printf("\nfile descriptor not received!!\n");
    }
    else
    {
        // printf("\nfile descriptor %d received\n", sockfd);
    }

    // bind()
    if (bind(sockfd, (struct sockaddr *)&addr_con, sizeof(addr_con)) == 0)
    {
        // printf("\nSuccessfully binded!\n");
    }
    else
    {
        perror("\nBinding Failed!\n");
        exit(EXIT_FAILURE);
    }

    // receive file name
    clearBuf(net_buf);

    nBytes = recvfrom(sockfd, net_buf,
                      NET_BUF_SIZE, sendrecvflag,
                      (struct sockaddr *)&addr_con, &addrlen);
    // printf("berapa byte: %d\n", nBytes);

    // printf("\nFile Name Received: %s\n", net_buf);
    if (fp == NULL)
    {
        printf("\nFile open failed!\n");
        exit(EXIT_FAILURE);
    }
    else
    {
        // printf("\nFile Successfully opened!\n");
    }
    // bagian ini harusnya recvFile yah
    //  TODO OK
    while (1)
    {
        // receive
        clearBuf(net_buf);
        nBytes = recvfrom(sockfd, net_buf, NET_BUF_SIZE,
                          sendrecvflag, (struct sockaddr *)&addr_con,
                          &addrlen);

        // process
        if (recvFile(fp, net_buf, NET_BUF_SIZE))
        {
            break;
        }
    }
    return 0;
}

int udpClientFile(char *host, long port, int use_udp, FILE *fp)
{
    int sockfd;
    struct sockaddr_in addr_con;
    socklen_t addrlen = sizeof(addr_con);
    addr_con.sin_family = AF_INET;
    addr_con.sin_port = htons(PORT_NO);
    addr_con.sin_addr.s_addr = inet_addr(IP_ADDRESS);
    char net_buf[NET_BUF_SIZE];

    // socket()
    sockfd = socket(AF_INET, SOCK_DGRAM,
                    IP_PROTOCOL);

    if (sockfd < 0)
    {
        printf("\nfile descriptor not received!!\n");
        exit(EXIT_FAILURE);
    }
    else
    {
        // printf("\nfile descriptor %d received\n", sockfd);
    }

    clearBuf(net_buf);
    strcat(net_buf, "hoo.x");
    sendto(sockfd, net_buf, NET_BUF_SIZE,
           sendrecvflag, (struct sockaddr *)&addr_con,
           addrlen);

    // printf("\n---------Data Received---------\n");
    int n = 0;
    while (1)
    {

        // process
        printf("batch %d\n", ++n);
        if (sendFile(fp, net_buf, NET_BUF_SIZE))
        {
            sendto(sockfd, net_buf, NET_BUF_SIZE,
                   sendrecvflag,
                   (struct sockaddr *)&addr_con, addrlen);
            break;
        }

        // send
        sendto(sockfd, net_buf, NET_BUF_SIZE,
               sendrecvflag,
               (struct sockaddr *)&addr_con, addrlen);
        clearBuf(net_buf);
    }
    if (fp != NULL)
        fclose(fp);
    // printf("\n-------------------------------\n");
    return 0;
}

int senddata(int sock, void *buf, int buflen)
{
    unsigned char *pbuf = (unsigned char *)buf;

    while (buflen > 0)
    {
        int num = send(sock, pbuf, buflen, 0);
        if (num < 1)
        {
            perror("error send\n");
            return 0;
        }

        pbuf += num;
        buflen -= num;
    }

    return 1;
}

int sendlong(int sock, long value)
{
    value = htonl(value);
    return senddata(sock, &value, sizeof(value));
}

int sendfilee(int sock, FILE *f)
{
    fseek(f, 0, SEEK_END);
    long filesize = ftell(f);
    rewind(f);
    if (filesize == EOF)
        return 0;
    if (!sendlong(sock, filesize))
        return 0;
    if (filesize > 0)
    {
        char buffer[1024];
        do
        {
            size_t num = min(filesize, sizeof(buffer));
            num = fread(buffer, 1, num, f);
            if (num < 1)
                return 0;
            if (!senddata(sock, buffer, num))
                return 0;
            filesize -= num;
        } while (filesize > 0);
    }
    return 1;
}

int readdata(int sock, void *buf, int buflen)
{
    unsigned char *pbuf = (unsigned char *)buf;

    while (buflen > 0)
    {
        int num = recv(sock, pbuf, buflen, 0);
        if (num < 0)
        {
            return 0;
        }

        pbuf += num;
        buflen -= num;
    }

    return 1;
}

int readlong(int sock, long *value)
{
    if (!readdata(sock, value, sizeof(value)))
        return 0;
    *value = ntohl(*value);
    return 1;
}

int readfile(int sock, FILE *f)
{
    long filesize;
    if (!readlong(sock, &filesize))
        return 0;
    if (filesize > 0)
    {
        char buffer[1024];
        do
        {
            int num = min(filesize, sizeof(buffer));
            if (!readdata(sock, buffer, num))
                return 0;
            int offset = 0;
            do
            {
                size_t written = fwrite(&buffer[offset], 1, num - offset, f);
                if (written < 1)
                    return 0;
                offset += written;
            } while (offset < num);
            filesize -= num;
        } while (filesize > 0);
    }
    return 1;
}

void tcpServerFile(char *iface, long port, FILE *fp)
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

    struct hostent *host = gethostbyname(iface);
    unsigned int server_address = *(unsigned long *)host->h_addr_list[0];
    unsigned short server_port = port;

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    server_addr.sin_addr.s_addr = server_address;

    n = bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
    while (n < 0)
    {
        // printf("waiting server available\n");
        n = bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
        sleep(1);
        // printf("cb lg %ld\n", time(NULL));
    }
    // printf("server available\n");
    // printf("[+]Bind to the port number: %ld\n", port);
    int client_sock;
    socklen_t addr_size;
    // char buffer[1024];
    struct sockaddr_in client_addr;
    listen(server_sock, 5);

    addr_size = sizeof(client_addr);
    client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_size);

    readfile(client_sock, fp);
    fclose(fp);

    close(n);
}

void tcpClientFile(char *host, long port, FILE *fp)
{
    // char *ip = "127.0.0.1";

    int sock;
    struct sockaddr_in addr;
    //   socklen_t addr_size;
    // char buffer[1024];
    //   int n;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("[-]Socket error");
        exit(1);
    }
    // printf("[+]TCP server socket created.\n");

    struct hostent *_host = gethostbyname(host);
    unsigned int server_address = *(unsigned long *)_host->h_addr_list[0];
    unsigned short server_port = port;
    memset(&addr, '\0', sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(server_port);
    addr.sin_addr.s_addr = server_address;

    int c = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
    if (c != 0)
    {
        perror("cant connect ");
        printf("code: %d\n", c);
        abort();
    }
    // printf("Connected to the server.\n");
    if (fp != NULL)
    {
        sendfilee(sock, fp);
        fclose(fp);
    }
    close(sock);
    // printf("Disconnected from the server.\n");
}

void file_server(char *iface, long port, int use_udp, FILE *fp)
{
    if (use_udp)
    {
        udpServerFile(iface, port, use_udp, fp);
    }
    else
    {
        tcpServerFile(iface, port, fp);
    }
}

void file_client(char *host, long port, int use_udp, FILE *fp)
{
    if (use_udp)
    {
        udpClientFile(host, port, use_udp, fp);
    }
    else
    {
        tcpClientFile(host, port, fp);
    }
}
