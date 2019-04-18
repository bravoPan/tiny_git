extern volatile char globalStop;

void output_error(int errnum);
int parse_port(char * port);
ssize_t send_all(int socket,const void * data,size_t len,int sig);
ssize_t read_all(int socket,void * data,size_t len,int sig);

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
    char hash[64];
    struct FolderStructureNode * nextFile, * folderHead;
} FolderStructureNode;

/* FolderDiff.txt
index type name oldhash newHash nextFile folderHead
*/

typedef struct FolderDiffNode{
    int index;
    char type;
    /*0: Uninitialized
      1: Add new file
      2: Delete file
      3: Modified file
      4: Add Folder
      5: Delete Folder
      6: Modified Folder
    */
    char name[256];
    char oldHash[64],newHash[64];
    struct FolderDiffNode * nextFile, * folderHead;
} FolderDiffNode;

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
FolderDiffNode * ConstructDifference(FolderStructureNode * oldTree, FolderStructureNode * newTree);
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

typedef struct ProgressBar{
    int posX,posY;
    int length;
    int currProgress; //Max 100
} ProgressBar;

void InitializeGUI(void);
void DrawProgressBar(ProgressBar * bar);
