// Assignment 4 Submission
// Name: Gayathri Anant
// Roll no. 22CS30026
// email: gayathrianant05@gmail.com

#include "ksocket.h"
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <sys/select.h>
#include <signal.h>

int shmidSM, shmidSOCK, semSM, semSOCK, s1, s2;
currsock* curr;
ksocket *SM;
int numTransmissions;

int min(int a, int b){
    return a < b ? a : b;
}

void initSock(int i){
    SM[i].free = 1;
    SM[i].nospace = 0;
    SM[i].peervalid = 0;
    SM[i].socket = -1;
    for(int j = 0; j < BUFSIZE; ++j){
        SM[i].recv_buffer[j].free = 1;
        SM[i].recv_buffer[j].ack = -1;
        SM[i].recv_buffer[j].seq = -1;
        SM[i].recv_buffer[j].rwsize = -1;
        SM[i].send_buffer[j].free = 1;
        SM[i].send_buffer[j].seq = -1;
        SM[i].send_buffer[j].ack = -1;
        SM[i].send_buffer[j].rwsize = -1;
    }
    SM[i].pid = -1;
    SM[i].send.st = 0;
    SM[i].send.size = 0;
    SM[i].recv.st = 0;
    SM[i].recv.size = 0;
    SM[i].sendw.base = 0;
    SM[i].sendw.currsize = 0;
    SM[i].sendw.timer = -1;
    SM[i].sendw.swsize = W;
    for(int j = 0; j < W; ++j){
        SM[i].sendw.sw[j] = -1;
    }
    SM[i].recvw.st = 0;
    SM[i].recvw.end = W-1;
    SM[i].recvw.rwsize = W;
    for(int j = 0; j < W; ++j){
        SM[i].recvw.rw[j] = j+1;
    }
    SM[i].recvw.timer = -1;
    SM[i].sendw.lastSent = 0;
    SM[i].sendw.lastAcked = 0;
    return;
}

void signal_handler(int signo){
    if(signo == SIGINT){
        printf("Received SIGINT\n");
        shmctl(shmidSM, IPC_RMID, NULL);
        shmctl(shmidSOCK, IPC_RMID, NULL);
        semctl(semSM, 0, IPC_RMID);
        semctl(semSOCK, 0, IPC_RMID);
        semctl(s1, 0, IPC_RMID);
        semctl(s2, 0, IPC_RMID);
        shmdt(SM);
        shmdt(curr);
        printf(">> Shared memory and semaphores detached\n");
        printf(">> Value of numTransmissions: %d\n", numTransmissions);
        exit(0);
    }
}

void construct_msg(packet b, char *message){
    sprintf(message, "%d#%d#%s#%d", b.seq, b.ack, b.data, b.rwsize);
    fflush(stdout);
    return;
}

packet deconstruct_msg(char *message){
    packet b;
    char *token = strtok(message, "#");
    b.seq = atoi(token);
    token = strtok(NULL, "#");
    b.ack = atoi(token);
    token = strtok(NULL, "#");
    strcpy(b.data, token);
    token = strtok(NULL, "#");
    b.rwsize = atoi(token);
    return b;
}

