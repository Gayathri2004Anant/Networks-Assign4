// Assignment 4 Submission
// Name: Gayathri Anant
// Roll no. 22CS30026
// email: gayathrianant05@gmail.com

#include <stdio.h>
#include "ksocket.h"
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define MYPORT 5491
#define OTHERPORT 5490

int main()
{
    int sockfd = k_socket(AF_INET, SOCK_KTP, 0);
    if(sockfd < 0){
        printf("Failed to create socket\n");
        return 1;
    }
    printf("Socket created\n");
    fflush(stdout);

    if(k_bind(IP, MYPORT, IP, OTHERPORT) < 0){
        printf("Failed to bind\n");
        return 1;
    }

    struct sockaddr_in peer_addr;
    socklen_t peerlen = sizeof(peer_addr);

    char buf[MSIZE];

    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(OTHERPORT);
    dest_addr.sin_addr.s_addr = inet_addr(IP);

    FILE * fp = fopen("output2.txt", "w");
    if(fp == NULL){
        printf("Failed to open file\n");
        return 1;
    }

    int idx = 0;

    while(1){
        int n = k_recvfrom(sockfd, buf, MSIZE, 0, (struct sockaddr *) &peer_addr, &peerlen);
        if(n <= 0){
            printf("no message yet.. retrying to receive..\n");
            sleep(2);
            continue;
        }
        buf[n] = '\0';
        if(buf[0] == '$'){
            // send EOF signalling I'm done
            sprintf(buf, "$");
            while(k_sendto(sockfd, buf, strlen(buf)+1, 0, (struct sockaddr *) &dest_addr, sizeof(dest_addr)) < 0){
                printf("retrying to send exit character...\n");
                sleep(1);
            }
            sleep(TCLOSE);
            printf("Received EOF, breaking...\n");
            break;
        }
        printf("Received token %d\n", ++idx);
        fprintf(fp, "%s", buf);
    }
    printf("File received\n");
    fclose(fp);
    printf("closing socket\n");
    k_close(sockfd);
    printf("Exiting...\n");
    return 0;
}