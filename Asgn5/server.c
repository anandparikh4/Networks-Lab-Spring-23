#include "mysocket.h"
#include <stdio.h>
int main(){
	int sockfd,newsockfd;

	sockfd = my_socket(AF_INET,SOCK_MyTCP,0);
	struct sockaddr_in serv_addr,cli_addr;
	unsigned int clilen;
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(20000);
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	if(my_bind(sockfd,(struct sockaddr*)&serv_addr,sizeof(serv_addr))<0){
		perror("bind()");
		exit(0);
	}
	my_listen(sockfd,5);
	while(1){
		clilen = sizeof(cli_addr);
		newsockfd = my_accept(sockfd,(struct sockaddr*)&cli_addr,&clilen);
		if(newsockfd<0){
			perror("accept()");
			exit(0);
		}
		for(int i=0;i<15;i++){
			char arr[7000];
			printf("here\n");
			int n=my_recv(newsockfd,arr,50,0);
			arr[99]='\0';
			printf("%d %s\n",n,arr);
			printf("here\n");
			
			char str[] = "I am Soni Aditya Bharatbhai\n";
			my_send(newsockfd,str,strlen(str)+1,0);
		}
		my_close(newsockfd);
		printf("17\n");
	}
	my_close(sockfd);
}