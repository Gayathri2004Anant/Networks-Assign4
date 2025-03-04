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
    SM[i].sendw.swsize = 0;
    for(int j = 0; j < W; ++j){
        SM[i].sendw.sw[j] = -1;
    }
    SM[i].recvw.st = 0;
    SM[i].recvw.end = W-1;
    SM[i].recvw.rwsize = W;
    for(int j = 0; j < W; ++j){
        SM[i].recvw.rw[j] = i+1;
    }
    SM[i].recvw.timer = -1;
    SM[i].sendw.lastSent = 0;
    return;
}

void signal_handler(int signo){
    if(signo == SIGINT){
        printf("Received SIGINT\n");
        shmdt(SM);
        shmdt(curr);
        shmctl(shmidSM, IPC_RMID, NULL);
        shmctl(shmidSOCK, IPC_RMID, NULL);
        semctl(semSM, 0, IPC_RMID);
        semctl(semSOCK, 0, IPC_RMID);
        semctl(s1, 0, IPC_RMID);
        semctl(s2, 0, IPC_RMID);
        exit(0);
    }
}

void construct_msg(packet *b, char *message, size_t len){
    sprintf(message, "%d\n%d\n%s\n%d", b->seq, b->ack, b->data, b->rwsize);
    return;
}

void deconstruct_msg(packet *b, char *message, size_t len){
    char *token = strtok(message, "\n");
    b->seq = atoi(token);
    token = strtok(NULL, "\n");
    b->ack = atoi(token);
    token = strtok(NULL, "\n");
    strcpy(b->data, token);
    token = strtok(NULL, "\n");
    b->rwsize = atoi(token);
    return;
}

void handlePacket(packet *b, int i, struct sockaddr_in *peer_addr){
    if(b->ack > 0){ //ack message
        //check if this is the ack to any message already sent
        int j;
        for(j = SM[i].sendw.base; j < SM[i].sendw.base + SM[i].sendw.currsize; ++j){
            if(b->ack == SM[i].sendw.sw[j % W]){
                break;
            }
        }
        if(j == SM[i].sendw.base + SM[i].sendw.currsize){ // not in window, ignore
            return;
        }
        SM[i].sendw.swsize = b->rwsize;
        for(int itr = SM[i].sendw.base; itr != (j+1)%W; itr = (itr+1)%W){

            SM[i].send_buffer[(SM[i].send.st + itr) % BUFSIZE].seq = -1;
            SM[i].send_buffer[(SM[i].send.st + itr) % BUFSIZE].free = 1;
            SM[i].send.size--;
            SM[i].send.st = (SM[i].send.st + 1) % BUFSIZE;

            SM[i].sendw.sw[itr] = -1;
            SM[i].sendw.currsize--;
            SM[i].sendw.base = (SM[i].sendw.base + 1) % W;
        }
        SM[i].sendw.timer = time(NULL);
    }
    else{ //data message
        if(b->seq == -1) return;
        if(b->seq == SM[i].recvw.rw[SM[i].recvw.st]){ //inorder message

            int acknum = b->seq;

            if(SM[i].recvw.rwsize <= 0 || SM[i].recv.size >= BUFSIZE) return;

            SM[i].recv_buffer[SM[i].recv.st + SM[i].recv.size].free = 0;
            SM[i].recv_buffer[SM[i].recv.st + SM[i].recv.size].seq = b->seq;
            SM[i].recv_buffer[SM[i].recv.st + SM[i].recv.size].ack = b->ack;
            strcpy(SM[i].recv_buffer[SM[i].recv.st + SM[i].recv.size].data, b->data);
            SM[i].recv.size++;

            int fst = (SM[i].recv.size + SM[i].recv.st) % W;
            int j = fst;
            while(SM[i].recv_buffer[j].free == 0){
                SM[i].recv.size++;
                acknum = SM[i].recv_buffer[j].seq;
                j = (j + 1) % W;
                if(j == fst) break;
            }

            SM[i].recvw.rw[SM[i].recvw.st] = -1;
            SM[i].recvw.rwsize--;
            SM[i].recvw.st = (SM[i].recvw.st + 1) % W;
            
            fst = SM[i].recv.st;
            while(SM[i].recvw.rw[SM[i].recvw.st] == -1){
                SM[i].recvw.rw[SM[i].recvw.st] = -1;
                SM[i].recvw.rwsize--;
                SM[i].recvw.st = (SM[i].recvw.st + 1) % W;
                if(SM[i].recvw.st == fst) break;
            }

            packet ack;
            ack.seq = -1;
            ack.ack = acknum;
            ack.rwsize = min(SM[i].recvw.rwsize, BUFSIZE - SM[i].recv.size);
            strcpy(ack.data, "This is an ack message");
            char message[MSIZE];
            construct_msg(&ack, message, MSIZE);
            sendto(SM[i].socket, message, MSIZE, 0, (struct sockaddr *) peer_addr, sizeof(*peer_addr));
            printf(">> Sent ack = %d, data = %s, window = %d\n", ack.ack, ack.data, ack.rwsize);
            fflush(stdout);
        }
        else{ //out of order message
            int stACK = SM[i].recvw.rw[SM[i].recvw.st];
            int idx = -1;
            for(int j = SM[i].recvw.st; j != (SM[i].recvw.end + 1)%W; ++j){
                idx++;
                if(b->seq == stACK){
                    break;
                }
                stACK = nextseq(stACK);
            }
            if(idx == -1) return;
            SM[i].recv_buffer[(SM[i].recv.st + SM[i].recv.size + idx) % BUFSIZE].free = 0;
            SM[i].recv_buffer[(SM[i].recv.st + SM[i].recv.size + idx) % BUFSIZE].seq = b->seq;
            SM[i].recv_buffer[(SM[i].recv.st + SM[i].recv.size + idx) % BUFSIZE].ack = b->ack;
            strcpy(SM[i].recv_buffer[(SM[i].recv.st + SM[i].recv.size + idx) % BUFSIZE].data, b->data);

            SM[i].recvw.rw[SM[i].recvw.st + idx] = -1;
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
                    if(!SM[i].free && SM[i].socket != -1){
                        FD_SET(SM[i].socket, &rfds);
                        if(SM[i].socket > maxfd){
                            maxfd = SM[i].socket;
                        }
                    }
                    rel(semSM, i);
                }
            }
            else{
                struct sockaddr_in peer_addr;
                socklen_t peerlen = sizeof(peer_addr);
                for(int i = 0; i < N; ++i){
                    lock(semSM, i);
                    if(FD_ISSET(SM[i].socket, &tempfds)){
                        char message[MSIZE];
                        recvfrom(SM[i].socket, message, MSIZE, 0, (struct sockaddr *) &peer_addr, &peerlen);
                        packet b;
                        deconstruct_msg(&b, message, MSIZE);
                        if(dropMessage()){
                            printf(">> Dropped packet seq = %d, ack = %d, data = %s, window = %d\n", b.seq, b.ack, b.data, b.rwsize);
                            fflush(stdout);
                            rel(semSM, i);
                            continue;
                        }
                        printf(">> Received seq = %d, ack = %d, data = %s, window = %d\n", b.seq, b.ack, b.data, b.rwsize);
                        fflush(stdout);
                        handlePacket(&b, i, &peer_addr);
                    }
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
            if(!SM[i].free){
                //check if timeout then send base
                if(SM[i].sendw.timer != -1 && time(NULL) - SM[i].sendw.timer >= T){
                    if(SM[i].sendw.currsize == 0) continue;
                    for(int j = 0; j < SM[i].sendw.currsize; ++j){
                        packet b = SM[i].send_buffer[(SM[i].sendw.base + j) % BUFSIZE];
                        printf(">> Timeout, retransmitting packet seq = %d, data = %s, window = %d\n", b.seq, b.data, b.rwsize);
                        fflush(stdout);
                        char message[MSIZE];
                        construct_msg(&b, message, MSIZE);
                        sendto(SM[i].socket, message, MSIZE, 0, (struct sockaddr *) &(SM[i].peer_addr), sizeof(SM[i].peer_addr));
                    }
                    SM[i].sendw.timer = time(NULL);
                }
                if(SM[i].sendw.currsize < SM[i].sendw.swsize && SM[i].send.size > 0){
                    packet b = SM[i].send_buffer[(SM[i].send.st + SM[i].sendw.currsize) % BUFSIZE];
                    b.seq = nextseq(SM[i].sendw.lastSent);
                    SM[i].send_buffer[(SM[i].send.st + SM[i].sendw.currsize) % BUFSIZE].seq = b.seq;
                    char message[MSIZE];
                    printf(">> Sending new packet seq = %d, data = %s, window = %d\n", b.seq, b.data, b.rwsize);
                    fflush(stdout);
                    construct_msg(&b, message, MSIZE);
                    sendto(SM[i].socket, message, MSIZE, 0, (struct sockaddr *) &(SM[i].peer_addr), sizeof(SM[i].peer_addr));
                    SM[i].sendw.sw[(SM[i].sendw.base + SM[i].sendw.currsize) % W] = b.seq;
                    SM[i].sendw.currsize++;
                    if(SM[i].sendw.currsize == 1) SM[i].sendw.timer = time(NULL);
                }
            }
            rel(semSM, i);
        }
        
    }
}

