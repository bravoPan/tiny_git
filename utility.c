#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/dir.h>
#include <fcntl.h>
#include <errno.h>

#include "utility.h"
#define __USE_XOPEN_EXTENDED 500
#include <ftw.h>

char *combine_path(const char* par_path, const char * cur_path);

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
  while (!globalStop && len > 0){
    ssize_t r = send(socket, data, len, sig);
    if (r <= 0 && errno != EAGAIN){return -1;} else if(errno == EAGAIN){sleep(0);}
    len -= r;
    data = (const char*)data + r;
  }
  if(len > 0){return -1;} else {return 1;}
}

ssize_t read_all(int socket,void * data,size_t len,int sig){
    while (!globalStop && len > 0){
      ssize_t r = recv(socket, data, len, sig);
      if (r <= 0 && errno != EAGAIN){return -1;} else if(errno == EAGAIN){sleep(0);continue;}
      len -= r;
      data = (char *)data + r;
    }
    if(len > 0){return -1;} else {return 1;}
}

/*
Command: send recv
*/

/*protocal format:
send size 4 to inform receiver
send char[8] 4 bytes for command, 4 bytes for size of content
send content
*/
int SendMessage(int sockfd, char command[4], const char * msg,int msg_len){
    int msg_size = 8;
    if(send_all(sockfd, &msg_size, sizeof(int), 0) == -1){
        printf("Socket %d send massage protocal step 1 fails\n", sockfd);
        return -1;
    }
    char msg_arr[8];
    memcpy(msg_arr, command, 4);
    memcpy(msg_arr + 4, &msg_len, sizeof(int));
    if(send_all(sockfd, msg_arr, 8, 0) == -1){
        printf("Socket %d send massage protocal step 2 fails\n", sockfd);
        return -1;
    }
    if(send_all(sockfd, msg, msg_len, 0) == -1){
        printf("Socket %d send massage protocal step 3 fails\n", sockfd);
        return -1;
    }
    printf("Send success\n");
    return 0;
}

char *convert_hexmd5_to_path(unsigned char hash[16]){
    char *filename = malloc(33);
    int i;
    for(i = 0;i < 16;++i){
        int p,q;
        p = hash[i] / 16;q = hash[i] % 16;
        if(p < 10){filename[i * 2] = p + '0';} else {filename[i * 2] = p - 10 + 'A';}
        if(q < 10){filename[i * 2 + 1] = q + '0';} else {filename[i * 2 + 1] = q - 10 + 'A';}
    }
    filename[32] = 0;
    return filename;
}

char *convert_path_to_hexmd5(char filename[32]){
    char *hash = malloc(16);
    int i;
    for(i = 0; i < 16; i++){
        char a = filename[2 * i];
        char b = filename[2 * i + 1];
        int p, q;
        if('A' <= a && a <= 'F'){p = a - 'A' + 10;}
        if('0' <= a && a <= '9'){p = a - '0';}
        if('A' <= b && b <= 'F'){q = b - 'A' + 10;}
        if('0' <= b && b <= '9'){q = b - '0';}
        hash[i] = (char)(p * 16 + q);
    }
    return hash;
}

/*Receive protocol
receive size 8 from sender
recive char[8] from sender
recieve content
return char[8] and real content
*/
char * ReceiveMessage(int sockfd){
    int msg_len = 0;
    char * recieve_str = malloc(1024);
    // char *content;
    if(read_all(sockfd, &msg_len, sizeof(int), 0) == -1 || (msg_len != 8)){
        printf("Socket %d recieve message step 1 fails\n", sockfd);
        return NULL;
    }
    if(read_all(sockfd, recieve_str, msg_len, 0) == -1){
        printf("Socket %d recieve message step 2 fails\n", sockfd);
        return NULL;
    }
    int file_size = *((int *)(recieve_str + 4));
    recieve_str = realloc(recieve_str, file_size + 8);
    if(read_all(sockfd, recieve_str + 8, file_size, 0) == -1){
        printf("Socket %d recieve message step 3 fails\n", sockfd);
        return NULL;
    }
    printf("Receive Success\n");
    return recieve_str;
}

HistoryBuffer * currentPushHead = NULL;

