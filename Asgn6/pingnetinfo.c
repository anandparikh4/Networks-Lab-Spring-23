/*
Networks Lab Assignment-6
Ping using ICMP
Spring 2023
Authors:
    Anand Manojkumar Parikh (20CS10007)
    Soni Aditya Bharatbhai (20CS10060)

Directions of usage:
    1) Compile as -     gcc pingnetinfo.c -o ping
    2) Run as -         sudo ./ping

[NOTE: This program opens a raw socket, so it cannot be run without superuser permissions]

Use websites: iitkgp.ac.in, moodlecse.iitkgp.ac.in for testing
Suggested number of probes (n) ~ [3,5]
Suggested time gap ~ [0,2] seconds
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h> 
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <math.h>
#include <errno.h>
#include <poll.h>
#include <sys/time.h>

#define ll long long
const int data_len = 500;
const int TIMEOUT = 3000;
const int MAX_TTL = 30;
const int INF = 1e9+7;
const int RECV_SIZE = 1480;

ll min(ll a, ll b){
    return (a<b?a:b);
}

void exit_with_error(char * s){
    perror(s);
    exit(0);
}

// compute 16-bit checksum out of given data
uint16_t compute_checksum(uint16_t * addr, int len){
    uint32_t sum = 0;           // 32 bit running sum
    uint16_t ans = 0;           // 16 bit final ans
    uint16_t * temp = addr;
    int remaining = len;        // number of bytes remaining

    while(remaining > 1){       // keep adding 16 bits
      sum += *temp;
      temp++;
      remaining -= 2;
    }
    if(remaining == 1){         // if 8 bits remain, add them also
        sum += *(uint8_t *)temp;
    }
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    ans = ~sum;
    return ans;
}

// each time a packet is received, the checksum must be verified
int verify_integrity(const struct iphdr * iph , const struct icmphdr * icmph , const int data_len){
    uint16_t ip_check = compute_checksum((uint16_t*)iph , sizeof(struct iphdr));
    if(ip_check != 0) return 0;
    uint16_t icmp_check = compute_checksum((uint16_t*)icmph , sizeof(struct icmphdr) + data_len);
    if(icmp_check!= 0) return 0;

    return 1;
}

// function to print ip headers
void print_ip_info(const char * buf , int flags){

    printf("----------------------------------------------------------\n");
    if(flags) printf("IP Info Received:\n");
    else printf("IP Info Sent:\n");
    printf("----------------------------------------------------------\n");

    struct iphdr * ip_info = (struct iphdr *)buf;

    printf("Version: %d\n" , ip_info->version);
    printf("IHL: %d\n" , ip_info->ihl);
    printf("Type of service: %d\n" , ip_info->tos);
    if(!flags) printf("Total Length: %d\n" , ip_info->tot_len);
    else printf("Total Length: %d\n" , ntohs(ip_info->tot_len));
    printf("ID: %d\n" , ip_info->id);
    printf("TTL: %d\n" , ip_info->ttl);
    printf("Protocol: %d\n" , ip_info->protocol);
    printf("From: %s\n" , inet_ntoa(*(struct in_addr *)&(ip_info->saddr)));
    printf("To: %s\n" , inet_ntoa(*(struct in_addr*)&(ip_info->daddr)));
    printf("----------------------------------------------------------\n");

    return;
}

// function to print icmp headers
void print_icmp_info(const char * buf , int data_len , int flags){

    printf("----------------------------------------------------------\n");
    if(flags) printf("ICMP Info Received:\n");
    else printf("ICMP Info Sent:\n");
    printf("----------------------------------------------------------\n");

    struct icmphdr * icmp_info = (struct icmphdr *)buf;

    printf("Type: %d\n" , icmp_info->type);
    printf("Code: %d\n" , icmp_info->code);
    printf("ID: %d\n" , icmp_info->un.echo.id);
    printf("Sequence No: %d\n" , icmp_info->un.echo.sequence);
    printf("----------------------------------------------------------\n");

    return;
}

// function to print the data
void print_data(char * data , int data_len , int flags){

    printf("----------------------------------------------------------\n");
    if(flags) printf("Data Received:");
    else printf("Data Sent:");

    printf(" [%d] bytes\n" , data_len);
    printf("----------------------------------------------------------\n");
    printf("%.*s\n", data_len, data);
    printf("----------------------------------------------------------\n");

    return;
}

int main(){
    
    // get server info
    char host[200];
    printf("Enter host name/IP: ");
    scanf("%s" , host);
    int N,T;
    printf("Enter number of probes(n): ");
    scanf("%d" , &N);
    printf("Enter time gap(T): ");
    scanf("%d" , &T);
    struct hostent * host_info = gethostbyname(host);
    if(host_info == NULL){
        exit_with_error("gethostbyname() failed");
    }
    printf("####\n");
    printf("Official Destination Name: %s\n" , host_info->h_name);
    printf("Destination IP Address: %s\n" , inet_ntoa(*(struct in_addr *) host_info->h_addr_list[0]));
    printf("####\n");
    
    struct sockaddr_in src_addr,dest_addr,inter_addr,dummy_addr;
    socklen_t socklen = 0;
    // setup source address
    src_addr.sin_family = AF_INET;
    src_addr.sin_port = 0;
    src_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    // setup destination address
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = 0;
    dest_addr.sin_addr = *(struct in_addr*)host_info->h_addr_list[0];

    // open raw socket to send and receive ICMP packets -  Needs to run with sudo
    int sockfd = socket(AF_INET , SOCK_RAW , IPPROTO_ICMP);
    if(sockfd < 0){
        exit_with_error("socket() failed");
    }
    
    // set socket option IP_HDRINCL to true
    int one = 1;
    if(setsockopt(sockfd , IPPROTO_IP , IP_HDRINCL , &one , sizeof(one)) < 0){
        exit_with_error("setsockopt() failed");
    }

    // allocate buffer space
    int bytes = sizeof(struct iphdr) + sizeof(struct icmphdr) + data_len;
    char * send_buf = (char *)malloc(sizeof(char) * bytes);
    char * recv_buf = (char *)malloc(sizeof(char) * bytes);

    // align ip headers
    struct iphdr* send_iph = (struct iphdr*)(send_buf);
    struct iphdr* recv_iph = (struct iphdr*)(recv_buf);

    // arrange send ip header
    send_iph->version = 4;                      // IPv4
    send_iph->ihl = 5;                          // 5 words (each 32 bits) = 20 bytes
    send_iph->tos = 0;                          // No bit set, normal service (anyways generally ignored)  
    send_iph->id = getpid();                    // ## Overflows, but will work
    send_iph->frag_off = 0;                     // basically setting all flags (3 MSBs) to 0. In case fragmentation is needed, it can still be done by IP layer automatically                     
    send_iph->protocol = IPPROTO_ICMP;          // since it is an ICMP packet
    send_iph->saddr = src_addr.sin_addr.s_addr; // source address is always this

    // align icmp headers
    struct icmphdr* send_icmph = (struct icmphdr*)(send_buf + sizeof(struct iphdr));
    struct icmphdr* recv_icmph = (struct icmphdr*)(recv_buf + sizeof(struct iphdr));

    // align send icmp header
    send_icmph->type = ICMP_ECHO;            // echo request
    send_icmph->code = 0;                    // useless, so set to 0
    send_icmph->un.echo.id = getppid();      // ## Overflows, but will work

    // align data
    char * send_data = (char *)(send_buf + sizeof(struct iphdr) + sizeof(struct icmphdr));
    char * recv_data = (char *)(recv_buf + sizeof(struct iphdr) + sizeof(struct icmphdr));

    // arrange data
    for(int i=0;i<data_len;i++) send_data[i] = 1;

    // some routers might not send or send impractically delayed replies, we do not wait for such packets
    struct pollfd fdset[1];
    fdset[0].fd = sockfd;
    fdset[0].events = POLLIN;

    // other data required for calcuation and maintenance
    int curr_ttl = 0;
    int seq_no = 0;         // a monotonically increasing sequence number
    int to_compute = 1;
    double wo_data_rtt[MAX_TTL+1];      // round-trip time without data
    wo_data_rtt[0] = 0;
    double w_data_rtt[MAX_TTL+1];       // rount-trip time with data
    w_data_rtt[0] = 0;
    double latency[MAX_TTL+1];          // latencies of all links
    double bandwidth[MAX_TTL+1];        // bandwidths of all links
    int dest_found = 0;

    while(!dest_found){
        ++curr_ttl;
        if(curr_ttl > MAX_TTL) break;

        // set ttl and recompute checksum and send to destination
        send_iph->check = 0;
        send_iph->daddr = dest_addr.sin_addr.s_addr;
        send_iph->tot_len = sizeof(struct iphdr) + sizeof(struct icmphdr);
        send_iph->ttl = curr_ttl;
        send_iph->check = compute_checksum((uint16_t *)send_iph , sizeof(struct iphdr));

        // send 5 packets for confirming next hop, each 1 second apart
        int skip = 0;
        struct in_addr prev_ip;
        for(int j=0;j<5;j++){
            // set sequence no. and recompute checksum
            send_icmph->checksum = 0;
            send_icmph->un.echo.sequence = seq_no++;
            send_icmph->checksum = compute_checksum((uint16_t *)send_icmph , sizeof(struct icmphdr));

            int send_status = sendto(sockfd , send_buf , sizeof(struct iphdr) + sizeof(struct icmphdr) , 0 , (struct sockaddr*)&dest_addr , sizeof(dest_addr));
            if(send_status < 0){
                exit_with_error("sendto() failed");
            }

            int recv_flag = 1;
            int timeout = TIMEOUT;
            struct timeval start_time,curr_time;
            gettimeofday(&start_time , NULL);

            while(recv_flag){
                int retval = poll(fdset , 1 , timeout);
                if(retval < 0){
                    exit_with_error("poll() failed");
                }
                if(retval == 0){
                    printf("%d: ****************************************\n" , curr_ttl);       // timed out
                    skip = 1;
                    to_compute = 0;
                    recv_flag = 0;
                    break;
                }
                int recv_status = recvfrom(sockfd , recv_buf , RECV_SIZE , 0 , (struct sockaddr*)&inter_addr , &socklen);
                if(recv_status < 0){
                    exit_with_error("recvfrom() failed");
                }

                int useless = 0;
                // verify checksums
                int correct = verify_integrity(recv_iph , recv_icmph , recv_status - sizeof(struct iphdr) - sizeof(struct icmphdr));
                if(correct != 1){
                    printf("Corrupted packet received and dropped\n");
                    useless = 1;

                }                

                struct iphdr * returned_ip = (struct iphdr*)(recv_buf + sizeof(struct iphdr) + sizeof(struct icmphdr));
                // check if the IP packet received is indeed yours
                if(recv_icmph->type != 0 && recv_icmph->type != 11){
                    printf("\nUnrelated ICMP packet received\n");
                    print_ip_info(recv_buf , 1);
                    print_icmp_info(recv_buf+sizeof(struct iphdr) , recv_status - sizeof(struct iphdr) , 1);
                    useless = 1;
                    if(returned_ip->protocol == 6){
                        printf("TCP Packet received:\n");
                        char * temp = (char*)(recv_buf + 2*sizeof(struct iphdr) + sizeof(struct icmphdr));
                        printf("Source Port: %d , Destination Port: %d\n" , *(uint16_t*)(temp) , *(uint16_t*)(temp+2));
                    }
                    if(returned_ip->protocol == 17){
                        printf("UDP Packet received:\n");
                        char * temp = (char *)(recv_buf + 2*sizeof(struct iphdr) + sizeof(struct icmphdr));
                        printf("Source Port: %d , Destination Port: %d\n" , *(uint16_t*)(temp) , *(uint16_t*)(temp+2));
                    }
                }
                else if(recv_icmph->type == 0 && recv_icmph->un.echo.id != getppid()){
                    useless = 1;
                    printf("\nUnrelated ICMP echo reply received:\n");
                    print_ip_info(recv_buf , 1);
                    print_icmp_info(recv_buf+sizeof(struct iphdr) , recv_status - sizeof(struct iphdr) , 1);
                }
                else if(recv_icmph->type == 11 && returned_ip->id != getpid()){
                    useless = 1;
                    printf("\nUnrelated ICMP time limit exceeded received:\n");
                    print_ip_info(recv_buf , 1);
                    print_icmp_info(recv_buf+sizeof(struct iphdr) , recv_status - sizeof(struct iphdr) , 1);
                }

                if(useless){
                    gettimeofday(&curr_time , NULL);
                    timeout = timeout - curr_time.tv_sec*1000 - curr_time.tv_usec/1000 + start_time.tv_sec*1000 + start_time.tv_usec/1000;
                    continue;
                }

                if(j == 0) prev_ip = *(struct in_addr*)&(recv_iph->saddr);
                else{
                    if(prev_ip.s_addr != ((struct in_addr*)&(recv_iph->saddr))->s_addr){
                        printf("Route is variable, so considering previous router as next hop\n");
                        recv_flag = 0;
                    }
                }
                break;
            }

            sleep(1);       // sleep for 1 second
        }

        if(skip) continue;

        if(recv_icmph->type == 11) printf("%d: %s " , curr_ttl , inet_ntoa(*((struct in_addr*)&(recv_iph->saddr))));
        else if(recv_icmph->type == 0){
            printf("%d: %s " , curr_ttl , inet_ntoa(*((struct in_addr*)&(recv_iph->saddr))));
            dest_found = 1;
        }

        if(!skip && !to_compute){
            printf("    -------------------\n");
            continue;
        }

        send_iph->check = 0;
        send_iph->daddr = prev_ip.s_addr;
        send_iph->tot_len = sizeof(struct iphdr) + sizeof(struct icmphdr);
        send_iph->ttl = 64;
        send_iph->check = compute_checksum((uint16_t *)send_iph , sizeof(struct iphdr));

        wo_data_rtt[curr_ttl] = INF;

        for(int j=0;j<N;j++){
            send_icmph->checksum = 0;
            send_icmph->un.echo.sequence = seq_no++;
            send_icmph->checksum = compute_checksum((uint16_t *)send_icmph , sizeof(struct icmphdr));

            struct timeval send_time;
            gettimeofday(&send_time , NULL);

            int send_status = sendto(sockfd , send_buf , sizeof(struct iphdr) + sizeof(struct icmphdr) , 0 , (struct sockaddr*)&inter_addr , sizeof(inter_addr));
            if(send_status < 0){
                exit_with_error("sendto() failed");
            }

            int recv_flag = 1;
            int timeout = TIMEOUT;
            struct timeval start_time,curr_time;
            gettimeofday(&start_time , NULL);

            while(1){
                int retval = poll(fdset , 1 , timeout);
                if(retval < 0){
                    exit_with_error("poll() failed");
                }
                if(retval == 0){
                    printf("%d: ****************************************\n" , curr_ttl);       // timed out
                    skip = 1;
                    recv_flag = 0;
                    to_compute = 0;
                    break;
                }
                int recv_status = recvfrom(sockfd , recv_buf , RECV_SIZE , 0 , (struct sockaddr*)&dummy_addr , &socklen);
                if(recv_status < 0){
                    exit_with_error("recvfrom() failed");
                }

                int useless = 0;
                // verify checksums
                int correct = verify_integrity(recv_iph , recv_icmph , recv_status - sizeof(struct iphdr) - sizeof(struct icmphdr));
                if(correct != 1){
                    printf("Corrupted packet received and dropped\n");
                    useless = 1;

                }                

                struct iphdr * returned_ip = (struct iphdr*)(recv_buf + sizeof(struct iphdr) + sizeof(struct icmphdr));
                // check if the IP packet received is indeed yours
                if(recv_icmph->type != 0 && recv_icmph->type != 11){
                    printf("\nUnrelated ICMP packet received\n");
                    print_ip_info(recv_buf , 1);
                    print_icmp_info(recv_buf+sizeof(struct iphdr) , recv_status - sizeof(struct iphdr) , 1);
                    useless = 1;
                    if(returned_ip->protocol == 6){
                        printf("TCP Packet received:\n");
                        char * temp = (char*)(recv_buf + 2*sizeof(struct iphdr) + sizeof(struct icmphdr));
                        printf("Source Port: %d , Destination Port: %d\n" , *(uint16_t*)(temp) , *(uint16_t*)(temp+2));
                    }
                    if(returned_ip->protocol == 17){
                        printf("UDP Packet received:\n");
                        char * temp = (char *)(recv_buf + 2*sizeof(struct iphdr) + sizeof(struct icmphdr));
                        printf("Source Port: %d , Destination Port: %d\n" , *(uint16_t*)(temp) , *(uint16_t*)(temp+2));
                    }
                }
                else if(recv_icmph->type == 0 && recv_icmph->un.echo.id != getppid()){
                    useless = 1;
                    printf("\nUnrelated ICMP echo reply received:\n");
                    print_ip_info(recv_buf , 1);
                    print_icmp_info(recv_buf+sizeof(struct iphdr) , recv_status - sizeof(struct iphdr) , 1);
                }
                else if(recv_icmph->type == 11 && returned_ip->id != getpid()){
                    useless = 1;
                    printf("\nUnrelated ICMP time limit exceeded received:\n");
                    print_ip_info(recv_buf , 1);
                    print_icmp_info(recv_buf+sizeof(struct iphdr) , recv_status - sizeof(struct iphdr) , 1);
                }

                if(useless){
                    gettimeofday(&curr_time , NULL);
                    timeout = timeout - curr_time.tv_sec*1000 - curr_time.tv_usec/1000 + start_time.tv_sec*1000 + start_time.tv_usec/1000;
                    continue;
                }
                
                break;
            }

            struct timeval recv_time;
            gettimeofday(&recv_time , NULL);

            wo_data_rtt[curr_ttl] = min(wo_data_rtt[curr_ttl] , recv_time.tv_usec - send_time.tv_usec);

            sleep(T);
        }

        if(skip){
            printf("    -------------------\n");
            continue;
        }

        latency[curr_ttl] = (wo_data_rtt[curr_ttl] - wo_data_rtt[curr_ttl-1])/2;    // in usec
        printf(", Latency: %f ms" , fabs(latency[curr_ttl]/1000));

        send_iph->check = 0;
        send_iph->daddr = inter_addr.sin_addr.s_addr;
        send_iph->tot_len = sizeof(struct iphdr) + sizeof(struct icmphdr) + data_len;
        send_iph->ttl = 64;
        send_iph->check = compute_checksum((uint16_t *)send_iph , sizeof(struct iphdr));

        w_data_rtt[curr_ttl] = INF;

        for(int j=0;j<N;j++){
            send_icmph->checksum = 0;
            send_icmph->un.echo.sequence = seq_no++;
            send_icmph->checksum = compute_checksum((uint16_t *)send_icmph , sizeof(struct icmphdr) + data_len);

            struct timeval send_time;
            gettimeofday(&send_time , NULL);

            int send_status = sendto(sockfd , send_buf , sizeof(struct iphdr) + sizeof(struct icmphdr) + data_len , 0 , (struct sockaddr*)&inter_addr , sizeof(inter_addr));
            if(send_status < 0){
                exit_with_error("sendto() failed");
            }

            int recv_flag = 1;
            int timeout = TIMEOUT;
            struct timeval start_time,curr_time;
            gettimeofday(&start_time , NULL);

            while(recv_flag){
                int retval = poll(fdset , 1 , timeout);
                if(retval < 0){
                    exit_with_error("poll() failed");
                }
                if(retval == 0){
                    printf("%d: ****************************************\n" , curr_ttl);       // timed out
                    skip = 1;
                    recv_flag = 0;
                    to_compute = 0;
                    break;
                }
                int recv_status = recvfrom(sockfd , recv_buf , RECV_SIZE , 0 , (struct sockaddr*)&dummy_addr , &socklen);
                if(recv_status < 0){
                    exit_with_error("recvfrom() failed");
                }

                int useless = 0;
                // verify checksums
                int correct = verify_integrity(recv_iph , recv_icmph , recv_status - sizeof(struct iphdr) - sizeof(struct icmphdr));
                if(correct != 1){
                    printf("Corrupted packet received and dropped\n");
                    useless = 1;

                }                

                struct iphdr * returned_ip = (struct iphdr*)(recv_buf + sizeof(struct iphdr) + sizeof(struct icmphdr));
                // check if the IP packet received is indeed yours
                if(recv_icmph->type != 0 && recv_icmph->type != 11){
                    printf("\nUnrelated ICMP packet received\n");
                    print_ip_info(recv_buf , 1);
                    print_icmp_info(recv_buf+sizeof(struct iphdr) , recv_status - sizeof(struct iphdr) , 1);
                    useless = 1;
                    if(returned_ip->protocol == 6){
                        printf("TCP Packet received:\n");
                        char * temp = (char*)(recv_buf + 2*sizeof(struct iphdr) + sizeof(struct icmphdr));
                        printf("Source Port: %d , Destination Port: %d\n" , *(uint16_t*)(temp) , *(uint16_t*)(temp+2));
                    }
                    if(returned_ip->protocol == 17){
                        printf("UDP Packet received:\n");
                        char * temp = (char *)(recv_buf + 2*sizeof(struct iphdr) + sizeof(struct icmphdr));
                        printf("Source Port: %d , Destination Port: %d\n" , *(uint16_t*)(temp) , *(uint16_t*)(temp+2));
                    }
                }
                else if(recv_icmph->type == 0 && recv_icmph->un.echo.id != getppid()){
                    useless = 1;
                    printf("\nUnrelated ICMP echo reply received:\n");
                    print_ip_info(recv_buf , 1);
                    print_icmp_info(recv_buf+sizeof(struct iphdr) , recv_status - sizeof(struct iphdr) , 1);
                }
                else if(recv_icmph->type == 11 && returned_ip->id != getpid()){
                    useless = 1;
                    printf("\nUnrelated ICMP time limit exceeded received:\n");
                    print_ip_info(recv_buf , 1);
                    print_icmp_info(recv_buf+sizeof(struct iphdr) , recv_status - sizeof(struct iphdr) , 1);
                }

                if(useless){
                    gettimeofday(&curr_time , NULL);
                    timeout = timeout - curr_time.tv_sec*1000 - curr_time.tv_usec/1000 + start_time.tv_sec*1000 + start_time.tv_usec/1000;
                    continue;
                }

                break;
            }
            struct timeval recv_time;
            gettimeofday(&recv_time , NULL);

            w_data_rtt[curr_ttl] = min(w_data_rtt[curr_ttl] , recv_time.tv_usec - send_time.tv_usec);

            sleep(T);
        }

        if(skip){
            printf("    -------------------\n");
            continue;
        }

        // printf("temp: %f\n" , (w_data_rtt[curr_ttl] - w_data_rtt[curr_ttl-1])/2000);
        bandwidth[curr_ttl] = 8 * data_len/((w_data_rtt[curr_ttl] - w_data_rtt[curr_ttl-1])/2 - latency[curr_ttl]);
        printf(" , Bandwidth: %f Mbps\n" , fabs(bandwidth[curr_ttl]));

    }

    free(send_buf);
    free(recv_buf);
    close(sockfd);

    printf("####\n");
    if(errno == 0) printf("Success!\n");
    else printf("Errno: %d" , errno);
    return 0;
}

