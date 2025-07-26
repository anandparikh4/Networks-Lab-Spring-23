/*
Networks Lab Assignment 3 : HTTP Browser
Authors: Anand Manojkumar Parikh (20CS10007)
         Soni Aditya Bharatbhai (20CS10060)
Spring 2023
*/

/* DIRECTIONS OF USAGE--
    0) Compile and run as : gcc MyBrowser.c -o browser
                            ./browser

    1) In case content body is present in a PUT response, it is dumped in a file: ./server_response.html, and displayed in the browser

    2) While running the browser, if the prompt does not appear, it is because the child process has given some output/warnings (which can be ignored) to stdin
       So, if prompt is not visible after executing a command, simply press ENTER at least once to see it
       The command will run even if the prompt is not seen, since the prompt is printed and the browser does wait for input, just that it gets jumbled with the child output, which is inevitable

    3) In any requests, do not add any spaces at the end
*/

#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <poll.h>
#include <wait.h>

#define MAX 4096
#define CHUNK_SIZE 128
#define TIMEOUT 3000

// the following variables are accessed and edited in many functions, so making them global is a good choice
char req_buf[MAX];
char resp_buf[MAX];
char METHOD[4];
char IP[16];
char PATH[1024];
char PORT[6];
char FILENAME[1024];
char EXTN[10];
char FILEPATH[1024];

// header is parsed and stored in an array of pairs of char *  type
typedef struct _pair{
	char *h_name;
	char *h_val;
}pair;

pair* parse_header(char *header,int *line_num);

int min(long long a, long long b){
    return (a<b?a:b);
}

void get_month(int m,char *arr);

void get_day(int d,char *arr);

char *date_extractor();

// parse the user input to separate out the parameters (the global variables)
int parse(char * input){

    METHOD[0] = '\0';
    IP[0] = '\0';
    PATH[0] = '\0';
    PORT[0] = '\0';
    FILENAME[0] = '\0';
    EXTN[0] = '\0';
    FILEPATH[0] = '\0';

    int i = 0;
    while(i < strlen(input) && input[i] == ' ') i++;    // remove initial spaces
    if(i >= strlen(input)) return -1;

    // find out method
    if(strncmp(input+i , "GET" , 3) == 0) strcpy(METHOD , "GET");
    else if(strncmp(input+i , "PUT" , 3) == 0) strcpy(METHOD , "PUT");
    else if(strncmp(input+i , "QUIT" , 4) == 0){
        exit(0);
    }
    else{
        printf("Method not supported\n");
        return -1;
    }
    METHOD[3] = '\0';

    i+=3;
    while(i < strlen(input) && input[i] == ' ') i++;    // remove central spaces
    if(i >= strlen(input)){
        printf("URL not provided\n");
        return -1;
    }

    if(strncmp(input+i , "http://" , 7) == 0) i+=7;          // ignore "http://"

    // find out IP
    int j = i;
    while(j < strlen(input) && input[j] != '/') j++;
    strncpy(IP , input+i , j-i);
    IP[j-i] = '\0';
    if(i >= strlen(input)){
        return 0;
    }

    // find out path
    i = j;
    while(j < strlen(input) && input[j] != ' ' && input[j] != ':') j++;
    strncpy(PATH , input+i , j-i);
    PATH[j-i] = '\0';

    if(j < strlen(input) && input[j] == ':'){
        j++;
        i = j;
        while(j < strlen(input) && input[j] != ' ') j++;
        PORT[0] = '\0';
        strncpy(PORT , input+i , j-i);
        PORT[j-i] = '\0';
    }

    if(strcmp(METHOD , "PUT") == 0){
        while(j < strlen(input) && input[j] == ' ') j++;
        if(j >= strlen(input)){
            printf("File not provided\n");
            return -1;
        }

        i = j;
        while(j < strlen(input) && input[j] != ' ') j++;
        FILEPATH[0] = '\0';
        strncpy(FILEPATH , input+i , j-i);
        FILEPATH[j-i] = '\0';
    }

    i = strlen(input)-1;
    while(i>=0 && input[i] != '/' && input[i]!=' ')i--;
    i++;
    j = i;
    while(j < strlen(input) && input[j] != '.') j++;
    if(j >= strlen(input)){
        printf("No file extension provided\n");
        return -1;
    }

    strncpy(FILENAME , input+i , j-i);
    FILENAME[j-i] = '\0';

    i = j;
    while(j < strlen(input) && input[j] != ' ' && input[j] != ':') j++;
    strncpy(EXTN , input+i , j-i);
    EXTN[j-i] = '\0';

    // printf("METHOD - %s  " , METHOD);
    // printf("IP - %s  " , IP);
    // printf("PATH - %s  " , PATH);
    // printf("PORT - %s " , PORT);
    // printf("FILENAME - %s " , FILENAME);
    // printf("EXTN - %s\n" , EXTN);
    // if(strcmp(METHOD , "PUT") == 0) printf("FILEPATH - %s\n" , FILEPATH);
    return 0;
}