void HandleSendFile(int sockfd, char *metadata_src){
    int msg_len = *(int *)(metadata_src + 4);
    char *metadata = metadata_src + 8;
    int project_name_len = strlen(metadata);
    char hash[16];

    memcpy(hash, metadata + project_name_len + 1, 16);
    char *project_name = malloc(project_name_len + 1);
    memcpy(project_name, metadata, project_name_len);
    project_name[project_name_len] = 0;
    char *str_hash = convert_hexmd5_to_path((unsigned char *)hash);
    char *file_path = combine_path(project_name, str_hash);
    int content_len = *(int *)(metadata + project_name_len + 1 + 16);
    // char *content = metadata + project_name_len + 1 + 16 + 4;
    char *content = malloc(content_len);
    read_all(sockfd, content, content_len, 0);

    uint32_t file_output_hash[4];
    GetMD5((uint8_t *)content, content_len, file_output_hash);
    if(memcmp(file_output_hash, hash, 16) != 0){
        int fail = -1;
        send_all(sockfd, &fail, 4, 0);
        free(project_name);
        free(str_hash);
        free(file_path);
        return;
    }
    if(IsProject(project_name) == -1){
        int fail = -1;
        send_all(sockfd, &fail, 4, 0);
        free(project_name);
        free(str_hash);
        free(file_path);
        return;
    }

    /*  Write .History file:
        Everytime the push command will push the client file on the server, so record the history
    */
    /*
    char *history_path = combine_path(project_name, ".History");
    FILE *history_fd = fopen(history_path, "a");
    fprintf(history_fd, "%s\n", str_hash);
    free(history_path);
    fclose(history_fd);
    */
    HistoryBuffer * newNode = calloc(sizeof(HistoryBuffer),1);
    memcpy(newNode->hash,str_hash,32);
    newNode->hash[32] = 0;
    newNode->nextBuffer = currentPushHead;
    currentPushHead = newNode;

    int file_fd = open(file_path, O_WRONLY|O_CREAT, 0666);
    int succ = 0;
    send_all(sockfd, &succ, 4, 0);

    write(file_fd, content, content_len);
    close(file_fd);
    free(project_name);
    free(str_hash);
    free(file_path);
}

int PushVersion(int sockfd, char *project_name){
    char *file_path = combine_path(project_name, ".Manifest");
    int project_name_len = strlen(project_name);
    FolderStructureNode *client_root = ConstructStructureFromFile(file_path);
    int client_version = client_root -> version + 1;
    int client_mini_fd = open(file_path, O_RDONLY);
    MD5FileInfo *client_mani_md5info = GetMD5FileInfo(client_mini_fd);
    close(client_mini_fd);

    char *client_mani = client_mani_md5info -> data;
    int client_mani_len = client_mani_md5info -> file_size;
    int msg_len = project_name_len + 1 + 4 + 4;
    char *metadata = malloc(msg_len);

    strcpy(metadata,project_name);
    // copy version num
    memcpy(metadata + project_name_len + 1, &client_version, 4);
    // copy .Manifest len
    memcpy(metadata + project_name_len + 1 + 4, &client_mani_len, 4);
    free(file_path);
    char command[4] = {'p', 'u', 's', 'h'};
    if(SendMessage(sockfd, command, metadata, msg_len) == -1){
        return -1;
    }
    send_all(sockfd, client_mani, client_mani_len, 0);
    free(client_mani);
    free(metadata);
    free(client_mani_md5info);
    return 0;
}

int HandlePushVersion(int sockfd, char *metadata_src){
    int msg_len = *(int *)(metadata_src + 4);
    char *metadata = metadata_src + 8;
    int project_name_len = strlen(metadata);
    char *project_name = malloc(project_name_len + 1);
    memcpy(project_name, metadata, project_name_len);
    project_name[project_name_len] = '\0';

    if(IsProject(project_name) == -1){
        printf("The project %s is not existed on the server, cannot fetched\n", project_name);
        free(project_name);
        return -1;
    }

    char *server_mani_path = combine_path(project_name, ".Manifest");
    FolderStructureNode *server_root = ConstructStructureFromFile(server_mani_path);
    int server_version = server_root -> version;
    int client_version = *(int *)(metadata + project_name_len + 1);
    if(server_version != client_version - 1){
        int fail = -1;
        send_all(sockfd, &fail, 4, 0);
        free(project_name);
        free(server_mani_path);
        FreeFolderStructNode(server_root);
        return -1;
    }
    int client_mani_len = *(int *)(metadata + project_name_len + 1 + 4);
    char *client_mani = malloc(client_mani_len);
    read_all(sockfd,client_mani,client_mani_len,0);
    char *server_temp_mani_name = combine_path(project_name, "~Manifest");
    int server_temp_mani_fd = open(server_temp_mani_name, O_WRONLY | O_CREAT, 0666);
    write(server_temp_mani_fd, client_mani, client_mani_len);

    close(server_temp_mani_fd);
    free(project_name);
    free(server_mani_path);
    FreeFolderStructNode(server_root);
    free(server_temp_mani_name);
    int succ = 0;
    send_all(sockfd, &succ,4, 0);

    return 0;
}

