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

#define NET_BUF_SIZE 200
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

int sockfdc;
int success;
Frame fsend[N_WINDOW];
Frame fresponse;
void alarmHandlerGbn(int signum, struct sockaddr_in serverAddr, int useAlarm, int i)
{
    socklen_t serverAddrLen = sizeof(serverAddr);
    if (useAlarm)
        alarm(1);
    if (signum != 1)
    {
        printf("triggered out of 1 %d\n", signum);
    }
    int dapet = sendto(sockfdc, &fsend[i], sizeof(Frame),
                       MSG_CONFIRM, (struct sockaddr *)&serverAddr,
                       serverAddrLen);
    printf("sent %d\n", dapet);
    if (useAlarm)
    {
        recvfrom(sockfdc, &fresponse, sizeof(Frame), MSG_CONFIRM,
                 (struct sockaddr *)&serverAddr, &serverAddrLen);
        success = fresponse.ack == 1 && fresponse.frame_kind == ACK;
    }
}

int _checksum(char buffer[NET_BUF_SIZE])
{
    int sum = 0;
    for (int i = 0; i < NET_BUF_SIZE; i++)
    {
        sum += buffer[i];
    }
    return sum;
}

int _min(int a, int b)
{
    if (a < b)
        return a;
    return b;
}

int _send_files(int sock, FILE *f, struct sockaddr_in serverAddr)
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
        int i = 0;
        do 
        {
            size_t num = _min(filesize, sizeof(fsend[i].buffer));
            num = fread(fsend[i].buffer, 1, num, f);
            fsend[i].length = num;
            fsend[i].seq_num = step;
            fsend[i].frame_kind = SEQ;
            fsend[i].check_sum = _checksum(fsend[i].buffer);

            if (num < 1)
                return 0;
            alarmHandlerGbn(1, serverAddr, 0, i);
            filesize -= num;
        } while (filesize > 0 && i < N_WINDOW); //TODO asumsi filesize aman

        do
        {
            success = 0;
            int attempt = 0;
            size_t num = _min(filesize, sizeof(fsend.buffer));
            num = fread(fsend.buffer, 1, num, f);
            fsend.length = num;
            fsend.seq_num = step;
            fsend.frame_kind = SEQ;
            fsend.check_sum = _checksum(fsend.buffer);

            if (num < 1)
                return 0;
            step++;
            while (!success)
            {
                alarmHandlerGbn(1, serverAddr);
                attempt++;
                if (attempt > 1)
                {
                    printf("step %d done in %dth time\n", step, attempt);
                }
            }
            filesize -= num;
        } while (filesize > 0);
    }
    memset(&fsend, 0, sizeof(Frame));
    fsend.length = 0;
    sendto(sock, &fsend, sizeof(Frame),
           MSG_CONFIRM, (struct sockaddr *)&serverAddr,
           sizeof(serverAddr));
    return 1;
}

int _read_file(int sock, FILE *f, struct sockaddr_in clientAddr)
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
        int sum = _checksum(fread.buffer);
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

void gbnClient(char *host, long port, FILE *fp)
{

    struct sockaddr_in servaddr;

    if ((sockfdc = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));

    struct hostent *hoost = gethostbyname(host);
    unsigned int server_address = *(unsigned long *)hoost->h_addr_list[0];

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = server_address;

    _send_files(sockfdc, fp, servaddr);

    close(sockfdc);
}

void gbnServer(char *iface, long port, FILE *fp)
{

    int sockfd;
    struct sockaddr_in servaddr, cliaddr;

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));

    servaddr.sin_family = AF_INET; // IPv4
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(port);

    if (bind(sockfd, (const struct sockaddr *)&servaddr,
             sizeof(servaddr)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    _read_file(sockfd, fp, servaddr);
}

void gbn_server(char *iface, long port, FILE *fp)
{
    gbnServer(iface, port, fp);
}

void gbn_client(char *host, long port, FILE *fp)
{
    gbnClient(host, port, fp);
}