int make_request(){
    req_buf[0] = '\0';
    // Request Line
    strcat(req_buf , METHOD);

    strcat(req_buf , " ");
    strcat(req_buf , PATH);     // in case of GET request, PATH contains the filename also

    if(strcmp(METHOD , "PUT") == 0){    // filename added explicitly only in case of PUT request
        strcat(req_buf , "/");
        strcat(req_buf , FILENAME);
        strcat(req_buf , EXTN);
    }

    strcat(req_buf , " HTTP/1.1\r\n");

    // Common Headers
    strcat(req_buf , "Host: ");
    strcat(req_buf , IP);
    strcat(req_buf , "\r\n");

    strcat(req_buf , "Connection: close\r\n");

    strcat(req_buf , "Date: ");
    strcat(req_buf , date_extractor(0));
    strcat(req_buf , "\r\n");
    
    // Get Headers
    if(strcmp(METHOD , "GET") == 0){
        strcat(req_buf , "Accept: ");
        if(strcmp(EXTN , ".html") == 0) strcat(req_buf , "text/html");
        else if(strcmp(EXTN , ".pdf") == 0) strcat(req_buf , "application/pdf");
        else if(strcmp(EXTN , ".jpg") == 0) strcat(req_buf , "image/jpeg");
        else strcat(req_buf , "text/*");
        strcat(req_buf , "\r\n");

        strcat(req_buf , "Accept-Language: en-US, en;q=0.5\r\n");

        strcat(req_buf , "If-Modified-Since: ");
        strcat(req_buf , date_extractor(1));
        strcat(req_buf , "\r\n");
    }

    // Put Headers
    else{
        strcat(req_buf , "Content-Language: en-US\r\n");

        FILE * fp = fopen(FILEPATH , "r");
        if(fp == NULL){
            printf("File not found\n");
            return -1;
        }
        int byte_count;
        for(byte_count = 0; fgetc(fp) != EOF; ++byte_count);
        fclose(fp);
        char content_length[20];
        sprintf(content_length , "%d" , byte_count);
        strcat(req_buf , "Content-Length: ");
        strcat(req_buf , content_length);
        strcat(req_buf , "\r\n");
    }

    // Content type - general header
    strcat(req_buf , "Content-Type: ");
    if(strcmp(EXTN , ".html") == 0) strcat(req_buf , "text/html");
    else if(strcmp(EXTN , ".pdf") == 0) strcat(req_buf , "application/pdf");
    else if(strcmp(EXTN , ".jpg") == 0) strcat(req_buf , "image/jpeg");
    else strcat(req_buf , "text/*");
    strcat(req_buf , "\r\n");

    // Empty Line
    strcat(req_buf , "\r\n\0");     // \0 is for application purposes (send_in_chunks see this as the end of message sentinel), not sent to server
    
    return 0;
}