int HandleComplete(int sockfd, char *metadata){
    char * project_name = metadata + 8;
    char *old_mani_path = combine_path(project_name, "~Manifest");
    char *new_mani_path = combine_path(project_name, ".Manifest");
    rename(old_mani_path, new_mani_path);
    free(old_mani_path);
    free(new_mani_path);
    int succ = 0;
    send_all(sockfd, &succ, 4, 0);
    return 0;
}

// send_all two times, the first is for succ/faill
// the second for fail content, basic format: (int)msg_len + (char *)content
int HandleHistory(int sockfd, char *project_name){
    if(IsProject(project_name) == -1){
        printf("The project %s is not eixsted on the server, cannot checkout to the client\n", project_name);
        int fail = -1;
        send_all(sockfd, &fail, 4, 0);
        return -1;
    }
    char *history_path = combine_path(project_name, ".History");
    int history_fd = open(history_path, O_RDONLY);
    MD5FileInfo *history_md5_info = GetMD5FileInfo(history_fd);
    int history_size = history_md5_info -> file_size;
    char *history = history_md5_info -> data;
    char *ret_msg = malloc(4 + history_size);
    memcpy(ret_msg, &history_size, 4);
    memcpy(ret_msg + 4, history, history_size);
    int succ = 0;
    send_all(sockfd, &succ, 4, 0);
    send_all(sockfd, ret_msg, history_size + 4, 0);
    free(history);
    free(ret_msg);
    free(history_md5_info);
    free(history_path);
    return 0;
}

// project_name \0 hash file_len content
int SendFile(int sockfd, char *project_name, char *file_name, char * mani_hash){
    int project_name_len = strlen(project_name);
    int total_len = project_name_len + 2 + strlen(file_name);
    char *temp_project_name = malloc(total_len);
    sprintf(temp_project_name, "%s/%s", project_name, file_name);
    int fd = open(temp_project_name, O_RDONLY);
    MD5FileInfo *md5_info = GetMD5FileInfo(fd);
    int file_size = md5_info -> file_size;
    uint8_t *hash = md5_info -> hash;
    char *content = md5_info -> data;
    if(mani_hash != NULL){
        if(memcmp(mani_hash,hash,16) != 0){
            int fail = -1;
            read_all(sockfd, &fail, 4, 0);
            free(content);
            free(md5_info);
            free(temp_project_name);
            return -1;
        }
    }
    close(fd);

    // realloc the temp to contain metadata
    int metadata_len = project_name_len + 1 + 16 + 4;
    temp_project_name = realloc(temp_project_name, metadata_len);
    temp_project_name[project_name_len] = '\0';
    memcpy(temp_project_name + project_name_len + 1, hash, 16);
    memcpy(temp_project_name + project_name_len + 1 + 16, &file_size, 4);

    //send file name
    char command[4] = {'s', 'e', 'n', 'd'};

    if(SendMessage(sockfd, command, temp_project_name, metadata_len) == -1){
        int fail = -1;
        read_all(sockfd, &fail, 4, 0);
        return -1;
    }

    //send text
    send_all(sockfd, content, file_size, 0);
    free(content);
    free(temp_project_name);
    free(md5_info);

    // success - 0, fail - -1
    int result;
    read_all(sockfd, &result, 4, 0);
    return result;
}

