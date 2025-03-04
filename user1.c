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

#define MYPORT 3491
#define OTHERPORT 3490

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

    // printf("Server running...\n");

    struct sockaddr_in peer_addr;
    socklen_t peerlen = sizeof(peer_addr);

    char buf[MSIZE];

    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(OTHERPORT);
    dest_addr.sin_addr.s_addr = inet_addr(IP);

    while(1){
        usleep(20000);
        while(k_recvfrom(sockfd, buf, MSIZE, 0, (struct sockaddr *) &peer_addr, &peerlen) < 0){
            sleep(2);
        }
        printf("%s received\n", buf);
    }

    k_close(sockfd);
    
    return 0;
}