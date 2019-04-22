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
    if(HashMapFind(repoHashMap, repo_name) != NULL){
        printf("The repository %s has been existed on the server, cannot create again\n", repo_name);
        return NULL;
    }
    // printf("The name is %s\n", repo_name);
    if(mkdir(repo_name, 0777) == -1){
        printf("%s\n",strerror(errno));
    }
    int dir_fd = dirfd(opendir(repo_name));
    int mani_fd = openat(dir_fd,".Manifest", O_WRONLY | O_CREAT, 0666);
    FolderStructureNode *init_dir;
    FolderStructureNode *mani;
    write(mani_fd, "0\n", 2);
    init_dir = CreateFolderStructNode(0, strdup(repo_name), NULL, NULL, init_dir);
    HashMapInsert(repoHashMap, init_dir -> name, init_dir);
    return init_dir;
}

void init_server_file_system(){
    repoHashMap = InitializeHashMap(20);
    DIR *cur_dir = opendir("./");
    struct dirent *dir_d;
    while((dir_d = readdir(cur_dir)) != 0){
        __uint8_t type = dir_d -> d_type;
        char *dir_name = dir_d -> d_name;
        if(type == DT_DIR && IsProject(dir_name) == 0){
            chdir(dir_name);
            FolderStructureNode *root = ConstructStructureFromFile(".Manifest");
            HashMapInsert(repoHashMap, dir_d -> d_name, root);
            chdir("../");
        }
    }
}

int server_checkout(int cli_socket, char *repo){
    if(HashMapFind(repoHashMap, repo) == NULL){
        printf("The project %s is not managed, cannot be checkedout\n", repo);
        return -1;
    }else{
        int repo_dir_fd = open(repo, O_RDONLY);
        int repo_path_len = strlen(repo);
        char *mani_path = malloc(repo_path_len + 1 + 9);
        sprintf(mani_path, "%s/.Manifest", repo);
        //send .Manifest
        SendFile(cli_socket, mani_path);
        FolderStructureNode *root = ConstructStructureFromFile(mani_path);
        int file_size = GetFileNumFromMani(root);
        //send file size
        printf("Fiel size is %d\n", file_size);
        char command[4] = {'n','u','l','l'};
        SendMessage(cli_socket, command, (char *)&file_size);
        //send all other files
        SendFileFromMani(cli_socket, root, repo_dir_fd, repo);
        close(repo_dir_fd);
        free(root);
        // FolderStructureNode *root =
        // chdir("../");
    }
    return 0;
}
int exist_checking(const char* file_name){
  if(HashMapFind(repoHashMap, file_name) == NULL){
    return -1;
  }
  return 0;
}
void * handle_customer(void * tls){
  thread_data * tls_data = (thread_data *)tls;
  int flags = fcntl(tls_data -> sockfd,F_GETFD,0);
  fcntl(tls_data -> sockfd,F_SETFD,flags | O_NONBLOCK);
  printf("Connection Established\n");
  char str[1024];
  while(!globalStop){
      char *receive_data = ReceiveMessage(tls_data -> sockfd);
      if(receive_data == NULL){break;}
      char *command = malloc(sizeof(char) * 5);
      int file_size;
      memcpy(command, receive_data, 4);
      command[4] = 0;
      memcpy(&file_size, receive_data + 4, sizeof(int));

      if(strncmp(command, "ckot", 4) == 0){
        // char *repo_name = malloc(file_size + 1);
        // memcpy(repo_name, receive_data + 8, 0);
        // repo_name[file_size] = 0;
        // printf("%s\n", );
        server_checkout(tls_data -> sockfd, receive_data + 8);
        free(receive_data);
      }else if(strncmp(command, "push",4) == 0){
        char *repo_name = receive_data+8;
        if(exist_checking(repo_name) == -1){
          printf("error: file "%s" not exist\n",repo_name);
          return;
        }
      }else if(strncmp(command, "dist",4) == 0){
        char *repo_name = receive_data +8;
        if(exist_checking(repo_name) == -1){
          printf("error: file "%s" not exist\n",repo_name);
          return;
        }











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

  init_server_file_system();

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
