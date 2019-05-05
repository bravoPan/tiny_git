#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../utility.h"

int read_commit(int sockfd, FILE *commit_fd, char *project_name){
    char mode, old_hash[33], new_hash[33];
    while (fscanf(commit_fd, " %s %s ", old_hash, new_hash) != EOF) {
        old_hash[32] = 0;
        new_hash[32] = 0;
        char *file_path = NULL;int len = 0;
        getline(&file_path, (size_t *)&len, commit_fd);
        file_path[strlen(file_path) - 1] = 0;
        // int file_fd = open()
        char *new_hex_hash = convert_path_to_hexmd5(new_hash);
        int i = 0;
        while(file_path[i] != '/'){++i;}
        file_path[i] = 0;
        int res = SendFile(sockfd, file_path, file_path + i + 1, new_hex_hash);
        if(res == -1){
            printf("Please reupdate project %s\n", project_name);
            char comand[4] = {'f', 'a', 'i', 'l'};
            SendMessage(sockfd, comand, NULL, 0);
            return -1;
        }
        free(file_path);
    }
    char complete_command[4] = {'c', 'p', 'l', 't'};
    int msg_len = strlen(project_name) + 1;
    SendMessage(sockfd, complete_command, project_name, msg_len);
    return 0;
}

int process_push(int sockfd, int argc, char **argv){
    char *project_name = argv[2];
    if(IsProject(project_name) == -1){
      printf("The repository %s is not existed, cannot push\n", project_name);
      return -1;
    }
    char *commit_path = combine_path(project_name, ".Commit");
    FILE *commit_fd = fopen(commit_path, "r");
    read_commit(sockfd, commit_fd, project_name);
    fclose(commit_fd);
    free(commit_path);
    return 0;
}