int check_response(int sockfd){

    int line_num;
    pair * arr = parse_header(resp_buf , &line_num);
    int status_code = atoi(arr[0].h_name);
    char * content_type = NULL;
    for(int i=0;i<line_num;i++){
        if(strcmp(arr[i].h_name , "Content-Type") == 0) content_type = strdup(arr[i].h_val);
    }
    free(arr);
                                                                                          // If file is not modified since the given time, then there is no response from the server
    if(status_code == 200 && strcmp(METHOD , "PUT") == 0 || status_code == 304 || status_code == 500) return 0; // if put request finishes succesfully, nothing to show

    int cpid = fork();  // so fork a process and outsource this job to it
    if(cpid < 0){
        perror("FATAL-client::fork() failed");
        close(sockfd);
        exit(0);        // fatal error, exit immediately
    }

    if(cpid == 0){      // child process
        close(sockfd);
        char * args[3];

        if(strncmp(content_type , "application/pdf" , 15) == 0 || strncmp(content_type , "image/jpeg", 10) == 0 || strncmp(content_type , "text/html" , 9) == 0) args[0] = strdup("xdg-open");
        else args[0] = strdup("gedit");     // gedit for all text/* files

        args[1] = (char *)malloc((strlen(FILENAME) + strlen(EXTN) + 1) * sizeof(char));
        args[1][0] = '\0';
        strcat(args[1] , FILENAME);
        strcat(args[1] , EXTN);
        strcat(args[1] , "\0");

        if(status_code/100 == 4){
            free(args[1]);
            args[1] = strdup("server_response.html");       // in case failure occurs (status code starts with 4) the output is stored in server_response.html
        }                                                   // else show the file that was received
                                                                    
        args[2] = NULL;

        execvp(args[0] , args);

        perror("client::execvp() failed");
        exit(0);
    }

    return 0;
}

int send_in_chunks(int sockfd){
    char * start = req_buf;
    int send_flag = 1;
    while(send_flag){
        int i;
        for(i=0;i<CHUNK_SIZE;i++){
            if(start[i] == '\0'){
                send_flag = 0;
                break;
            }
        }

        int send_status;
        if((send_status = send(sockfd , start , i , 0)) < 0){
            perror("client::send() failed");
            return -1;
        }
        start += i;
    }

    if(strcmp(METHOD , "PUT") == 0){
        FILE * fp = fopen(FILEPATH , "r");
        send_flag = 1;
        char buf[CHUNK_SIZE];
        int line_num, content_length,total = 0;
        pair * p = parse_header(req_buf , &line_num);
        for(int i=0;i<line_num;i++){
            if(strcmp(p[i].h_name , "Content-Length") == 0){
                content_length = atoi(p[i].h_val);
                break;
            }
        }
        free(p);
        while(send_flag){
            int i;
            for(i=0;i<CHUNK_SIZE;i++){
                buf[i] = fgetc(fp);
                total++;
                if(total == content_length){
                    send_flag = 0;
                    break;
                }
            }
            int send_status;
            if(!send_flag) i++;
            if((send_status = send(sockfd , buf , i , 0)) < 0){
                perror("client::send() failed");
                fclose(fp);
                return -1;
            }
        }
        fclose(fp);
    }

    return 0;
}

