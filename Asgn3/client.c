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

#define RECV_BUF_SIZE 20

// custom receive function to receive message in chunks and print it on the go
void my_recv(int sockfd , char * recv_buf){
    int recv_flag = 1;
    while(recv_flag){
        int recv_status;
        if((recv_status = recv(sockfd , recv_buf , RECV_BUF_SIZE-1 , 0)) < 0){
            perror("client::recv() failed");
            exit(0);
        }
        for(int i=0;i<recv_status;i++){
            if(recv_buf[i] == '\0'){
                recv_flag = 0;
                break;
            }
            printf("%c",recv_buf[i]);
        }
    }
    return;
}

int main(int argc , char * argv[]){ //  ./ client.c PORT

    if(argc != 2){
        printf("Expected 2 arguments, got %d\n", argc);
        exit(0);
    }
    int PORT = atoi(argv[1]);       // get PORT (of server) from command line argument
    int sockfd;
    struct sockaddr_in serv_addr;

    /*create socket*/
    if((sockfd = socket(AF_INET , SOCK_STREAM , 0)) < 0){
        perror("client::socket() failed");
        exit(0);
    }

    /*setup server address*/
    serv_addr.sin_family = AF_INET;
    inet_aton("127.0.0.1" , &serv_addr.sin_addr);
    serv_addr.sin_port = htons(PORT);

    /*send connect request to server*/
    if(connect(sockfd , (struct sockaddr *)&serv_addr , sizeof(serv_addr)) < 0){
        perror("client::connect() failed");
        exit(0);
    }

    /*receive and print the time*/
    char recv_buf[RECV_BUF_SIZE];
    my_recv(sockfd , recv_buf);

    /*close socket*/
    if(close(sockfd) < 0){
        perror("client::close() failed");
        exit(0);
    }

    return 0;    
}