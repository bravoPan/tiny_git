#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "../utility.h"


int process_remove(int sockfd, int argc, char **argv){
    char *project_name = argv[2];
    if(IsProject(project_name) == -1){
      printf("The repository %s is not existed, cannot remove\n", project_name);
      return -1;
    }
    char *file_name = argv[3];
    char *mani_name = combine_path(project_name, ".Manifest");
    FolderStructureNode *root = ConstructStructureFromFile(mani_name);
    root = remove_node_from_root(root, file_name);

    if(root == NULL)
        return -1;

    FILE *mani_fd = fopen(mani_name, "w");
    SerializeStructure(root, mani_fd);
    return 0;
}