void handlePacket(packet *b, int i, struct sockaddr_in *peer_addr){
    // printf("Socket[%d] >> recwnd: st = %d, end = %d, rwsize = %d\n", i, SM[i].recvw.st, SM[i].recvw.end, SM[i].recvw.rwsize);
    if(b->ack > 0){ //ack message
        //check if this is the ack to any message already sent
        printf("Socket[%d]>> Processing ack = %d, lastAcked = %d\n", i, b->ack, SM[i].sendw.lastAcked);
        if(b->ack == SM[i].sendw.lastAcked){
            SM[i].sendw.swsize = b->rwsize;
            return;
        }
        int j;
        for(j = SM[i].sendw.base; j < SM[i].sendw.base + SM[i].sendw.currsize; ++j){
            if(b->ack == SM[i].sendw.sw[j % W]){
                break;
            }
        }
        if(j == SM[i].sendw.base + SM[i].sendw.currsize){ // not in window, ignore
            printf("Socket[%d]>> Ignoring ack %d\n", i, b->ack);
            return;
        }
        SM[i].sendw.swsize = b->rwsize;
        SM[i].sendw.lastAcked = b->ack;
        int lim = SM[i].sendw.base + SM[i].sendw.currsize;
        int lim2 = SM[i].send.st + SM[i].sendw.currsize;
        for(int itr = SM[i].sendw.base; itr < lim; itr++){
            int idx = itr % W;
            int temp = SM[i].sendw.sw[idx];
            // printf("%d ", idx);
            SM[i].sendw.sw[idx] = -1;
            SM[i].sendw.currsize--;
            SM[i].sendw.base = (SM[i].sendw.base + 1) % W;
            if(temp == b->ack) break;
        }
        // printf("\n");
        for(int itr = SM[i].send.st; itr < lim2; itr++){
            int idx = itr % BUFSIZE;
            // printf("%d ", idx);
            int temp = SM[i].send_buffer[idx].seq;

            SM[i].send_buffer[idx].seq = -1;
            SM[i].send_buffer[idx].free = 1;
            SM[i].send.size--;
            SM[i].send.st = (SM[i].send.st + 1) % BUFSIZE;

            if(temp == b->ack) break;
        }
        #ifdef DEBUG
        // printf("\n");
        // printf("Socket[%d]>> ", i);
        // for(int j = 0; j < BUFSIZE; ++j) printf("%d ", SM[i].send_buffer[j].seq);
        // printf("\n");
        // printf("Socket[%d]>> ", i);
        // for(int j = 0; j < W; ++j) printf("%d ", SM[i].sendw.sw[j]);
        // printf("\n");
        #endif
        SM[i].sendw.timer = time(NULL);
    }
    else{ //data message
        if(b->seq == -1) return;
        printf("Socket[%d]>> Processing message seq = %d, rw[%d] = %d\n", i, b->seq, SM[i].recvw.st, SM[i].recvw.rw[SM[i].recvw.st]);
        if(b->seq == SM[i].recvw.rw[SM[i].recvw.st]){ //inorder message

            int acknum = b->seq;

            // if(SM[i].recvw.rwsize <= 0 || SM[i].recv.size >= BUFSIZE) return;

            SM[i].recv_buffer[(SM[i].recv.st + SM[i].recv.size)%BUFSIZE].free = 0;
            SM[i].recv_buffer[(SM[i].recv.st + SM[i].recv.size)%BUFSIZE].seq = b->seq;
            SM[i].recv_buffer[(SM[i].recv.st + SM[i].recv.size)%BUFSIZE].ack = b->ack;
            strcpy(SM[i].recv_buffer[(SM[i].recv.st + SM[i].recv.size)%BUFSIZE].data, b->data);
            printf("Socket[%d]>> Inorder message: buffer[%d] = seq %d\n", i, (SM[i].recv.st + SM[i].recv.size)%BUFSIZE, b->seq);
            SM[i].recv.size++;

            SM[i].recvw.rw[SM[i].recvw.st] = -1;
            SM[i].recvw.rwsize--;
            SM[i].recvw.st = (SM[i].recvw.st + 1) % W;

            int fst = (SM[i].recv.size + SM[i].recv.st) % W;
            int j = fst;
            while(SM[i].recv_buffer[j].free == 0){
                SM[i].recv.size++;
                SM[i].recvw.st = (SM[i].recvw.st + 1) % W;
                acknum = SM[i].recv_buffer[j].seq;
                j = (j + 1) % W;
                if(j == fst) break;
            }

            packet ack;
            ack.seq = -1;
            ack.ack = acknum;
            ack.rwsize = min(SM[i].recvw.rwsize, BUFSIZE - SM[i].recv.size);
            if(ack.rwsize <= 0){
                SM[i].recvw.timer = time(NULL);
                SM[i].nospace = 1;
            }
            else{
                SM[i].recvw.timer = -1;
                SM[i].nospace = 0;
            }
            strcpy(ack.data, "This is an ack message");
            char message[MSIZE];
            construct_msg(ack, message);
            sendto(SM[i].socket, message, MSIZE, 0, (struct sockaddr *) peer_addr, sizeof(*peer_addr));
            printf("Socket[%d]>> Sent ack = %d, window = %d\n", i, ack.ack, ack.rwsize);
            fflush(stdout);
        }
        else if(SM[i].recvw.rw[SM[i].recvw.st] == nextseq(b->seq)){ //prev ack was lost
            packet ack;
            ack.seq = -1;
            ack.ack = b->seq;
            ack.rwsize = min(SM[i].recvw.rwsize, BUFSIZE - SM[i].recv.size);
            if(ack.rwsize <= 0){
                SM[i].recvw.timer = time(NULL);
                SM[i].nospace = 1;
            }
            else{
                SM[i].recvw.timer = -1;
                SM[i].nospace = 0;
            }
            strcpy(ack.data, "This is an ack message");
            char message[MSIZE];
            construct_msg(ack, message);
            sendto(SM[i].socket, message, MSIZE, 0, (struct sockaddr *) peer_addr, sizeof(*peer_addr));
            printf("Socket[%d]>> Retransmitted ack = %d, window = %d\n", i, ack.ack, ack.rwsize);
            fflush(stdout);
        }
        else{ //out of order message
            int stACK = SM[i].recvw.rw[SM[i].recvw.st];
            int idx = -1;
            int j = SM[i].recvw.st;
            do{
                idx++;
                if(b->seq == stACK){
                    break;
                }
                stACK = nextseq(stACK);
                j = (j + 1) % W;

            }while(j != (SM[i].recvw.end + 1) % W);
        
            if(b->seq != stACK) return;
            SM[i].recv_buffer[(SM[i].recv.st + SM[i].recv.size + idx) % BUFSIZE].free = 0;
            SM[i].recv_buffer[(SM[i].recv.st + SM[i].recv.size + idx) % BUFSIZE].seq = b->seq;
            SM[i].recv_buffer[(SM[i].recv.st + SM[i].recv.size + idx) % BUFSIZE].ack = b->ack;
            strcpy(SM[i].recv_buffer[(SM[i].recv.st + SM[i].recv.size + idx) % BUFSIZE].data, b->data);

            printf("Socket[%d]>> Out of order message: buffer[%d] = seq: %d\n", i, (SM[i].recv.st + SM[i].recv.size + idx) % BUFSIZE, b->seq);
            if(SM[i].recvw.timer != -1) SM[i].recvw.timer = time(NULL);
            SM[i].recvw.rw[(SM[i].recvw.st + idx)%W] = -1;
            SM[i].recvw.rwsize--;
        }
    }
}

