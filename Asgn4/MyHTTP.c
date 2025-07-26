/*
Networks Lab Assignment 3 : HTTP Server
Authors: Anand Manojkumar Parikh (20CS10007)
         Soni Aditya Bharatbhai (20CS10060)
Spring 2023
*/

/*
Compile and run as:	gcc MyHTTP.c -o server
					./server
*/

//below two #define statements are to avoid the implicit function definition warning from strptime()
#define __USE_XOPEN
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/stat.h>
#include <time.h>

//port number and max size of header
#define PORT 8000
#define MAX_SIZE 4096

//used to store the header field names and attributes
typedef struct _pair{
	char *h_name;
	char *h_val;
}pair;

//method = GET/PUT, header_buff stores the header, file_extra is additional bytes of entity-body obtained while receiving header
char method[4],header_buff[MAX_SIZE],file_extra[20];

//len_extra is number of bytes in file_extra,header_cnt is number of elements in header_list
int len_extra,header_cnt;
pair* header_list;

//frees the arr and also h_name and h_val
void handle_free(pair *arr,int n){
	int i;
	if(arr==NULL)return;
	for(i=0;i<n;i++){
		if(arr[i].h_name!=NULL)free(arr[i].h_name);
		if(arr[i].h_val!=NULL)free(arr[i].h_val);
	}
	free(arr);
	return;
}

//parses the header array and stores fiels,values in header_list
pair* parse_header(char *header,int *line_num){
	int curr,line_idx,i,j;
	line_idx = curr = i = 0;
	int flag=0,cnt=0;
	while(cnt<4){
		if(header[i]=='\r')cnt+=1;
		else if(header[i]=='\n'){
			cnt+=1;
			line_idx++;
		}
		else cnt=0;
		i++;
	}
	if(line_idx<=1){
		return NULL;
	}
	pair *arr = (pair*)malloc((line_idx-1)*sizeof(pair));
	cnt = line_idx-1;
	memset(arr,0,(line_idx-1)*sizeof(pair));
	*line_num = line_idx-1;
	line_idx=0;
	i=curr=0;
	while(1){
		flag=0;
		while(curr!=2){
			if(header[i]=='\r'){
				if(curr!=0&&curr!=2){
					handle_free(arr,cnt);
					return NULL;
				}
				curr = 1;
				i++;
			}
			else if(header[i]=='\n'){
				if(curr!=1 && curr!=3){
					handle_free(arr,cnt);
					return NULL;
				}
				curr = 2;
				i++;
				line_idx++;
			}
			else{
				if(curr==1 || curr==3){
					handle_free(arr,cnt);
					return NULL;
				}
				flag=1;
				char c1;
				if(line_idx == 0){
					c1=' ';
					while(i<MAX_SIZE && header[i]!=' ')i++;
					if(i>=MAX_SIZE){
						handle_free(arr,cnt);
						return NULL;
					}
					i++;
				}
				else c1 = ':';
				int prev = i;
				while(i<MAX_SIZE && header[i]!=c1)i++;
				if(i>=MAX_SIZE){
					handle_free(arr,cnt);
					return NULL;
				}
				arr[line_idx].h_name = (char*)malloc((i-prev+1)*sizeof(char));
				for(j=prev;j<i;j++)arr[line_idx].h_name[j-prev] = header[j];
				arr[line_idx].h_name[j-prev] = '\0';
				prev = i+2;
				if(line_idx==0)prev--;
				while(i<MAX_SIZE && header[i]!='\r')i++;
				if(i>=MAX_SIZE){
					handle_free(arr,cnt);
					return NULL;
				}
				arr[line_idx].h_val = (char*)malloc((i-prev+1)*sizeof(char));
				for(j=prev;j<i;j++)arr[line_idx].h_val[j-prev] = header[j];
				arr[line_idx].h_val[j-prev] = '\0';
			}
		}
		if(flag==0)break;
		curr=0;
	}
	line_idx--;
	return arr;
}

