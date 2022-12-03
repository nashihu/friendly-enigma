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
#include <errno.h>

#define NET_BUF_SIZE 256
#define N_WINDOW 5
#ifndef MSG_CONFIRM
#define MSG_CONFIRM 0
#endif
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

int sockfdgbn;
int success;
int sequence;
int lastSuccessIndex;
long filesize;
FILE *ff;
Frame fsend[N_WINDOW];
Frame fresp;
struct sockaddr_in serverAddr;
void alarmHandlerData(int signum, struct sockaddr_in serverAddr)
{
    socklen_t serverAddrLen = sizeof(serverAddr);
    if (signum != 1)
    {
        printf("triggered out of 1 %d\n", signum);
    }
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    if (setsockopt(sockfdgbn, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
    {
        perror("Error");
    }
    sendto(sockfdgbn, &fsend[0], sizeof(Frame),
                       MSG_CONFIRM, (struct sockaddr *)&serverAddr,
                       serverAddrLen);
    printf("sent packet size %d\n", fsend[0].length);
    recvfrom(sockfdgbn, &fresp, sizeof(Frame), MSG_CONFIRM,
             (struct sockaddr *)&serverAddr, &serverAddrLen);
    success = fresp.ack == 1 && fresp.frame_kind == ACK;
}

int check_sum(char buffer[NET_BUF_SIZE])
{
    int sum = 0;
    for (int i = 0; i < NET_BUF_SIZE; i++)
    {
        sum += buffer[i];
    }
    return sum;
}

int ming(int a, int b)
{
    if (a < b)
        return a;
    return b;
}

int sendFiles(int sock, FILE *f, struct sockaddr_in serverAddr)
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
            memset(&fsend[0], 0, sizeof(fsend[0]));
            memset(&fresp, 0, sizeof(fresp));
            success = 0;
            int attempt = 0;
            char buffer[NET_BUF_SIZE];
            size_t num = ming(filesize, sizeof(buffer));
            num = fread(fsend[0].buffer, 1, num, f);
            fsend[0].length = num;
            fsend[0].seq_num = step;
            fsend[0].frame_kind = SEQ;
            fsend[0].check_sum = check_sum(fsend[0].buffer);

            if (num < 1)
                return 0;
            step++;
            while (!success)
            {
                alarmHandlerData(1, serverAddr);
                attempt++;
                if (attempt > 1)
                {
                    printf("step %d done in %dth time\n", step, attempt);
                }
            }
            filesize -= num;
        } while (filesize > 0);
    }
    memset(&fsend[0], 0, sizeof(Frame));
    fsend[0].length = 0;
    sendto(sock, &fsend[0], sizeof(Frame),
           MSG_CONFIRM, (struct sockaddr *)&serverAddr,
           sizeof(serverAddr));
    return 1;
}

int readfileg(int sock, FILE *f, struct sockaddr_in clientAddr)
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
        printf("recv packet size %d\n", fread.length);
        int ack = 1;
        int sum = check_sum(fread.buffer);
        int isBuffer = fread.frame_kind == SEQ;
        int sumValid = fread.check_sum == sum;
        int debug = 1; // rand() % 100 < 50;
        int isValid = debug && isBuffer && sumValid;
        if (!isValid)
        {
            ack = 0;
        }
        fresponse.ack = ack;
        fresponse.frame_kind = ACK;
        if (0)
        {
            fread.seq_num = -1;
			printf("skipped\n");
        }
        else
        {
            sendto(sock, &fresponse, sizeof(Frame), MSG_CONFIRM,
                   (struct sockaddr *)&clientAddr, clientAddrLen);
        }

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

void gbnClient(char *host, long port, FILE *fp)
{

    // char buffer[NET_BUF_SIZE];
    // char *hello = "Hello from client";
    struct sockaddr_in servaddr;

    // Creating socket file descriptor
    if ((sockfdgbn = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));

    struct hostent *hoost = gethostbyname(host);
    unsigned int server_address = *(unsigned long *)hoost->h_addr_list[0];

    // Filling server information
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
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

    sendFiles(sockfdgbn, fp, servaddr);

    close(sockfdgbn);
}

void gbnServer(char *iface, long port, FILE *fp)
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
    servaddr.sin_port = htons(port);

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
    readfileg(sockfd, fp, servaddr);
}

void gbn_server(char *iface, long port, FILE *fp)
{
	gbnServer(iface, port, fp);
}

void gbn_client(char *host, long port, FILE *fp)
{
	// signal(SIGALRM, sendDataAlarm);
	gbnClient(host, port, fp);
}
