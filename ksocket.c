// Assignment 4 Submission
// Name: Gayathri Anant
// Roll no. 22CS30026
// email: gayathrianant05@gmail.com

#include "ksocket.h"

int shmidSMk, shmidSOCKk, semSMk, semSOCKk, s1k, s2k;
currsock* currk;
ksocket *SMk;

void getshm(){
    if(SMk == NULL) {
        shmidSMk = shmget(ftok("makefile", SMKEY), sizeof(ksocket) * N, 0666);
        if(shmidSMk < 0) {
            perror("shmget failed for SMk");
            exit(1);
        }

        SMk = (ksocket *) shmat(shmidSMk, NULL, 0);
        if(SMk == (void *) -1){
            perror("Failed to attach shared memory for sockets");
            exit(1);
        }
    }

    if(currk == NULL) {
        shmidSOCKk = shmget(ftok("makefile", SOCKKEY), sizeof(currsock), 0666);
        if(shmidSOCKk < 0) {
            perror("shmget failed for currsock");
            exit(1);
        }

        currk = (currsock *) shmat(shmidSOCKk, NULL, 0);
        if(currk == (void *) -1){
            perror("Failed to attach shared memory for currsock");
            exit(1);
        }
    }

    // Initialize semaphores only once
    static int semaphores_initialized = 0;
    if (!semaphores_initialized) {
        semSMk = semget(ftok(SMPATH, SEMSMKEY), N, 0666);
        semSOCKk = semget(ftok(SOCKPATH, SEMSOCKKEY), 1, 0666);
        s1k = semget(ftok(S1PATH, S1KEY), 1, 0666);
        s2k = semget(ftok(S2PATH, S2KEY), 1, 0666);
        semaphores_initialized = 1;
    }
}

void lock(int semid, int semnum){
    struct sembuf sops;
    sops.sem_num = semnum;
    sops.sem_op = -1;
    sops.sem_flg = 0;
    semop(semid, &sops, 1);
    return;
}

void rel(int semid, int semnum){
    struct sembuf sops;
    sops.sem_num = semnum;
    sops.sem_op = 1;
    sops.sem_flg = 0;
    semop(semid, &sops, 1);
    return;
}

int k_socket(int domain, int type, int protocol){
    getshm();
    for(int i = 0; i < N; ++i){
        lock(semSMk, i);
        if(SMk[i].free){
            SMk[i].free = 0;
            SMk[i].pid = getpid();
            lock(semSOCKk, 0);
            currk->socket = -1;
            currk->addressvalid = 0;
            currk->idx = i;
            rel(semSOCKk, 0);
            rel(s1k, 0);
            lock(s2k, 0);
            rel(semSMk, i);
            return i;
        }
        rel(semSMk, i);
    }

    lock(semSOCKk, 0);
    currk->socket = -1;
    currk->addressvalid = 0;
    currk->idx = -1;
    rel(semSOCKk, 0);

    errno = ENOSPACE;

    return -1;
}

int k_bind(char * src_ip, int src_port, char * dst_ip, int dst_port){
    getshm();
    int sockfd = -1;
    int idx = 0;
    for(int i = 0; i < N; ++i){
        lock(semSMk, i);
        if(!SMk[i].free && SMk[i].pid == getpid()){
            idx = i;
            sockfd = SMk[i].socket;
            rel(semSMk, i);
            break;
        }
        rel(semSMk, i);
    }

    struct sockaddr_in my_addr, peer_addr;
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(src_port);
    my_addr.sin_addr.s_addr = inet_addr(src_ip);

    lock(semSMk, idx);
    SMk[idx].peer_addr.sin_family = AF_INET;
    SMk[idx].peer_addr.sin_port = htons(dst_port);
    SMk[idx].peer_addr.sin_addr.s_addr = inet_addr(dst_ip);
    SMk[idx].peervalid = 1;
    rel(semSMk, idx);

    lock(semSOCKk, 0);
    currk->socket = sockfd;
    currk->my_addr = my_addr;
    currk->addressvalid = 1;
    currk->idx = idx;
    rel(semSOCKk, 0);

    rel(s1k, 0);
    lock(s2k, 0);

    return 1;
}

