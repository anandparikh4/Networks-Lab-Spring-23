#include "mysocket.h"
#include <stdio.h>
int main(){
	int sockfd = my_socket(AF_INET,SOCK_MyTCP,0);
	if(sockfd<0){
		perror("socket");
		exit(0);
	}
	struct sockaddr_in servaddr;
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(20000);
	inet_aton("127.0.0.1",&servaddr.sin_addr);
	printf("here1\n");
	if(my_connect(sockfd,(struct sockaddr*)&servaddr,sizeof(servaddr))<0){
		perror("connect");
		exit(0);
	}
	for(int i=0;i<15;i++){
		char arr[7000];
		sprintf(arr,"send call %d\n",i+1);
		my_send(sockfd,arr,1+strlen(arr),0);
		printf("%d\n",i+1);
		my_recv(sockfd,arr,50,0);
		printf("%s",arr);
	}
	sleep(10);
	my_close(sockfd);
	exit(0);
}