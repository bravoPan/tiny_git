#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <dirent.h>
#include <netdb.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include "utility.h"

typedef struct _thread_data{
  int sockfd;
  volatile char hasReturned;
} thread_data;

typedef struct _thread_node{
  pthread_t thread_h;
  thread_data * tls_data;
  struct _thread_node * volatile prev, * volatile next;
} thread_node;

thread_node * volatile head_node = NULL;
int port;
volatile char globalStop = 0;
int sockfd = -1;
int selfpipe[2];
struct pollfd pollfds[2];
HashMap *repoHashMap;

void output_error(int e){
  switch(e){
  case 0:
    fprintf(stderr,"Usage: WTFserver [portNumber]\n");
    break;
  default:
    fprintf(stderr,"Unknown error occurred\n");
    break;
  }
  exit(0);
}

int test_receive_file(int socket){
    char *metadata_src = ReceiveMessage(socket);
    char *metadata = metadata_src + 8;
    if(metadata_src == NULL){return -1;}
    // printf("The project name is %s\n", file_name + 8);
    printf("The project name is %s\n", metadata);
    int project_name_len = strlen(metadata) + 1;

    unsigned char hash[16];
    char filename[33];
    memcpy(hash, metadata + project_name_len, 16);
    int file_len = *(int *)(metadata + project_name_len + 16);
    for(int i = 0;i < 16;++i){
        int p,q;
        p = hash[i] / 16;q = hash[i] % 16;
        if(p < 10){filename[i * 2] = p + '0';} else {filename[i * 2] = p - 10 + 'A';}
        if(q < 10){filename[i * 2 + 1] = q + '0';} else {filename[i * 2 + 1] = q - 10 + 'A';}
    }
    filename[32] = 0;

    FILE *new_file = fopen(filename, "w");
    int file_size  = *(int *)(metadata + project_name_len + 16);
    char * text = malloc(file_size);
    read_all(socket, text, file_size, 0);
    fprintf(new_file, "%s", text);

    free(metadata_src);
    free(text);
    fclose(new_file);
    return 0;
}

void * handle_customer(void * tls){
  thread_data * tls_data = (thread_data *)tls;
  int flags = fcntl(tls_data -> sockfd,F_GETFD,0);
  fcntl(tls_data -> sockfd,F_SETFD,flags | O_NONBLOCK);
  printf("Connection Established\n");
  while(!globalStop){
      if(test_receive_file(tls_data -> sockfd) == -1){break;}
    //   char *receive_data = ReceiveMessage(tls_data -> sockfd);
    //   if(receive_data == NULL){break;}
    //   printf("%4s",receive_data);
    //   free(receive_data);
  }
  printf("Connection Terminated\n");
  shutdown(tls_data -> sockfd,2);
  close(tls_data -> sockfd);
  tls_data -> hasReturned = 1;

  return NULL;
}

void on_sig_intp(int signum){
    globalStop = 1;
    write(selfpipe[1],"a",1);
    return;
}

int main(int argc,char ** argv){
  if(argc != 2){
    output_error(0);
  }

  pipe(selfpipe);

  struct sigaction act;
  memset(&act,0,sizeof(act));
  act.sa_handler = on_sig_intp;
  sigaction(SIGINT,&act,NULL);
  signal(SIGPIPE,SIG_IGN);

  port = parse_port(argv[1]);
  sockfd = socket(AF_INET,SOCK_STREAM,0);

  struct linger lin;
  lin.l_onoff = 0;
  lin.l_linger = 0;
  setsockopt(sockfd,SOL_SOCKET,SO_LINGER,&lin,sizeof(lin));
  int reuse = 1;
  setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(int));

  struct sockaddr_in address;
  int addrlen = sizeof(address);
  address.sin_family = AF_INET;
  address.sin_port = htons(port);
  address.sin_addr.s_addr = INADDR_ANY;
  bind(sockfd,(struct sockaddr *)&address,addrlen);
  listen(sockfd,256);
  printf("WTFserver listening on port %d\n",port);

  int flags = fcntl(sockfd,F_GETFL,0);
  fcntl(sockfd,F_SETFL,flags | O_NONBLOCK);
  flags = fcntl(selfpipe[0],F_GETFL,0);
  fcntl(sockfd,F_SETFL,flags | O_NONBLOCK);

  pollfds[0].fd = sockfd;
  pollfds[0].events = POLLIN;
  pollfds[1].fd = selfpipe[0];
  pollfds[1].events = POLLIN;

  while(!globalStop){
    struct sockaddr_in cli_addr;
    unsigned int clilen = sizeof(cli_addr);
    poll(pollfds,2,-1);
    if(globalStop){break;}
    int clisockfd = accept(sockfd,(struct sockaddr *)&cli_addr,&clilen);
    if(clisockfd == -1){continue;}
    thread_node * tmp_node = malloc(sizeof(thread_node));
    tmp_node -> tls_data = malloc(sizeof(thread_data));
    tmp_node -> tls_data -> sockfd = clisockfd;
    tmp_node -> tls_data -> hasReturned = 0;
    tmp_node -> next = head_node;
    tmp_node -> prev = NULL;
    if(head_node != NULL){
      head_node -> prev = tmp_node;
    }
    head_node = tmp_node;
    pthread_create(&(tmp_node -> thread_h),NULL,handle_customer,tmp_node -> tls_data);
  }

  shutdown(sockfd,2);
  close(sockfd);
  thread_node * ptr = head_node;
  while(ptr != NULL){
    if(ptr -> tls_data != NULL){
	  pthread_join(ptr->thread_h,NULL);
      free(ptr -> tls_data);
    }
    thread_node * ptr2 = ptr -> next;
    free(ptr);
    ptr = ptr2;
  }

  return 0;
}
