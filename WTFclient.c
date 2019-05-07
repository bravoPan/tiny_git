#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
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

char *combine_path(const char* par_path, const char * cur_path);

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

int process_create(int argc, char **argv){
    char *repo_name = argv[2];
    if(IsProject(repo_name) == 0){
      printf("The repository %s has been created\n", repo_name);
      return -1;
    }

    int i, msg_len = 8, str_size = strlen(repo_name);
    // send_all(sockfd, &msg_len, sizeof(int), 0);
    char command[8] = {'c', 'r', 'e', 't'};
    memcpy(command + 4, &str_size, 4);
    // memcpy(msg + 4, &str_size, sizeof(int));
    SendMessage(sockfd, command, repo_name, str_size);
    int is_succ;
    read_all(sockfd, &is_succ, 4, 0);
    if(is_succ == -1){
        printf("Connection error, cannot create project %s on the server\n", repo_name);;
        return -1;
    }

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
    return 0;
}

void create_empty_file_from_node(FolderStructureNode *root, const char *parent_name){
    char *child_name = root -> name;
    char *child_path = combine_path(parent_name, child_name);
    char type = root -> type;
    switch (type) {
        case 1:{
            CreateEmptyFolderStructFromPath(child_path);
            return;
        }
        case 2:{
            root = root -> folderHead;
            while(root != NULL){
                create_empty_file_from_node(root, child_path);
                root = root -> nextFile;
            }
            return;
        }
    }
    free(child_path);
}

void write_stream_into_file(const char *receive_data, const char *path){
    int msg_len = *(int *)receive_data;
    const char *content = receive_data + 4;
    int file_fd = open(path, O_WRONLY);
    write(file_fd, content, msg_len);
    close(file_fd);
}

void receive_file_from_node(int socket, FolderStructureNode *root, const char *parent_path, const char *project_name){
    char *child_name = root -> name;
    char type = root -> type;
    char *child_path = combine_path(parent_path, child_name);
    switch (type) {
        case 1:{
            char hash[16];
            memcpy(hash, root -> hash, 16);
            char *receive_data = ReceiveFile(socket, project_name, hash);
            write_stream_into_file(receive_data, child_path);
            free(receive_data);
            return;
        }
        case 2:{
            FolderStructureNode *temp = root -> folderHead;
            while (temp != NULL) {
                receive_file_from_node(socket, root -> folderHead, child_path, project_name);
                temp = temp -> nextFile;
            }
            return;
        }
    }
    free(child_path);
}

int process_checkout(int argc, char **argv){
    char *repo_name = argv[2];
    // if(IsProject(repo_name) == 0){
    //     printf("The project %s has existed on the client, it cannot be checked out\n", repo_name);
    //     return -1;
    // }

    if(mkdir(repo_name, 0777) == -1){
      printf("%s\n",strerror(errno));
    }
    int project_fd = dirfd(opendir(repo_name));
    char *mani_name = combine_path(repo_name, ".Manifest");
    int manifest_fd = open(mani_name, O_WRONLY | O_CREAT,0666);
    // int manifest_fd = openat(project_fd, ".Manifest", O_WRONLY | O_CREAT,0666);
    close(project_fd);
    //Receive .Manifest from server
    char *mani_data = ReceiveFile(sockfd, repo_name, ".Manifest");

    int mani_len = *(int *)mani_data;
    write(manifest_fd, mani_data + 4, mani_len);
    close(manifest_fd);


    FolderStructureNode *root = ConstructStructureFromFile(mani_name);
    // escape the project node
    FolderStructureNode *temp = root -> folderHead;
    while (temp != NULL) {
        create_empty_file_from_node(temp, repo_name);
        temp = temp -> nextFile;
    }

    temp = root -> folderHead;
    while (temp != NULL) {
        receive_file_from_node(sockfd, temp, root -> name, repo_name);
        temp = temp -> nextFile;
    }

    printf("Project %s checked out successfully\n", repo_name);
    free(root);
    // close(dir_pointer);
    // close(manifest_pointer);
    // free(mani_data);
    // free(temp_repo_name);
    return 0;
}

// 1 u, 2 m, 3 a, 4 d

int write_update_line(FILE *fd, char mode, char client_hash[16], char server_hash[16], char *path){
    char *str_client_hash = convert_hexmd5_to_path((unsigned char*)client_hash);
    str_client_hash = realloc(str_client_hash, 33);
    str_client_hash[32] = 0;
    char *str_server_hash = convert_hexmd5_to_path((unsigned char*)server_hash);
    str_server_hash = realloc(str_server_hash, 33);
    str_server_hash[32] = 0;
    fprintf(fd, "%c %s %s\n%s\n", mode, str_client_hash, str_server_hash, path);
    // fprintf(fd, "%s\n", );
    free(str_client_hash);
    return 0;
}

