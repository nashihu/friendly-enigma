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

#define NET_BUF_SIZE 200
#define N_WINDOW 5
#ifndef MSG_CONFIRM
#define MSG_CONFIRM 0
#endif

int msleep(long msec)
{
    struct timespec ts;
    int res;

    if (msec < 0)
    {
        errno = EINVAL;
        return -1;
    }

    ts.tv_sec = msec / 10;
    ts.tv_nsec = (msec % 10) * 1000000;

    do
    {
        res = nanosleep(&ts, &ts);
    } while (res && errno == EINTR);

    return res;
}
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
Frame fresponse;
struct sockaddr_in serverAddr;
int recvResponse(struct sockaddr_in serverAddr)
{
    socklen_t serverAddrLen = sizeof(serverAddr);
    recvfrom(sockfdgbn, &fresponse, sizeof(Frame), MSG_CONFIRM,
             (struct sockaddr *)&serverAddr, &serverAddrLen);
    success = fresponse.ack == 1 && fresponse.frame_kind == ACK && fresponse.seq_num == sequence;
    if (success)
    {
        lastSuccessIndex = fresponse.seq_num;
    }
    return success;
}

int sdcLog = 0;
void sendDataChunk(struct sockaddr_in serverAddr, int useAlarm, int i)
{
    socklen_t serverAddrLen = sizeof(serverAddr);
    if (useAlarm)
        alarm(1);
    int dapet = sendto(sockfdgbn, &fsend[i], sizeof(Frame),
                       MSG_CONFIRM, (struct sockaddr *)&serverAddr,
                       serverAddrLen);
    sdcLog++;
    // printf("sent %d %zu %d\n", dapet, time(NULL), (sdcLog * NET_BUF_SIZE));
    dapet++;
    if (useAlarm)
    {
        recvResponse(serverAddr);
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

void slide()
{
    for (int i = 1; i < N_WINDOW; i++)
    {
        fsend[i] = fsend[i - 1];
    }
}

void sendDataAlarm(int signum)
{
    int i = lastSuccessIndex;
    do
    {
        memset(&fsend[i], 0, sizeof(fsend[i]));
        size_t num = _min(filesize, sizeof(fsend[i].buffer));
        num = fread(fsend[i].buffer, 1, num, ff);
        fsend[i].length = num;
        fsend[i].seq_num = sequence;
        sequence++;
        fsend[i].frame_kind = SEQ;
        fsend[i].check_sum = _checksum(fsend[i].buffer);

        if (num < 1)
            return;
        sendDataChunk(serverAddr, 0, i);
        filesize -= num;
        printf("%d %lu %zu\n", i, filesize, num);
        i++;
    } while (filesize > 0 && i < N_WINDOW); // TODO asumsi filesize aman
}

int _send_files(int sock, FILE *f, struct sockaddr_in serverAddr)
{
    // sleep(2);
    ff = f;
    fseek(f, 0, SEEK_END);
    filesize = ftell(f);
    rewind(f);
    if (filesize == EOF)
        return 0;

    sequence = 0;
    lastSuccessIndex = 0;
    while (filesize > 0)
    {

        sendDataAlarm(1);

        int goback = 0;
        do
        {
            recvResponse(serverAddr);
            if (!success)
            {
                goback = 1;
                break;
            }
            slide();
            int last = N_WINDOW - 1;
            memset(&fsend[last], 0, sizeof(Frame));
            size_t num = _min(filesize, sizeof(fsend[last].buffer));
            num = fread(fsend[last].buffer, 1, num, f);
            fsend[last].length = num;
            fsend[last].seq_num = sequence;
            fsend[last].frame_kind = SEQ;
            fsend[last].check_sum = _checksum(fsend[last].buffer);

            if (num < 1)
                return 0;
            sequence++;
            sendDataChunk(serverAddr, 1, last);
            filesize -= num;
        } while (filesize > 0);
        if (goback)
        {
            goback = 0;
            continue;
        }
    }
    memset(&fsend[0], 0, sizeof(Frame));
    fsend[0].length = 0;
    sendto(sock, &fsend, sizeof(Frame),
           MSG_CONFIRM, (struct sockaddr *)&serverAddr,
           sizeof(serverAddr));
    return 1;
}

int rfLog = 0;
int _read_file(int sock, FILE *f, struct sockaddr_in clientAddr)
{
    int num = 0;
    int step = 0;
    int filesize = 0;
    while (1)
    {
        Frame fread;
        Frame fresponse;
        socklen_t clientAddrLen = sizeof(clientAddr);
        num = recvfrom(sock, &fread,
                       sizeof(Frame), MSG_CONFIRM,
                       (struct sockaddr *)&clientAddr, &clientAddrLen);
        rfLog++;
        printf("recv %d %d %d\n", fread.length, num, (rfLog * NET_BUF_SIZE));
        filesize += fread.length;
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
            printf("done %d\n", filesize);
            return 0;
        }
        fwrite(&fread.buffer[0], 1, fread.length, f);
        step++;
    };
}

void gbnClient(char *host, long port, FILE *fp)
{

    if ((sockfdgbn = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&serverAddr, 0, sizeof(serverAddr));

    struct hostent *hoost = gethostbyname(host);
    unsigned int server_address = *(unsigned long *)hoost->h_addr_list[0];

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = server_address;

    _send_files(sockfdgbn, fp, serverAddr);

    close(sockfdgbn);
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
    signal(SIGALRM, sendDataAlarm);
    gbnClient(host, port, fp);
}
