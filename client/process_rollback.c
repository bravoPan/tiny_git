#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../utility.h"

int process_rollback(int sockfd, int argc, char **argv){
    char *project_name = argv[2];
    int project_name_len = strlen(project_name);
    int version = parse_port(argv[3]);

    char command[4] = {'r', 'o', 'l', 'l'};
    char *metadata = malloc(project_name_len + 1 + 4);
    memcpy(metadata, project_name, project_name_len + 1);
    memcpy(metadata + project_name_len + 1, &version, 4);
    SendMessage(sockfd, command, metadata, project_name_len + 1 + 4);

    int is_succ;
    read_all(sockfd, &is_succ, 4, 0);
    if(is_succ == -1){
        printf("The project %s version %d is invalid, cannot rollback\n", project_name, version);
        return -1;
    }
    free(metadata);
    return 0;
}
