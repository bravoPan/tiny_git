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
#include "utility.h"

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
    char *filename = malloc(32);
    for(int i = 0;i < 16;++i){
        int p,q;
        p = hash[i] / 16;q = hash[i] % 16;
        if(p < 10){filename[i * 2] = p + '0';} else {filename[i * 2] = p - 10 + 'A';}
        if(q < 10){filename[i * 2 + 1] = q + '0';} else {filename[i * 2 + 1] = q - 10 + 'A';}
    }
    // filename[32] = 0;
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

// project_name \0 hash file_len content
int SendFile(int sockfd, char *project_name, char *file_name){
    int project_name_len = strlen(project_name);
    int total_len = project_name_len + 1 + strlen(file_name);
    char *temp_project_name = malloc(total_len);
    sprintf(temp_project_name, "%s/%s", project_name, file_name);
    int fd = open(temp_project_name, O_RDONLY);
    MD5FileInfo *md5_info = GetMD5FileInfo(fd);
    int file_size = md5_info -> file_size;
    uint8_t *hash = md5_info -> hash;
    char *content = md5_info -> data;
    close(fd);

    // realloc the temp to contain metadata
    int metadata_len = project_name_len + 1 + 16 + 4;
    temp_project_name = realloc(temp_project_name, metadata_len);
    temp_project_name[project_name_len] = '\0';
    memcpy(temp_project_name + project_name_len + 1, hash, 16);
    memcpy(temp_project_name + project_name_len + 1 + 16, &file_size, 4);

    //send file name
    char command[4] = {'s', 'e', 'n', 'd'};

    SendMessage(sockfd, command, temp_project_name, metadata_len);
    // send_all(sockfd, hash, 16, 0);
    // printf("The project name is %s\n", project_name);
    // printf("The hash is %s\n", );

    //send text
    send_all(sockfd, content, file_size, 0);
    free(content);
    free(temp_project_name);
    return 0;
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

/*SendFile protocol
256(name) 16(hash) 4(file_size) real content
*/
// void SendFile(int sockfd, const char * path){
//     int fd = open(path,O_RDONLY);
//     MD5FileInfo *md5_info = GetMD5FileInfo(fd);
//     int file_size = md5_info -> file_size;
//     uint8_t *hash = md5_info -> hash;
//     // printf("The file size is %d\n", file_size);
//     // printf("The file name is %s\n", file_name);
//     // printf("The has\n", );
//     close(fd);
//
//     fd = open(path, O_RDONLY);
//     //send file name
//     char command[4] = {'n', 'u', 'l', 'l'};
//     SendMessage(sockfd, command, );
//     send_all(sockfd, hash, 16, 0);
//
//     //send text
//     char *text = malloc(file_size);
//     read(fd, text, file_size);
//     send_all(sockfd, &file_size, 4, 0);
//     send_all(sockfd, text, file_size, 0);
// }

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
        // char *temp_project_name = malloc(project_name_len + 1 + file_name_len);
        // sprintf(temp_project_name, "%s/%s", project_name, file_name);
        // int fd = open(temp_project_name, O_RDONLY);
        // MD5FileInfo *md5_info = GetMD5FileInfo(fd);
        // int file_size = md5_info -> file_size;
        // uint8_t *hash = md5_info -> hash;
        // char hash_value[16];
        // strdup()
        char *hash_path = convert_hexmd5_to_path(strdup(file_name));
        memcpy(metadata + project_name_len + 1, hash_path, 32);
        // close(fd);
        // free(temp_project_name);
        free(hash_path);
    }

    // memcpy(metadata + project_name_len + 1, hash, 16);
    // memcpy(metadata + project_name_len + 1 + 16, file_name, file_name_len);

    char command[4] = {'r', 'e', 'c', 'v'};
    SendMessage(sockfd, command, metadata, msg_size);
    free(metadata);

    int handle_msg_size;
    read_all(sockfd, &handle_msg_size, 4, 0);
    char *ret_msg = malloc(handle_msg_size + 4);
    memcpy(ret_msg, &handle_msg_size, 4);
    read_all(sockfd, ret_msg + 4, handle_msg_size, 0);
    // char *ret_msg = malloc(handle_msg_size + 4);

    // read_all(sockfd, ret_msg, handle_msg_size, 0);
    return ret_msg;
}

int HandleRecieveFile(int sockfd){
    char *metadata_src = ReceiveMessage(sockfd);
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
    free(metadata_src);
    free(file_name);
    free(file_path);
    free(content);
    return 0;
}

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

/*
void DeleteFile(int socket, const char * path){
    int msg_len = 8, size = strlen(path);
    send_all(socket, &msg_len, sizeof(int), 0);
    char msg[8] = {'d', 'e', 'l', 't'};
    memcpy(msg + 4, &size, sizeof(int));
    send_all(socket, msg, msg_len, 0);
    send_all(socket, path, size, 0);
}
*/

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
    // int SIZE = hmap -> size;
    int hash_code = GetHash(key);
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

/*
void CreateEmptyFolderStructFromMani(FolderStructureNode *root, int parent_folder_fd, char *parent_folder_name){
    while (root != NULL) {
        int cur_len = strlen(parent_folder_name) + 1 + strlen(root -> name), new_folder_fd;
        char *new_path = malloc(cur_len);
        sprintf(new_path, "%s/%s", parent_folder_name, root -> name);

        switch (root -> type) {
            case 1:
                create_file(root -> name, parent_folder_fd, 1);
                break;
            case 2:
                new_folder_fd = create_file(new_path, parent_folder_fd, 2);
                if(root -> folderHead != NULL)
                    CreateEmptyFolderStructFromMani(root -> folderHead, new_folder_fd, new_path);
        }
        free(new_path);
        root = root -> nextFile;
    }
}

int GetFileNumFromMani(FolderStructureNode *root){
    if(root == NULL)
        return 0;
    int cur_num = 0;
    while (root != NULL) {
        if(root -> folderHead == NULL){
            cur_num++;
        }
        else
            cur_num = cur_num + GetFileNumFromMani(root -> folderHead);
        root = root -> nextFile;
    }
    return cur_num;
}

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
