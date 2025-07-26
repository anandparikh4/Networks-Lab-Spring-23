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
#include <time.h>           // for local timestamp
#include <string.h>

#define SERV_PORT 8181

int main(){

    int sockfd;                                 // server socket id
    struct sockaddr_in serv_addr , cli_addr;    // server address, client address
    socklen_t clilen;                           // length of cli_addr

    /*create a new socket*/
    if((sockfd = socket(AF_INET , SOCK_DGRAM , 0)) < 0){
        perror("server::socket() failed");
        exit(0);
    }

    /*setup server address*/
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = SERV_PORT;
    
    /*server obviously needs to receive messages from clients, so bind address*/
    if(bind(sockfd , (struct sockaddr *)&serv_addr , sizeof(serv_addr)) < 0){
        perror("server::bind() failed");
        exit(0);
    }

    char recv_buf[20];
    char * send_buf;

    /*wait for clients*/  
    while(1){
        int recv_status;
        /*wait till some client sends a request to send the time*/
        clilen = sizeof(cli_addr);
        if((recv_status = recvfrom(sockfd , recv_buf , 20 , 0 , (struct sockaddr *)&cli_addr , &clilen)) < 0){
            perror("server::recvfrom() failed");
            exit(0);
        }

        // no need to check recv_status > 0 or the content of recv_buf either, since it is just a prompt from some client to send the time
        /*get system timestamp*/
        time_t tm;
        time(&tm);
        send_buf = strdup(ctime(&tm));

        /*send to client*/
        int send_status;
        if((send_status = sendto(sockfd , send_buf , strlen(send_buf)+1 , 0 , (struct sockaddr *)&cli_addr , sizeof(cli_addr))) < 0){
            perror("server::sendto() failed");
            exit(0);
        }

        free(send_buf);
        
        // parent's control flow, simply re-iterate
    }

}
