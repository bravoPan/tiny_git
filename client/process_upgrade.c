#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include "../utility.h"

void read_update(int sockfd, FILE *update_fd, char *project_name){
    char mode, old_hash[33], new_hash[33];
    while (fscanf(update_fd, " %c %s %s ", &mode, old_hash, new_hash) != EOF) {
        old_hash[32] = 0;
        new_hash[32] = 0;
        char *file_path = NULL;int len = 0;
        getline(&file_path, (size_t *)&len, update_fd);
        int file_len = strlen(file_path);
        file_path[file_len-1] = 0;
        switch (mode) {
            case 'D':{
                remove(file_path);
                int project_name_len = strlen(project_name);
                // int file_path_len = strlen(file_path);
                // while (file_path[delimiter_index]) {
                // }
                char *mani_path = combine_path(project_name, ".Manifest");
                FolderStructureNode *root = ConstructStructureFromFile(mani_path);
                FILE *mani_fd = fopen(mani_path, "w");
                root = remove_node_from_root(root, file_path + project_name_len + 1);
                // root = remove_node_from_root(root, file_name);
                SerializeStructure(root, mani_fd);
                fclose(mani_fd);
                free(mani_path);
                break;
            }
            case 'M':case 'A':{
                int modified_file_fd = open(file_path, O_RDWR);
                MD5FileInfo *modified_file_md5 = GetMD5FileInfo(modified_file_fd);
                char *metadata_src = ReceiveFile(sockfd, project_name, (const char*)modified_file_md5 -> hash);
                int msg_len = *(int *)(metadata_src);

                write(modified_file_fd, metadata_src + 4, msg_len);
                // free(metadata_src -> data);
                free(metadata_src);
                close(modified_file_fd);
                if(mode == 'M'){break;}
                //mode == 'A'
                char *mani_name = combine_path(project_name, ".Manifest");
                FolderStructureNode *root =  ConstructStructureFromFile(mani_name), *original_root = root;
                int i, len = strlen(file_path), index = 0;
                char name[256];
                for(i = 0; i < len; i++){
                    if(file_path[i] != '/'){
                        name[index++] = file_path[i];
                    }else{
                        name[index] = 0;
                        //search this node
                        HashMap *cur_folder_hmap = hashify_layer(root);
                        FolderStructureNode *cur_folder_node = (FolderStructureNode *)(HashMapFind(cur_folder_hmap, name) -> nodePtr);
                        root = cur_folder_node;
                        index = 0;
                        free(cur_folder_hmap);
                    }
                }
                name[index] = 0;
                FolderStructureNode *insert_node = CreateFolderStructNode(1, name, (const char*)modified_file_md5 -> hash, root -> folderHead, NULL, 0);
                root -> folderHead = insert_node;
                FILE *mani_fd = fopen(mani_name, "w");
                SerializeStructure(original_root, mani_fd);
                fclose(mani_fd);
                free(original_root);
                free(modified_file_md5 -> data);
                free(modified_file_md5);
            }
        }
        free(file_path);
    }
}

int process_upgrade(int sockfd, int argc, char **argv){
    char *project_name = argv[2];

    if(IsProject(project_name) == -1){
      printf("The repository %s is not existed, cannot update\n", project_name);
      return -1;
    }
    char *update_name = combine_path(project_name, ".Update");
    FILE *update_fd = fopen(update_name, "r");
    if(update_fd == NULL){
        printf("The file %s is not exist, cannot update\n", update_name);
        return -1;
    }
    read_update(sockfd, update_fd, project_name);
    return 0;
}
