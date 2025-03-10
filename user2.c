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

#define MYPORT 3490
#define OTHERPORT 3491

int main()
{
    int sockfd = k_socket(AF_INET, SOCK_KTP, 0);
    if(sockfd < 0){
        printf("Failed to create socket\n");
        return 1;
    }

    if(k_bind(IP, MYPORT, IP, OTHERPORT) < 0){
        printf("Failed to bind\n");
        return 1;
    }

    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(OTHERPORT);
    dest_addr.sin_addr.s_addr = inet_addr(IP);

    struct sockaddr_in peer_addr;
    socklen_t peerlen = sizeof(peer_addr);

    FILE * fp = fopen("input.txt", "r");
    if(fp == NULL){
        printf("Failed to open file\n");
        return 1;
    }

    // read by MSIZE no of bytes and send them.
    char buf[MSIZE];
    char msg[MSIZE];
    int n;
    int idx = 0;
    while((n = fread(buf, 1, PAYLOAD, fp)) > 0){
        strncpy(msg, buf, n);
        msg[n] = '\0';
        while(k_sendto(sockfd, msg, strlen(msg)+1, 0, (struct sockaddr *) &dest_addr, sizeof(dest_addr)) < 0){
            printf("retrying to send message..\n");
            sleep(2);
        }
        printf("Sent token %d\n", ++idx);
    }
    sprintf(buf, "$");
    while(k_sendto(sockfd, buf, strlen(buf)+1, 0, (struct sockaddr *) &dest_addr, sizeof(dest_addr)) < 0);
    printf("Sent EOF : %s\n", buf);

    fclose(fp);

    while(k_recvfrom(sockfd, buf, MSIZE, 0, (struct sockaddr *) &peer_addr, &peerlen) < 0){
        printf("retrying to recieve final exit confirmation...\n");
        sleep(1);
    }
    printf("Received final exit confirmation: %s\n", buf);
    printf("Exiting...\n");

    k_close(sockfd);
    
    return 0;
}