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
#define NET_BUF_SIZE 200
#define udpflag 0

struct sockaddr_in serverAddr;
socklen_t serverAddrLen = sizeof(serverAddr);

struct sockaddr_in clientAddr;
socklen_t clientAddrLen = sizeof(clientAddr);

typedef struct frame
{
    char buffer[NET_BUF_SIZE];
    int ack;
    int length;
    int frame_kind;
    int seq_num;
    int check_sum;
} Frame;

// framekind
#define ACK 0
#define SEQ 1

int sockfdc;
int success;
Frame fsend;
Frame fresponse;
void alarmHandler(int signum)
{
    alarm(1);
    if (signum != 1)
    {
        printf("triggered out of 1 %d\n", signum);
    }
    int dapet = sendto(sockfdc, &fsend, sizeof(Frame),
                       udpflag, (struct sockaddr *)&serverAddr,
                       serverAddrLen);
    printf("sent %d\n", dapet);
    recvfrom(sockfdc, &fresponse, sizeof(Frame), udpflag,
             (struct sockaddr *)&serverAddr, &serverAddrLen);
    success = fresponse.ack == 1 && fresponse.frame_kind == ACK;
}

int checksum(char buffer[NET_BUF_SIZE])
{
    int sum = 0;
    for (int i = 0; i < NET_BUF_SIZE; i++)
    {
        sum += buffer[i];
    }
    return sum;
}
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
    // sleep(2);
    fseek(f, 0, SEEK_END);
    long filesize = ftell(f);
    rewind(f);
    if (filesize == EOF)
        return 0;

    int step = 0;
    if (filesize > 0)
    {
        do
        {
            memset(&fsend, 0, sizeof(fsend));
            memset(&fresponse, 0, sizeof(fresponse));
            success = 0;
            int attempt = 0;
            char buffer[NET_BUF_SIZE];
            size_t num = mins(filesize, sizeof(buffer));
            num = fread(fsend.buffer, 1, num, f);
            fsend.length = num;
            fsend.seq_num = step;
            fsend.frame_kind = SEQ;
            fsend.check_sum = checksum(fsend.buffer);

            if (num < 1)
                return 0;
            step++;
            while (!success)
            {
                alarmHandler(1);
                attempt++;
                if (attempt > 1)
                {
                    printf("step %d done in %dth time\n", step, attempt);
                }
            }
            printf("sent %zu %d\n", num, step);
            filesize -= num;
        } while (filesize > 0);
    }
    sendto(sock, NULL, 0,
           udpflag, (struct sockaddr *)&serverAddr,
           serverAddrLen);
    return 1;
}

int read_file(int sock, FILE *f)
{
    int num = 0;
    int step = 0;
    while (1)
    {
        Frame fread;
        Frame fresponse;
        num = recvfrom(sock, &fread,
                       sizeof(Frame), udpflag,
                       (struct sockaddr *)&clientAddr, &clientAddrLen);
        printf("recv %d\n", fread.length);
        int ack = 1;
        int sum = checksum(fread.buffer);
        int isBuffer = fread.frame_kind == SEQ;
        int sumValid = fread.check_sum == sum;
        int debug = 1; // rand() % 100 < 50;
        int isValid = debug && isBuffer && sumValid;
        if (!isValid)
        {
            ack = 0;
        }
        // if (rand() % 3 < 1) {
        //     sleep(4);
        // }
        fresponse.ack = ack;
        fresponse.frame_kind = ACK;
        sendto(sock, &fresponse, sizeof(Frame), udpflag,
               (struct sockaddr *)&clientAddr, clientAddrLen);
        if (num == 0)
        {
            return 0;
        }
        if (step == fread.seq_num)
        {
            fwrite(&fread.buffer[0], 1, fread.length, f);
            step++;
        }
    };
}

int rdtServerFile(char *iface, long port, FILE *fp)
{
    int sockfd;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = inet_addr(iface);

    sockfd = socket(AF_INET, SOCK_DGRAM, IP_PROTOCOL);

    if (sockfd < 0)
    {
        perror("failed create socket");
    }

    if (bind(sockfd, (struct sockaddr *)&serverAddr, serverAddrLen) != 0)
    {
        perror("\nBinding Failed!\n");
        exit(EXIT_FAILURE);
    }

    read_file(sockfd, fp);
    fclose(fp);
    return 0;
}

int rdtClientFile(char *host, long port, FILE *fp)
{
    signal(SIGALRM, alarmHandler);
    memset(&clientAddr, 0, sizeof(clientAddr));
    clientAddr.sin_family = AF_INET;
    clientAddr.sin_port = htons(port);
    clientAddr.sin_addr.s_addr = inet_addr(host);

    sockfdc = socket(AF_INET, SOCK_DGRAM, IP_PROTOCOL);

    if (sockfdc < 0)
    {
        perror("failed create socket");
        exit(EXIT_FAILURE);
    }

    send_files(sockfdc, fp);
    fclose(fp);
    return 0;
}

void stopandwait_server(char *iface, long port, FILE *fp)
{
    rdtServerFile(iface, port, fp);
}

void stopandwait_client(char *host, long port, FILE *fp)
{
    rdtClientFile(host, port, fp);
}
