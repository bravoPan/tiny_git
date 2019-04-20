#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <ncurses.h>
#include <sys/socket.h>
#include <sys/dir.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "utility.h"

ssize_t send_all(int socket,const void * data,size_t len,int sig){
  while (!globalStop && len > 0){
    ssize_t r = send(socket, data, len, sig);
    if (r <= 0 && errno != EAGAIN){return -1;} else if(errno == EAGAIN){sleep(0);}
    len -= r;
    data = (const char*)data + r;
  }
  if(len > 0){return -1;} else {return 1;}
}

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

ssize_t read_all(int socket,void * data,size_t len,int sig){
    while (!globalStop && len > 0){
      ssize_t r = recv(socket, data, len, sig);
      if (r <= 0 && errno != EAGAIN){return -1;} else if(errno == EAGAIN){sleep(0);continue;}
      len -= r;
      data = (char *)data + r;
    }
    if(len > 0){return -1;} else {return 1;}
}

void InitializeGUI(){
    initscr();
    cbreak();
    noecho();
}

void DrawProgressBar(ProgressBar * bar){
    move(bar -> posX, bar -> posY);
    printw("[  ");
    int i;
    int t = bar -> currProgress / (100 / bar -> length);
    for(i = 0;i < t;++i){
        printw("#");
    }
    for(i = t;i < bar -> length;++i){
        printw(" ");
    }
    printw("  ]  %3d%% / 100%%", bar -> currProgress);
    refresh();
}

HashMap* InitializeHashMap(int size){
    HashMap * new_map = calloc(sizeof(HashMap),1);
    new_map -> map = calloc(sizeof(HashMapNode *),size);
    new_map -> size = size;
    return new_map;
}

int GetHash(const char* str){
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

HashMapNode *HashMapFind(HashMap *hmap, const char * key){
    int SIZE = hmap -> size;
    int hash_code = GetHash(key) % SIZE;
    int i;
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

// FolderStructureNode *ConstructStructureFromPath(const char * path){
//     DIR * dd = opendir(path);
//     FolderStructureNode *root = calloc(sizeof(FolderStructureNode*), 1);
//     root -> index = 0;
//     root -> type = '2';
//     root -> name = path;
//     root -> folderHead = 0;
//     FolderStructureNode *temp = root;
//     if(dd == NULL){
//         fprintf(stderr, "Invalid path:  %s\n", path);
//         return -1;
//     }
//     struct dirent * dir_strct;
//     int index = 1;
//
//     while((dir_strct = readdir(dd)) != NULL){
//         char *name = NULL;
//         FolderStructureNode *next_node =  calloc(sizeof(FolderStructureNode *), 1);
//         next_node -> index = index;
//         __uint8_t dir_type = dir_strct -> d_type;
//         switch (dir_type) {
//             next_node ->
//             case DT_REG:
//         }
//     }
// }

void SendFile(int socket, const char * path){
    int fd = open(path, O_RDONLY);
    struct stat fileStat;
    stat(path, &fileStat);
    int file_size = (int)fileStat.st_size;
    char *text = malloc(file_size);
    read(fd,text,file_size);
    int msg_len = 8;
    send_all(socket, &msg_len, sizeof(int), 0);
    char msg[8] = {'s','e','n','d'};
    memcpy(msg + 4,&file_size,4);
    send_all(socket, msg, msg_len, 0);
    send_all(socket, text, file_size, 0);
    uint32_t *md5_arr = calloc(sizeof(uint32_t),4);
    send_all(socket,md5_arr,sizeof(md5_arr),0);
    //printf("%"PRIu32 " %"PRIu32 " %"PRIu32 " %"PRIu32"\n", md5_arr[0], md5_arr[1], md5_arr[2], md5_arr[3]);
}

void DeleteFile(int socket, const char * path){
    int msg_len = 8, size = strlen(path);
    send_all(socket, &msg_len, sizeof(int), 0);
    char msg[8] = {'d', 'e', 'l', 't'};
    memcpy(msg + 4, &size, sizeof(int));
    send_all(socket, msg, msg_len, 0);
    send_all(socket, path, size, 0);
}

FolderStructureNode * CreateFolderStructNode(const char type, const char *name, const char *hash, FolderStructureNode *nextFile, FolderStructureNode *folderHead){
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
    return node;
}

// 0 for find success, -1 for find none
int IsProject(const char *project_name){
    DIR *dir_fp = opendir(project_name);
    if(dir_fp == NULL){
        printf("The project %s doest not exist\n", project_name);
        return -1;
    }
    struct dirent *dir_info;
    while ((dir_info = readdir(dir_fp)) != NULL) {
        if(strcmp(dir_info -> d_name, ".Manifest") == 0)
            return 0;
    }
    return -1;
}

MD5FileInfo *GetMD5FileInfo(const char *file_name){
    int file_fd = open(file_name, O_RDONLY);
    MD5FileInfo *fileinfo = malloc(sizeof(MD5FileInfo));
    memcpy(fileinfo->file_name, file_name, strlen(file_name));
    uint32_t *file_md5 = calloc(sizeof(uint32_t), 4);
    struct stat fileStat;
    stat(file_name, &fileStat);
    int file_size = (int)fileStat.st_size;
    fileinfo->file_size = fileStat.st_size;
    uint8_t *text = malloc(file_size);
    read(file_fd, text, file_size);
    GetMD5(text, file_size, file_md5);
    free(text);
    memcpy(fileinfo->hash, file_md5, 16);
    return fileinfo;
}
