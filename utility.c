#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <ncurses.h>
#include <sys/socket.h>
#include "utility.h"

ssize_t send_all(int socket,const void * data,size_t len,int sig){
  while (!globalStop && len > 0){
    ssize_t r = send(socket, data, len, sig);
    if (r <= 0){return -1;}
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

// ConstructStructureFromPath
