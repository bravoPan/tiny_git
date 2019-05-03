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

int sockfd, port;
volatile char globalStop = 0;

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

// int process_push(int ){
//
// }
int process_add(int argc, char **argv){
    char *project_name = argv[2], *file_name = argv[3];

    if(IsProject(project_name) == -1){
        printf("Project doest not exist %s\n", project_name);
        return -1;
    }

    int project_name_len = strlen(project_name), file_len = strlen(file_name);
    char *tentitive_path = malloc(project_name_len + 1 + file_len);
    sprintf(tentitive_path, "%s/%s", project_name, file_name);
    int file_fd = open(tentitive_path, O_RDONLY);
    if(file_fd < 0){
        printf("The file %s does not exist, cannot add\n", file_name);
        return -1;
    }

    chdir(project_name);
    FolderStructureNode *root = ConstructStructureFromFile(".Manifest");
    chdir("..");

    int dirfd = open(project_name,O_RDONLY),tmpfd;
    FolderStructureNode *parent = root, *curr = root;

    int new_version = root -> version + 1;
    char *path = malloc(256), token;
    int index = 0, i, len = strlen(file_name);
    // FolderStructureNode *parent = SearchStructNodeLayer(), ;

    for(i = 0; i < len; i++){
        if(file_name[i] == '/'){
            path[index] = 0;
            //create a folder node
            FolderStructureNode *temp = SearchStructNodeLayer(path, parent);
            // The middle folder does not exist
            if(temp == NULL){
                FolderStructureNode *new_path = CreateFolderStructNode(2, path, 0, parent->folderHead, NULL, new_version);
                parent -> folderHead = new_path;
                temp = new_path;
                curr = parent;
            }
            parent = temp;
            //curr = temp->folderHead;
            index = 0;
        }else
            path[index++] = file_name[i];
    }

    path[index] = 0;
    FolderStructureNode * temp = SearchStructNodeLayer(path, parent);

    if(temp != NULL){
        printf("File %s is already added\n", file_name);
        return -1;
    }

    MD5FileInfo *fileinfo = GetMD5FileInfo(file_fd);
    FolderStructureNode *new_file = CreateFolderStructNode(1, path, (char *)fileinfo->hash, parent->folderHead , NULL, new_version);
    parent->folderHead = new_file;

    close(dirfd);
    dirfd = open(project_name,O_RDONLY);
    printf("File %s added\n", path);
    int mani_rfd = openat(dirfd,".Manifest",O_WRONLY);
    FILE * mani_fd = fdopen(mani_rfd,"w");
    SerializeStructure(root, mani_fd);
    fclose(mani_fd);
    close(mani_rfd);
    close(dirfd);
    // free(project_name);
    // free(file_name);
    return 0;
}

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
    int repo_name_len = strlen(repo_name);
    // repo_name = malloc(repo_name_len + 1);
    write(manifest_pointer, repo_name, repo_name_len);
    write(manifest_pointer, "\n",1);
}

/*
int process_checkout(int argc, char **argv){
    char *repo_name = argv[2];
    if(IsProject(repo_name) == 0){
        printf("The project %s has existed on the client, it cannot be checked out\n", repo_name);
        return -1;
    }
    char command[4] = {'c', 'k', 'o', 't'};
    SendMessage(sockfd, command, repo_name);

    //Receive .Manifest from server
    char *mani_data = ReceiveFile(sockfd, project_name, ".Manifest");
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
        char *content_data = ReceiveFile(sockfd, );
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
*/

// 1 u, 2 m, 3 a, 4 d
/*
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
*/

HashMap *hashify_layer(FolderDiffNode *root){
    HashMap *fd_hmap = InitializeHashMap(20);
    FolderDiffNode *temp = root -> folderHead;
    while (temp != NULL) {
        HashMapInsert(fd_hmap, temp -> name, temp);
        temp = temp -> nextFile;
    }
    return fd_hmap;
}

int write_update_line(FILE *fd, char mode, char client_hash[16], char server_hash[16], char *path){
    char *str_client_hash = convert_hexmd5_to_path(client_hash);
    char *str_server_hash =
    convert_hexmd5_to_path(server_hash);
    fprintf(fd, "%c %s %s %s\n", mode, client_hash, server_hash, path);
}

char *combine_path(const char* par_path, const char * cur_path){
    int par_path_len = strlen(par_path);
    int cur_path_len = strlen(cur_path);
    char *new_path = malloc(par_path_len + 1 + cur_path_len);
    sprintf(new_path, "%s/%s", par_path_len, cur_path);
    return new_path;
}

