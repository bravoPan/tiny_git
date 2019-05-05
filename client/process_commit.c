#include <stdio.h>
#include <stdlib.h>
#include "../utility.h"

int copy_u_mode(FILE *update_fd, FILE *commit_fd){
    char mode, old_hash[33], new_hash[33];
    while (fscanf(update_fd, " %c %s %s ", &mode, old_hash, new_hash) != EOF) {
        old_hash[32] = 0;
        new_hash[32] = 0;
        char *file_path = NULL;int len = 0;
        getline(&file_path, (size_t *)&len, update_fd);
        switch (mode) {
            case 'U':{
                fprintf(commit_fd, "%s %s\n%s\n", old_hash, new_hash, file_path);
                break;
            }
        }
        free(file_path);
    }
    return 0;
}

int process_commit(int sockfd, int argc, char **argv){
    char *project_name = argv[2];
    if(IsProject(project_name) == -1){
        printf("The project %s is not exist, cannot commit\n", project_name);
        return -1;
    }
    char *file_data = ReceiveFile(sockfd, project_name, ".Manifest");
    if(file_data == NULL){
        printf("Server error, repo %s cannot be feteched, commit finished\n", project_name);
        return -1;
    }
    char *update_name = combine_path(project_name, ".Update");
    FILE *update_fd = fopen(update_name, "r");
    char *commit_name = combine_path(project_name, ".Commit");
    FILE *commit_fd = fopen(commit_name, "w");
    copy_u_mode(update_fd, commit_fd);
    free(update_name);
    free(commit_name);
    fclose(update_fd);
    fclose(commit_fd);
    return 0;
}