int recv_in_chunks(int sockfd){
    char * start = resp_buf;
    int state = 0;
    char pass_over[CHUNK_SIZE+1];
    int remaining_body = 0;
    int content_length , status_code;
    int pass_over_length , recv_status;
    while(1){
        if((recv_status = recv(sockfd , start , CHUNK_SIZE , 0)) < 0){
            perror("client::recv() failed");
            return -1;
        }
        if(recv_status == 0){
            start[0] = '\0';
            return 0;
        }

        int i;
        // simple automaton to find end of headers : detect the string "\r\n\r\n"
        for(i=0;i<recv_status;i++){
            char temp = start[i];
            if(state == 0){
                if(temp == '\r') state = 1;
                else state = 0;
            }
            else if(state == 1){
                if(temp == '\n') state = 2;
                else state = 0;
            }
            else if(state == 2){
                if(temp == '\r') state = 3;
                else state = 0;
            }
            else if(state == 3){
                if(temp == '\n'){
                    state = 4;
                    break;
                }
                else state = 0;
            }
        }

        if(state == 4){     // headers have ended, body has started
            int line_num;
            
            pair * p = parse_header(resp_buf , &line_num);
            for(int i=0;i<line_num;i++){
                if(strcmp(p[i].h_name , "Content-Length") == 0){
                    content_length = atoi(p[i].h_val);
                    break;
                }
            }
            status_code = atoi(p[0].h_name);
            if(strcmp(METHOD , "PUT") == 0 &&  status_code == 200 || status_code == 304 || status_code == 500){
                start[recv_status] = '\0';      // in case of internal server error, no html to render
                return 0;        // assuming that if a PUT request finishes succesfully, no content body is received
            }                    // and no content received when the file is not modified since the given date, so no server response
            pass_over[0] = '\0';
            pass_over_length = min(recv_status - i - 1 , content_length);
            for(int j = 0;j<pass_over_length;j++){
                pass_over[j] = *(start+i+1+j);
            }
            start[i+1] = '\0';
            pass_over[pass_over_length] = '\0';
            remaining_body = content_length - pass_over_length;
            free(p);
            break;
        }

        start += recv_status;
    }

    start[recv_status] = '\0';
    if(remaining_body <= 0) return 0;
    
    char file_name[1050];
    file_name[0] = '\0';
    strcat(file_name , FILENAME);
    strcat(file_name , EXTN);

    if(status_code/100 == 4) strcpy(file_name , "server_response.html");

    FILE * fp = fopen(file_name, "w");
    if(fp == NULL){
        perror("Error in creating/opening file\n");
        return -1;
    }
    for(int i=0;i<pass_over_length;i++){
        fprintf(fp , "%c" , pass_over[i]);
    }

    fflush(fp);

    char buf[CHUNK_SIZE+1];
    int total = 0;
    while(1){
        int recv_status;
        for(int i=0;i<=CHUNK_SIZE;i++) buf[i] = '\0';
        if((recv_status = recv(sockfd , buf , CHUNK_SIZE , 0)) < 0){
            perror("client::recv() failed");
            return -1;
        }
        total += recv_status;
        for(int i=0;i<recv_status;i++){
            fprintf(fp,"%c",buf[i]);
            fflush(fp);

        }
        if(recv_status == 0){
            fflush(fp);
            fclose(fp);
            return 0;
        }
    }
    return 0;
}

