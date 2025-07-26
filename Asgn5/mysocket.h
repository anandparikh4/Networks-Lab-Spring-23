#ifndef MYSOCKET_H
#define MYSOCKET_H

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>

#define SOCK_MyTCP 7 //socket type
#define MAX_LENGTH 5000 //max length of message in my_send/my_recv
#define TABLE_SIZE 10 //max no. of entries in table
#define SLEEP_TIME 1
#define CHUNK_SIZE 100 //chunk size used in send and recv calls

//structure of one message, stores pointer to buffer and number of bytes(len)
typedef struct msg{
	void *buff;
	int len;
}message;

//table has a dynamic array of message pointers
//table->entries is a circular buffer(queue)
//cnt denotes no. of entries filled in table->entries
//"in": next index (in table->entries) into which message pointer must be written
//"out": next index (in table->entries) from which message pointer must be read 
typedef struct table_{
	message **entries;
	int cnt;
	int in;
	int out;
}table;

//all Sock_MyTCP calls
int my_socket(int domain,int type,int protocol);
int my_bind(int sockfd,const struct sockaddr* addr,socklen_t len);
int my_listen(int sockfd,int n);
int my_accept(int sockfd,struct sockaddr* addr,socklen_t *len_ptr);
int my_connect(int sockfd, const struct sockaddr *addr, socklen_t len);
ssize_t my_send(int sockfd,const void *buf,size_t n,int flags);
ssize_t my_recv(int sockfd,void *buf,size_t n,int flags);
int my_close(int sockfd);

#endif