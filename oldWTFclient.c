#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
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

int process_push(int argc, char ** argv){
    char command[4] = "push";
    char* repo_name = argv[2];
    if(SendMessage(sockfd, command, repo_name)==-1){
        printf("error\n");
    }
    return 0;
}

int process_destroy(int argc, char ** argv){
    char command[4] = "dest";
    char* repo_name = argv[2];
    if(SendMessage(sockfd, command, repo_name) ==-1){
        printf("error\n");
    }
    return 0;
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

// void process_update(int argc,char ** argv){}

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
    write(manifest_pointer, "1\n0 2 0 -1 -1\n",14);
    write(manifest_pointer, "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00\n",48);
    write(manifest_pointer, "\n",1);
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
        // printf("The file path is %s\n", path_data + 8);
        // printf("The real content is %s\n", content_data + 256 + 16 + 4);
        int content_size = *((int *)(content_data + 256 + 16));
        write(file_fd, content_data + 256 + 16 + 4, content_size);
        close(file_fd);
    }
    printf("Project %s checked out successfully\n", repo_name);
    close(dir_pointer);
    close(manifest_pointer);
    free(mani_data);
    free(temp_repo_name);
    return 0;
}

// 1 u, 2 m, 3 a, 4 d
void recur_add(FolderDiffNode *diff, FolderStructureNode *folder, char mode){
    FolderDiffNode *new_node = calloc(sizeof(FolderDiffNode), 1);
    while (folder != NULL) {
        char type = folder -> type;
        new_node -> name = strdup(mode);
        new_node ->
        switch (type) {
            case 1:
                new_node -> type = mode;
                new_node -> oldHash = 0000;
                memcpy(new_node -> newHash,  folder -> hash, 16);
                diff -> nextFile = new_node;
                diff = new_node;
                break;
            case 2:
                new_node -> type = mode + 4;
                diff -> folderHead = new_node;
                diff = new_node;
                recur_add(diff, folder -> folderHead, mode);
                break;
        }
    }
}

