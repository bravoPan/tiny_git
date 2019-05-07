#include <stdio.h>
#include <stdint.h>
#include "utility.h"
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
// #include "fdstruct.h"

volatile char globalStop = 0;
void output_error(int err){return;}

int main(){
    int txt_1_fd = open("server/ttt/1.txt", O_RDONLY);
    int txt_2_fd = open("server/222/2.txt", O_RDONLY);
    MD5FileInfo *txt_1_hash = GetMD5FileInfo(txt_1_fd);
    MD5FileInfo *txt_2_hash = GetMD5FileInfo(txt_2_fd);

    char *txt_1_hash_path = convert_hexmd5_to_path((unsigned char*)txt_1_hash -> hash);
    char *txt_2_hash_path = convert_hexmd5_to_path((unsigned char*)txt_2_hash  -> hash);

    txt_1_hash_path = realloc(txt_1_hash_path, 33);
    txt_2_hash_path = realloc(txt_2_hash_path, 33);
    txt_1_hash_path[32] = 0;
    txt_2_hash_path[32] = 0;
    printf("The txt1 hash is %s\n", txt_1_hash_path);
    printf("The txt2 hash is %s\n", txt_2_hash_path);
    // char *path = "955C8603EE5DC7BE4E551C44966E24DC";
    // char *hash = convert_path_to_hexmd5(path);
    // char *original_path = convert_hexmd5_to_path(hash);
    // original_path = realloc(original_path, 33);
    // original_path[32] = 0;
    // printf("The path is %s\n", original_path);
    return 0;
}
