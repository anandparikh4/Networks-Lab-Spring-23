#include "mysocket.h"

//table pointers, malloc'ed by my_socket
table *send_table;
table *recv_table;

//send_lock is mutex lock over send_table
//recv_lock is mutex lock over recv_table
//fd_lock is mutex lock over use_sockfd
pthread_mutex_t send_lock,recv_lock,fd_lock;

//Thread ids of threads S and R
pthread_t S_tid,R_tid;

//sockfd on which communication takes place
//in client it is set by my_connect call
//in server it is set by my_accept call
int use_sockfd;
int cleared;
//runners for threads R,S
void *R_runner(void *param);
void *S_runner(void *param);

//creates sockets,initializes tables, creates threads S,R
int my_socket(int domain,int type,int protocol){
	if(type!=SOCK_MyTCP){
		return -1;
	}
	
	int sockfd = socket(domain,SOCK_STREAM,protocol);
	if(sockfd<0){
		//error
		return sockfd;
	}

	//allocate space for 2 tables
	send_table = (table*)malloc(sizeof(table));
	recv_table = (table*)malloc(sizeof(table));

	//initialize cnt,in,out for tables
	send_table->cnt = recv_table->cnt = 0;
	send_table->in = recv_table->in = 0;
	send_table->out = recv_table->out = 0;
	
	//allocate space for 10 message* to be stored
	send_table->entries = (message**)malloc(TABLE_SIZE*sizeof(message*));
	recv_table->entries = (message**)malloc(TABLE_SIZE*sizeof(message*));

	//initialize locks and create threads
	pthread_attr_t attr;
	use_sockfd = -1;
	cleared=0;	
	pthread_mutex_init(&fd_lock,NULL);

	pthread_attr_init(&attr);
	pthread_mutex_init(&send_lock,NULL);
	pthread_create(&S_tid,&attr,S_runner,NULL);
	
	pthread_attr_destroy(&attr);

	pthread_attr_init(&attr);
	pthread_mutex_init(&recv_lock,NULL);
	pthread_create(&R_tid,&attr,R_runner,NULL);

	return sockfd;
}

//bind socket to addr
int my_bind(int sockfd,const struct sockaddr* addr,socklen_t len){
	return bind(sockfd,addr,len);
}

//listen call
int my_listen(int sockfd,int n){
	return listen(sockfd,n);
}

// accept call made by server, initializes use_sockfd
int my_accept(int sockfd,struct sockaddr* addr,socklen_t *len_ptr){
	int ret = accept(sockfd,addr,len_ptr);
	if(ret<0)return ret;
	pthread_mutex_lock(&fd_lock);
	use_sockfd = ret;
	pthread_mutex_unlock(&fd_lock);
	return ret;
}

// connect call made by client, initializes use_sockfd to sockfd
int my_connect(int sockfd, const struct sockaddr *addr, socklen_t len){
	int ret = connect(sockfd,addr,len);
	if(ret<0)return ret;
	pthread_mutex_lock(&fd_lock);
	use_sockfd = sockfd;
	pthread_mutex_unlock(&fd_lock);
	return ret;
}

//my_send call to copy message into the table
//dynamically allocates space for a new struct message, then copies the message pointer to send_table->entries
ssize_t my_send(int sockfd,const void *buf,size_t n,int flags){
	if(n==0)return 0;
	if(n>MAX_LENGTH)n=MAX_LENGTH; //only 5000 bytes can be sent at max
	message *msg = (message*)malloc(sizeof(message)); //allocate space for new message
	msg->buff = malloc(n); //allocate space for the buffer
	msg->len = n; //initialize length of above buffer
	memcpy(msg->buff,buf,n); //copy buf into msg->buff
	while(1){
		//as long as send_table is full, wait
		pthread_mutex_lock(&send_lock);
		if(send_table->cnt == TABLE_SIZE){
			pthread_mutex_unlock(&send_lock);
			sleep(SLEEP_TIME);
			continue;
		}
		//copy msg to send_table->entries queue, increment cnt and update send_table->in value
		send_table->entries[send_table->in] = msg;
		send_table->cnt = (send_table->cnt)+1;
		send_table->in = ((send_table->in)+1)%TABLE_SIZE;
		pthread_mutex_unlock(&send_lock);
		break;
	}
	return n;
}


//my_recv call to read message pointer from the table
//after obtaining the pointer from the table, we copy the bytes to buf
ssize_t my_recv(int sockfd,void *buf,size_t n,int flags){
	if(n==0)return 0;
	while(1){
		//as long as there is nothing to read from recv_table, wait
		pthread_mutex_lock(&recv_lock);
		if(recv_table->cnt==0){
			pthread_mutex_unlock(&recv_lock);
			sleep(SLEEP_TIME);
			continue;
		}
		//read message pointer into msg, decrement recv_table->cnt and update recv_table->out
		message *msg = (recv_table->entries)[recv_table->out];
		recv_table->cnt = (recv_table->cnt) - 1;
		recv_table->out = ((recv_table->out)+1)%TABLE_SIZE;
		pthread_mutex_unlock(&recv_lock);
		//since message length is shorter we update n
		if(n>msg->len){
			n = msg->len;
		}
		//copy msg->buff to buf and then free space for msg and msg->buff
		memcpy(buf,msg->buff,n);
		free(msg->buff);
		free(msg);
		break;
	}
	return n;
}