int main(){
    printf(">> Initiating ksocket server\n");
    signal(SIGINT, signal_handler);

    int key = ftok(FILEPATH, SMKEY);
    shmidSM = shmget(key, N*sizeof(ksocket), 0666 | IPC_CREAT);
    if(shmidSM == -1){
        perror(">> Failed to create shmidSM shared memory");
        exit(1);
    }
    key = ftok(FILEPATH, SOCKKEY);
    shmidSOCK = shmget(key, sizeof(currsock), 0666 | IPC_CREAT);
    if(shmidSOCK == -1){
        perror(">> Failed to create shmidSOCK shared memory");
        exit(1);
    }

    semSM = semget(ftok("makefile", SEMSMKEY), N, 0666 | IPC_CREAT);
    if(semSM == -1){
        perror(">> Failed to create semSM semaphore");
        exit(1);
    }
    semSOCK = semget(ftok("makefile", SEMSOCKKEY), 1, 0666 | IPC_CREAT);
    if(semSOCK == -1){
        perror(">> Failed to create semSOCK semaphore");
        exit(1);
    }
    s1 = semget(ftok("makefile", S1KEY), 1, 0666 | IPC_CREAT);
    if(s1 == -1){
        perror(">> Failed to create s1 semaphore");
        exit(1);
    }
    s2 = semget(ftok("makefile", S2KEY), 1, 0666 | IPC_CREAT);
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

    pthread_t r_thread, s_thread;

    if (pthread_create(&r_thread, NULL, R, NULL) != 0) {
        perror("Failed to create R thread");
        exit(1);
    }
    
    if (pthread_create(&s_thread, NULL, S, NULL) != 0) {
        perror("Failed to create S thread");
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
                printf("Socket %d created\n", curr->socket);
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

    return 0;

}