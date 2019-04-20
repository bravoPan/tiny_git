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
    char project[256];
    char type;
    /*0: Uninitialized
      1: Add new file
      2: Delete file
      3: Modified file
      4: Add Folder
      5: Delete Folder
      6: Modified Folder
    */
    char *version;
    char oldHash[16],newHash[16];
    struct FolderDiffNode * nextFile, * folderHead;
} FolderDiffNode;

typedef struct MD5FileInfo{
    char file_name[256];
    uint8_t hash[16];
    int file_size;
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
HashMapNode * HashMapInsert(HashMap * hmap, const char * key, void * nodePtr);
HashMapNode * HashMapFind(HashMap * hmap, const char * key);
void DestroyHashMap(HashMap * hmap);
FolderStructureNode * ConstructStructureFromPath(const char * path);
void PrintHashMap(HashMap * hmap);
FolderStructureNode * ConstructStructureFromFile(const char * path);
FolderStructureNode* CreateFolderStructNode(int index, const char *name, const char *hash, FolderStructureNode *nextFile, FolderStructureNode *folderHead);
FolderDiffNode * ConstructDifference(FolderStructureNode * oldTree, FolderStructureNode * newTree);
FolderStructureNode *SearchStructNode(FolderStructureNode *root, const char *path); 
FolderDiffNode * ConstructDifferenceFromFile(FILE * fd);
void DestroyStructure(FolderStructureNode * tree);
void DestroyDifference(FolderDiffNode * diff);
void ApplyDiff(FolderStructureNode * oldTree, FolderDiffNode * diff);
void RevertDiff(FolderStructureNode * newTree, FolderDiffNode * diff);
void SerializeStructure(FolderStructureNode * tree);
void SerializeDifference(FolderDiffNode * diff);

uint32_t ComputeCRC32(const char * data, int len);
void ComputeMD5(const char * data, int len);

void SendMessage(int socket, const char * msg, int len);
//Send message length first, then message body
void SendPacket(int socket, const char * pkt,int len);
//Send packet length first, then packet data, finally packet checksum
void SendFile(int socket, const char * path);
//Split the file into packets of fixed size, and send each packet sequentially.
void DeleteFile(int socket, const char * path);

int IsProject(const char *path);

typedef struct ProgressBar{
    int posX,posY;
    int length;
    int currProgress; //Max 100
} ProgressBar;

void InitializeGUI(void);
void DrawProgressBar(ProgressBar * bar);


void GetMD5(const uint8_t * data, size_t data_len, uint32_t * output_array);
MD5FileInfo *GetMD5FileInfo(const char *file_name);
