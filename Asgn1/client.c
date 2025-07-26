/* Networks Lab Assignment 1 Problem 2
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

#define PORT 20000
#define RECV_BUF_SIZE 10      // max number of bytes received in 1 recv() call, independent of server
#define SEND_BUF_SIZE 20      // max number of bytes sent in 1 send() call, independent of server

int main(){

    int sockfd;                     // client socket id
    struct sockaddr_in serv_addr;   // server address

    /*create client socket*/
    if((sockfd = socket(AF_INET , SOCK_STREAM , 0)) < 0){
        perror("client::socket() failed");
        exit(0);
    }

    /*setup server address*/
    serv_addr.sin_family = AF_INET;
    inet_aton("127.0.0.1" , &serv_addr.sin_addr);
    serv_addr.sin_port = htons(PORT);

    //char* send_buf = NULL;
    char send_buf[20];
    char recv_buf[RECV_BUF_SIZE];     

    /*attempt to connect to server*/
    if(connect(sockfd , (struct sockaddr *)&serv_addr , sizeof(serv_addr)) < 0){
        perror("client::connect() failed");
        exit(0);
    }

    /*get queries from user and send to server till user enters -1 or ctrl+C (interrupt)*/
    while(1){
        
        for(int i=0;i<RECV_BUF_SIZE;i++) send_buf[i] = recv_buf[i] = '\0';

        printf("\nEnter an arithmetic expression : ");

        int first_iteration = 1;    // unset flag after first chunk is scanned
        int terminate = 0;          // set flag if user enters -1
        while(1){
            int scan_flag = 1;      // keep scanning chunks of 20 till it is unset
            int i=0;
            for(i=0;i<SEND_BUF_SIZE;i++){   // scan 20
                char temp;
                scanf("%c",&temp);
                if(temp == ' '){            // ignore spaces
                    i--;
                    continue;
                }
                if(temp == '\n'){
                    send_buf[i++] = '\0';
                    scan_flag = 0;
                    break;
                }
                send_buf[i] = temp;
            }

            if(first_iteration && send_buf[0] == '-' && send_buf[1] == '1' && send_buf[2] == '\0'){ // if -1 is entered, break
                terminate = 1;
                break;
            }

            int send_status;
            // send the scanned chunk to server (only partial input expression sent)
            if((send_status = send(sockfd , send_buf , i , 0)) < 0){
                perror("client::send() failed");
                exit(0);
            }

            if(!scan_flag) break;
            first_iteration = 0;
        }

        if(terminate) break;        // if set, disconnect from server

        printf("Result = ");

        int recv_flag = 1;      // keep receiving in chunks till set
        while(recv_flag){
            int recv_status;
            if((recv_status = recv(sockfd , recv_buf , RECV_BUF_SIZE , 0)) < 0){    // receive 10
                perror("client::recv() failed");
                exit(0);
            }

            for(int i=0;i<RECV_BUF_SIZE;i++){       
                printf("%c",recv_buf[i]);           // print the received chunk (partial output)
                if(recv_buf[i] == '\0'){            // stop when \0 detected
                    recv_flag = 0;
                    break;
                }
            }
        }
        
        printf("\n");
    }

    /*terminate*/
    if(close(sockfd) < 0){
        perror("client::close() failed");
        exit(0);
    }

    return 0;
}