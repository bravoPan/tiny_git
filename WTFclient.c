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
#include "WTFclient.h"
#include "utility.h"

#define BUFFERSIZE 1024

int sockfd, port;
volatile char globalStop = 0;
volatile char responseReceived = 0;
/*0: No response
  1: Success
  2: Error occurred
*/

void output_error(int errnum){
    switch (errnum) {
        case 0:
        fprintf(stderr, "%s\n", "Usage: WTFclient <command> <command args>\nAvailable commands: configure, checkout, update, upgrade, commit, push, create, destroy, add, remove, currentversion, history, rollback.\nRead docs for more info.");
        break;
        case 1:
        fprintf(stderr,"%s\n", "Usage: WTFclient configure <IP> <port>");
        break;
        case 2:
        fprintf(stderr,"%s\n", "Unable to open configure file\n");
        break;
        case 3:
        fprintf(stderr,"%s\n", "Invalid configure file. Please reconfigure.\n");
    }
    exit(0);
}

void *receiveFromServer(void * unused){
  char buffer[BUFFERSIZE];

  while(!globalStop){
    int msg_len;
    int status = read_all(sockfd,&msg_len,sizeof(msg_len),0);
    if(status <= 0){break;}
    printf("%d\n",msg_len);
    status = read_all(sockfd,buffer,msg_len,0);
    if(status <= 0){break;}
    buffer[msg_len] = 0;
    printf("%s\n",buffer);
    responseReceived = 1;
  }
  printf("Server connection interrupted\n");
  if(globalStop == 0 && responseReceived == 0){printf("Server has shutdown\n");kill(0,SIGINT);}
  return NULL;
}

pthread_t receive_thread;

void on_sig_intp(int sig_num){
    globalStop = 1;
    shutdown(sockfd,2);
    return;
}

void connect_server(char * domainStr,char * portStr){
    port = parse_port(portStr);

    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = on_sig_intp;
    sigaction(SIGINT, &act, NULL);

    struct addrinfo hints, *servinfo, *p, *first;
    int rv;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if((rv = getaddrinfo(domainStr, portStr, &hints, &servinfo)) != 0){
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    }

    char connected = 0;

    do{
        for(p = servinfo; p != NULL && !globalStop; p = p->ai_next) {
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
    } while(!connected && !globalStop);

    if(connected){
        printf("Connected to server\n");
        freeaddrinfo(servinfo);
    }
}

void process_checkout(int argc,char ** argv){
    //Check argument
    //Send message to server
    //Wait for response
    const char * str = "Test checkout";
    int len = strlen(str);
    send_all(sockfd,&len,sizeof(len),0);
    send_all(sockfd,str,len,0);
    while(!globalStop && responseReceived == 0){sleep(0);}
    shutdown(sockfd,2);
}
void process_update(int argc,char ** argv){}

int main(int argc,char ** argv){
    if(argc < 2){output_error(0);}
    if(strcmp(argv[1],"configure") == 0){
        if(argc != 4){output_error(1);}
        FILE *f = fopen(".configure","w");
        if(f == NULL){output_error(2);}
        fprintf(f,"%s\n",argv[2]);
        fprintf(f,"%s\n",argv[3]);
        fclose(f);
        return 0;
    } else {
        FILE *f = fopen(".configure","r");
        if(f == NULL){output_error(2);}
        char domain[64],port[64];
        fgets(domain,64,f);
        fgets(port,64,f);
        int t;
        t = strlen(domain);
        if(domain[t - 1] != '\n'){output_error(3);} else {domain[t - 1] = 0;}
        t = strlen(port);
        if(port[t - 1] != '\n'){output_error(3);} else {port[t - 1] = 0;}
        connect_server(domain,port);
    }

    if(globalStop)
        return 0;

    int flags = fcntl(sockfd,F_GETFL,0);
    fcntl(sockfd,F_SETFL,flags | O_NONBLOCK);

    pthread_create(&receive_thread, NULL, receiveFromServer, (void *) NULL);

    char buffer[BUFFERSIZE];
    int i, j;

    if(strcmp(argv[1],"checkout") == 0){
        process_checkout(argc,argv);
    } else if(strcmp(argv[1],"update") == 0){
        process_update(argc,argv);
    } else {
        output_error(0);
    }
    /*
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
    */
    pthread_join(receive_thread,NULL);

    printf("finish\n");
    return 0;
}