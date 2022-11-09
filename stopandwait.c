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
#define sendrecvflag 0
#define nofile "File Not Found!"

struct sockaddr_in addr_con;
socklen_t adrlen = sizeof(addr_con);

struct sockaddr_in addr_cli;
socklen_t adrlencli = sizeof(addr_cli);

int x = 0;

typedef struct frame
{
    unsigned char *pbuf;
    int ack;
    int length;
} Frame;

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


int send_files(int sock, FILE *f)
{
    fseek(f, 0, SEEK_END);
    long filesize = ftell(f);
    rewind(f);
    if (filesize == EOF)
        return 0;
    // if (!sendzero(sock, filesize))
    //     return 0;
    printf("FILE SIZEW %lu\n", filesize);
    if (filesize > 0)
    {
        do
        {
            char buffer[NET_BUF_SIZE];
            size_t num = mins(filesize, sizeof(buffer));
            num = fread(buffer, 1, num, f);
            if (num < 1)
                return 0;
            int res = sendto(sock, &buffer, num,
                             sendrecvflag, (struct sockaddr *)&addr_cli,
                             adrlencli);
            if (!res)
            {
                return 0;
            }
            filesize -= num;
            // printf("fsiz %ld\n", filesize);
        } while (filesize > 0);
    }
    sendto(sock, NULL, 0,
                 sendrecvflag, (struct sockaddr *)&addr_cli,
                 adrlencli);
    return 1;
}

int read_file(int sock, FILE *f)
{
    int num = 0;
    while (1)
    {
        char buffer[NET_BUF_SIZE];
        num = recvfrom(sock, &buffer,
                       NET_BUF_SIZE, sendrecvflag,
                       (struct sockaddr *)&addr_con, &adrlen);
        if (num == 0)
        {
            return 0;
        }
        fwrite(&buffer[0], 1, num, f);
    };
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
    // readzero(sockfd, &sip);
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
    send_files(sockfd, fp); // udp
    fclose(fp);
    // printf("\n-------------------------------\n");
    return 0;
}

void stopandwait_server(char *iface, long port, FILE *fp)
{
    rdtServerFile(iface, port, 1, fp);
}

void stopandwait_client(char *host, long port, FILE *fp)
{
    rdtClientFile(host, port, 1, fp);
}
