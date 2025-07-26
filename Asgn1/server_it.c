/* Networks Lab Assignment 1 Problem 2
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
#define MAX_CLI_QUEUE 5     // At most 5 clients can wait while the server is occupied
#define DECIMAL_PLACES 15   // While converting the answer from double to string, only a fixed number of decimal places are taken care of
#define RECV_BUF_SIZE 10    // number of bytes received in 1 recv() call

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

// some helper functions
int is_num(char curr){      // check if input is an operand
    if(curr >= '0' && curr <= '9') return 1;
    return 0;
}

int is_op(char curr){       // check if input is an operator
    if(curr == '+' || curr == '-' || curr == '*' || curr == '/') return 1;
    return 0;
}

double binary_op(double x, char op, double y){      // return ( x op y )
    if(op == '+') return x+y;
    if(op == '-') return x-y;
    if(op == '*') return x*y;
    if(op == '/') return x/y;
    return 0;       // to avoid warning
}

// A recursive function to evaluate expressions
// The only invalidity detected and evaded is division by 0
// The reason this is of utmost importance is that if some client gives the server a divide by 0 operation, the server will crash, which is highly undesirable
// This custom expression evaluation function handles ANY LEVEL OF NESTING of parentheses
double compute(char * s , int l , int * r , int * valid){       
    
    // s = string start pointer
    // l = left end to start exploring from
    // r = a pointer value to tell caller (during recursion) that computation till this index is complete
    // valid = 0/1 if the prefix explored yet is invalid/valid respectively
    
    int found = 0;      // number of operands found in current nesting
    double left,right;     // current left and right operands
    char curr_op;       // current operator to apply on left and right

    while(s[l] != '\0'){   

        if(is_num(s[l])){   // if digit found, keep parsing till an integer / float value is seen
            right = 0;
            while(is_num(s[l])){
                right = right * 10 + s[l] - '0';
                l++;
            }
            if(s[l] == '.'){
                l++;
                double power = 1;
                while(is_num(s[l])){
                    power = power * 0.1;
                    right += power * (s[l] - '0');
                    l++;
                }
            }
            found++;
            if(found == 1){
                left = right;
            }
            else{
                if(curr_op == '/' && right == 0){
                    *valid = 0;
                    return 0;
                }

                left = binary_op(left, curr_op , right);
            }
        }

        else if(is_op(s[l])){       // update curr_op
            curr_op = s[l];
            l++;
        }

        else if(s[l] == '('){       // if opening parentheses found, recursively compute result of parentheses
            right = compute(s,l+1,r,valid);
            if(*valid == 0) return 0;
            l = (*r)+1;
            found++;
            if(found == 1){
                left = right;
            }
            else{
                if(curr_op == '/' && right == 0){
                    *valid = 0;
                    return 0;
                }
                
                left = binary_op(left , curr_op , right);
            }
        }

        else if(s[l] == ')'){       // if closing parentheses found, return to caller
            *r = l;
            return left;
        }

        else if(s[l] == ' '){       // ignore spaces
            l++;
        }

    }

    *r = l;
    return left;        // return to caller
}

int double_to_string(char * buff , double f){      // convert double to string

buff[0] = '\0';
int i = 0;

if(f == 0){                 // handle f = 0 seperately
    buff[i++] = '0';
}

else{                       // f < 0 || f > 0
    int j = 0;              // the index to start reversing it from, since LSB is scanned first
    if(f < 0){              // if f < 0
        f = -f;             // make it positive
        buff[i++] = '-';    // first character = '-'
        j = 1;              // and start reversing from index 1
    }

    long n = f;        	    // store the integer part in a long

    while(n){               // convert the integer part to a string like in function printInt
        buff[i++] = (char)('0' + n%10);
        n /= 10;
    }

    int k = i-1;            // reverse the sub-string from index j to i-1, using 2 pointers
    while(j<k){
        char temp = buff[j];
        buff[j] = buff[k];
        buff[k] = temp;
        j++; k--;
    }
}

if(i == 0 || (buff[0] == '-' && i == 1)){                  // if i == 0, it means it is of the form 0.xxxxx, or if it is negative and i == 1, it means it is of the form -0.xxxx.
    buff[i++] = '0';        // in both cases, we need to add a 0 manually, since the loop while(n) will not run
}

buff[i++] = '.';            // add a decimal point

long n = f;
float dec = f - n;          // dec = decimal part = f - (integer) f

for(int cnt=0;cnt<DECIMAL_PLACES;cnt++){    // take a fixed number (6) of decimal places
    dec = dec * 10;                     // multiply the fractional part by 10
    n = dec;                            // n = integer part of dec
    buff[i++] = (char)('0' + (n%10));   // take LSB of n
}

buff[i] = '\0';             // null terminate
return i+1;

}

int main(){
    
    int sockfd,newsockfd;           // original and duplicate server socket ids
    struct sockaddr_in serv_addr;   // server address

    struct sockaddr_in cli_addr;    // client (active) address
    int clilen;             // size of client (active) address

    /*create server socket*/
    if((sockfd = socket(AF_INET , SOCK_STREAM , 0)) < 0){
        perror("server::socket() failed");
        exit(0);
    }

    /*setup socket address*/
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(PORT);

    /*bind socket address to socket id*/
    if(bind(sockfd , (struct sockaddr *)&serv_addr , sizeof(serv_addr)) < 0){
        perror("server::bind() failed");
        exit(0);
    }

    /*ready to listen for clients' requests*/
    if(listen(sockfd , MAX_CLI_QUEUE) < 0){
        perror("server::listen() failed");
        exit(0);
    }

    // variable buffer lengths are required for some purposes, so space is allocated dynamically as required
    char* recv_buf = NULL;
    char* send_buf = NULL;

    while(1){
        clilen = sizeof(cli_addr);
        /*accept clients' connect requests*/
        if((newsockfd = accept(sockfd , (struct sockaddr *)&cli_addr , &clilen)) < 0){
            perror("server::accept() failed");
            exit(0);
        }

        while(1){

            recv_buf = (char *)malloc(RECV_BUF_SIZE * sizeof(char));
            
            struct vector v;
            init(&v);

            int flag = 1;
            int terminate = 0;
            while(flag){
                int recv_status;
                // keep receiving chunks of 10 till \0 is encountered
                if((recv_status = recv(newsockfd , recv_buf , RECV_BUF_SIZE , 0)) < 0){
                    perror("server::recv() failed");
                    exit(0);
                }
                
                if(recv_status == 0){       // if 0 bytes received, then client has disconnected, so close connection from server side also
                    clear(&v);
                    terminate = 1;
                    break;
                }

                for(int i=0;i<10;i++){
                    push(&v,recv_buf[i]);
                    if(recv_buf[i] == '\0'){
                        flag = 0;
                        break;
                    }
                }

            }

            free(recv_buf);
            if(terminate){             // if flag set, disconnect
                clear(&v);
                break;
            }

            send_buf = (char *)malloc(100 * sizeof(char));      // length 100 is sufficient to store the string format of a double

            recv_buf = (char *)malloc(v.len * sizeof(char));
            for(int i=0;i<v.len;i++) recv_buf[i] = v.vec[i];

            clear(&v);

            int valid = 1,right_end;
            double answer = compute(recv_buf,0,&right_end,&valid);  // compute answer
            int l = double_to_string(send_buf, answer);             // convert to string format

            char * new_send_buf = (char *)realloc(send_buf , l);        // allocate new space for sending the result to client
            if(new_send_buf == NULL){
                perror("server::realloc() failed");
                exit(0);
            }
            send_buf = new_send_buf;

            int send_status;
            // here, sending in one go is OK, since the result (a double) obviously wont be longer than, say, 100 characters, so there is no danger of overflowing
            if((send_status = send(newsockfd , send_buf , strlen(send_buf)+1 , 0)) != strlen(send_buf)+1){
                perror("server::send() failed");
                exit(0);
            }

            // free buffers
            free(recv_buf);
            free(send_buf);
        }

        /*terminate duplicate*/
        if(close(newsockfd) < 0){
            perror("server::close() failed");
            exit(0);
        }
    }

    return 0;
}