//function to send buf in chunks of length chunk_size
void send_in_chunks(int newsockfd,char *buf,int len,int chunk_size){
	int i = 0;
	while(i<len){
		if(i+chunk_size-1<len){
			send(newsockfd,buf+i,chunk_size,0);
			i += chunk_size;
		}
		else{
			send(newsockfd,buf+i,len-i,0);
			break;
		}
	}
}


//gets Content-Length value from header if present
int get_content_len(pair *arr,int cnt){
	for(int i=0;i<cnt;i++){
		if(strcmp(arr[i].h_name,"Content-Length")==0){
			return atoi(arr[i].h_val);
		}
	}
	return -1;
}

//adds the client request in the specified format to AccessLog.txt
void add_entry(struct sockaddr_in *cli_addr){
	FILE *fp = fopen("AccessLog.txt","a+");
	if(fp==NULL){
		perror("fopen()");
		exit(0);
	}
	time_t t = time(NULL);
	struct tm x = *gmtime(&t);
	char buff[512],port[40];
	
	char *ip = inet_ntoa(cli_addr->sin_addr);
	sprintf(port,"%d",cli_addr->sin_port);
	sprintf(buff, "%02d%02d%02d:%02d%02d%02d:%s:%d:%s:%s", x.tm_mday, x.tm_mon + 1, x.tm_year + 1900 - 2000, x.tm_hour, x.tm_min, x.tm_sec,ip,ntohs(cli_addr->sin_port),method,header_list[0].h_name);
	
	fprintf(fp,"%s\n",buff);
	fclose(fp);
	return;
}

