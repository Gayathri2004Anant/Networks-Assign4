// Assignment 4 Submission
// Name: Gayathri Anant
// Roll no. 22CS30026
// email: gayathrianant05@gmail.com

#include "ksocket.h"

int shmidSM, shmidSOCK, semSM, semSOCK, s1, s2;
currsock* curr;
ksocket *SM;

void getshm(){
    shmidSM = shmget(ftok(FILEPATH, SMKEY), sizeof(ksocket) * N, 0666);
    shmidSOCK = shmget(ftok(FILEPATH, SOCKKEY), sizeof(currsock), 0666);
    semSM = semget(ftok("makefile", SEMSMKEY), 1, 0666);
    semSOCK = semget(ftok("makefile", SEMSOCKKEY), 1, 0666);
    s1 = semget(ftok("makefile", S1KEY), 1, 0666);
    s2 = semget(ftok("makefile", S2KEY), 1, 0666);

    SM = (ksocket *) shmat(shmidSM, NULL, 0);
    if(SM == (void *) -1){
        perror("Failed to attach shared memory");
        exit(1);
    }
    curr = (currsock *) shmat(shmidSOCK, NULL, 0);
    if(curr == (void *) -1){
        perror("Failed to attach shared memory");
        exit(1);
    }
    return;
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
        lock(semSM, i);
        if(SM[i].free){
            SM[i].free = 0;
            SM[i].pid = getpid();
            lock(semSOCK, 0);
            curr->socket = -1;
            curr->addressvalid = 0;
            curr->idx = i;
            rel(semSOCK, 0);
            rel(s1, 0);
            lock(s2, 0);
            rel(semSM, i);
            return i;
        }
        rel(semSM, i);
    }

    lock(semSOCK, 0);
    curr->socket = -1;
    curr->addressvalid = 0;
    curr->idx = -1;
    rel(semSOCK, 0);

    errno = ENOSPACE;

    return -1;
}

int k_bind(char * src_ip, int src_port, char * dst_ip, int dst_port){
    getshm();
    int sockfd = -1;
    int idx = 0;
    for(int i = 0; i < N; ++i){
        lock(semSM, i);
        if(!SM[i].free && SM[i].pid == getpid()){
            idx = i;
            sockfd = SM[i].socket;
            rel(semSM, i);
            break;
        }
        rel(semSM, i);
    }

    struct sockaddr_in my_addr, peer_addr;
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(src_port);
    my_addr.sin_addr.s_addr = inet_addr(src_ip);

    lock(semSM, idx);
    SM[idx].peer_addr.sin_family = AF_INET;
    SM[idx].peer_addr.sin_port = htons(dst_port);
    SM[idx].peer_addr.sin_addr.s_addr = inet_addr(dst_ip);
    SM[idx].peervalid = 1;
    rel(semSM, idx);

    lock(semSOCK, 0);
    curr->socket = sockfd;
    curr->my_addr = my_addr;
    curr->addressvalid = 1;
    curr->idx = idx;
    rel(semSOCK, 0);

    rel(s1, 0);
    lock(s2, 0);

    return 1;
}

int k_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen){
    getshm();
    int idx = sockfd;
    if(idx < 0 || idx >= N){
        errno = EBADF;
        return -1;
    }
    lock(semSM, idx);
    if(!SM[idx].peervalid || ntohs(SM[idx].peer_addr.sin_port != ((struct sockaddr_in *) dest_addr)->sin_port) || SM[idx].peer_addr.sin_addr.s_addr != ((struct sockaddr_in *) dest_addr)->sin_addr.s_addr){
        errno = ENOTBOUND;
        rel(semSM, idx);
        return -1;
    }
    if(SM[idx].send.size < BUFSIZE){
        int i = (SM[idx].send.st + SM[idx].send.size)%BUFSIZE;
        SM[idx].send_buffer[i].free = 0;
        memcpy(SM[idx].send_buffer[i].data, buf, len);
        SM[idx].send_buffer[i].seq = -1;
        SM[idx].send_buffer[i].ack = -1;
        SM[idx].send.size++;
        rel(semSM, idx);
        return len;
    }
    rel(semSM, idx);
    errno = ENOSPACE;
    return -1;
}

int k_recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen){
    getshm();
    int idx = sockfd;
    lock(semSM, idx);
    *addrlen = sizeof(SM[idx].peer_addr); 
    memcpy(src_addr, &SM[idx].peer_addr, sizeof(SM[idx].peer_addr));

    if(SM[idx].recv.size > 0){
        int i = SM[idx].recv.st;
        memcpy(buf, SM[idx].recv_buffer[i].data, len);
        SM[idx].recv_buffer[i].free = 1;
        SM[idx].recv.st = (SM[idx].recv.st + 1) % BUFSIZE;
        SM[idx].recv.size--;

        SM[idx].recvw.rw[(SM[idx].recvw.end + 1) % W] = nextseq(SM[idx].recvw.rw[SM[idx].recvw.end]);
        SM[idx].recvw.rwsize++;
        SM[idx].recvw.end = (SM[idx].recvw.end + 1) % W;

        rel(semSM, idx);
        return len;
    }
    rel(semSM, idx);  
    errno = ENOMESSAGE;
    return -1;
}

int k_close(int sockfd){
    getshm();
    int idx = sockfd;
    lock(semSM, idx);
    SM[idx].free = 1;
    SM[idx].socket = -1;
    SM[idx].pid = -1;
    SM[idx].peervalid = 0;
    rel(semSM, idx);
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