void *R(){
    printf(">> R thread started\n");
    while(1){
        fd_set rfds, tempfds;
        FD_ZERO(&rfds);
        int maxfd = 0;
        while(1){
            tempfds = rfds;
            struct timeval tv;
            tv.tv_sec = T;
            tv.tv_usec = 0;
            int retval = select(maxfd+1, &tempfds, NULL, NULL, &tv);

            if(retval < 0){
                printf(">> Error in select\n");
                fflush(stdout);
                exit(1);
            }
            else if(retval == 0){
                FD_ZERO(&rfds);
                for(int i = 0; i < N; ++i){
                    lock(semSM, i);
                    // if(i == 0) printf(">> R locked sem[0]\n");
                    if(!SM[i].free && SM[i].socket != -1){
                        FD_SET(SM[i].socket, &rfds);
                        if(SM[i].socket > maxfd){
                            maxfd = SM[i].socket;
                        }
                    }
                    // if(i == 0) printf(">> R releasing sem[0]\n");
                    rel(semSM, i);
                }
            }
            else{
                struct sockaddr_in peer_addr;
                socklen_t peerlen = sizeof(peer_addr);
                for(int i = 0; i < N; ++i){
                    lock(semSM, i);
                    // if(i == 0) printf(">> R locked sem[0]\n");
                    if(FD_ISSET(SM[i].socket, &tempfds)){
                        char message[MSIZE];
                        recvfrom(SM[i].socket, message, MSIZE, 0, (struct sockaddr *) &peer_addr, &peerlen);
                        packet b;
                        b = deconstruct_msg(message);
                        if(dropMessage()){
                            printf("Socket[%d]>> Dropped packet seq = %d, ack = %d, window = %d\n", i, b.seq, b.ack, b.rwsize);
                            fflush(stdout);
                            rel(semSM, i);
                            continue;
                        }
                        printf("Socket[%d]>> Received seq = %d, ack = %d, window = %d\n", i, b.seq, b.ack, b.rwsize);
                        fflush(stdout);
                        handlePacket(&b, i, &peer_addr);
                        printf("Socket[%d]>> Packet successfully handled\n", i);
                    }
                    // if(i == 0) printf(">> R releasing sem[0]\n");
                    rel(semSM, i);
                }
            }
        }
    }
}

