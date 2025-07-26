/* Networks Lab Assignment 1 Problem 1
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
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>           // for local timestamp
#include <string.h>

#define CLI_QUEUE_LIMIT 5

// Function calls are within "if" statements to check for failure and handle appropriately

int main(){

    /*create main socket*/
    int sockfd,newsockfd;           // original and dulicate server socket ids
    struct sockaddr_in serv_addr;   // server address

    int clilen;                     // client address size
    struct sockaddr_in cli_addr;    // client (active) address

    if((sockfd = socket(AF_INET , SOCK_STREAM , 0)) < 0){   // create a new socket
        perror("socket() failed");
        exit(0);
    }

    /*setup address structure*/
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;             
    serv_addr.sin_port = htons(20000);      // 20000 for demonstration purposes   

    /*bind address to socket*/
    if(bind(sockfd , (struct sockaddr*)&serv_addr , sizeof(serv_addr)) < 0){
        perror("bind() failed");
        exit(0);
    }

    /*server now waits for client connections (non-blocking call)*/
    listen(sockfd,CLI_QUEUE_LIMIT);         // CLI_QUEUE_LIMIT = 5

    while(1){
        clilen = sizeof(cli_addr);

        /*ready to accept client connections (blocking call)*/
        if((newsockfd = accept(sockfd,(struct sockaddr * )&cli_addr, &clilen)) < 0){
            perror("accept() failed");
            exit(0);
        }

        /*get system timestamp*/
        time_t tm;
        time(&tm);
        char *send_buf = strdup(ctime(&tm));

        /*send message to client*/
        if(send(newsockfd , send_buf , strlen(send_buf)+1 , 0) < 0){
            perror("send() failed");        // FIX THIS PART
            exit(0);
        }
        
        /*terminate connection*/
        if(close(newsockfd) < 0){
            perror("close() failed");
            exit(0);
        }

    }

    return 0;
}