// recur_
void recur_comp_oneside(FILE *update_fd, FolderStructureNode *temp, char *parent_path, int client_version, int server_version, int version_same_token, int version_diff_token){
    if((client_version == server_version && version_same_token == 0) || (client_version != server_version && version_diff_token == 0)){return;}
    //char *temp = folder_root -> folderHead;
    char *child_path = combine_path(parent_path, temp -> name);
    switch(temp->type){
        case 1:{
            char emptyHash[16] = {0};
            char new_hash[16];
            memcpy(new_hash, temp -> hash, 16);
            if(client_version == server_version){
                if(version_same_token == 'D'){
                    write_update_line(update_fd, version_same_token, emptyHash, emptyHash, child_path);
                }
                else{
                    write_update_line(update_fd, version_same_token, emptyHash, new_hash, child_path);
                }
            }
            else
                write_update_line(update_fd, version_diff_token, emptyHash, emptyHash, child_path);
            return;
        }
        case 2:{
            temp = temp->folderHead;
            while(temp != NULL){
                recur_comp_oneside(update_fd, temp, child_path, client_version, server_version, version_same_token, version_diff_token);
                temp = temp->nextFile;
            }
            return;
        }
        default:return;
    }
    /*
    while (temp != NULL) {
        char type = temp -> type;
        switch (type) {
            case 1:{
                char emptyHash[16] = {0};
                if(client_version == server_version)
                    write_update_line(update_fd, version_same_token, emptyHash, emptyHash, child_path);
                else
                    write_update_line(update_fd, version_diff_token, emptyHash, emptyHash, child_path);
                //free(old_hash);
                //parent_path = child_path;
                temp = temp -> nextFile;
                // free(act_file_name);
                // close(act_file_fd);
                //close(act_file_fd);
                //free(act_file_fd_md5);
                break;
            }
            case 2:{
                char folder_hash[16] = {0};
                if(client_version == server_version)
                    write_update_line(update_fd, version_same_token, folder_hash, folder_hash, child_path);
                else
                    write_update_line(update_fd, version_same_token, folder_hash, folder_hash, child_path);
                recur_comp_oneside(update_fd, temp -> folderHead, child_path, client_version, server_version, version_same_token, version_diff_token);
                //free(folder_hash);
                break;
            }
        }
    }
    */
}

// folder1 is sever, folder2 is client, be sure the server_root and client_root are folder nodes
void compare_client_server_diff(FILE *update_fd, FolderStructureNode *client_root, FolderStructureNode *server_root, char *project_name, int client_version, int server_version){
    char *parent_path = project_name;
    HashMap *server_hmap =  hashify_layer(server_root);
    FolderStructureNode *client_temp = client_root -> folderHead;
    while(client_temp != NULL){
        // if the file on the server
        char *client_temp_name = client_temp -> name;
        HashMapNode *server_search_node = HashMapFind(server_hmap, client_temp_name);
        char *cur_path = combine_path(parent_path, client_temp_name);
        if(server_search_node == NULL || ((FolderStructureNode *)(server_search_node->nodePtr))->type != client_temp->type){
            // if()
            recur_comp_oneside(update_fd, client_temp, parent_path, client_version, server_version, 'U', 'D');
        }else {
            // The file exists both on server and client
            FolderStructureNode *temp2_folder_node = server_search_node -> nodePtr;
            char type = temp2_folder_node -> type;
            if(type == 1){
                char temp1_mani_md5[16]; //server_mani
                memcpy(temp1_mani_md5, client_temp -> hash, 16);
                char temp2_mani_md5[16];
                memcpy(temp2_mani_md5, temp2_folder_node -> hash, 16);
                int cur_file_fd = open(cur_path, O_RDONLY); //We assume this never fails
                MD5FileInfo *cur_file_info = GetMD5FileInfo(cur_file_fd);
                char cur_file_md5[16]; //cur_md5
                memcpy(cur_file_md5, cur_file_info -> hash, 16);
                free(cur_file_info->data);
                free(cur_file_info);
                close(cur_file_fd);
                if(client_version == server_version){
                    int cmp2 = memcmp(temp1_mani_md5,cur_file_md5,16);
                    if(cmp2 != 0){
                        write_update_line(update_fd, 'U', temp1_mani_md5, cur_file_md5, cur_path);
                    }
                }else{
                    int cmp1 = memcmp(temp1_mani_md5,temp2_mani_md5,16);
                    int cmp2 = memcmp(temp1_mani_md5,cur_file_md5,16);
                    /*
                    if(memcmp(temp1_mani_md5, temp2_mani_md5, 16) == 0 && memcmp(temp2_mani_md5, cur_file_md5, 16) == 0){
                        write_update_line(update_fd, 'U', temp1_mani_md5, cur_file_md5, cur_path);
                    }
                    */
                    if(cmp1 == 0){
                        if(cmp2 != 0){
                            write_update_line(update_fd, 'U', temp1_mani_md5, cur_file_md5, cur_path);
                        }
                    } else {
                        if(cmp2 == 0){
                            write_update_line(update_fd, 'M', temp1_mani_md5, temp2_mani_md5, cur_path);
                        } else {
                            int choice;
                            printf("There is a conflict of file %s between server and the client, please choose one to preserve, 1 for server, 2 for cient.\n", cur_path);
                            scanf("%d\n", &choice);
                            // preserver sever file
                            if(choice == 1){
                                write_update_line(update_fd, 'M', cur_file_md5, temp2_mani_md5, cur_path);
                            }
                            // preserve the client file
                            if(choice == 2){
                                write_update_line(update_fd, 'U', temp2_mani_md5, cur_file_md5, cur_path);
                            }
                        }
                    }
                }
            } else {
                compare_client_server_diff(update_fd, client_temp, temp2_folder_node, cur_path, client_version, server_version);
            }
        }
        client_temp = client_temp -> nextFile;
    }
    DestroyHashMap(server_hmap);
}

