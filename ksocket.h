#include <sys/time.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdint.h> 
#include <stdio.h>
#include <string.h>
#include <semaphore.h>
#include <pthread.h>
#include <sys/select.h>

#define IP "127.0.0.1"
#define SOCK_KTP 2025
#define W 10
#define MSIZE 512
#define PAYLOAD 480
#define N 5
#define SMKEY 4
#define SOCKKEY 3
#define SMPATH "makefile"
#define SOCKPATH "/"
#define S1PATH "./ksocket.c"
#define S2PATH "./ksocket.h"
#define SEMSMKEY 'A'
#define SEMSOCKKEY 'B'
#define S1KEY 'C'
#define S2KEY 'D'
#define BUFSIZE 10
#define T 5
#define TCLOSE 10
#define TNOSPACE 10
#define MAXSEQ 255
#define p 0.05

#define ENOSPACE 3001
#define ENOTBOUND 3002
#define ENOMESSAGE 3003

#define RESETMSG memset(message, '\0', MSIZE)

typedef struct{
    int free;
    int seq;
    int ack;
    char data[MSIZE];
    int rwsize;
} packet;

typedef struct{
    int base;
    int currsize;
    int swsize;
    int sw[W];
    time_t timer;
    int lastSent;
} swnd;

typedef struct{
    int st;
    int end;
    int rwsize;
    int rw[W];
    time_t timer;
} rwnd;

typedef struct {
    int st;
    int size;
} bufdetails;

typedef struct {
    int free;
    pid_t pid;
    int socket;
    struct sockaddr_in peer_addr;
    packet send_buffer[BUFSIZE];
    packet recv_buffer[BUFSIZE];
    bufdetails send;
    bufdetails recv;
    int peervalid;
    int nospace;
    swnd sendw;
    rwnd recvw;
} ksocket;

typedef struct{
    int socket;
    struct sockaddr_in my_addr;
    int idx;
    int addressvalid;
} currsock;

int k_socket(int domain, int type, int protocol);
int k_bind(char * src_ip, int src_port, char * dst_ip, int dst_port);
int k_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);
int k_recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);
int k_close(int sockfd);
void getshm();
int dropMessage();
void lock(int sem, int semnum);
void rel(int sem, int semnum);
int nextseq(int last);