int main(){

    while(1){       // wait for user's input

        int sockfd;
        if((sockfd = socket(AF_INET , SOCK_STREAM , 0)) < 0){       // create a new socket
            perror("client::socket() failed");
            exit(0);
        }

        printf("MyOwnBrowser> ");       // prompt
        size_t input_size = 1024;
        char * input = (char *)malloc(input_size * sizeof(char));   // take use input
        getline(&input , &input_size , stdin);

        input[strlen(input)-1] = '\0';
        
        if(parse(input) < 0){       // parse the input to separate out the parameters
            free(input);
            close(sockfd);
            continue;
        }

        // setup server address based on user's input
        struct sockaddr_in serv_addr;
        serv_addr.sin_family = AF_INET;
        inet_aton(IP , &serv_addr.sin_addr);
        int _port = 80;
        if(strlen(PORT) > 0) _port = atoi(PORT);
        serv_addr.sin_port = htons(_port);

        // connect to server
        if(connect(sockfd , (struct sockaddr *)&serv_addr , sizeof(serv_addr)) < 0){
            perror("client::connect() failed");
            free(input);
            close(sockfd);
            continue;
        }

        // printf("Connected!\n");
        // create a request in standard HTTP format
        if(make_request() < 0){
            free(input);
            close(sockfd);
            continue;
        }
        // send the request message (in chunks)
        if(send_in_chunks(sockfd) < 0){
            free(input);
            close(sockfd);
            continue;
        }
        printf("\nRequest---\n%s" , req_buf);

        // then , wait for response with a timeout of 3000 ms = 3 s
        struct pollfd fdset[1];
        fdset[0].fd = sockfd;
        fdset[0].events = POLLIN;

        int retval = poll(fdset , 1 , TIMEOUT);
        if(retval < 0){
            perror("client::poll() failed");
            free(input);
            close(sockfd);
            continue;
        }
        if(retval == 0){    // if timeout, go back to prompt
            printf("Connection timed out\n");
            free(input);
            close(sockfd);
            continue;
        }

        // else, receive the response message (in chunks)
        if(recv_in_chunks(sockfd) < 0){
            free(input);
            close(sockfd);
            continue;
        }
        printf("Response---\n%s" , resp_buf);

        // parse the headers received to extract the status code and message
        int line_num;
        pair * p = parse_header(resp_buf , &line_num);
        printf("Status code received: %s\n" , p[0].h_name);
        printf("Message: %s\n\n" , p[0].h_val);

        check_response(sockfd);     // check the response message and fire the respective application to render the resceived content / error 

        close(sockfd);  // close socket

        // printf("Connection closed\n");
        free(input);
    }

    return 0;    
}

// the following 3 functions help to get the date in GMT HTTP format
void get_month(int m,char *arr){
	char* months[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
	strcpy(arr,months[m]);
	return;
}

void get_day(int d,char *arr){
	char *days[7] = {"Sun","Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
	strcpy(arr,days[d]);
	return;
}

char *date_extractor(int flag_2){
	time_t t = time(NULL);
	if(flag_2)t = t - (48*60*60); //two days before
	struct tm x = *gmtime(&t);
	char *buff = (char*)malloc(256*sizeof(char));
	char month[4]; 
	get_month(x.tm_mon,month);
	char day[4];
	get_day(x.tm_wday,day);
	sprintf(buff,"%s, %02d %s %d %02d:%02d:%02d GMT", day,x.tm_mday, month, x.tm_year + 1900, x.tm_hour, x.tm_min, x.tm_sec);
	return buff;
}
// parsing the received headers at the client (browser) side
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
	pair *arr = (pair*)malloc((line_idx-1)*sizeof(pair));
	*line_num = line_idx-1;
	line_idx=0;
	i=0;
	while(1){
		flag=0;
		while(curr!=2){
			if(header[i]=='\r'){
				curr = 1;
				i++;
			}
			else if(header[i]=='\n'){
				curr = 2;
				i++;
				line_idx++;
			}
			else{
				flag=1;
				char c1;
				if(line_idx == 0){
					c1=' ';
					while(header[i]!=' ')i++;
					i++;
				}
				else c1 = ':';
				int prev = i;
				while(header[i]!=c1)i++;
				arr[line_idx].h_name = (char*)malloc((i-prev+1)*sizeof(char));
				for(j=prev;j<i;j++)arr[line_idx].h_name[j-prev] = header[j];
				arr[line_idx].h_name[j-prev] = '\0';
				prev = i+2;
				if(line_idx==0)prev--;
				while(header[i]!='\r')i++;
				arr[line_idx].h_val = (char*)malloc((i-prev+1)*sizeof(char));
				for(j=prev;j<i;j++)arr[line_idx].h_val[j-prev] = header[j];
				arr[line_idx].h_val[j-prev] = '\0';
			}
		}
		if(flag==0)break;
		curr=0;
	}
	line_idx--;
	for(i=0;i<line_idx;i++){
		// printf("%s %s\n",arr[i].h_name,arr[i].h_val);
	}
	return arr;
}