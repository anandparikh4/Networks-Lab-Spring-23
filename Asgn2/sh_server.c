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
#include <dirent.h>

#define PORT 20000
#define MAX_CLI_QUEUE 5     
#define SEND_BUF_SIZE 40
#define RECV_BUF_SIZE 40
#define MAX_PATH_SIZE 4096

// character vector (extensible array) implementation
struct vector{
    char * vec;
    int len,max;
};

// initialize members
void init(struct vector * v){
    v->vec = NULL;
    v->len = 0;
    v->max = 0;
    return;
}

// push element to the back
void push(struct vector * v, char x){
    if(v->max == 0){
        v->vec = (char *)malloc(sizeof(char));
        v->max = 1;
    }
    else if(v->max < v->len + 1){
        v->max = v->max * 2;
        char * vec_new = (char *)realloc(v->vec,(v->max) * sizeof(char));
        if(vec_new) v->vec = vec_new;
        else{
            perror("client::vector::realloc() failed");
            exit(0);
        }
    }
    v->vec[(v->len)++] = x;
    return;
}

// clear vector (de-allocate space, if any)
void clear(struct vector * v){
    if(v->len > 0) free(v->vec);
    v->vec = NULL;
    v->len = 0;
    v->max = 0;
    return;
}

// a custom wrapper around the generic send function with a flag to control it's behavior
void my_send(int newsockfd , char * send_buf, char * buf,int flag){
    //printf("%s\n" , buf);

    int j=0;
    int send_flag = 1;
    int send_status;
    while(send_flag){
        int i;
        for(i=0;i<SEND_BUF_SIZE;i++){
            if(flag == 1 && buf[j] == '\0'){        // if flag is set, then on encountering a \0, append a \n to the buffer and send that
                send_buf[i++] = '\n';               // this was introduced so that the returns of the "dir" command which are sent in multiple loops of send are received in one loop of receive 
                send_flag = 0;
                break;
            }
            send_buf[i] = buf[j];
            if(buf[j++] == '\0'){
                i++;
                send_flag = 0;
                break;
            }
        }
        if((send_status = send(newsockfd , send_buf , i , 0)) < 0){
            perror("server::send() failed");
            exit(0);
        }
    }

    return;
}

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
    
    int sockfd, newsockfd;          // server original and duplicate ids
    struct sockaddr_in serv_addr;   // server address
    int clilen;                     // length of client address
    struct sockaddr_in cli_addr;    // client address (set later)

    /*create socket*/
    if((sockfd = socket(AF_INET , SOCK_STREAM , 0)) < 0){
        perror("server::socket() failed");
        exit(0);
    }

    /*setup server address*/
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(PORT);

    /*bind address to socket*/
    if(bind(sockfd , (struct sockaddr *)&serv_addr , sizeof(serv_addr)) < 0){
        perror("server::bind() failed");
        exit(0);
    }

    /*listen for connections*/
    listen(sockfd , MAX_CLI_QUEUE);

    /*initialize buffers*/
    char send_buf[SEND_BUF_SIZE];
    char recv_buf[RECV_BUF_SIZE];
    char BUF[RECV_BUF_SIZE];

    /*wait for clients' connect request*/
    while(1){
        clilen = sizeof(cli_addr);
        /*accept request*/
        if((newsockfd = accept(sockfd , (struct sockaddr *)&cli_addr , &clilen)) < 0){
            perror("server::accept() failed");
            exit(0);
        }

        /*fork to child process after accepting*/
        if(fork() == 0){
            
            /*close sockfd since it is not used by the child*/
            close(sockfd);

            /*send acknowledgement "LOGIN" to client*/
            for(int i=0;i<SEND_BUF_SIZE;i++) send_buf[i] = '\0';
            strcpy(send_buf , "LOGIN:");
            int send_status;
            if((send_status = send(newsockfd , send_buf , strlen(send_buf)+1 , 0)) < 0){
                perror("server::send() failed");
                exit(0);
            }

            /*receive username*/
            int recv_status;
            my_recv(newsockfd , recv_buf , BUF);

            /*check for existence of the username in the file users.txt (present in the same directory as the server)*/
            FILE* fP;
            fP = fopen("./users.txt", "r");

            int user_found = 0;
            char name_buf[RECV_BUF_SIZE];
            while(fgets(name_buf , RECV_BUF_SIZE , fP)){
                for(int i=0;i<RECV_BUF_SIZE;i++){                   // replace the \n by \0 for correct comparisions
                    if(name_buf[i] == '\n') name_buf[i] = '\0';
                }
                if(strcmp(name_buf , BUF) == 0){       // compare against every line in the file
                    user_found = 1;
                    break;
                }
            }

            fclose(fP);

            /*send appropriate message*/
            for(int i=0;i<SEND_BUF_SIZE;i++) send_buf[i] = '\0';
            if(user_found) strcpy(send_buf , "FOUND");
            else strcpy(send_buf , "NOT-FOUND");

            if((send_status = send(newsockfd , send_buf , strlen(send_buf)+1 , 0)) < 0){
                perror("server::send() failed");
                exit(0);
            }

            /*if user is found, start receiving commands*/
            while(user_found){       // receive each command, only if valid user
                /*command can be very long and of variable length, so using a custom vector structure to store it is the best choice*/
                struct vector v;
                init(&v);

                int terminate = 0;
                int recv_flag = 1;
                /*receive command*/
                while(recv_flag){
                    if((recv_status = recv(newsockfd , recv_buf , RECV_BUF_SIZE , 0)) < 0){
                        perror("server::recv() failed");
                        exit(0);
                    }
                    if(recv_status == 0){
                        terminate = 1;
                        break;
                    }
                    for(int i=0;i<recv_status;i++){
                        push(&v , recv_buf[i]);
                        if(recv_buf[i] == '\0'){
                            recv_flag = 0;
                            break;
                        }
                        //printf("%c" , recv_buf[i]);
                    }
                }

                if(terminate) break;                // if flag is set, terminate process

                // execute the commands (seperately handled)

                char buf[MAX_PATH_SIZE];

            // pwd
                if(strlen(v.vec)>=3 && v.vec[0] == 'p' && v.vec[1] == 'w' && v.vec[2] == 'd'){
                    if(strlen(v.vec) > 3 && v.vec[3] != ' '){        // invalid command
                        strcpy(buf , "$$$$");
                        buf[4] = '\0';
                    }
                    else{
                        if(getcwd(buf , MAX_PATH_SIZE) == NULL){    // if not NULL, then getcwd automatically writes the directory name into buf
                            strcpy(buf , "####");   // else error
                            buf[4] = '\0';
                        }
                    }
                    my_send(newsockfd , send_buf , buf, 0);
                }

            // cd
                else if(strlen(v.vec)>=2 && v.vec[0] == 'c' && v.vec[1] == 'd'){
                    if(strlen(v.vec) > 2){
                        if(v.vec[2] != ' '){    // invalid command
                            strcpy(buf , "$$$$");
                            buf[4] = '\0';
                        }
                        else{
                            // ignore spaces (if any) from front and back
                            int l = 3;
                            int r = strlen(v.vec) - 1;
                            while(l < strlen(v.vec) && v.vec[l] == ' ') l++;
                            while(r >=0 && v.vec[r] == ' ') r--;
                            if(r == 1){     // error, no path
                                strcpy(buf , "####");
                                buf[4] = '\0';
                            }
                            int i;
                            for(i=l;i<=r;i++){
                                buf[i-l] = v.vec[i];
                            }
                            buf[i-l] = '\0';
                            if(chdir(buf) < 0){     // error, invalid path
                                strcpy(buf , "####");
                                buf[4] = '\0';
                            }
                            else buf[0] = '\0';     // else, chdir successful, no output in terminal
                        }
                    }
                    else{   // error, no path
                        strcpy(buf , "####");
                        buf[4] = '\0';
                    }
                    my_send(newsockfd , send_buf , buf ,0);
                }

            // dir
                else if(strlen(v.vec)>=3 && v.vec[0] == 'd' && v.vec[1] == 'i' && v.vec[2] == 'r'){
                    struct dirent * pDirent;
                    DIR *pDir;
                    int argument_found = 0;
                    int error_found = 0;
                    if(strlen(v.vec)>3){
                        if(v.vec[3] != ' '){    // invalid command
                            strcpy(buf , "$$$$");
                            buf[4] = '\0';
                            error_found = 1;
                        }
                        else{
                            // ignore spaces (if any) from front and back
                            int l = 3;
                            int r = strlen(v.vec)-1;
                            while(l < strlen(v.vec) && v.vec[l] == ' ') l++;
                            while(r >= 0 && v.vec[r] == ' ') r--;
                            if(r > 2) argument_found = 1;
                            if(argument_found){
                                int i;
                                for(i=l;i<=r;i++){
                                    buf[i-l] = v.vec[i];
                                }
                                buf[i-l] = '\0';
                            }
                        }
                    }
                    if(!error_found && !argument_found){               // no argument in dir means : dir .
                        buf[0] = '.';
                        buf[1] = '\0';
                    }

                    int sent = 0;
                    if(!error_found){
                        if((pDir = opendir(buf)) == NULL){      // error
                            strcpy(buf , "####");
                            buf[4] = '\0';
                        }
                        else{
                            while((pDirent = readdir(pDir)) != NULL){   // else, read all contents
                                strcpy(buf , pDirent->d_name);
                                buf[strlen(pDirent->d_name)] = '\0';
                                my_send(newsockfd , send_buf , buf,1);
                                //printf("%s",buf);
                                //break;
                            }
                            buf[0] = '\0';
                            my_send(newsockfd , send_buf , buf,0);
                            sent = 1;
                        }
                    }
                    if(!sent) my_send(newsockfd , send_buf , buf ,0);
                }

            // all other commands are invalid
                else{
                    strcpy(buf , "$$$$");
                    buf[4] = '\0';
                    my_send(newsockfd , send_buf , buf,0 );
                }

                if(v.len > 0){
                    //printf("%s\n" , v.vec);
                    clear(&v);
                }

            }

            close(newsockfd);
            exit(0);        // return 0 or exit(0) ???
        }

        /*parent process*/

        close(newsockfd);
    }

    close(sockfd);

    return 0;
}
/*
handle error printing when input command is : dirt (basically no space after dir form of command)
*/
