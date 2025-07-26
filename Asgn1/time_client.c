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

int main(){

   int sockfd;      // client socket id
   struct sockaddr_in serv_addr;   // server socket address

   /*create client socket*/
   if((sockfd = socket(AF_INET , SOCK_STREAM , 0)) < 0){
      perror("socket() failed");
      exit(0);
   }

   /*setup server address*/
   serv_addr.sin_family = AF_INET;
   inet_aton("127.0.0.1" , &serv_addr.sin_addr);
   serv_addr.sin_port = htons(20000);     

   /*connect to server*/         
   if(connect(sockfd , (struct sockaddr *)&serv_addr , sizeof(serv_addr)) < 0){
      perror("connect() failed");
      exit(0);
   }

   /*create and initialize a receive buffer*/
   char recv_buf[100];
   for(int i=0;i<100;i++) recv_buf[i] = '\0';

   /*receive from server*/
   if(recv(sockfd , recv_buf , 100 , 0) < 0){
      perror("recv() failed");
      exit(0);
   }

   /*Display message*/
   printf("%s\n",recv_buf);

   /*terminate connection*/
   if(close(sockfd) < 0){
      perror("close() failed");
      exit(0);
   }

   return 0;
}