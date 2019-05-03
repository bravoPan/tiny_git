extern volatile char globalStop;

void output_error(int errnum);
int parse_port(char * port);
ssize_t send_all(int socket,const void * data,size_t len,int sig);
ssize_t read_all(int socket,void * data,size_t len,int sig);

/* .Mainfest: The records of all files and folders (exclude the manifest)
    hash:  for folder, md5_hash for files
    <nodecount>
    <index> <type> <folderHead> <nextFile>\n<hash>\n<name>\n
*/

/* Folder.txt
index type hash nextFile folderHead
*/

typedef struct FolderStructureNode{
    int index;
    char type;
    int version;
    /*0: Uninitialized
      1: File
      2: Folder
    */
    char name[256];
    uint8_t hash[16];
    struct FolderStructureNode * nextFile, * folderHead;
} FolderStructureNode;

/* FolderDiff.txt
index type name oldhash newHash nextFile folderHead
*/

typedef struct FolderDiffNode{
    char name[256];
    char type;
    /*0: Uninitialized
      1: Uplaod new file
      2: Modified file
      3: Add new file
      4: Delete file
      5: Upload folder
      6: Modified Folder
      7: Add Folder
      8: Delete Folder
    */
    char oldHash[16],newHash[16];
    struct FolderDiffNode * nextFile, * folderHead;
} FolderDiffNode;

// typedef struct UpdateInfo{
//     char *file_name;
//     char
// }

typedef struct MD5FileInfo{
    uint8_t hash[16];
    int file_size;
    char * data;
}MD5FileInfo;

typedef struct HashMapNode{
    const char * key;
    void * nodePtr;
    struct HashMapNode * next;
} HashMapNode;

typedef struct HashMap{
    HashMapNode ** map;
    int size;
} HashMap;

int GetHash(const char * str);
HashMap * InitializeHashMap(int size);
HashMapNode * HashMapInsert(HashMap * hmap, const char * key,void * nodePtr);
HashMapNode * HashMapFind(HashMap * hmap,const char * key);
void DestroyHashMap(HashMap * hmap);
//FolderStructureNode * ConstructStructureFromPath(const char * path);
void PrintHashMap(HashMap * hmap);
// return NULL if fails
FolderStructureNode * ConstructStructureFromFile(const char * path);

FolderStructureNode * CreateFolderStructNode(const char type, const char *name, const char *hash, FolderStructureNode *nextFile, FolderStructureNode *folderHead, int version);

FolderStructureNode *SearchStructNodeLayer(const char *name, FolderStructureNode *root);

FolderDiffNode * ConstructDifference(FolderStructureNode * oldTree,FolderStructureNode * newTree);
FolderDiffNode * ConstructDifferenceFromFile(FILE * fd);
void DestroyStructure(FolderStructureNode * tree);
void DestroyDifference(FolderDiffNode * diff);
void ApplyDiff(FolderStructureNode * oldTree,FolderDiffNode * diff);
void RevertDiff(FolderStructureNode * newTree,FolderDiffNode * diff);
void SerializeStructure(FolderStructureNode * tree,FILE *fd);
void SerializeDifference(FolderDiffNode * diff,FILE * fd);

//uint32_t ComputeCRC32(const char * data,int len);
void ComputeMD5(const char * data, int len);

/* Types of command:
    delt
    null
    updt
    ckot
    pushs
    dist
*/
int SendMessage(int sockfd, char command[4], const char *msg,int msg_len);
//Send message length first, then message body
void SendPacket(int socket, const char * pkt);
/*SendFile protocol
256(name) 16(hash) 4(file_size) real content
*/
int SendFile(int socket, char *project_name, char *file_name);
//Split the file into packets of fixed size, and send each packet sequentially.

//void SendFileFromMani(int sockfd, FolderStructureNode *root, int parent_folder_fd, char *parent_folder_name);

void DeleteFile(int socket, const char * path);

//void CreateEmptyFolderStructFromMani(FolderStructureNode *root, int parent_folder_fd, char *parent_folder_name);

char *ReceiveMessage(int sockfd);

char *ReceiveFile(int sockfd, const char *project_name, const char *file_name);
int HandleRecieveFile(int sockfd);
// 0 for find success, -1 for find none
int IsProject(const char *path);

void GetMD5(const uint8_t * data, size_t data_len, uint32_t * output_array);

MD5FileInfo *GetMD5FileInfo(int file_fd);

char *convert_path_to_hexmd5(char filename[32]);
char *convert_hexmd5_to_path(unsigned char hash[16]);