//returns month in arr based on m value
void get_month(int m,char *arr){
	char* months[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
	strcpy(arr,months[m]);
	return;
}

//returns day in arr based on d value
void get_day(int d,char *arr){
	char *days[7] = {"Sun","Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
	strcpy(arr,days[d]);
	return;
}

//gets GMT time in HTTP format, flag_cnt specifies number of days before current day
char *date_extractor(int flag_cnt){
	time_t t = time(NULL);
	t = t - (flag_cnt*24*60*60); //flag_cnt days before
	struct tm x = *gmtime(&t);
	char *buff = (char*)malloc(256*sizeof(char));
	char month[4]; 
	get_month(x.tm_mon,month);
	char day[4];
	get_day(x.tm_wday,day);
	sprintf(buff,"%s, %02d %s %d %02d:%02d:%02d GMT", day,x.tm_mday, month, x.tm_year + 1900, x.tm_hour, x.tm_min, x.tm_sec);
	return buff;
}

//gets If-Modified-Since field value from HTTP header if present
char *extract_date_from_header(){
	char *arr = NULL;
	for(int i=0;i<header_cnt;i++){
		if(strcmp(header_list[i].h_name,"If-Modified-Since")==0){
			arr = header_list[i].h_val;
			break;
		}
	}
	return arr;
}


//handles HTTP responses for status codes 400,403,404,304
void handle_failure(int newsockfd,int err){
	char response[MAX_SIZE];
	char *date = date_extractor(0);
	char html_content[100];
	// printf("ERROR : %d\n" , err);
	if(err == EACCES){
		//forbidden
		strcpy(html_content,"<div id=\"main\">\n<div class=\"fof\">\n<h1>403 Forbidden</h1>\n</div>\n</div>");
		sprintf(response,"HTTP/1.1 403 Fordidden\r\nExpires: %s\r\nCache-Control: no-store\r\nDate: %s\r\nContent-Length: %lu\r\nContent-Type: text/html\r\nContent-Language: en-us\r\n\r\n",date_extractor(-3),date,strlen(html_content));		
	}
	else if(err == ENOENT){
		//not found
		strcpy(html_content,"<div id=\"main\">\n<div class=\"fof\">\n<h1>404 Not Found</h1>\n</div>\n</div>");
		sprintf(response,"HTTP/1.1 404 Not Found\r\nExpires: %s\r\nCache-Control: no-store\r\nDate: %s\r\nContent-Length: %lu\r\nContent-Type: text/html\r\nContent-Language: en-us\r\n\r\n",date_extractor(-3),date,strlen(html_content));
	}
	else if(err == -2){
		//bad request
		strcpy(html_content,"<div id=\"main\">\n<div class=\"fof\">\n<h1>400 Bad Request</h1>\n</div>\n</div>");
		sprintf(response,"HTTP/1.1 400 Bad Request\r\nExpires: %s\r\nCache-Control: no-store\r\nDate: %s\r\nContent-Length: %lu\r\nContent-Type: text/html\r\nContent-Language: en-us\r\n\r\n",date_extractor(-3),date,strlen(html_content));
	}
	else if(err == -3){
		//not modified
		sprintf(response,"HTTP/1.1 304 Not Modified\r\nExpires: %s\r\nCache-Control: no-store\r\nDate: %s\r\n\r\n",date_extractor(-3),date);		
	}
	else{
		sprintf(response,"HTTP/1.1 500 Internal Server Error\r\nExpires: %s\r\nCache-Control: no-store\r\nDate: %s\r\n\r\n",date_extractor(-3),date);
	}
	printf("Response:\n%s\n\n",response);
	strcat(response,html_content);
	send_in_chunks(newsockfd,response,strlen(response),10);
	free(date);
	return;
}

//services put request of client and sends appropriate response
void service_put(int newsockfd){
	int status=0,err=0;
	FILE *fp = fopen(header_list[0].h_name,"wb");
	if(fp!=NULL){
		status = 200;
	}
	else err = errno;
	if(status == 200){
		fwrite(file_extra,1,len_extra,fp);
	}
	char buff[20];
	int len,tot_len = get_content_len(header_list,header_cnt) - len_extra;

	if(tot_len<0){
		handle_failure(newsockfd,-2);
		return;
	}

	while(tot_len>0){
		len = recv(newsockfd,buff,sizeof(buff),0);
		tot_len -= len;
		if(status==200){
			fwrite(buff,1,len,fp);
		}
	}
	if(fp!=NULL)fclose(fp);
	else{
		handle_failure(newsockfd,err);
		return;
	}
	//send response to client
	char response[MAX_SIZE];
	char *date = date_extractor(0);
	sprintf(response,"HTTP/1.1 200 Ok\r\nExpires: %s\r\nCache-Control: no-store\r\nDate: %s\r\nContent-Length: 0\r\n\r\n",date_extractor(-3),date);
	printf("Response:\n%s\n",response);
	free(date);
	send_in_chunks(newsockfd,response,strlen(response),10);
	return;
}

//gets the Content-Type field from HTTP header if present
char* get_content_type(){
	for(int i=0;i<header_cnt;i++){
		if(strcmp(header_list[i].h_name,"Content-Type")==0){
			return header_list[i].h_val;
		}
	}
	return NULL;
}

//services get request of the client and then sends appropriate response
void service_get(int newsockfd){
	FILE *fp = fopen(header_list[0].h_name,"rb");	
	if(fp!=NULL){
		fseek(fp, 0, SEEK_END); 
		int content_length=ftell(fp);
		rewind(fp);
		
		char buff[50];
		
		char *check_date = extract_date_from_header();
		
		struct stat file_attr;
		stat(header_list[0].h_name,&file_attr);
		time_t mod_seconds = file_attr.st_mtime;
		struct tm x = *gmtime(&mod_seconds);
		
		char last_modified[256],month[4],day[4]; 
		get_month(x.tm_mon,month);
		get_day(x.tm_wday,day);
		sprintf(last_modified,"%s, %02d %s %d %02d:%02d:%02d GMT", day,x.tm_mday, month, x.tm_year + 1900, x.tm_hour, x.tm_min, x.tm_sec);
		
		if(check_date!=NULL){
			struct tm check_tm;
  			strptime(check_date, "%a, %d %b %Y %H:%M:%S %Z", &check_tm);

			time_t check_seconds = timegm(&check_tm);
			if(file_attr.st_mtime < check_seconds){
				handle_failure(newsockfd,-3);
				return;	
			}
		}
		//send response before sending file in chunks
		char response[MAX_SIZE];
		char *content_type = get_content_type();
		if(content_type==NULL){
			handle_failure(newsockfd,-2);
			return;
		}
		char *date = date_extractor(0);
		sprintf(response,"HTTP/1.1 200 Ok\r\nExpires: %s\r\nCache-Control: no-store\r\nDate: %s\r\nContent-Language: en-us\r\nContent-Type: %s\r\nContent-Length: %d\r\nLast-Modified %s\r\n\r\n",date_extractor(-3),date,content_type,content_length,last_modified);
		free(date);
		printf("Response:\n%s\n",response);
		send_in_chunks(newsockfd,response,strlen(response),10);
		int y=0;
		while((y=fread(buff,1,50,fp))>0){
			send_in_chunks(newsockfd,buff,y,10);
		}
		fclose(fp);
	}
	else{
		//unable to open file
		handle_failure(newsockfd,errno);
		return;
	}
	return;
}

//function to receive the HTTP request in chunks
void receive_in_chunks(int newsockfd){
	int tot_len,len,i;
	tot_len=len=i=0;
	while(tot_len<3){
		len = recv(newsockfd,method+tot_len,3-tot_len,0);
		tot_len+=len;
	}
	method[tot_len] = '\0';
	strcpy(header_buff,method);
	char buff[20];
	int idx=3;
	int state = 0,flag=0;
	len_extra = 0;
	while(1){
		len = recv(newsockfd,buff,sizeof(buff),0);
		tot_len += len;
		if(len==0)break;
		for(i=0;i<len;i++){
			if(flag==1){
				file_extra[len_extra++] = buff[i];
				continue;
			}
			header_buff[idx++] = buff[i];
			if(buff[i]=='\r'){
				state = (state==0)? 1:3;
			}
			else if(buff[i]=='\n'){
				state = (state==1)? 2:4;
			}
			else state = 0;
			if(state==4){
				flag=1;
			}
		}
		if(flag==1)break;
	}
	printf("Request:\n");
	for(i=0;i<idx;i++){
		printf("%c",header_buff[i]);
	}
	return;
}

//main function, handles all socket creation,binding,accepting client requests,forking and then calling appropriate functions to service request
int main(){
	int sockfd,newsockfd;
	struct sockaddr_in serv_addr,cli_addr;
	
	sockfd = socket(AF_INET,SOCK_STREAM,0);
	if(sockfd<0){
		perror("socket() failed");
		exit(0);
	}
	
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);
	serv_addr.sin_addr.s_addr = INADDR_ANY;

	if(bind(sockfd,(struct sockaddr*)&serv_addr,sizeof(serv_addr))<0){
		perror("bind() failed");
		exit(0);
	}

	listen(sockfd,5);
	unsigned int cli_len;
	header_list = NULL;
	while(1){
		cli_len = sizeof(cli_addr);
		newsockfd = accept(sockfd,(struct sockaddr*)&cli_addr,&cli_len);
		if(newsockfd < 0){
			perror("accept() failed");
			continue;
		}

		int cpid = fork();
		if(cpid == 0){
			//child
			close(sockfd);
			receive_in_chunks(newsockfd);
			header_list = parse_header(header_buff,&header_cnt);
			if(header_list==NULL){
				handle_failure(newsockfd,-2);
				close(newsockfd);
				exit(0);
			}
			if(strcmp(method,"GET")==0)service_get(newsockfd);
			else if(strcmp(method,"PUT")==0)service_put(newsockfd);
			else{
				handle_failure(newsockfd,-2);
				close(newsockfd);
				exit(0);
			}
			add_entry(&cli_addr);
			close(newsockfd);
			handle_free(header_list,header_cnt);
			exit(0);
		}
		close(newsockfd);
	}
}