#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>

#define BUFFERSIZE 1024

int sockfd, port;
volatile char stop_receive = 0;

void output_error(int errnum){
    switch (errnum) {
        case 0:
        fprintf(stderr, "%s\n", "Usage: the arg is not correct");
        break;
    }
    exit(0);
}

int parse_port(char *port){
    if(port == NULL){output_error(0);}
    int len = strlen(port);
    int i, p = 0;
    for(i = 0; i < len; i++){
        if(!isdigit(port[i])){
            output_error(0);
        }
        p = p * 10 + (port[i] - '0');
        if(p > 65535){output_error(0);}
    }
    return p;
}

ssize_t send_all(int socket,const void * data,size_t len,int sig){
  while (!stop_receive && len > 0){
    ssize_t r = send(socket, data, len, sig);
    if (r <= 0){return -1;}
    len -= r;
    data = (const char*)data + r;
  }
  if(len > 0){return -1;} else {return 1;}
}

void *receiveFromServer(void * unused){
  char buffer[BUFFERSIZE];

  while(!stop_receive){
    memset(buffer, 0, BUFFERSIZE);
    int t,i;
    t = read(sockfd, buffer, BUFFERSIZE);
    if(t <= 0 && errno != EAGAIN){break;} else if(errno == EAGAIN){sleep(0);continue;}
    printf("%s",buffer);
  }
  printf("Server connection interrupted\n");
  if(stop_receive == 0){printf("Server has shutdown\n");kill(0,SIGINT);}
  return NULL;
}

pthread_t receive_thread;

void on_sig_intp(int sig_num){
    stop_receive = 1;
    return;
}



int main(int argc,char ** argv){
    if(argc != 3){
        output_error(0);
    }
    port = parse_port(argv[2]);

    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = on_sig_intp;
    sigaction(SIGINT, &act, NULL);

    struct addrinfo hints, *servinfo, *p, *first;
    int rv;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if((rv = getaddrinfo(argv[1], argv[2], &hints, &servinfo)) != 0){
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    }

    char connected = 0;

    do{
        for(p = servinfo; p != NULL && stop_receive == 0; p = p->ai_next) {
            if((sockfd = socket(p -> ai_family, p -> ai_socktype, p -> ai_protocol)) == -1){
                perror("socket");
                continue;
            }
            if(connect(sockfd, p->ai_addr, p->ai_addrlen) == -1){
                close(sockfd);
                continue;
            }
            connected = 1;
            break;
        }
        if(!connected){
            printf("Fail connected. Will try again 3 seconds later.\n");
            sleep(3);
        }
    } while(!connected && ! stop_receive);

    if(stop_receive == 1)
        return 0;

    printf("Connected to server\n");
    freeaddrinfo(servinfo);

    int flags = fcntl(sockfd,F_GETFL,0);
    fcntl(sockfd,F_SETFL,flags | O_NONBLOCK);

    pthread_create(&receive_thread, NULL, receiveFromServer, (void *) NULL);

    char buffer[BUFFERSIZE];
    int i, j;

    while (!stop_receive) {
        memset(buffer, 0, BUFFERSIZE);
        printf("Enter your message:\n");
        if(fgets(buffer, BUFFERSIZE, stdin) == NULL){
            stop_receive = 1;break;
        }
        int msg_len = strlen(buffer);
        send_all(sockfd, &msg_len,sizeof(msg_len),0);
        send_all(sockfd, buffer, strlen(buffer), 0);
        if(stop_receive)
            break;
    }
    pthread_join(receive_thread,NULL);


    return 0;
}
