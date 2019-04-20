#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include "utility.h"

int ComputeNewIndex(FolderStructureNode * tree, int beginIndex){
  if(tree == NULL){return beginIndex;}
  tree -> index = beginIndex;
  int n = ComputeNewIndex(tree -> folderHead, beginIndex + 1);
  return ComputeNewIndex(tree -> nextFile, n);
}

void RecursivePrintFdStruct(FolderStructureNode * root){
  if(root == NULL){return;}
  printf("%d %d ",root -> index, (int)root -> type);
  if(root -> folderHead == NULL){
    printf("-1 ");
  } else {
    printf("%d ",root -> folderHead -> index);
  }
  if(root -> nextFile == NULL){
    printf("-1\n");
  } else {
    printf("%d\n",root -> nextFile -> index);
  }
  int i;
  for(i = 0;i < 64;++i){
    printf("%02X ",(int)root -> hash[i]);
  }
  printf("\n%s\n",root -> name);
  RecursivePrintFdStruct(root -> folderHead);
  RecursivePrintFdStruct(root -> nextFile);
}

void SerializeStructure(FolderStructureNode * tree){
  int nodeCount = ComputeNewIndex(tree,0);
  printf("%d\n",nodeCount);
  RecursivePrintFdStruct(tree);
}

FolderStructureNode * ConstructStructureFromFile(const char * path){
  FILE * fd = fopen(path,"r");
  if(fd == NULL){return NULL;}
  int nodeCount;
  fscanf(fd," %d",&nodeCount);
  FolderStructureNode * nodearr = calloc(sizeof(FolderStructureNode),nodeCount);
  int i,j;
  int index,type,li,ri,hs;
  for(i = 0;i < nodeCount;++i){
    fscanf(fd," %d %d %d %d",&index,&type,&li,&ri);
    nodearr[index].index = index;
    nodearr[index].type = type;
    if(li == -1){
      nodearr[index].folderHead = NULL;
    } else {
      nodearr[index].folderHead = nodearr + li;
    }
    if(ri == -1){
      nodearr[index].nextFile = NULL;
    } else {
      nodearr[index].nextFile = nodearr + ri;
    }
    for(j = 0;j < 16;++j){
      fscanf(fd," %X",&hs);
      nodearr[index].hash[j] = hs;
    }
    fscanf(fd," %s",nodearr[index].name);
  }
  return nodearr;
}






// FolderStructureNode nodearr[10];

/*
int main(){
  int i;
  for(i = 0;i < 10;++i){
    nodearr[i].index = i;
    nodearr[i].type = 1;
    sprintf(nodearr[i].name,"%d",i);
    if(i < 9){
      nodearr[i].nextFile = nodearr + i + 1;
    } else {
      nodearr[i].nextFile = NULL;
    }
    nodearr[i].folderHead = NULL;
  }
  SerializeStructure(nodearr);
  FolderStructureNode * input = ConstructStructureFromFile("test.manifest");
  SerializeStructure(input);
  return 0;
}
*/
