#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/dir.h>
#include "utility.h"

const char * random_string(int len){
    char * result = malloc(len + 1);
    int i;
    for(i = 0;i < len;++i){
        result[i] = rand() % 26 + 'A';
    }
    result[i] = 0;
    return result;
}

volatile char globalStop = 0;

void output_error(int errnum){exit(0);}

void testinsert(){
    int i;
    srand(time(NULL));
    HashMap *hmap = InitializeHashMap(20);
    for(i = 0; i < 20; i++){
        FolderStructureNode *nodePtr = calloc(sizeof(FolderStructureNode), 1);
        HashMapInsert(hmap, random_string(10), nodePtr);
    }
    PrintHashMap(hmap);
}

void testfindnode(){
    int i;
    HashMap *hmap = InitializeHashMap(20);
    const char * strings[100];
    for(i = 0;i < 100;i++){
        strings[i] = random_string(10);
    }
    for(i = 0; i < 100; i++){
        FolderStructureNode *nodePtr = calloc(sizeof(FolderStructureNode), 1);
        nodePtr -> name[0] = i;
        HashMapInsert(hmap, strings[i], nodePtr);
    }
    PrintHashMap(hmap);

    for(i = 0;i < 10;++i){
        int t = rand() % 100;
        HashMapNode *hptr = HashMapFind(hmap,strings[t]);
        printf("%d %d\n",t,(int)(((FolderStructureNode*)(hptr->nodePtr))->name[0]));
    }
}

/*
void testSendFile(){
    FILE * fp = fopen("LICENSE", "r");
    SendFile(5, fp);
}
*/

void testMD5(){
    int fd = open("LICENSE", O_RDONLY);
    struct stat fileStat;
    stat("LICENSE", &fileStat);
    int file_size = (int)fileStat.st_size;
    uint32_t *md5_arr = calloc(sizeof(uint32_t), 4);
    uint8_t *text = malloc(file_size);
    read(fd, text, file_size);
    GetMD5(text, file_size, md5_arr);
    printf("%"PRIu32 " %"PRIu32 " %"PRIu32 " %"PRIu32"\n", md5_arr[0], md5_arr[1], md5_arr[2], md5_arr[3]);
}

int main(){
    // testinsert();
    // testSendFile();
    testMD5();
    return 0;
}
