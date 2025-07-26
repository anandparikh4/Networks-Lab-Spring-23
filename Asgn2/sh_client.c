/* Networks Lab Assignment 2 Problem 2
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
#define SEND_BUF_SIZE 30
#define RECV_BUF_SIZE 30

/*custom receive function to receive all things in chunks and store in BUF*/
void my_recv(int sockfd , char * recv_buf, char * BUF){
    int recv_flag = 1;
    int recv_status;
    int idx = 0;
    while(recv_flag){
        recv_status = recv(sockfd , recv_buf , RECV_BUF_SIZE-1 , 0);
        if(recv_status < 0){
            perror("client::recv() failed");
            exit(0);
        }
        for(int i=0;i<recv_status;i++){
            BUF[idx++] = recv_buf[i];
            if(recv_buf[i] == '\0'){
                recv_flag = 0;
                break;
            }
        }
    }
}

int main(){

    int sockfd;                         // client sockfd
    struct sockaddr_in serv_addr;       // server address

    /*create socket*/
    if((sockfd = socket(AF_INET , SOCK_STREAM , 0)) < 0){
        perror("client::socket() failed");
        exit(0);
    }

    /*copy of server address at client side*/
    serv_addr.sin_family = AF_INET;
    inet_aton("127.0.0.1" , &serv_addr.sin_addr);
    serv_addr.sin_port = htons(PORT);

    /*connect attempt*/
    if(connect(sockfd , (struct sockaddr *)&serv_addr , sizeof(serv_addr)) < 0){
        perror("client::connect() failed");
        exit(0);
    }

    /*initialize buffers*/
    char send_buf[SEND_BUF_SIZE];
    char recv_buf[RECV_BUF_SIZE];
    char BUF[50];

    for(int i=0;i<RECV_BUF_SIZE;i++) recv_buf[i] = '\0';
    for(int i=0;i<SEND_BUF_SIZE;i++) send_buf[i] = '\0';

    int recv_status;
    /*receive login prompt*/
    my_recv(sockfd , recv_buf , BUF);

    printf("%s",BUF);
    scanf("%s",send_buf);

    /*send username*/
    int send_status;
    if((send_status = send(sockfd , send_buf , strlen(send_buf)+1 , 0)) < 0){
        perror("client::send() failed");
        exit(0);
    }

    /*receive validation status*/
    my_recv(sockfd , recv_buf , BUF);

    /*accordingly break the loop if access is unauthorized*/
    if(strcmp(BUF , "NOT-FOUND") == 0){
        printf("Invalid username\n");
        close(sockfd);
        return 0;
    }

    getchar();
    while(1){       // else, send the user's commands to the server sequentially
        printf("Enter command: ");
        
        int terminate = 0;
        int first_it = 1;
        int send_flag = 1;
        while(send_flag){
            int i;
            for(i=0;i<SEND_BUF_SIZE;i++){
                char temp;
                scanf("%c",&temp);
                if(i == 0 && temp == ' '){
                    i--;
                    continue;
                }
                if(temp == '\n'){
                    send_buf[i++] = '\0';
                    send_flag = 0;
                    break;
                }
                send_buf[i] = temp;
            }
            /*if exit, finish process*/
            if(first_it && strcmp(send_buf , "exit") == 0){
                terminate = 1;
                break;
            }
            /*else, send the command*/
            if((send_status = send(sockfd , send_buf , i , 0)) < 0){
                perror("server::send() failed");
                exit(0);
            }
            first_it = 0;
        }

        if(terminate) break;    // if flag is set, terminate

        /*receive the result of the command*/
        int recv_flag = 1;
        first_it = 1;
        while(recv_flag){
            if((recv_status = recv(sockfd , recv_buf , RECV_BUF_SIZE-1 , 0)) < 0){
                perror("client::recv() failed");
                exit(0);
            }
            /*handle all cases*/
            if(first_it == 1 && strcmp(recv_buf , "$$$$") == 0){    /*Invalid command, means something other than pwd, cd or dir was sent*/
                printf("Invalid command\n");
                break;
            }
            else if(first_it && strcmp(recv_buf , "####") == 0){    /*Error in running command, means that the command was valid, but the arguments were faulty or there was a runtime error*/
                printf("Error in running command\n");
                break;
            }
            
            /*else, execution was successful, the items returned are printed*/
            for(int i=0;i<recv_status;i++){
                if(recv_buf[i] == '\0'){
                    recv_flag = 0;
                    break;
                }
                printf("%c",recv_buf[i]);
            }

            first_it = 0;
        }

        printf("\n");
    }

    /*close socket after completion*/
    close(sockfd);

    return 0;
}

