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

void file_server(char *iface, long port, int use_udp, FILE *fp)
{
    udpServerFile(iface, port, use_udp, fp);
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

void file_client(char *host, long port, int use_udp, FILE *fp)
{
    udpClientFile(host, port, use_udp, fp);
}