// folder1 is sever, folder2 is client
void compare_diff(FolderStructureNode *folder1, FolderStructureNode *folder2, FolderDiffNode *diff, char *project_name, char *cur_path){

    HashMap *fd2_hmap =  InitializeHashMap(20);
    //create fd1_map
    FolderStructureNode *temp2 = folder2;
    while(temp2 != NULL){
        HashMapInsert(fd2_hmap, temp2 -> name, temp2);
        temp2 = temp2 -> nextFile;
    }

    FolderStructureNode *temp1 = folder1;
    while(temp1 != NULL){
        // if the file on the server
        char *temp1_name = temp1 -> name;
        int temp1_path_len = strlen(cur_path) + 1 + strlen(temp1_name);
        char *temp1_path = malloc(temp1_path_len + 1);
        sprintf(temp1_path, "%s/%s", cur_path, temp1_name);
        HashMapNode *temp2_hmap_node = HashMapFind(fd2_hmap, temp1_name);
        FolderDiffNode *new_diff_node = calloc(sizeof(FolderDiffNode), 1);
        new_diff_node -> name = temp1_name;
        // The file exists on server, but does not on client
        if(temp2_hmap_node == NULL){
            if(temp1 -> type == 1){
                new_diff_node -> type = 3;
                memcpy(new_diff_node -> newHash, temp2_hmap_node -> hash, 16);
                temp1 = temp1 -> nextFile;
            }
            if(temp1 -> type == 2){
                new_diff_node -> type = 7;
                diff -> folderHead = new_diff_node;
                diff = new_diff_node;
                // temp1 = temp1 -> folderHead;
                recur_add(diff, temp1, 2);
                // compare_diff(folder1 -> folderHead, )
            }
        }else {
            // The file exists both on server and client
            temp2_folder_node = temp2_hmap_node -> nodePtr;
            MD5FileInfo *cur_file_info = GetMD5FileInfo(open(temp1_path, O_RDONLY));
            uint8_t cur_file_md5 = cur_file_info -> hash; //cur_md5
            unit8_t temp1_mani_md5 = temp1 -> hash; //server_mani
            unit8_t temp2_mani_md5 = temp2_folder_node -> hash; //client_mani
            // server_mani == client_mani, client_mani != cur_md5, client updated
            if(CompareMD5(temp1_mani_md5, temp2_mani_md5) == 0 && CompareMD5(temp2_mani_md5, cur_file_md5) == -1){
                if(temp1 -> type == 1){
                    new_diff_node -> type = 1;
                    memcpy(new_diff_node -> oldHash, temp2_mani_md5, 16);
                    memcpy(new_diff_node -> newHash, cur_file_md5, 16);
                }
                if(temp1 -> type == 2)
                    new_diff_node -> type = 5;
            }
            // server_mani != client_mani, client_mani == cur_md5, server updated
            if(CompareMD5(temp1_mani_md5, temp2_mani_md5) == -1 && CompareMD5(temp2_mani_md5, cur_file_md5) == 0){
                if(temp1 -> type == 1){
                    new_diff_node -> type = 2;
                    memcpy(new_diff_node -> oldHash, temp2_mani_md5, 16);
                    memcpy(new_diff_node -> newHash, temp1_mani_md5, 16);
                }
                if(temp1 -> type == 2)
                    new_diff_node -> type = 6;
            }
            // server_mani != client_mani, cient_mani != cur_md5, conflict
            if(CompareMD5(temp1_mani_md5, temp2_mani_md5) == -1 && CompareMD5(temp2_mani_md5, cur_file_md5) == -1){
                int choice;
                printf("There is a conflict of file %s between server and the client, please choose one to preserve, 1 for server, 2 for cient.\n", temp1_name);
                scanf("%d\n", &choice);
                // Keep the server lastest
                if(choice == 1){
                    if(temp1 -> type == 1){
                        new_diff_node -> type = 2;
                        memcpy(new_diff_node -> oldHash, temp2_mani_md5, 16);
                        memcpy(new_diff_node -> newHash, temp1_mani_md5, 16);
                        diff -> nextFile = new_diff_node;
                        diff = new_diff_node;
                    }
                    if(temp1 -> type == 2){
                        new_diff_node -> type = 6;
                        diff -> folderHead = new_diff_node;
                        diff = new_diff_node;
                        compare_diff(temp1 -> folderHead, temp2 -> folderHead, diff, project_name, temp1_path);
                    }
                }
                if(choide == 2){
                    if(temp1 -> type == 1){
                        new_diff_node -> type = 1;
                        memcpy(new_diff_node -> oldHash, temp1_mani_md5, 16);
                        memcpy(new_diff_node -> newHash, cur_file_md5, 16);
                        diff -> nextFile = new_diff_node;
                        diff = new_diff_node;
                    }
                    if(temp1 -> type == 2){
                        new_diff_node -> type = 5;
                        diff -> folderHead = new_diff_node;
                        diff = new_diff_node;
                        compare_diff(temp1 -> folderHead, temp2 -> folderHead, diff, project_name, temp1_path);
                    }
                }
            }
    }

    free(fd2_hmap);
    // Find the difference set of server
    HashMap *fd1_hmap = InitializeHashMap(20);
    FolderDiffNode *diff_set_temp1 = folder1;
    while (diff_set_temp1 != NULL) {
        HashMapInsert(fd1_hmap, diff_set_temp1 -> name, diff_set_temp1);
        diff_set_temp1 = diff_set_temp1 -> nextFile;
    }
    FolderStructureNode *diff_set_temp2 = folder2;
    while (diff_set_temp2 != NULL) {
        if(diff_set_temp2)
    }

    // FolderDiffNode *diff_set_temp1 = folder1;
    // while (diff_set_temp1 != NULL) {
    //     HashMapInsert(fd1_hmap, diff_set_temp1 -> name, diff_set_temp1);
    //     diff_set_temp1 = diff_set_temp1 -> next;
    // }
}

int process_update(int argc, char *argv){
    char *project_name = argv[2];
    if(IsProject(project_name) == -1){
        printf("Project dose not exist on client, cannot be updated%s\n", project_name);
        return -1;
    }
    char *mani_path = strcat(project_name, "/.Manifest");
    FolderStructureNode *client_mani_root = ConstructStructureFromFile(mani_path);
    int mani_len = strlen(mani_path);
    char command[8] = {'u', 'p', 'd', 't'};
    memcpy(command + 4, &mani_len, 4);
    // Send project name to server
    SendMessage(sockfd, command, mani_path);
    // Receive server prjoect .Manifest
    char *server_data = ReceiveFile(sockfd);
    // Create a temp Mani file to receive the mani data from server
    char *server_mani = server_data + 256 + 16 + 4;
    FILE *temp_dir = open(strcmp(project_name, "/~Manifest"), "w");
    fprintf(temp_dir, "%s", server_mani);
    fclose(temp_dir);
    char *server_mani_root = ConstructStructureFromFile("~/.Manifest");

    // Compare the MD5 data to generate .Update files
    FILE *update_fd = open(strcmp(project_name, "/.Update"), "w");

    //HashMap *
    FolderStructureNode *server_folder_first;
    // Insert server nodes into hashmap
    while(server_mani_root != NULL){
        HashMap *server_hash_map = InitializeHashMap(20);
        if(server_mani_root -> type == 1){
            HashMapInsert(server_hash_map, server_mani_root -> name, server_mani_root);
        }
        server_mani_root = server_mani_root
    }
    free(server_folder_first);
    fclose(update_fd);
    remove(temp_dir);
    free(server_data);
    free(server_mani);
    free(mani_path);
    free(project_name);
    free(server_mani_root);
}

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

    if(globalStop) return 0;

    int flags = fcntl(sockfd,F_GETFL,0);
    fcntl(sockfd,F_SETFL,flags | O_NONBLOCK);

    //pthread_create(&receive_thread, NULL, receiveFromServer, (void *) NULL);

    char buffer[BUFFERSIZE];
    int i, j;

    if(strcmp(argv[1], "checkout") == 0){
        process_checkout(argc, argv);
    }else if(strcmp(argv[1], "create") == 0){
        process_create(argc, argv);
    }else if(strcmp(argv[1], "add") == 0){
        process_add(argc, argv);
    }else if(strcmp(argv[1], "push") == 0){
        process_push(argc, argv);
    }else if(strcmp(argv[1], "update") == 0){
        process_update(argc, argv);
    }

    pthread_join(receive_thread,NULL);

    printf("finish\n");
    return 0;
}
