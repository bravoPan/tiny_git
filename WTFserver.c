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

extern HistoryBuffer * currentPushHead;

void FreeHistoryBuffer(void){
    HistoryBuffer * ptr = currentPushHead;
    while(ptr != NULL){
        HistoryBuffer * ptr2 = ptr->nextBuffer;
        free(ptr);
        ptr = ptr2;
    }
}

typedef struct _thread_data{
  int sockfd;
  int isPushing;
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

pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;

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
    // int file_len = *(int *)(metadata + project_name_len + 16);
    int i;
    for(i = 0;i < 16;++i){
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

void handle_test_file(int socket){
    return;
    // int a = HandleRecieveFile(socket, );
    // printf("hello\n");
}

void process_handle_currentversion(int sockfd, char *metadata){
    int msg_len = *(int *)(metadata + 4);
    char *project_name = malloc(msg_len + 1);
    memcpy(project_name, metadata + 8, msg_len);
    project_name[msg_len] = 0;
    if(IsProject(project_name) == -1){
        int fail = -1;
        send_all(sockfd, &fail, 4, 0);
        return;
    }
    int succ = 1;
    send_all(sockfd, &succ, 4, 0);
    char *mani_path = combine_path(project_name, ".Manifest");
    FolderStructureNode *root = ConstructStructureFromFile(mani_path);
    int server_version = root -> version;
    // int succ_msg[2];
    // succ_msg[0] = 0;
    // succ_msg[1] = server_version;
    send_all(sockfd, &server_version, 4, 0);
    free(mani_path);
    free(project_name);
    FreeFolderStructNode(root);
}

int handle_create(int sockfd, char *project_name){
    if(IsProject(project_name) == 0){
        printf("The project %s exists, cannot create again\n", project_name);
        int fail = 0;
        send_all(sockfd, &fail, 4, 0);
        return -1;
    }
    mkdir(project_name, 0777);
    char *mani_path = combine_path(project_name, ".Manifest");
    char *history_path = combine_path(project_name, ".History");
    int manifest_pointer = open(mani_path, O_WRONLY | O_CREAT, 0666);
    write(manifest_pointer, "1\n0 2 0 -1 -1\n",14);
    write(manifest_pointer, "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00\n",48);
    int project_name_len = strlen(project_name);
    write(manifest_pointer, project_name, project_name_len);
    write(manifest_pointer, "\n",1);

    int history_fd = open(history_path, O_WRONLY | O_CREAT, 0666);
    close(manifest_pointer);
    close(history_fd);

    free(mani_path);
    free(history_path);
    int succ = 0;
    send_all(sockfd, &succ, 4, 0);
    return 0;
}

int handle_rollback(int sockfd, char *metadata_src){
    char *metadata = metadata_src + 8;
    int project_name_len = strlen(metadata);
    char *project_name = malloc(project_name_len + 1);
    memcmp(project_name, metadata, project_name_len);
    project_name[project_name_len] = 0;
    if(IsProject(project_name) == -1){
        printf("The project %s is not existed on the server, cannot rollback\n", project_name);
        int fail = -1;
        send_all(sockfd, &fail, 4, 0);
        return -1;
    }
    int version = *(int *)metadata + 4 + 1;
    char *history_path = combine_path(project_name, ".History");
    FILE *history_fd = fopen(history_path, "r");

    int i;
    char *buf = malloc(1024);
    for(i = 0; i < version - 1; i++){
        int file_num;
        fscanf(history_fd, "%d\n", &file_num);
        int j;
        for(j = 0; j < 3 * file_num; j++){
            fgets(buf, 1024, history_fd);
        }
        fgets(buf, 1024, history_fd);
        fgets(buf, 1024, history_fd);
        int hash_num;
        sscanf(buf, "%d", &hash_num);
        for(j = 0; j < hash_num; j++){
            fgets(buf, 1024, history_fd);
        }
        fgets(buf, 1024, history_fd);
    }

    char *temp_mani_name = combine_path(project_name, "~Manifest");
    FILE *temp_mani_fd = fopen(temp_mani_name, "w");
    int node_num;
    fscanf(history_fd, "%d\n", &node_num);
    // node_num = strlen();
    fprintf(temp_mani_fd, "%d\n", node_num);
    int j, file_num;
    for(j = 0; j < file_num; j++){
        fgets(buf, 1024, history_fd);
        fprintf(temp_mani_fd, "%s", buf);
    }
    char *new_name = combine_path(project_name, ".Manifest");
    rename(temp_mani_name, new_name);
    free(new_name);
    fclose(temp_mani_fd);
    free(temp_mani_name);
    free(buf);
    fclose(history_fd);
    free(history_path);
    int succ = 0;
    send_all(sockfd, &succ, 4, 0);
    return 0;
}

int handle_destory(int sockfd, char *metadata_src){
    char *metadata = metadata_src + 8;
    int project_name_len = strlen(metadata);
    char *project_name = malloc(project_name_len + 1);
    memcpy(project_name, metadata, project_name_len);
    project_name[project_name_len] = 0;
    if(IsProject(metadata) == -1){
        printf("The project %s is not existed on the server, cannot rollback\n", project_name);
        int fail = -1;
        send_all(sockfd, &fail, 4, 0);
        return -1;
    }
    int succ = 0;
    send_all(sockfd, &succ, 4, 0);
    remove_dir(project_name);

    return 0;
}

void * handle_customer(void * tls){
    pthread_mutex_lock(&client_mutex);
    thread_data * tls_data = (thread_data *)tls;
    int flags = fcntl(tls_data -> sockfd,F_GETFD,0);
    fcntl(tls_data -> sockfd,F_SETFD,flags | O_NONBLOCK);
    printf("Connection Established\n");
    while(!globalStop){
        char *receive_data = ReceiveMessage(tls_data -> sockfd);
        if(receive_data == NULL){break;}
        if(strncmp(receive_data, "test", 4) == 0){
            handle_test_file(tls_data -> sockfd);
            int file_size = *(int *)(receive_data + 4);
            printf("The fiel size is %d\n", file_size);
            char *print_test = malloc(file_size + 1);
            memcpy(print_test, receive_data + 8, file_size);
            print_test[file_size] = '\0';
            printf("The real %s\n", print_test);
        }else if(strncmp(receive_data, "cret", 4) == 0){
            int project_name_len = *(int *)(receive_data + 4);
            char *project_name = malloc(project_name_len + 1);
            memcpy(project_name, receive_data + 8, project_name_len);
            project_name[project_name_len] = 0;
            handle_create(tls_data -> sockfd, project_name);
            free(project_name);
        }else if(strncmp(receive_data, "tesf", 4) == 0){
            // int file_size =*(int *)(receive_data + 4);
            // printf("The fiel size is %d\n", file_size);
            handle_test_file(tls_data -> sockfd);
        }else if(strncmp(receive_data, "recv", 4) == 0){
            HandleRecieveFile(tls_data -> sockfd, receive_data);
        }else if(strncmp(receive_data, "send", 4) == 0){
            if(tls_data -> isPushing == 0){
                //Return error
                int fail = -1;
                send_all(tls_data -> sockfd, &fail, 4, 0);
            } else
            HandleSendFile(tls_data -> sockfd, receive_data);
        }else if(strncmp(receive_data, "fail", 4) == 0){
            break;
        }else if(strncmp(receive_data, "push", 4) == 0){
            if(tls_data -> isPushing == 1){
                //Return error
                int fail = -1;
                send_all(tls_data -> sockfd, &fail, 4, 0);
            } else {
                tls_data -> isPushing = 1;
                if(HandlePushVersion(tls_data -> sockfd, receive_data) == -1){
                    tls_data->isPushing = 0;
                }
            }
        }else if(strncmp(receive_data, "cplt", 4) == 0){
            if(tls_data -> isPushing == 0){
                //Return error
                int fail = -1;
                send_all(tls_data -> sockfd, &fail, 4, 0);
            } else {
                tls_data -> isPushing = 0;
                HandleComplete(tls_data -> sockfd, receive_data);
            }
            // Write the new .Manifest in the client
            int project_name_len = *(int *)(receive_data + 4);
            char *project_name = malloc(project_name_len + 1);
            memcpy(project_name, receive_data + 8, project_name_len);
            project_name[project_name_len] = 0;
            char *mani_path = combine_path(project_name, ".Manifest");
            char *history_path = combine_path(project_name, ".History");
            FILE * history_fd = fopen(history_path, "a");
            int mani_fd = open(mani_path, O_RDONLY);
            MD5FileInfo *mani_md5_info = GetMD5FileInfo(mani_fd);
            int mani_size = mani_md5_info -> file_size;
            char *mani_data = mani_md5_info -> data;
            fwrite(mani_data, mani_size,1,history_fd);

            FolderStructureNode *root = ConstructStructureFromFile(mani_path);
            int version = root -> version;
            char *version_line = malloc(8 + 4);
            sprintf(version_line, "version:%d\n", version);
            fwrite(version_line, 1,12,history_fd);
            int sendFileCount = 0;
            HistoryBuffer * ptr = currentPushHead;
            while(ptr != NULL){
                ++sendFileCount;ptr = ptr->nextBuffer;
            }
            fprintf(history_fd,"%d\n",sendFileCount);
            ptr = currentPushHead;
            while(ptr != NULL){
                fprintf(history_fd,"%s\n",ptr->hash);
                ptr = ptr->nextBuffer;
            }
            char *delimiter = "--------------------\n";
            fwrite(delimiter, 21,1,history_fd);
            free(version_line);
            fclose(history_fd);
            // close(mani_fd);
            free(mani_data);
            free(mani_md5_info);
            free(mani_path);
            free(history_path);
        }else if(strncmp(receive_data, "crvs", 4) == 0){
            process_handle_currentversion(tls_data -> sockfd, receive_data);
        }else if(strncmp(receive_data, "fail", 4) == 0){
            tls_data->isPushing = 0;
            break;
        }else if(strncmp(receive_data, "hist", 4) == 0){
            int project_name_len = *(int *)(receive_data + 4);
            char *project_name = malloc(project_name_len + 1);
            memcpy(project_name, receive_data + 8, project_name_len);
            project_name[project_name_len] = 0;
            HandleHistory(tls_data -> sockfd, project_name);
            free(project_name);
        }else if(strncmp(receive_data, "roll", 4) == 0){
            handle_rollback(tls_data -> sockfd, receive_data);
        }else if(strncmp(receive_data, "dest", 4) == 0){
            handle_destory(tls_data -> sockfd, receive_data);
        }
        free(receive_data);
    }
    printf("Connection Terminated\n");
    pthread_mutex_unlock(&client_mutex);
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
    tmp_node -> tls_data -> isPushing = 0;
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