void compare_server_client_diff(FILE *update_fd, FolderStructureNode *client_root, FolderStructureNode *server_root, char *project_name, int client_version, int server_version){
    char *parent_path = project_name;
    HashMap *client_hmap = hashify_layer(client_root);
    FolderStructureNode *server_temp = server_root -> folderHead;

    while (server_temp != NULL) {
        char *server_temp_name = server_temp -> name;
        char *cur_path = combine_path(project_name, server_temp_name);
        FolderStructureNode *client_search_node = HashMapFind(client_hmap, server_temp_name) -> nodePtr;
        if(client_search_node == NULL || client_search_node->type != server_temp->type){
            recur_comp_oneside(update_fd, server_temp, parent_path, client_version, server_version, 0, 'A');
        }else{
            char type = client_search_node -> type;
            if(type == 2){
                compare_server_client_diff(update_fd, client_search_node, server_temp, parent_path, client_version, server_version);
            }
        }
        server_temp = server_temp -> nextFile;
        free(cur_path);
    }
    DestroyHashMap(client_hmap);
    //free(parent_path);
}

void comp_diff(FolderStructureNode *client_root, FolderStructureNode *server_root, char *project_name){
    char *update_path = combine_path(project_name, ".Update");
    FILE *update_fd = fopen(update_path, "w");
    int client_version = client_root -> version;
    int server_version = server_root -> version;

    FolderStructureNode *client_temp = client_root;
    FolderStructureNode *server_temp = server_root;
    compare_client_server_diff(update_fd, client_temp, server_temp, project_name, client_version, server_version);

    client_temp = client_root;
    server_temp = server_root;
    compare_server_client_diff(update_fd, client_temp, server_temp, project_name, client_version, server_version);

    fclose(update_fd);
}

int process_update(int argc, char **argv){
    char *project_name = argv[2];
    if(IsProject(project_name) == -1){
        printf("The project %s is not existed, cannot update\n", project_name);
        return -1;
    }
    char *server_temp_mani_name = combine_path(project_name, "~Manifest");
    int server_temp_mani_fd = open(server_temp_mani_name, O_WRONLY | O_CREAT, 0666);
    char *received_data = ReceiveFile(sockfd, project_name, ".Manifest");
    int server_temp_mani_len = *(int *)received_data;
    char *server_temp_mani_data = received_data + 4;
    write(server_temp_mani_fd, server_temp_mani_data, server_temp_mani_len);
    close(server_temp_mani_fd);
    free(received_data);

    char *client_mani_name = combine_path(project_name, ".Manifest");
    char *server_mani_name = combine_path(project_name, "~Manifest");
    FolderStructureNode *client = ConstructStructureFromFile(client_mani_name);
    FolderStructureNode *server = ConstructStructureFromFile(server_mani_name);

    remove(server_temp_mani_name);
    free(server_temp_mani_name);
    comp_diff(client, server, project_name);
    free(client_mani_name);
    free(server_mani_name);
    FreeFolderStructNode(client);
    FreeFolderStructNode(server);
    return 0;
}

