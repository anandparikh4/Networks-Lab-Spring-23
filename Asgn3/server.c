/* Networks Lab Assignment 3
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

#define RECV_BUF_SIZE 30
#define MAX_CLI_QUEUE 5     // maximum client queue size

// custom receive function to receive in chunks and return 0,1 based on message received
int my_recv(int newsockfd , char * recv_buf){
    char buf[RECV_BUF_SIZE];
    int idx = 0;
    int recv_flag = 1;
    while(recv_flag){
        int recv_status;
        if((recv_status = recv(newsockfd , recv_buf , RECV_BUF_SIZE-1 , 0)) < 0){
            perror("client::recv() failed");
            exit(0);
        }
        for(int i=0;i<recv_status;i++){
            buf[idx++] = recv_buf[i];
            if(recv_buf[i] == '\0'){
                recv_flag = 0;
                break;
            }
        }
    }
    if(strcmp(buf , "Send Load") == 0) return 0;
    if(strcmp(buf , "Send Time") == 0) return 1;

    return -1;
}

int main(int argc , char * argv[]){ //  ./server.c PORT
    
    srand(time(NULL));  // to get different random numbers each time

    if(argc != 2){
        printf("Expected 2 arguments, got %d\n", argc);
        exit(0);
    }
    int PORT = atoi(argv[1]);       // get PORT from command line argument

    int sockfd , newsockfd;         // server original and duplicate socket ids
    struct sockaddr_in serv_addr;   // server address

    int clilen;                     // size of client address
    struct sockaddr_in cli_addr;    // client address

    /*create socket*/
    if((sockfd = socket(AF_INET , SOCK_STREAM , 0)) < 0){
        perror("server::socket() failed");
        exit(0);
    }

    /*setup address*/
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(PORT);

    /*bind address to socket*/
    if(bind(sockfd , (struct sockaddr *)&serv_addr , sizeof(serv_addr)) < 0){
        perror("server::bind() failed");
        exit(0);
    }

    /*wait for incoming connect requests*/
    listen(sockfd , MAX_CLI_QUEUE);

    while(1){                           // service each client
        clilen = sizeof(cli_addr);
        
        /*accecp connection*/
        if((newsockfd = accept(sockfd , (struct sockaddr *)&cli_addr , &clilen)) < 0){
            perror("server::accept() failed");
            exit(0);
        }

        char recv_buf[RECV_BUF_SIZE];
        /*receive message*/
        int recv_status = my_recv(newsockfd , recv_buf);

        if(recv_status == 0){       // send load
            int load = (rand()%100) + 1;        // random number in [1,100]
            int send_status;
            if((send_status = send(newsockfd , &load , sizeof(int) , 0)) < 0){
                perror("server::send() failed");
                exit(0);
            }
            printf("Load sent : %d\n",load);
            // time_t tm;
            // time(&tm);
            // char *send_buf = strdup(ctime(&tm));
            // printf("Curr Time : %s\n",send_buf);
            // free(send_buf);
        }

        else if(recv_status == 1){  // send time
            time_t tm;
            time(&tm);
            char *send_buf = strdup(ctime(&tm));
            int send_status;
            if((send_status = send(newsockfd , send_buf , strlen(send_buf)+1 , 0)) < 0){
                perror("server::send() failed");
                exit(0);
            }
            free(send_buf);
        }

        /*close duplicate socket*/
        if(close(newsockfd) < 0){
            perror("server::close() failed");
            exit(0);
        }
    }

    /*close original socket*/
    if(close(sockfd) < 0){
        perror("server::close() failed");
        exit(0);
    }

    return 0;
}