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

    char buf[MSIZE];
    int idx = 0;

    while(1){
        sleep(2);
        sprintf(buf, "%d : Hello from user2", idx++);
        int bytes_sent = k_sendto(sockfd, buf, strlen(buf)+1, 0, (struct sockaddr *) &dest_addr, sizeof(dest_addr));
        if(bytes_sent < 0){
            printf("Failed to send\n");
            return 1;
        }
        // printf("Sent %d bytes\n", bytes_sent);
        printf("%s sent\n", buf);
    }

    k_close(sockfd);
    
    return 0;
}