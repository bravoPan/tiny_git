#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <inttypes.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/dir.h>
#include <sys/stat.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include "WTFclient.h"
#include "utility.h"

#define BUFFERSIZE 1024

int sockfd, port;
volatile char globalStop = 0;
volatile char responseReceived = 0;
// HashMap *projectHashMap;
/*0: No response
  1: Successs
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

void process_update(int argc,char ** argv){}

void process_create(int argc, char **argv){
    char *repo_name = argv[2];
    if(IsProject(repo_name) == 0){
      printf("The repository %s has been created\n", repo_name);
      return;
    }

    int i, msg_len = 8, str_size = strlen(repo_name);
    send_all(sockfd, &msg_len, sizeof(int), 0);
    char msg[8] = {'c', 'r', 'e', 't'};
    memcpy(msg + 4, &str_size, sizeof(int));
    send_all(sockfd, msg, msg_len, 0);
    send_all(sockfd, repo_name, str_size, 0);


    if(mkdir(repo_name, 0777) == -1){
      printf("%s\n",strerror(errno));
    }
    int file_pointer = dirfd(opendir(repo_name));
    int manifest_pointer = openat(file_pointer, ".Manifest", O_WRONLY | O_CREAT,0666);
    write(manifest_pointer, "0\n",2);
}

int process_add(int argc, char **argv){
    char *project_name = argv[2], *file_name = argv[3];

    int file_fd;
    if(IsProject(project_name) == -1){
        printf("Project doest not exist %s\n", project_name);
        return -1;
    }

    chdir(project_name);
    FolderStructureNode *root = ConstructStructureFromFile(".Manifest");
    chdir("..");

    int dirfd = open(project_name,O_RDONLY),tmpfd;
    FolderStructureNode *parent = NULL, *curr = root;

    char *path = malloc(256), token;
    int index = 0, i, len = strlen(file_name);
    for(i = 0; i < len; i++){
        if(file_name[i] == '/'){
            path[index] = 0;
            //create a folder node
            FolderStructureNode *temp = SearchStructNode(curr, path);
            if(temp == NULL){
                FolderStructureNode *new_path = CreateFolderStructNode(2, path, 0, curr, NULL);
                //if root isn't existed
                if(parent == NULL){
                    root = new_path;
                } else {
                    parent->folderHead = new_path;
                }
                temp = new_path;
            }
            parent = temp;curr = temp->folderHead;
            tmpfd = openat(dirfd,path,O_RDONLY);
            if(tmpfd == -1){
                printf("File %s does not exist\n", file_name);
                return -1;
            }
            close(dirfd);dirfd = tmpfd;
            index = 0;
        }else
            path[index++] = file_name[i];
    }

    path[index] = 0;
    FolderStructureNode * temp = SearchStructNode(curr,path);
    if(temp != NULL){
        printf("File %s is already added\n", file_name);
        return -1;
    }

    if((file_fd = openat(dirfd, path, O_RDONLY)) == -1){
        printf("File %s does not exist.\n", path);
        return -1;
    }
    close(file_fd);

    MD5FileInfo *fileinfo = GetMD5FileInfo(file_fd);
    FolderStructureNode *new_file = CreateFolderStructNode(1, path, (char *)fileinfo->hash, curr, NULL);
    if(parent == NULL){
        root = new_file;
    } else {
        parent->folderHead = new_file;
    }

    close(dirfd);
    dirfd = open(project_name,O_RDONLY);
    printf("File %s added\n", path);
    int mani_rfd = openat(dirfd,".Manifest",O_WRONLY);
    FILE * mani_fd = fdopen(mani_rfd,"w");
    SerializeStructure(root, mani_fd);
    fclose(mani_fd);
    close(mani_rfd);
    close(dirfd);
    return 0;
}

int process_checkout(int argc, char **argv){
    char *repo_name = argv[2];
    if(IsProject(repo_name) == 0){
        printf("The project %s has existed on the client, it cannot be checked out\n", repo_name);
        return -1;
    }
    char command[4] = {'c', 'k', 'o', 't'};
    SendMessage(sockfd, command, repo_name);

    //Receive .Manifest from server
    char *mani_data = ReceiveFile(sockfd);
    char name[256];
    uint8_t hash[16];
    int file_size;
    memcpy(name, mani_data, 256);
    memcpy(hash, mani_data + 256, 16);
    memcpy(&file_size, mani_data + 256 + 16, 4);
    char * content = malloc(file_size + 1);
    memcpy(content, mani_data + 256 + 16 + 4, file_size);
    content[file_size] = 0;
    //create a folder
    if(mkdir(repo_name, 0777) == -1){
      printf("%s\n",strerror(errno));
    }
    int dir_pointer = open(repo_name,O_RDONLY);
    int manifest_pointer = openat(dir_pointer, ".Manifest", O_WRONLY | O_CREAT, 0666);

    write(manifest_pointer, content, file_size);


    char *temp_repo_name = strdup(repo_name);
    FolderStructureNode *root = ConstructStructureFromFile(strcat(temp_repo_name, "/.Manifest"));

    CreateEmptyFolderStructFromMani(root, dir_pointer, repo_name);

    char *file_num_data = ReceiveMessage(sockfd);
    int file_num = *((int *)(file_num_data + 8)), i;
    // printf("The file num is %d\n", file_num);
    for(i = 0; i < file_num; i++){
        char *path_data = ReceiveMessage(sockfd);
        int file_fd = open(path_data + 8, O_WRONLY);
        char *content_data = ReceiveFile(sockfd);
        printf("The file path is %s\n", path_data + 8);
        printf("The real content is %s\n", content_data + 256 + 16 + 4);
        int conten_size = *((int *)(content_data + 256 + 16));
        write(file_fd, content + 256 + 16 + 4, conten_size);
        close(file_fd);
    }
    printf("Project %s checked out successfully\n", repo_name);
    close(dir_pointer);
    close(manifest_pointer);
    free(mani_data);
    free(temp_repo_name);
    // printf("The real content is %s\n", content);
    return 0;
}

// int process_push(int argc, char **argv){
// }

void init_client_file_system(){

}

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

    //pthread_create(&receive_thread, NULL, receiveFromServer, (void *) NULL);

    char buffer[BUFFERSIZE];
    int i, j;

    /*
    if(strcmp(argv[1],"checkout") == 0){
        process_checkout(argc,argv);
    } else if(strcmp(argv[1],"update") == 0){
        process_update(argc,argv);
    } else {
        output_error(0);
    }
    */
    if(strcmp(argv[1], "checkout") == 0){
        process_checkout(argc, argv);
    }else if(strcmp(argv[1], "create") == 0){
        process_create(argc, argv);
    }else if(strcmp(argv[1], "add") == 0){
        process_add(argc, argv);
    }


    // SendFile(sockfd,"./utility.h");
    // DeleteFile(sockfd, "./test_dir/a.txt");
    // process_checkout(argc, argv);
    // process_add(argc, argv);
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
