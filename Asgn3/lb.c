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
#include <time.h>           // for local timestamp
#include <string.h>
#include<poll.h>

#define SEND_BUF_SIZE 25
#define RECV_BUF_SIZE 25
#define MAX_CLI_QUEUE 5
#define TIMEOUT 5          // set timeout of 5000ms = 5 sec

int min(int a,int b){   // helper function
    return (a<b?a:b);
}

int main(int argc , char * argv[]){ //  ./lb.c PORT SERV_1_PORT SERV_2_PORT

    if(argc != 4){
        printf("Expected 4 arguments, got %d\n", argc);
        exit(0);
    }
    int PORT = atoi(argv[1]);           // get all PORTs (self, serv1, serv2) from command line argument
    int SERV_1_PORT = atoi(argv[2]);
    int SERV_2_PORT = atoi(argv[3]);

// Acting as a server for clients
    int serv_sockfd, new_serv_sockfd;   // load balancer's sockets that act as server original and duplicate
    struct sockaddr_in serv_addr;       // address of above socket

    int clilen;                         // size of client address
    struct sockaddr_in cli_addr;        // client address

    /*create the socket that acts as a server for the clients*/
    if((serv_sockfd = socket(AF_INET , SOCK_STREAM , 0)) < 0){
        perror("lb::socket() failed");
        exit(0);
    }

    /*setup address*/
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(PORT);

    /*bind address to socket*/
    if(bind(serv_sockfd , (struct sockaddr *)&serv_addr , sizeof(serv_addr)) < 0){
        perror("lb::bind() failed");
        exit(0);
    }

    /*listen for incoming client connections*/
    listen(serv_sockfd , MAX_CLI_QUEUE);

// Acting as a client for servers
    int cli_sockfd;                             // load balancer's socket that acts as a client for the server
    struct sockaddr_in serv_1_addr,serv_2_addr; // address of the 2 actual servers (the ones to forward the actual clients' requests to)

    /*setup server 1 address*/
    serv_1_addr.sin_family = AF_INET;
    inet_aton("127.0.0.1" , &serv_1_addr.sin_addr);
    serv_1_addr.sin_port = htons(SERV_1_PORT);

    /*setup server 2 address*/
    serv_2_addr.sin_family = AF_INET;
    inet_aton("127.0.0.1" , &serv_2_addr.sin_addr);
    serv_2_addr.sin_port = htons(SERV_2_PORT);

    char send_buf[SEND_BUF_SIZE];
    char recv_buf[RECV_BUF_SIZE];

    while(1){                           // get loads from both servers and wait for clients
        int send_status,recv_status;

        strcpy(send_buf , "Send Load");
        send_buf[9] = '\0';

        // Get load from serv_1
        if((cli_sockfd = socket(AF_INET , SOCK_STREAM , 0)) < 0){   // create socket
            perror("lb::socket() failed");
            exit(0);
        }
        if(connect(cli_sockfd , (struct sockaddr *)&serv_1_addr , sizeof(serv_1_addr)) < 0){    // connect to server 1
            perror("lb::connect() failed");
            exit(0);
        }
        if((send_status = send(cli_sockfd , send_buf , strlen(send_buf)+1 , 0)) < 0){   // ask for load
            perror("lb::send() failed");
            exit(0);
        }
        int load1;
        if((recv_status = recv(cli_sockfd , &load1 , sizeof(int) , 0)) < 0){    // receive load (server 1)
            perror("lb::recv() failed");
            exit(0);
        }
        printf("Load received from IP<%s> PORT<%d>: %d\n",inet_ntoa(serv_1_addr.sin_addr) , SERV_1_PORT , load1);
        if(close(cli_sockfd) < 0){          // close socket
            perror("lb::close() failed");
            exit(0);
        }

        // Get load from serv_2
        if((cli_sockfd = socket(AF_INET , SOCK_STREAM , 0)) < 0){   // create another socket
            perror("lb::socket() failed");
            exit(0);
        }
        if(connect(cli_sockfd , (struct sockaddr *)&serv_2_addr , sizeof(serv_2_addr)) < 0){    // connect to server 2
            perror("lb::connect() failed");
            exit(0);
        }
        if((send_status = send(cli_sockfd , send_buf , strlen(send_buf)+1 , 0)) < 0){   // ask for load
            perror("lb::send() failed");
            exit(0);
        }
        int load2;
        if((recv_status = recv(cli_sockfd , &load2 , sizeof(int) , 0)) < 0){    // receive load
            perror("lb::recv() failed");
            exit(0);
        }
        printf("Load received from IP<%s> PORT<%d>: %d\n",inet_ntoa(serv_2_addr.sin_addr) , SERV_2_PORT , load2);
        if(close(cli_sockfd) < 0){          // close socket
            perror("lb::close() failed");
            exit(0);
        }

        time_t start = time(NULL);            // get system time
        int timeout = TIMEOUT;              // set timer of 5000 ms = 5 s
        while(timeout > 0){                 // wait for <timeout> seconds before updating server loads    

            struct pollfd fdset[1];
            fdset[0].fd = serv_sockfd;
            fdset[0].events = POLLIN;

            int poll_ret = poll(fdset , 1 , timeout * 1000);   // poll on incoming connect request from client and timeout
            if(poll_ret < 0){           // error
                perror("lb::pollfd() failed");
                exit(0);
            }
            else if(poll_ret == 0) break;       // if timeout, break

            // else, client request has arrived before timing out
            // accept that request, connection now established
            clilen = sizeof(cli_addr);
            if((new_serv_sockfd = accept(serv_sockfd , (struct sockaddr *)&cli_addr , &clilen)) < 0){
                perror("lb::accept() failed");
                exit(0);
            }

            if(fork() == 0){        // fork a child, outsource job to child process

                if((cli_sockfd = socket(AF_INET , SOCK_STREAM , 0)) < 0){   // create duplicate socket
                    perror("lb::socket() failed");
                    exit(0);
                }
                // compare loads to find out which server to send requests to
                int SERV_FINAL_PORT;    
                struct sockaddr_in serv_final_addr;
                if(load1 < load2){
                    SERV_FINAL_PORT = SERV_1_PORT;
                    serv_final_addr = serv_1_addr;
                }
                else{
                    SERV_FINAL_PORT = SERV_2_PORT;
                    serv_final_addr = serv_2_addr;
                }
                printf("Sending client request to IP<%s> PORT<%d>\n",inet_ntoa(serv_final_addr.sin_addr),SERV_FINAL_PORT);
                
                strcpy(send_buf,"Send Time");
                send_buf[9] = '\0';

                if(connect(cli_sockfd , (struct sockaddr *)&serv_final_addr , sizeof(serv_final_addr)) < 0){    // connect to less loaded server
                    perror("lb::connect() failed");
                    exit(0);
                }

                if((send_status = send(cli_sockfd , send_buf , strlen(send_buf)+1 , 0)) < 0){   // ask for time
                    perror("lb::send() failed");
                    exit(0);
                }

                int quid_pro_quo = 1;       // quid_pro_quo = 1, means send whatever you receive (a take and a give), to simply forward on the information from server to client
                while(quid_pro_quo){        // keep doing this till flag is set
                    if((recv_status = recv(cli_sockfd , recv_buf , RECV_BUF_SIZE - 1 ,0)) < 0){ // receive a chunk
                        perror("lb::recv() failed");
                        exit(0);
                    }
                    for(int i=0;i<recv_status;i++){     // if this chunk contains a '\0', unset flag, to stop this receive and send business
                        if(recv_buf[i] == '\0') quid_pro_quo = 0;
                    }
                    int send_length = min(RECV_BUF_SIZE - 1 , strlen(recv_buf)+1);  // send only what is required
                    if(send_status = send(new_serv_sockfd , recv_buf , send_length , 0) < 0){   // send whatever you receive
                        perror("lb::send() failed");
                        exit(0);
                    }
                }

                close(cli_sockfd);      // close load balancer-server connection
                close(new_serv_sockfd); // close load balancer-client connection

                return 0;
            }

            close(new_serv_sockfd);     // close duplicate socket in parent process
            
            time_t difference = time(NULL) - start;   // find out how much time has elapsed since start of loop
            timeout = TIMEOUT - difference;        // resume loop with remaining time (out of 5s)
        }

    }

    close(serv_sockfd); // close original load balancer's server socket

    return 0;
}
