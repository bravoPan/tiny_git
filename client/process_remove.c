#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "../utility.h"

// file name is except the project name, assume the file name is 222/2.txt
FolderStructureNode *remove_node_from_root(FolderStructureNode *root, const char *file_name){
    FolderStructureNode *original_root = root;
    int file_len = strlen(file_name), i, index = 0;
    char name[256];
    for(i = 0; i < file_len; i++){
        if(file_name[i] == '/'){
            name[index] = 0;
            HashMap *hmap = hashify_layer(root);
            HashMapNode *hmap_folder_node = HashMapFind(hmap, name);
            if(hmap_folder_node == NULL){
                printf("The file %s is not existed in the .Manifest, cannot remove\n", file_name);
                return NULL;
            }
            root = (FolderStructureNode *)hmap_folder_node -> nodePtr;
            DestroyHashMap(hmap);
            index = 0;
        }else{
            name[index++] = file_name[i];
        }
    }

    name[index] = 0;
    FolderStructureNode *file_layer_folderHead = root -> folderHead;
    if(strcmp(file_layer_folderHead -> name, name) == 0){
        root -> folderHead = file_layer_folderHead -> nextFile;
        return original_root;
    }
    while ((file_layer_folderHead -> nextFile) != NULL) {
        if(strcmp((file_layer_folderHead -> nextFile) -> name, name) == 0){
            file_layer_folderHead -> nextFile = file_layer_folderHead -> nextFile -> nextFile;
            return original_root;
        }
        file_layer_folderHead = file_layer_folderHead -> nextFile;
    }
    printf("The file %s is not eixsted in the .Manifest, cannot remove\n", file_name);
    return NULL;
}

int process_remove(int sockfd, int argc, char **argv){
    char *project_name = argv[2];
    if(IsProject(project_name) == -1){
      printf("The repository %s is not existed, cannot remove\n", project_name);
      return -1;
    }
    char *file_name = argv[3];
    char *mani_name = combine_path(project_name, ".Manifest");
    FolderStructureNode *root = ConstructStructureFromFile(mani_name);
    FILE *mani_fd = fopen(mani_name, "w");
    root = remove_node_from_root(root, file_name);

    if(root == NULL)
        return -1;

    SerializeStructure(root, mani_fd);
    return 0;
}