// file name could be hashp[16] or .Manifest
// file_szie content
char* ReceiveFile(int sockfd, const char *project_name, const char *file_name){
    int project_name_len = strlen(project_name);
    // int file_name_len = strlen(file_name);

    int msg_size = project_name_len + 1;
    char *metadata = malloc(msg_size);
    memcpy(metadata, project_name, project_name_len);
    metadata[project_name_len] = '\0';

    if(strcmp(file_name, ".Manifest") == 0){
        msg_size += 9;
        // file_name_len = 9;
        memcpy(metadata + project_name_len + 1, ".Manifest", 9);
    }else{
        // file_name_len = 16;
        msg_size += 32;
        char *hash_path = convert_hexmd5_to_path((unsigned char *)file_name);
        memcpy(metadata + project_name_len + 1, hash_path, 32);
        // close(fd);
        // free(temp_project_name);
        free(hash_path);
    }

    char command[4] = {'r', 'e', 'c', 'v'};
    if(SendMessage(sockfd, command, metadata, msg_size) == -1){
        return NULL;
    }
    free(metadata);

    int handle_msg_size;
    read_all(sockfd, &handle_msg_size, 4, 0);
    char *ret_msg = malloc(handle_msg_size + 4);
    memcpy(ret_msg, &handle_msg_size, 4);
    read_all(sockfd, ret_msg + 4, handle_msg_size, 0);

    return ret_msg;
}

int HandleRecieveFile(int sockfd, char *metadata_src){
    char *metadata_contain_msg_len = metadata_src + 4;
    char *metadata = metadata_contain_msg_len + 4;

    int msg_len = *(int*)metadata_contain_msg_len;

    if(metadata_src == NULL){return -1;}

    int project_name_len = strlen(metadata);
    int file_len = msg_len - project_name_len - 1;
    char *file_name;
    if(file_len == 9){
        file_name = malloc(9);
        memcpy(file_name, ".Manifest", 9);
    }
    if(file_len == 32){
        // save for name
        // file_len = 33;
        file_name = malloc(33);
        // char hash[32];
        memcpy(file_name, metadata + project_name_len + 1, 32);
        file_name[32] = 0;
        // file_name = convert_hexmd5_to_path(hash);

        // printf("The md5 is %s\n", file_name);
        // file_name = convert_hexmd5_to_path(hash);
        // file_name = malloc(16);
        // memcpy(file_name, metadata_contain_msg_len, 16);
    }

    char *file_path = malloc(project_name_len + 2 + file_len);

    sprintf(file_path, "%s/%s", metadata, file_name);

    int fd = open(file_path, O_RDONLY);
    char *content = (char *)GetMD5FileInfo(fd) -> data;
    int content_len = strlen(content);
    // char *ret_data = malloc(content_len + 4);
    send_all(sockfd, &content_len, 4, 0);
    send_all(sockfd, content, content_len, 0);

    // send_all(sockfd, )
    // char hash[16];
    // memcpy(hash, metadata + project_name_len + 1, 16);
    //free(metadata_src);
    free(file_name);
    free(file_path);
    free(content);
    return 0;
}

// char *SendManifest(int sockfd, const char *project_name){
//
// }

// char * ReceiveFile(const project_name, char *file_name){
    // char *data = malloc(256 + 16 + 4);
    // char *name_data = ReceiveMessage(sockfd);
    // int name_size = *((int *)(name_data + 4));
    // memcpy(data, name_data + 8, name_size);
    // data[name_size] = 0;
    // //read hash
    // read_all(sockfd, data + 256, 16, 0);
    // //read content size
    // int content_size;
    // read_all(sockfd, &content_size, 4, 0);
    // //copy content
    // memcpy(data + 256 + 16, &content_size, 4);
    // data = realloc(data, 256 + 16 + 4 + content_size);
    // //read content
    // read_all(sockfd, data + 256 + 16 + 4, content_size, 0);
    // free(name_data);
    // return data;
// }


HashMap * InitializeHashMap(int size){
    HashMap * new_map = calloc(sizeof(HashMap),1);
    new_map -> map = calloc(sizeof(HashMapNode *),size);
    new_map -> size = size;
    return new_map;
}

int GetHash(const char * str){
    int len = strlen(str), i, sum = 0;
    for(i = 0; i < len; i++){
        sum += str[i];
    }
    return sum;
}

HashMapNode * HashMapInsert(HashMap * hmap, const char * key, void * nodePtr){
    int SIZE = hmap -> size;
    int hash_code = GetHash(key) % SIZE;
    HashMapNode * new_node = calloc(sizeof(HashMapNode),1);
    new_node -> key = strdup(key);
    new_node -> nodePtr = nodePtr;
    new_node -> next = hmap -> map[hash_code];
    hmap -> map[hash_code] = new_node;
    return new_node;
}