int k_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen){
    getshm();
    int idx = sockfd;
    if(idx < 0 || idx >= N){
        errno = EBADF;
        return -1;
    }
    lock(semSMk, idx);
    if(!SMk[idx].peervalid || ntohs(SMk[idx].peer_addr.sin_port != ((struct sockaddr_in *) dest_addr)->sin_port) || SMk[idx].peer_addr.sin_addr.s_addr != ((struct sockaddr_in *) dest_addr)->sin_addr.s_addr){
        errno = ENOTBOUND;
        rel(semSMk, idx);
        return -1;
    }
    while(SMk[idx].send.size >= BUFSIZE){
        rel(semSMk, idx);
        sleep(2);
        lock(semSMk, idx);
    }
    // printf("Socket[%d]: send size = %d\n", idx, SMk[idx].send.size);
    if(SMk[idx].send.size < BUFSIZE){
        int i = (SMk[idx].send.st + SMk[idx].send.size)%BUFSIZE;
        // printf("Adding to send buffer at %d\n", i);
        SMk[idx].send_buffer[i].free = 0;
        memcpy(SMk[idx].send_buffer[i].data, buf, len);
        SMk[idx].send_buffer[i].data[len] = '\0';
        SMk[idx].send_buffer[i].seq = -1;
        SMk[idx].send_buffer[i].ack = -1;
        SMk[idx].send_buffer[i].rwsize = SMk[idx].recvw.rwsize;
        SMk[idx].send.size++;
        printf("Socket[%d]: Send size: %d\n", idx, SMk[idx].send.size);
        rel(semSMk, idx);
        return len;
    }
    rel(semSMk, idx);
    errno = ENOSPACE;
    return -1;
}

int k_recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen){
    getshm();
    int idx = sockfd;
    lock(semSMk, idx);
    *addrlen = sizeof(SMk[idx].peer_addr); 
    memcpy(src_addr, &SMk[idx].peer_addr, sizeof(SMk[idx].peer_addr));

    if(SMk[idx].recv.size > 0){
        int i = SMk[idx].recv.st;
        memcpy(buf, SMk[idx].recv_buffer[i].data, len);
        SMk[idx].recv_buffer[i].free = 1;
        SMk[idx].recv.st = (SMk[idx].recv.st + 1) % BUFSIZE;
        SMk[idx].recv.size--;

        SMk[idx].recvw.rw[(SMk[idx].recvw.end + 1) % W] = nextseq(SMk[idx].recvw.rw[SMk[idx].recvw.end]);
        // printf("This: %d:%d, next: %d:%d\n", SMk[idx].recvw.end, SMk[idx].recvw.rw[SMk[idx].recvw.end], (SMk[idx].recvw.end + 1) % W, SMk[idx].recvw.rw[(SMk[idx].recvw.end + 1) % W]);
        SMk[idx].recvw.rwsize++;
        SMk[idx].recvw.end = (SMk[idx].recvw.end + 1) % W;

        rel(semSMk, idx);
        return len;
    }
    rel(semSMk, idx);  
    errno = ENOMESSAGE;
    return -1;
}

int k_close(int sockfd){
    getshm();
    int idx = sockfd;
    // printf("inside k_close: %d\n", idx);
    lock(semSMk, idx);
    // printf("locked: %d\n", idx);
    SMk[idx].socket = -1;
    SMk[idx].pid = -1;
    SMk[idx].peervalid = 0;
    rel(semSMk, idx);
    return -1;
}

int dropMessage(){
    float r = rand() / (float) RAND_MAX;
    return (r < p);
}

int nextseq(int last){
    if(last == MAXSEQ - 1) return MAXSEQ;
    return (last + 1) % MAXSEQ;
}



