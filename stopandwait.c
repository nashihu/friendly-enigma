#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>

#define PORT 9000

#define IP_PROTOCOL 0
#define NET_BUF_SIZE 200
#ifndef MSG_CONFIRM
#define MSG_CONFIRM 0
#endif

// struct sockaddr_in serverAddr;
// socklen_t serverAddrLen = sizeof(serverAddr);

// struct sockaddr_in clientAddr;
// socklen_t clientAddrLen = sizeof(clientAddr);

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
int successsaw;
Frame fsends;
Frame fresponse;
void alarmHandler(int signum, struct sockaddr_in serverAddr)
{
    socklen_t serverAddrLen = sizeof(serverAddr);
    alarm(1);
    if (signum != 1)
    {
        printf("triggered out of 1 %d\n", signum);
    }
    int dapet = sendto(sockfdc, &fsends, sizeof(Frame),
                       MSG_CONFIRM, (struct sockaddr *)&serverAddr,
                       serverAddrLen);
    printf("sent %d\n", dapet);
    recvfrom(sockfdc, &fresponse, sizeof(Frame), MSG_CONFIRM,
             (struct sockaddr *)&serverAddr, &serverAddrLen);
    successsaw = fresponse.ack == 1 && fresponse.frame_kind == ACK;
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

int send_files(int sock, FILE *f, struct sockaddr_in serverAddr)
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
            memset(&fsends, 0, sizeof(fsends));
            memset(&fresponse, 0, sizeof(fresponse));
            successsaw = 0;
            int attempt = 0;
            char buffer[NET_BUF_SIZE];
            size_t num = mins(filesize, sizeof(buffer));
            num = fread(fsends.buffer, 1, num, f);
            fsends.length = num;
            fsends.seq_num = step;
            fsends.frame_kind = SEQ;
            fsends.check_sum = checksum(fsends.buffer);

            if (num < 1)
                return 0;
            step++;
            while (!successsaw)
            {
                alarmHandler(1, serverAddr);
                attempt++;
                if (attempt > 1)
                {
                    printf("step %d done in %dth time\n", step, attempt);
                }
            }
            filesize -= num;
        } while (filesize > 0);
    }
    memset(&fsends, 0, sizeof(Frame));
    fsends.length = 0;
    sendto(sock, &fsends, sizeof(Frame),
           MSG_CONFIRM, (struct sockaddr *)&serverAddr,
           sizeof(serverAddr));
    return 1;
}

int read_file(int sock, FILE *f, struct sockaddr_in clientAddr)
{
    int num = 0;
    int step = 0;
    while (1)
    {
        Frame fread;
        Frame fresponse;
        socklen_t clientAddrLen = sizeof(clientAddr);
        num = recvfrom(sock, &fread,
                       sizeof(Frame), MSG_CONFIRM,
                       (struct sockaddr *)&clientAddr, &clientAddrLen);
        printf("recv %d %d\n", fread.length, num);
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
        sendto(sock, &fresponse, sizeof(Frame), MSG_CONFIRM,
               (struct sockaddr *)&clientAddr, clientAddrLen);
        if (fread.length == 0)
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

void rdtClientFile(char *host, long port, FILE *fp)
{

    // char buffer[NET_BUF_SIZE];
    // char *hello = "Hello from client";
    struct sockaddr_in servaddr;

    // Creating socket file descriptor
    if ((sockfdc = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));

    struct hostent *hoost = gethostbyname(host);
    unsigned int server_address = *(unsigned long *)hoost->h_addr_list[0];

    // Filling server information
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
    servaddr.sin_addr.s_addr = server_address;

    // int n, len;

    // sendto(sockfd, (const char *)hello, strlen(hello),
    // 	MSG_CONFIRM, (const struct sockaddr *) &servaddr,
    // 		sizeof(servaddr));
    // printf("Hello message sent.\n");

    // n = recvfrom(sockfd, (char *)buffer, NET_BUF_SIZE,
    // 			MSG_WAITALL, (struct sockaddr *) &servaddr,
    // 			&len);
    // buffer[n] = '\0';
    // printf("Server : %s\n", buffer);

    send_files(sockfdc, fp, servaddr);

    close(sockfdc);
}

void rdtServerFile(char *iface, long port, FILE *fp)
{

    int sockfd;
    // char buffer[NET_BUF_SIZE];
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
    servaddr.sin_port = htons(PORT);

    // Bind the socket with the server address
    if (bind(sockfd, (const struct sockaddr *)&servaddr,
             sizeof(servaddr)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // int n;
    // int len;

    // len = sizeof(cliaddr); //len is value/result

    // n = recvfrom(sockfd, (char *)buffer, NET_BUF_SIZE,
    // 			MSG_WAITALL, ( struct sockaddr *) &cliaddr,
    // 			&len);
    // buffer[n] = '\0';
    // printf("Client : %s\n", buffer);
    // sendto(sockfd, (const char *)hello, strlen(hello),
    // 	MSG_CONFIRM, (const struct sockaddr *) &cliaddr,
    // 		len);
    // printf("Hello message sent.\n");
    read_file(sockfd, fp, servaddr);
}

// int main(int argc, char* argv[]) {
//     if (argc > 1) {
//         rdtServerFile();
//     } else {
//         rdtClientFile();
//     }
// }

void stopandwait_server(char *iface, long port, FILE *fp)
{
    rdtServerFile(iface, port, fp);
}

void stopandwait_client(char *host, long port, FILE *fp)
{
    rdtClientFile(host, port, fp);
}