HashMapNode *HashMapFind(HashMap * hmap, const char * key){
    int SIZE = hmap -> size;
    int hash_code = GetHash(key) % SIZE;
    HashMapNode *cur_node = hmap -> map[hash_code];
    while(cur_node != NULL){
        if(strcmp(cur_node -> key, key) == 0)
            return cur_node;
        cur_node = cur_node -> next;
    }
    return NULL;
}

void FreeFolderStructNode(FolderStructureNode *root){
    FolderStructureNode * tmp1 = root->nextFile, *tmp2 = root->folderHead;
    free(root);
    if(tmp1 != NULL){FreeFolderStructNode(tmp1);}
    if(tmp2 != NULL){FreeFolderStructNode(tmp2);}
}

void DestroyHashMap(HashMap * hmap){
    int SIZE = hmap -> size, i;
    for(i = 0; i < SIZE; i++){
        HashMapNode *cur_node = hmap -> map[i];
        while(cur_node != NULL){
            HashMapNode *temp_node = cur_node -> next;
            free((char *)(cur_node -> key));
            free(cur_node);
            cur_node = temp_node;
        }
    }
    free(hmap);
}

/*
void DeleteHashMapNode(HashMap * hmap, char * str){
    int hmap_size = hmap->size;
    if(hmap_size == 0){
        printf("error: no file to destroy\n");
        return;
    }
    HashMapNode *pre = NULL;
    HashMapNode *current = hmap->map[0];
    for(int i =0; i<hmap_size; i++){
    const char* tmp = hmap->map[i]->key;
        if(strcmp(str,tmp) ==0){
            pre->next = current->next;
            break;
        }else{
            pre = current;
            current = current->next;
        }
    }
    return;
}
*/

void PrintHashMap(HashMap * hmap){
    int SIZE = hmap -> size, i;
    HashMapNode **cur_map = hmap -> map;
    for(i = 0; i < SIZE; i++){
        HashMapNode *cur_node = cur_map[i];
        while(cur_node != NULL){
            printf("(%s, p, %d)\n", cur_node -> key, i);
            cur_node = cur_node -> next;
        }
        if(cur_map[i] != NULL){
            printf("--------\n");
        }
    }
}

/*
int create_file(const char * name, int parent_folder_fd, char type){
    int fd;
    switch (type) {
        case 1:
            fd = openat(parent_folder_fd, name, O_WRONLY | O_CREAT, 0666);
            break;
        case 2:
            mkdir(name, 0777);
            fd = open(name, O_RDONLY);
            break;
    }
    return fd;
}
*/

void CreateEmptyFolderStructFromPath(const char *original_path){
    // char *cur_path = malloc(256), token;
    int index = 0, i, len = strlen(original_path);
    char *path = malloc(len + 1);

    for(i = 0; i < len; i++){
        if(original_path[i] == '/'){
            path[index] = 0;
            // check if the folder is existed
            DIR *dir_fd = opendir(path);
            if(dir_fd){
                closedir(dir_fd);
            }
            if(ENOENT == errno){
                mkdir(path, 0777);
            }
            path[index++] = '/';
        }else
            path[index++] = original_path[i];
    }

    //if it is a dir
    if(path[index] == '/')
        return;
    // check if the file eixst
    path[index] = 0;
    int file_fd = open(path, O_RDONLY);
    if(file_fd < 0){
        file_fd = open(path, O_WRONLY | O_CREAT, 0666);
        close(file_fd);
    }
    free(path);
}


/*
void SendFileFromMani(int sockfd, FolderStructureNode *root, int parent_folder_fd, char *parent_folder_name){

    while (root != NULL) {
        int new_path_len = strlen(parent_folder_name) + 1 + strlen(root -> name);
        char *new_path = malloc(new_path_len);
        sprintf(new_path, "%s/%s", parent_folder_name, root -> name);

        if(root -> type == 1){
            //send path and the file
            char command[4] = {'n', 'u', 'l', 'l'};
            SendMessage(sockfd, command, new_path);
            // printf("The path is %s\n", new_path);
            SendFile(sockfd, new_path);
        }
        else if(root -> type == 2){
            parent_folder_fd = open(new_path, O_RDONLY);
            SendFileFromMani(sockfd, root -> folderHead, parent_folder_fd, new_path);
        }
        root = root -> nextFile;
        free(new_path);
    }
}
*/

