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
int project_num;

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

FolderStructureNode *create_repo(char *repo_name){
    struct stat st;
    int pro_dir_fd = open(repo_name, O_WRONLY | O_CREAT, 0666);
    int mani_fd = open(".Manifest", O_WRONLY | O_CREAT, 0666);
    FolderStructureNode *init_dir;
    FolderStructureNode *mani;
    init_dir = CreateFolderStructNode(0, 2, repo_name, 0, mani_fd, init_dir);
    int index = 1;
    // uint32_t *md5_arr = calloc(sizeof(unit32_t), 4);
    // uint8_t *text = malloc()
    // char mani_md5 = GetMD5()
    // mani_fd = CreateFolderStructNode(1, 1, ".Manifest", 0, 0, init_dir);
    return init_dir;
}

void init_file_system(){
    repoHashMap = InitializeHashMap(20);
}

void * handle_customer(void * tls){
  thread_data * tls_data = (thread_data *)tls;
  int flags = fcntl(tls_data -> sockfd,F_GETFD,0);
  fcntl(tls_data -> sockfd,F_SETFD,flags | O_NONBLOCK);
  printf("Connection Established\n");
  char str[1024];
  while(!globalStop){
      int msg_len = 0;
      int status = read_all(tls_data -> sockfd,&msg_len,sizeof(msg_len),0);
      if(status <= 0){break;}
      status = read_all(tls_data -> sockfd,str,msg_len,0);
      if(status <= 0){break;}
      if(strncmp(str, "send", 4) == 0){
          int filesize = *((int *)(str + 4));
          char * filedata = malloc(filesize + 1);
          read_all(tls_data -> sockfd,filedata,filesize,0);
          filedata[filesize] = 0;
        //   int fd = open(strcat("./test_dir/", &(int)filedata));
        //   write()
          printf("%d\n",filesize);
          printf("%s\n",filedata);
          int md5_arr_size = (int)(sizeof(uint32_t)*4);
          uint32_t* md5_arr = malloc(sizeof(uint32_t));
          read_all(tls_data ->sockfd, md5_arr,md5_arr_size,0);
          printf("%"PRIu32 " %"PRIu32 " %"PRIu32 " %"PRIu32"\n", md5_arr[0], md5_arr[1], md5_arr[2], md5_arr[3]);
      }else if(strncmp(str, "delt", 4) == 0){
          int path_size = *((int *) (str + 4));
          char * path_str = malloc(path_size + 1);
          read_all(tls_data -> sockfd, path_str, path_size, 0);
          printf("The deleted file name is %s.\n", path_str);
      }else if(strncmp(str, "cret", 4) == 0){
          printf("Cret has been received.\n");
      }
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
      if(ptr -> tls_data -> hasReturned == 0){
	pthread_join(ptr->thread_h,NULL);
      }
      free(ptr -> tls_data);
    }
    thread_node * ptr2 = ptr -> next;
    free(ptr);
    ptr = ptr2;
  }

  return 0;
}
