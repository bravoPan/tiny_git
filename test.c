#include <stdio.h>

int main(){
    char *path = "955C8603EE5DC7BE4E551C44966E24DC";
    char *hash = convert_path_to_hexmd5(path);
    char *original_path = convert_hexmd5_to_path(hash);
    original_path = realloc(original_path, 33);
    original_path[32] = 0;
    printf("The path is %s\n", original_path);
    return 0;
}