void *S(){
    printf(">> S thread started\n");
    while(1){
        sleep(T/2);

        for(int i = 0; i < N; ++i){
            lock(semSM, i);
            // if(i == 0) printf(">> S locked sem[0]\n");
            if(!SM[i].free){
                //check if timeout then send base
                if(SM[i].sendw.timer != -1 && time(NULL) - SM[i].sendw.timer >= T){
                    if(SM[i].sendw.currsize >= SM[i].send.size) {
                        rel(semSM, i);
                        continue;
                    }
                    for(int j = 0; j < SM[i].sendw.currsize; ++j){
                        packet b = SM[i].send_buffer[(SM[i].sendw.base + j) % BUFSIZE];
                        printf("Socket[%d]>> Timeout, retransmitting packet seq = %d, window = %d\n", i, b.seq, b.rwsize);
                        fflush(stdout);
                        char message[MSIZE];
                        construct_msg(b, message);
                        sendto(SM[i].socket, message, MSIZE, 0, (struct sockaddr *) &(SM[i].peer_addr), sizeof(SM[i].peer_addr));
                        numTransmissions++;
                    }
                    SM[i].sendw.timer = time(NULL);
                }
                printf("Socket[%d]>> Currsize: %d Window size: %d Send size: %d\n", i, SM[i].sendw.currsize, SM[i].sendw.swsize, SM[i].send.size);
                if(SM[i].sendw.currsize < SM[i].sendw.swsize && SM[i].send.size > 0 && SM[i].sendw.currsize < SM[i].send.size){
                    // printf("SM[i].send.size: %d, SM[i].sendw.currsize: %d\n", SM[i].send.size, SM[i].sendw.currsize);
                    packet b = SM[i].send_buffer[(SM[i].send.st + SM[i].sendw.currsize) % BUFSIZE];
                    b.seq = nextseq(SM[i].sendw.lastSent);
                    SM[i].sendw.lastSent = b.seq;
                    SM[i].send_buffer[(SM[i].send.st + SM[i].sendw.currsize) % BUFSIZE].seq = b.seq;
                    char message[MSIZE];
                    construct_msg(b, message);
                    printf("Socket[%d]>> Sending new packet seq = %d, window = %d\n", i, b.seq, b.rwsize);
                    fflush(stdout);
                    sendto(SM[i].socket, message, MSIZE, 0, (struct sockaddr *) &(SM[i].peer_addr), sizeof(SM[i].peer_addr));
                    numTransmissions++;
                    SM[i].sendw.sw[(SM[i].sendw.base + SM[i].sendw.currsize) % W] = b.seq;
                    SM[i].sendw.currsize++;
                    SM[i].sendw.swsize--;
                    if(SM[i].sendw.currsize == 1) SM[i].sendw.timer = time(NULL);
                }
            }
            // if(i == 0) printf(">> S releasing sem[0]\n");
            if(SM[i].recvw.timer != -1 && (time(NULL) - SM[i].recvw.timer) > TNOSPACE && min(SM[i].recvw.rwsize, BUFSIZE - SM[i].recv.size) > 0){
                packet ack;
                ack.seq = -1;
                ack.ack = nextseq((SM[i].recvw.rw[SM[i].recvw.st]-2+MAXSEQ)%MAXSEQ);
                ack.rwsize = min(SM[i].recvw.rwsize, BUFSIZE - SM[i].recv.size);
                strcpy(ack.data, "This is an ack message");
                char message[MSIZE];
                construct_msg(ack, message);
                sendto(SM[i].socket, message, MSIZE, 0, (struct sockaddr *) &(SM[i].peer_addr), sizeof(SM[i].peer_addr));
                printf("Socket[%d]>> Retransmitted ack for updating window size = %d, window = %d\n", i, ack.ack, ack.rwsize);
                fflush(stdout);
                SM[i].recvw.timer = time(NULL);
            }
            rel(semSM, i);
        }
        
    }
}

void *G(){ //garbage collector
    while(1){
        sleep(2);
        for(int i = 0; i < N; ++i){
            lock(semSM, i);
            // if(i == 0) printf(">> G locked sem[0]\n");
            if(SM[i].free == 0 && SM[i].pid == -1){
                printf("Socket[%d]>> Process ended, resetting socket.\n", i);
                initSock(i);
            }
            rel(semSM, i);
            // if(i == 0) printf(">> G releasing sem[0]\n");
        }
    }
}

