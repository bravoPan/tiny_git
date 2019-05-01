#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include "utility.h"

int ComputeNewIndex(FolderStructureNode * tree, int beginIndex){
  if(tree == NULL){return beginIndex;}
  tree -> index = beginIndex;
  int n = ComputeNewIndex(tree -> folderHead, beginIndex + 1);
  return ComputeNewIndex(tree -> nextFile, n);
}

void RecursivePrintFdStruct(FolderStructureNode * root, FILE *fd){
  if(root == NULL){return;}
  fprintf(fd, "%d %d %d ",root -> index, (int)root -> type,root->version);
  if(root -> folderHead == NULL){
    fprintf(fd, "-1 ");
  } else {
    fprintf(fd, "%d ",root -> folderHead -> index);
  }
  if(root -> nextFile == NULL){
    fprintf(fd, "-1\n");
  } else {
    fprintf(fd, "%d\n",root -> nextFile -> index);
  }
  int i;
  for(i = 0;i < 16;++i){
    fprintf(fd, "%02X ",(int)root -> hash[i]);
  }
  fprintf(fd, "\n%s\n",root -> name);
  RecursivePrintFdStruct(root -> folderHead, fd);
  RecursivePrintFdStruct(root -> nextFile, fd);
}

void SerializeStructure(FolderStructureNode * tree, FILE *fd){
    if(tree == NULL){
        fprintf(fd,"0\n");
        return;
    }
  int nodeCount = ComputeNewIndex(tree,0);
  fprintf(fd, "%d\n",nodeCount);
  RecursivePrintFdStruct(tree, fd);
}

FolderStructureNode * ConstructStructureFromFile(const char * path){
  FILE * fd = fopen(path,"r");
  if(fd == NULL){return NULL;}
  int nodeCount;
  fscanf(fd," %d",&nodeCount);
  if(nodeCount == 0){return NULL;}
  FolderStructureNode ** nodearr = calloc(sizeof(FolderStructureNode *),nodeCount);
  int i,j;
  int index,type,li,ri,hs,vernum;
  for(i = 0;i < nodeCount;++i){nodearr[i] = calloc(sizeof(FolderStructureNode),1);}
  for(i = 0;i < nodeCount;++i){
    fscanf(fd," %d %d %d %d %d",&index,&type,&vernum,&li,&ri);
    nodearr[index]->index = index;
    nodearr[index]->type = type;
    nodearr[index]->version = vernum;
    if(li == -1){
      nodearr[index]->folderHead = NULL;
    } else {
      nodearr[index]->folderHead = nodearr[li];
    }
    if(ri == -1){
      nodearr[index]->nextFile = NULL;
    } else {
      nodearr[index]->nextFile = nodearr[ri];
    }
    for(j = 0;j < 16;++j){
      fscanf(fd," %X",&hs);
      nodearr[index]->hash[j] = hs;
    }
    fscanf(fd," %s",nodearr[index]->name);
  }
  fclose(fd);
  FolderStructureNode * result = nodearr[0];
  free(nodearr);
  return result;
}