int process_currentversion(int sockfd, int argc, char **argv){
    char *project_name = argv[2];
    int project_name_len = strlen(project_name);
    char *client_mani_path = combine_path(project_name, ".Manifest");
    if(IsProject(project_name) == -1){
        free(client_mani_path);
        printf("The project %s is not existed on the client, cannot connect to contace with the server\n", project_name);
        return -1;
    }
    char command[4] = {'c', 'r', 'v', 's'};
    SendMessage(sockfd, command, project_name, project_name_len);
    int is_succ;
    read_all(sockfd, &is_succ, 4, 0);
    if(is_succ == -1){
        printf("The project %s is not eisted on the server, cannot check currentversion\n", project_name);
        return -1;
    }
    int server_version;
    read_all(sockfd, &server_version, 4, 0);
    printf("The current version on the server is: %d\n", server_version);
    free(client_mani_path);
    return 0;
}

void process_test(int argc, char **argv){
    char command[4] = {'t', 'e', 's', 't'};
    char *test = "I am a string.";
    SendMessage(sockfd, command, test, strlen(test));
}

void process_test_file(int argc, char **argv){
    char command[4] = {'t', 'e', 's', 'f'};
    SendMessage(sockfd, command, "NULL", 4);
    char *path = "955C8603EE5DC7BE4E551C44966E24DC";
    char *hash = convert_path_to_hexmd5(path);
    char *data = ReceiveFile(sockfd, "ttt", hash);
    int msg_len = *(int *)data;
    printf("The size of the hash is %d\n", *(int *)data);
    write(1,data + 4,msg_len);
    write(1,"\n",1);
}

int process_history(int sockfd, int argc, char **argv){
    char *project_name = argv[2];
    int project_name_len = strlen(project_name);
    if(IsProject(project_name) == -1){
        printf("The project %s is not existed on the client, please check before checking history\n", project_name);
        return -1;
    }
    char command[4] = {'h', 'i', 's', 't'};
    if(SendMessage(sockfd, command, project_name, project_name_len) == -1){
        printf("Connection error, cannot checkout repo %s history\n", project_name);
        return -1;
    }
    int is_succ;
    read_all(sockfd, &is_succ, 4, 0);
    // char *history_metadata_src = ReceiveFile(project_name, ".History");
    if(is_succ == -1){
        printf("Connection error, cannot checkout repo %s history\n", project_name);
        return -1;
    }
    int history_len;
    read_all(sockfd, &history_len, 4, 0);
    char *history = malloc(history_len);
    read_all(sockfd, history, history_len, 0);
    char *history_path = combine_path(project_name, ".History");
    int history_fd = open(history_path, O_WRONLY | O_CREAT, 0666);
    write(history_fd, history, history_len);
    close(history_fd);
    free(history_path);
    free(history);
    return 0;
}

int process_destroy(int sockfd, int argc, char **argv){
    char *project_name = argv[2];
    int project_name_len = strlen(project_name);
    if(IsProject(project_name) == -1){
        printf("The project %s is not existed on the client, please check before checking history\n", project_name);
        return -1;
    }
    char command[4] = {'d', 'e', 's', 't'};
    SendMessage(sockfd, command, project_name, project_name_len);
    int is_succ;
    read_all(sockfd, &is_succ, 4, 0);
    if(is_succ == -1){
        printf("Cannot connect to server, the project %s destroy error\n", project_name);
        return -1;
    }
    remove_dir(project_name);
    printf("Project %s destroy successfully\n", project_name);
    return 0;
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
    if(strcmp(argv[1], "create") == 0){
        process_create(argc, argv);
    }else if(strcmp(argv[1], "add") == 0){
        process_add(argc, argv);
    }else if(strcmp(argv[1], "test") == 0){
        process_test(argc, argv);
    }else if(strcmp(argv[1], "testfile") == 0){
        process_test_file(argc, argv);
    }else if(strcmp(argv[1], "update") == 0){
        process_update(argc, argv);
    }else if(strcmp(argv[1], "checkout") == 0){
        process_checkout(argc, argv);
    }else if(strcmp(argv[1], "upgrade") == 0){
        process_upgrade(sockfd, argc, argv);
    }else if(strcmp(argv[1], "commit") == 0){
        process_commit(sockfd, argc, argv);
    }else if(strcmp(argv[1], "testsendfile") == 0){
        int res = SendFile(sockfd, "xxx", "1.txt",NULL);
        printf("The send result is %d\n", res);
    }else if(strcmp(argv[1], "push") == 0){
        PushVersion(sockfd, argv[2]);
        process_push(sockfd, argc, argv);
    }else if(strcmp(argv[1], "remove") == 0){
        process_remove(sockfd, argc, argv);
    }else if(strcmp(argv[1], "currentversion") == 0){
        process_currentversion(sockfd, argc, argv);
    }else if(strcmp(argv[1], "history") == 0){
        process_history(sockfd, argc, argv);
    }else if(strcmp(argv[1], "rollback") == 0){
        process_rollback(sockfd, argc, argv);
    }else if(strcmp(argv[1], "destroy")){
        process_destroy(sockfd, argc, argv);
    }
    printf("finish\n");
    return 0;
}