//this function sends num_bytes number of bytes stored in buff
//sending is done in chunks of size CHUNK_SIZE
//sockfd is the socket descriptor to be used
void send_in_chunks(int sockfd,void *buff,int num_bytes){
	int len;
	while(num_bytes>0){		
		if(num_bytes>CHUNK_SIZE)len = CHUNK_SIZE;
		else len = num_bytes;
		int numsent = send(sockfd,buff,len,0);
		if(numsent<=0)break;

		char *tmp = ((char*)buff);
		tmp = tmp+numsent;
		buff = (void*)tmp;
		num_bytes -= numsent;
	}
	return;
}

//runner function for thread S
//can be asynchronously cancelled
void *S_runner(void *param){
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,NULL);
	int sockfd = -1;
	while(1){
		//as long as sockfd is not set by my_accept/my_connect, wait
		if(sockfd < 0){
			pthread_mutex_lock(&fd_lock);
			sockfd = use_sockfd;
			pthread_mutex_unlock(&fd_lock);
			sleep(SLEEP_TIME);
			continue;
		}
		
		//as long as there is no message to be sent, wait
		pthread_mutex_lock(&send_lock);
		if(send_table->cnt==0){
			pthread_mutex_unlock(&send_lock);
			sleep(SLEEP_TIME);
			continue;
		}
		
		//copy message pointer from send_table->entries, update out and decrement cnt
		message *msg = (send_table->entries)[send_table->out];
		send_table->out = ((send_table->out)+1)%TABLE_SIZE;
		send_table->cnt = (send_table->cnt) - 1;
		pthread_mutex_unlock(&send_lock);

		//in each message first 4 bytes are message length and then there is message
		
		//send message length first
		void *arr = malloc(sizeof(int));
		memcpy(arr,&(msg->len),sizeof(int));
		send_in_chunks(sockfd,arr,sizeof(int));
		free(arr);
		
		//send the message content and then deallocate msg->buff,msg
		send_in_chunks(sockfd,msg->buff,msg->len);
		free(msg->buff);
		free(msg);
	}
}

//function to recv len number of bytes on sockfd and then store in buff
void recv_in_chunks(int sockfd,void *buff,int len){
	void *temp = buff;
	int chunk;
	while(len>0){
		if(len<CHUNK_SIZE){
			chunk = len;
		}
		else{
			chunk = CHUNK_SIZE;
		}
		int numrecv = recv(sockfd,temp,chunk,0);
		if(numrecv<=0)break;
		len -= numrecv;
		temp = (char*)temp;
		temp = temp + numrecv;
		temp = (void*)temp;
	}
	return;
}

//runner for thread R
//can be asynchronously cancelled
void *R_runner(void *param){
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,NULL);
	int sockfd = -1;
	int x = sizeof(int);
	char arr[sizeof(int)];
	while(1){
		//as long as use_sockfd is not set, wait
		if(sockfd<0){
			pthread_mutex_lock(&fd_lock);
			sockfd = use_sockfd;
			pthread_mutex_unlock(&fd_lock);
			sleep(SLEEP_TIME);
			continue;
		}
		
		//first 4 bytes received so as to get message length in bytes 
		recv_in_chunks(sockfd,(void*)(&arr[0]),x);
		int msg_len = *((int*)arr);
		
		//allocate buffer
		void *buff = malloc(msg_len);
		//receive the message in buff
		recv_in_chunks(sockfd,buff,msg_len);

		// allocate space for a new message
		message *msg = (message*)malloc(sizeof(message));
		msg->buff = buff;
		msg->len = msg_len;
		while(1){
			//as long as recv_table is full, wait
			pthread_mutex_lock(&recv_lock);
			if(recv_table->cnt==TABLE_SIZE){
				pthread_mutex_unlock(&recv_lock);
				sleep(SLEEP_TIME);
				continue;
			}

			//copy msg to recv_table->entries, increment cnt and update in
			int idx = recv_table->in;
			recv_table->entries[idx] = msg;
			recv_table->in = (idx+1)%TABLE_SIZE;
			recv_table->cnt = (recv_table->cnt)+1; 
			pthread_mutex_unlock(&recv_lock);
			break;
		}
	}
}

//deallocates all the buffers, the entries and the table t itself
void clean_table(table *t){
	int i = 0;
	while(i<t->cnt){
		i++;
		message *msg = (t->entries)[t->out];
		free(msg->buff);
		free(msg);
		t->out = ((t->out)+1)%TABLE_SIZE;
	}
	free(t->entries);
	free(t);
}

//closes sockfd, cleans up tables,buffers and kills threads
int my_close(int sockfd){
	sleep(5);
	if(cleared==1){
		return close(sockfd);
	}
	cleared=1;
	//send cancel request to threads
	pthread_cancel(S_tid);
	pthread_cancel(R_tid);
	
	//wait for threads to exit
	pthread_join(S_tid,NULL);
	pthread_join(R_tid,NULL);
	
	//close sockfd and clear tables
	int ret = close(sockfd);
	clean_table(send_table);
	clean_table(recv_table);
	return ret;
}