// folder1 is sever, folder2 is client
void compare_diff(FolderStructureNode *client_root, FolderStructureNode *server_root, FolderDiffNode *diff, char *project_name, char *cur_path){
    
    char *parent_path = project_name;

    HashMap *server_hmap =  hashify_layer(server_root);
    FolderStructureNode *client_temp = folder1 -> folderHead;
    int parent_folder_fd = open(project_name, O_RDONLY);
    char *temp =
    // iterate client folder
    while(client_temp != NULL){
        // if the file on the server
        char *client_temp_name = client_temp -> name;
        // int client_temp_path_len = strlen(cur_path) + 1 + strlen(temp1_name);
        // char *temp1_path = malloc(temp1_path_len + 1);
        // sprintf(temp1_path, "%s/%s", cur_path, temp1_name);
        //
        HashMapNode *server_search_node = HashMapFind(server_hmap, client_temp_name);
        // FolderDiffNode *new_diff_node = calloc(sizeof(FolderDiffNode), 1);
        // new_diff_node -> name = temp1_name;
        // The file exists on server, but does not on client
        if(server_search_node == NULL){
            // The file is redundent, delete it
            if(client_temp -> type == 1){
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
            FolderStructureNode *server_folder_node = server_search_node -> nodePtr;

            MD5FileInfo *client_file_info = GetMD5FileInfo(open(temp1_path, O_RDONLY));
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

void process_test(int argc, char **argv){
    char command[4] = {'t', 'e', 's', 't'};
    char *test = "I am a string.";
    SendMessage(sockfd, command, test, strlen(test));
    // char hash[16] = {'8','2',''}
    // char
    // char *msg = ReceiveFile(sockfd, "ttt", ".Manifest");
    // int str_len;
    // memcpy(&str_len, (int *)msg, 4);
    // char *test_str = malloc(str_len + 1);
    // memcpy(test_str, msg + 4, str_len);
    // test_str[str_len + 1] = 0;
    // // int msg_len = *(ing *)msg;
    // printf("%s\n", test_str);
    // char *msg = read_all()
}

void process_test_file(int argc, char **argv){
    char command[4] = {'t', 'e', 's', 'f'};
    SendMessage(sockfd, command, "NULL", 4);
    const char *path = "955C8603EE5DC7BE4E551C44966E24DC";
    char *hash = convert_path_to_hexmd5(path);
    char *data = ReceiveFile(sockfd, "ttt", hash);
    int msg_len = *(int *)data;
    printf("The size of the hash is %d\n", *(int *)data);
    write(1,data + 4,msg_len);
    write(1,"\n",1);

    // char *hash = convert_path_to_hexmd5(path);
    // char *original_path = convert_hexmd5_to_path(hash);
    // original_path = realloc(original_path, 33);
    // original_path[32] = 0;
    // printf("The path is %s\n", original_path);

    // char *path_hash[32];
    // memcpy(path_hash, "955C8603EE5DC7BE4E551C44966E24DC", 32);
    // char hash[16];
    // char *hexmd5 = convert_path_to_hexmd5(path_hash);
    // memcpy(hash, hexmd5, 16);
    //
    // char *checked_hash = convert_hexmd5_to_path(hash);
    // char check_hash[33];
    // memcpy(check_hash, checked_hash, 32);
    // check_hash[32] = 0;
    // printf("The hash is %s\n", check_hash);

    // char *metadata = ReceiveFile(sockfd, "ttt", hash);
    // int msg_len = *(int *)(metadata);
    // printf("The file size is %d\n", msg_len);
    // char *content = malloc(msg_len + 1);
    // memcpy(content, metadata + 4, msg_len);
    // content[msg_len] = '\0';
    // printf("%s\n", content);
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
    // process_create(argc,argv);

    //pthread_create(&receive_thread, NULL, receiveFromServer, (void *) NULL);
    // SendFile(sockfd, "ppp", ".Manifest");
    // if(strcmp(argv[1], "checkout") == 0){
    //     process_checkout(argc, argv);
    // }
    if(strcmp(argv[1], "create") == 0){
        process_create(argc, argv);
    }else if(strcmp(argv[1], "add") == 0){
        process_add(argc, argv);
    }else if(strcmp(argv[1], "test") == 0){
        process_test(argc, argv);
    }else if(strcmp(argv[1], "testfile") == 0){
        process_test_file(argc, argv);
    }
    // else if(strcmp(argv[1], "push") == 0)
    //     process_push(argc, argv);
    // }else if(strcmp(argv[1], "update") == 0){
    //     process_update(argc, argv);
    // }

    //pthread_join(receive_thread,NULL);

    printf("finish\n");
    return 0;
}