int main(){
    printf(">> Initiating ksocket server\n");
    signal(SIGINT, signal_handler);

    int key = ftok("makefile", SMKEY);
    printf(">> SM key: %d\n", key);
    shmidSM = shmget(key, N*sizeof(ksocket), 0666 | IPC_CREAT);
    if(shmidSM == -1){
        perror(">> Failed to create shmidSM shared memory");
        exit(1);
    }
    key = ftok("makefile", SOCKKEY);
    shmidSOCK = shmget(key, sizeof(currsock), 0666 | IPC_CREAT);
    if(shmidSOCK == -1){
        perror(">> Failed to create shmidSOCK shared memory");
        exit(1);
    }

    semSM = semget(ftok(SMPATH, SEMSMKEY), N, 0666 | IPC_CREAT);
    if(semSM == -1){
        perror(">> Failed to create semSM semaphore");
        exit(1);
    }
    semSOCK = semget(ftok(SOCKPATH, SEMSOCKKEY), 1, 0666 | IPC_CREAT);
    if(semSOCK == -1){
        perror(">> Failed to create semSOCK semaphore");
        exit(1);
    }
    s1 = semget(ftok(S1PATH, S1KEY), 1, 0666 | IPC_CREAT);
    if(s1 == -1){
        perror(">> Failed to create s1 semaphore");
        exit(1);
    }
    s2 = semget(ftok(S2PATH, S2KEY), 1, 0666 | IPC_CREAT);
    if(s2 == -1){
        perror(">> Failed to create s2 semaphore");
        exit(1);
    }

    printf(">> Creation of shared memory and semaphores successful\n");

    for(int i = 0; i < N; ++i){
        if (semctl(semSM, i, SETVAL, 1) == -1) {
            perror("semctl SETVAL failed");
            exit(1);
        }
    }

    if (semctl(semSOCK, 0, SETVAL, 1) == -1) {
        perror("semctl SETVAL failed");
        exit(1);
    }

    if (semctl(s1, 0, SETVAL, 0) == -1) {
        perror("semctl SETVAL failed");
        exit(1);
    }

    if (semctl(s2, 0, SETVAL, 0) == -1) {
        perror("semctl SETVAL failed");
        exit(1);
    }

    SM = (ksocket *) shmat(shmidSM, NULL, 0);
    if(SM == (void *) -1){
        perror("Failed to attach shared memory");
        exit(1);
    }

    for(int i = 0; i < N; ++i){
        initSock(i);
    }

    curr = (currsock *) shmat(shmidSOCK, NULL, 0);
    if(curr == (void *) -1){
        perror("Failed to attach shared memory");
        exit(1);
    }

    curr->addressvalid = 0;
    curr->socket = -1;
    curr->idx = -1;

    printf(">> Shared memory and semaphores created\n");

    numTransmissions = 0;

    pthread_t r_thread, s_thread, g_thread;

    if (pthread_create(&r_thread, NULL, R, NULL) != 0) {
        perror("Failed to create R thread");
        exit(1);
    }
    
    if (pthread_create(&s_thread, NULL, S, NULL) != 0) {
        perror("Failed to create S thread");
        exit(1);
    }

    if (pthread_create(&g_thread, NULL, G, NULL) != 0) {
        perror("Failed to create G thread");
        exit(1);
    }

    while(1){
        lock(s1, 0);
        lock(semSOCK, 0);
        if(curr->idx >= 0 && curr->socket == -1){
            curr -> socket = socket(AF_INET, SOCK_DGRAM, 0);
            if(curr->socket < 0){
                curr->idx = -1;
                continue;
            }
            else{
                SM[curr->idx].socket = curr->socket;
                SM[curr->idx].free = 0;
                printf(">> Socket %d created\n", curr->socket);
            }
        }
        if(curr->idx >= 0 && curr->socket >= 0 && curr->addressvalid){
            if(bind(curr->socket, (struct sockaddr *) &(curr->my_addr), sizeof(curr->my_addr)) >= 0){
                curr->idx = -1;
                curr->addressvalid = 0;
                curr->socket = -1;  
            }
        }
        rel(semSOCK, 0);
        rel(s2, 0);
    }

    pthread_join(r_thread, NULL);
    pthread_join(s_thread, NULL);
    pthread_join(g_thread, NULL);

    printf(">> Total number of transmissions: %d\n", numTransmissions);
    printf(">> Exiting ksocket server\n");

    return 0;

}