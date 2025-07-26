/* Networks Lab Assignment 2 Problem 1
   Name : Anand Manojkumar Parikh
   Roll : 20CS10007
   Spring 2023
*/

#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <string.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h> 
#include <poll.h>           // for struct pollfd, poll()

#define SERV_PORT 8181
#define MAX_ATTEMPTS 5      // try reconnecting atmost 5 times
#define TIMEOUT 3000        // wait for 3000ms (3s) before next attempt

int main(){

    int sockfd;                             // client address id
    struct sockaddr_in serv_addr;  // addresses of client and server, respectively
    socklen_t servlen;

    /*create client socket*/
    if((sockfd = socket(AF_INET , SOCK_DGRAM , 0)) < 0){
        perror("client::socket() failed");
        exit(0);
    }

    /*copy of server information at client end*/
    serv_addr.sin_family = AF_INET;
    inet_aton("127.0.0.1" , &serv_addr.sin_addr);
    serv_addr.sin_port = SERV_PORT;

    char send_buf[10];
    char recv_buf[50];
    strcpy(send_buf , "time_req");
    send_buf[8] = '\0';

    /*try connecting atmost MAX_ATTEMPTS times*/
    int trials;
    for(trials=0;trials<MAX_ATTEMPTS;trials++){ 
        if(trials == 0) printf("Connecting...\n");
        /*send a request to the server to return the time*/
        int send_status;
        if((send_status = sendto(sockfd , send_buf , strlen(send_buf) + 1 , 0 , (struct sockaddr*)&serv_addr , sizeof(serv_addr))) < 0){
            perror("client::sendto() failed");
            exit(0);
        }

        /*setup client socket id*/
        struct pollfd fdset[1];
        fdset[0].fd = sockfd;
        fdset[0].events = POLLIN;
        //fdset[0].revents = 0;     // necessary??
        
        /*poll on timeout and client socket id*/
        int poll_ret = poll(fdset , 1 , TIMEOUT);
        if(poll_ret < 0){
            perror("client::pollfd() failed");
            exit(0);
        }
        else if(poll_ret == 0){
            if(trials < MAX_ATTEMPTS - 1) printf("Retrying connection...\n");
            continue;        // retry on timeout
        }
        
        // else, poll_ret will be > 0 (1, in this case, since we are only polling one socket)
        /*receive message*/
        int recv_status;
        if((recv_status = recvfrom(sockfd , recv_buf , 50 , 0 , (struct sockaddr *)&serv_addr , &servlen)) < 0){
            perror("client::recvfrom() failed");
            exit(0);
        }

        /*print received message and terminate*/
        printf("%s\n",recv_buf);
        break;
    }

    if(trials == MAX_ATTEMPTS) printf("Timeout\n");

    return 0;
}
