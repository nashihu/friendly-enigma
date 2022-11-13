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
int success;
Frame fsend;
Frame fresponse;
void alarmHandler(int signum, struct sockaddr_in serverAddr)
{   
    int serverAddrLen = sizeof(serverAddr);
    alarm(1);
    if (signum != 1)
    {
        printf("triggered out of 1 %d\n", signum);
    }
    int dapet = sendto(sockfdc, &fsend, sizeof(Frame),
                       MSG_CONFIRM, (struct sockaddr *)&serverAddr,
                       serverAddrLen);
    printf("sent %d\n", dapet);
    recvfrom(sockfdc, &fresponse, sizeof(Frame), MSG_CONFIRM,
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
                alarmHandler(1, serverAddr);
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
        int clientAddrLen = sizeof(clientAddr);
        num = recvfrom(sock, &fread,
                       sizeof(Frame), MSG_CONFIRM,
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
        sendto(sock, &fresponse, sizeof(Frame), MSG_CONFIRM,
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

void rdtClientFile() {
    FILE *fp;
    fp = fopen("lil.png", "r");
    if (fp == NULL) {
        printf("null file\n");
        exit(EXIT_FAILURE);
    }

	char buffer[NET_BUF_SIZE];
	char *hello = "Hello from client";
	struct sockaddr_in	 servaddr;
	
	// Creating socket file descriptor
	if ( (sockfdc = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
		perror("socket creation failed");
		exit(EXIT_FAILURE);
	}
	
	memset(&servaddr, 0, sizeof(servaddr));
		
	// Filling server information
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(PORT);
	servaddr.sin_addr.s_addr = INADDR_ANY;
		
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

void rdtServerFile() {
    FILE *fp;
    fp = fopen("hoi.png", "w");
    if (fp == NULL) {
        printf("null file\n");
        exit(EXIT_FAILURE);
    }

    int sockfd;
	char buffer[NET_BUF_SIZE];
	char *hello = "Hello from server";
	struct sockaddr_in servaddr, cliaddr;
		
	// Creating socket file descriptor
	if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
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
	if ( bind(sockfd, (const struct sockaddr *)&servaddr,
			sizeof(servaddr)) < 0 )
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

int main(int argc, char* argv[]) {
    if (argc > 1) {
        rdtServerFile();
    } else {
        rdtClientFile();
    }
}