FolderStructureNode *SearchStructNodeLayer(const char *name, FolderStructureNode *root){
    // if(root )
    FolderStructureNode *temp = root -> folderHead;
    while (temp != NULL) {
        if(strcmp(temp -> name, name) == 0)
            return temp;
        temp = temp -> nextFile;
    }
    return NULL;
}

FolderStructureNode * CreateFolderStructNode(const char type, const char *name, const char *hash, FolderStructureNode *nextFile, FolderStructureNode *folderHead, int version){
    FolderStructureNode *node = calloc(sizeof(FolderStructureNode), 1);
    node -> type = type;
    memcpy(node -> name, name, strlen(name) + 1);
    if(hash != NULL) {
        memcpy(node -> hash, hash, 16);
    } else {
        memset(node -> hash,0,16);
    }
    node -> nextFile = nextFile;
    node -> folderHead = folderHead;
    node -> version = version;
    return node;
}

int IsProject(const char * project_name){
    DIR *dir_fp = opendir(project_name);
    if(dir_fp == NULL){
        // printf("The project %s doest not exist\n", project_name);
        return -1;
    }
    struct dirent *dir_info;
    while ((dir_info = readdir(dir_fp)) != NULL) {
        if(strcmp(dir_info -> d_name, ".Manifest") == 0)
            return 0;
    }
    closedir(dir_fp);
    return -1;
}

HashMap *hashify_layer(FolderStructureNode *root){
    HashMap *fd_hmap = InitializeHashMap(20);
    FolderStructureNode *temp = root -> folderHead;
    while (temp != NULL) {
        HashMapInsert(fd_hmap, temp -> name, temp);
        temp = temp -> nextFile;
    }
    return fd_hmap;
}

MD5FileInfo * GetMD5FileInfo(int file_fd){
    MD5FileInfo * fileinfo = malloc(sizeof(MD5FileInfo));
    int file_size = 0,buff_size = 4096;
    char * text = calloc(buff_size,1);
    int t;
    do{
        t = read(file_fd,text,4096);
        if(t <= 0){break;}
        file_size += t;
        if(file_size == buff_size){
            buff_size *= 2;
            text = realloc(text,buff_size);
        }
    } while(true);
    fileinfo->file_size = file_size;
    GetMD5((uint8_t *)text, file_size, (uint32_t *)fileinfo->hash);
    fileinfo->data = text;
    return fileinfo;
}

char *combine_path(const char* par_path, const char * cur_path){
    int par_path_len = strlen(par_path);
    int cur_path_len = strlen(cur_path);
    char *new_path = malloc(par_path_len + 2 + cur_path_len);
    sprintf(new_path, "%s/%s", par_path, cur_path);
    return new_path;
}

// file name is except the project name, assume the file name is 222/2.txt
FolderStructureNode *remove_node_from_root(FolderStructureNode *root, const char *file_name){
    FolderStructureNode *original_root = root;
    int file_len = strlen(file_name), i, index = 0;
    char name[256];
    for(i = 0; i < file_len; i++){
        if(file_name[i] == '/'){
            name[index] = 0;
            HashMap *hmap = hashify_layer(root);
            HashMapNode *hmap_folder_node = HashMapFind(hmap, name);
            if(hmap_folder_node == NULL){
                printf("The file %s is not existed in the .Manifest, cannot remove\n", file_name);
                return NULL;
            }
            root = (FolderStructureNode *)hmap_folder_node -> nodePtr;
            DestroyHashMap(hmap);
            index = 0;
        }else{
            name[index++] = file_name[i];
        }
    }

    name[index] = 0;
    FolderStructureNode *file_layer_folderHead = root -> folderHead;
    if(strcmp(file_layer_folderHead -> name, name) == 0){
        root -> folderHead = file_layer_folderHead -> nextFile;
        return original_root;
    }
    while ((file_layer_folderHead -> nextFile) != NULL) {
        if(strcmp((file_layer_folderHead -> nextFile) -> name, name) == 0){
            file_layer_folderHead -> nextFile = file_layer_folderHead -> nextFile -> nextFile;
            return original_root;
        }
        file_layer_folderHead = file_layer_folderHead -> nextFile;
    }
    printf("The file %s is not eixsted in the .Manifest, cannot remove\n", file_name);
    return NULL;
}

int remove_dir_helper(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
    int rv = remove(fpath);
    if (rv)
        perror(fpath);
    return rv;
}

int remove_dir(char *path)
{
    return nftw(path, remove_dir_helper, 64, FTW_DEPTH | FTW_PHYS);
}
