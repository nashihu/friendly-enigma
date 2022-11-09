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
#include <time.h>
#include <unistd.h>

#define IP_PROTOCOL 0
// #define PORT_NO 15050
// #define IP_ADDRESS "127.0.0.1" // localhost
#define NET_BUF_SIZE 2048
#define cipherKey 'S'
#define sendrecvflag 0
#define nofile "File Not Found!"

struct sockaddr_in addr_con;
socklen_t adrlen = sizeof(addr_con);

struct sockaddr_in addr_cli;
socklen_t adrlencli = sizeof(addr_cli);

int x = 0;

typedef struct frame {
    unsigned char *pbuf;
    int ack;
    int length;
}Frame;

// function to clear buffer
void clearBufs(char *b)
{
    int i;
    for (i = 0; i < NET_BUF_SIZE; i++)
        b[i] = '\0';
}

int mins(int a, int b)
{
    if (a < b)
        return a;
    return b;
}

// function to encrypt
char cipher(char ch)
{
    return ch;
}

int send_data(int sock, void *buf, int buflen)
{
    Frame frame_send;
    unsigned char *pbuf = (unsigned char *)buf;

    int num;

    while (buflen > 0)
    {
        frame_send.pbuf = pbuf;
        frame_send.length = buflen;
        num = sendto(sock, &frame_send, sizeof(Frame),
                         sendrecvflag, (struct sockaddr *)&addr_cli,
                         adrlencli);
        // num = sendto(sock, pbuf, buflen,
        //                  sendrecvflag, (struct sockaddr *)&addr_cli,
        //                  adrlencli);
                         printf("sent %d %d\n", ++x, buflen);
        if (num < 1)
        {
            perror("error send");
            return 0;
        }

        pbuf += buflen;
        buflen -= buflen;
    }

    return 1;
}

int sendzero(int sock, long value)
{
    Frame frame_zero;
    frame_zero.length = 0;
    // value = htonl(value);
    int num;
    num = sendto(sock, &frame_zero, sizeof(Frame),
                         sendrecvflag, (struct sockaddr *)&addr_cli,
                         adrlencli);
    return num;
}

int send_files(int sock, FILE *f)
{
    fseek(f, 0, SEEK_END);
    long filesize = ftell(f);
    rewind(f);
    if (filesize == EOF)
        return 0;
    // if (!sendzero(sock, filesize))
    //     return 0;
    if (filesize > 0)
    {
        char buffer[NET_BUF_SIZE];
        do
        {
            size_t num = mins(filesize, sizeof(buffer));
            num = fread(buffer, 1, num, f);
            if (num < 1)
                return 0;
            if (!send_data(sock, buffer, num))
            {
                return 0;
            }
            filesize -= num;
            // printf("fsiz %ld\n", filesize);
        } while (filesize > 0);
    }
    sendzero(sock, 0);
    return 1;
}


int read_data(int sock, void *buf, int buflen)
{
    Frame frame_read;
    unsigned char *pbuf = (unsigned char *)buf;

    while (buflen > 0)
    {
        int fileSize;
        // fileSize = recvfrom(sock, pbuf,
        //                    NET_BUF_SIZE, sendrecvflag,
        //                    (struct sockaddr *)&addr_con, &adrlen);
        fileSize = recvfrom(sock, &frame_read,
                           sizeof(Frame), sendrecvflag,
                           (struct sockaddr *)&addr_con, &adrlen);
        pbuf = frame_read.pbuf;
        if (fileSize <= 0)
        {
            return 0;
        }
        fileSize = frame_read.length;
        printf("dpt %d %d\n", ++x, fileSize);
        if (fileSize <= 0)
        {
            return 0;
        }
        pbuf += fileSize;
        buflen -= fileSize;
    }

    return 1;
}

int read_file(int sock, FILE *f)
{
    long filesize = NET_BUF_SIZE;
    if (filesize > 0)
    {
        char buffer[NET_BUF_SIZE];
        do
        {
            int num = mins(filesize, sizeof(buffer));
            if (!read_data(sock, buffer, num))
                return 0;
            int offset = 0;
            do
            {
                size_t written = fwrite(&buffer[offset], 1, num - offset, f);
                if (written < 1)
                    return 0;
                offset += written;
            } while (offset < num);
            // filesize -= num;
            if (num < filesize)
            {
                filesize = num;
            }
        } while (filesize > 0);
    }
    return 1;
}

int rdtServerFile(char *iface, long port, int use_udp, FILE *fp)
{
    int sockfd;
    addr_con.sin_family = AF_INET;
    addr_con.sin_port = htons(port);
    addr_con.sin_addr.s_addr = INADDR_ANY;
    char net_buf[NET_BUF_SIZE];

    // socket()
    sockfd = socket(AF_INET, SOCK_DGRAM, IP_PROTOCOL);

    if (sockfd < 0)
    {
        printf("\nfile descriptor not received!!\n");
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
    clearBufs(net_buf);
    // long sip = 0;
    read_file(sockfd, fp);
    fclose(fp);
    return 0;
}

int rdtClientFile(char *host, long port, int use_udp, FILE *fp)
{
    int sockfd;

    memset(&addr_cli, 0, sizeof(addr_cli));
    addr_cli.sin_family = AF_INET;
    addr_cli.sin_port = htons(port);
    addr_cli.sin_addr.s_addr = INADDR_ANY;
    // addr_cli.sin_addr.s_addr = inet_addr(host);
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

    clearBufs(net_buf);
    send_files(sockfd, fp); //udp
    fclose(fp);
    // printf("\n-------------------------------\n");
    return 0;
}

void stopandwait_server(char* iface, long port, FILE* fp) {
  rdtServerFile(iface, port, 1, fp);
}

void stopandwait_client(char* host, long port, FILE* fp) {
    rdtClientFile(host, port, 1, fp);
}
