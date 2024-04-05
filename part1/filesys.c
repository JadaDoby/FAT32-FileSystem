#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include "lexer.h"
#include<ctype.h>

#define MAX_STACK_SIZE 128

typedef struct
{
    uint16_t bytesPerSector;
    uint8_t sectorsPerCluster;
    uint32_t totalSectors;
    uint32_t FATSize; // Sectors per FAT
    uint32_t rootCluster;
    uint16_t reservedSectors; // Added: Number of reserved sectors
    uint8_t numFATs;          // Added: Number of FATs
    uint32_t firstDataSector; // Calculated first data sector
} FAT32BootSector;

typedef struct __attribute__((packed)) directory_entry
{
    char DIR_Name[11];
    uint8_t DIR_Attr;
    char padding_1[8]; // Placeholder for unused fields
    uint16_t DIR_FstClusHI;
    char padding_2[4]; // Placeholder for unused fields
    uint16_t DIR_FstClusLO;
    uint32_t DIR_FileSize;
} dentry_t;




FAT32BootSector bs;
uint32_t currentDirectoryCluster; // Global variable to store the current directory cluster
FILE *fp;                         // File pointer to the FAT32 image

// Function prototypes
int mountImage(const char *imageName);
void printInfo();
uint32_t clusterToSector(uint32_t cluster);
void readCluster(uint32_t clusterNumber, uint8_t *buffer);
uint32_t readFATEntry(uint32_t clusterNumber);
void dbg_print_dentry(dentry_t *dentry);
uint32_t findDirectoryCluster(const char *dirName);
void processCommand(tokenlist *tokens);
typedef struct
{
    char *directoryPath[MAX_STACK_SIZE];
    int size;
    uint32_t clusterNumber[MAX_STACK_SIZE];
} DirectoryStack;

DirectoryStack dirStack;

// Initialize the directory stack
void initDirStack()
{
    dirStack.size = 0;
    for (int i = 0; i < MAX_STACK_SIZE; ++i)
    {
        dirStack.directoryPath[i] = NULL;
    }
}

// Push a directory onto the stack
void pushDir(const char *dir,u_int32_t clusternum)
{
    if (dirStack.size < MAX_STACK_SIZE)
    {
        dirStack.directoryPath[dirStack.size] = strdup(dir); // Copies the string
        dirStack.clusterNumber[dirStack.size] = clusternum;
        dirStack.size++;
    }
}

// Pop a directory from the stack (call free on the popped directory when done)
char *popDir()
{
    if (dirStack.size > 0)
    {
        dirStack.size--;
        return dirStack.directoryPath[dirStack.size];
    }
    return NULL;
}

// Free resources allocated for the stack
void freeDirStack()
{
    for (int i = 0; i < dirStack.size; ++i)
    {
        free(dirStack.directoryPath[i]);
    }
}
char currentPath[128];
const char *getCurrentDirPath()
{
    strcpy(currentPath, ""); 
    for (int i = 0; i < dirStack.size; ++i)
    {
        strcat(currentPath, dirStack.directoryPath[i]);
        if (i < dirStack.size - 1)
        {
            strcat(currentPath, "/"); 
        }
    }
    return currentPath;
}
int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <FAT32 image file>\n", argv[0]);
        return 1;
    }

    if (mountImage(argv[1]) != 0)
    {
        return 1;
    }
    initDirStack();

    pushDir(argv[1],2);

    char *input;
    while (1)
    {
        printf("%s/> ", getCurrentDirPath()); 
        input = get_input();
        tokenlist *tokens = get_tokens(input);
        processCommand(tokens); // Ensure this updates the dirStack as necessary
        free_tokens(tokens);
        free(input);
    }
    freeDirStack();
    return 0;
}

    char *get_input(void)
    {
        char *buffer = NULL;
        int bufsize = 0;
        char line[5];
        while (fgets(line, 5, stdin) != NULL)
        {
            int addby = 0;
            char *newln = strchr(line, '\n');
            if (newln != NULL)
                addby = newln - line;
            else
                addby = 5 - 1;
            buffer = (char *)realloc(buffer, bufsize + addby);
            memcpy(&buffer[bufsize], line, addby);
            bufsize += addby;
            if (newln != NULL)
                break;
        }
        buffer = (char *)realloc(buffer, bufsize + 1);
        buffer[bufsize] = 0;
        return buffer;
}

tokenlist *new_tokenlist(void)
{
    tokenlist *tokens = (tokenlist *)malloc(sizeof(tokenlist));
    tokens->size = 0;
    tokens->items = (char **)malloc(sizeof(char *));
    tokens->items[0] = NULL;
    return tokens;
}

void add_token(tokenlist *tokens, char *item)
{
    int i = tokens->size;

    tokens->items = (char **)realloc(tokens->items, (i + 2) * sizeof(char *));
    tokens->items[i] = (char *)malloc(strlen(item) + 1);
    tokens->items[i + 1] = NULL;
    strcpy(tokens->items[i], item);

    tokens->size += 1;
}

tokenlist *get_tokens(char *input)
{
    char *buf = (char *)malloc(strlen(input) + 1);
    strcpy(buf, input);
    tokenlist *tokens = new_tokenlist();
    char *tok = strtok(buf, " ");
    while (tok != NULL)
    {
        add_token(tokens, tok);
        tok = strtok(NULL, " ");
    }
    free(buf);
    return tokens;
}

void free_tokens(tokenlist *tokens)
{
    for (int i = 0; i < tokens->size; i++)
        free(tokens->items[i]);
    free(tokens->items);
    free(tokens);
}

int mountImage(const char *imageName)
{
    fp = fopen(imageName, "rb");
    if (!fp)
    {
        perror("Error opening image file");
        return -1;
    }

    fseek(fp, 11, SEEK_SET);
    fread(&bs.bytesPerSector, sizeof(bs.bytesPerSector), 1, fp);
    // Read sectors per cluster
    fseek(fp, 13, SEEK_SET);
    fread(&bs.sectorsPerCluster, sizeof(bs.sectorsPerCluster), 1, fp);
    // Read reserved sectors
    fseek(fp, 14, SEEK_SET);
    fread(&bs.reservedSectors, sizeof(bs.reservedSectors), 1, fp);
    // Read number of FATs
    fseek(fp, 16, SEEK_SET);
    fread(&bs.numFATs, sizeof(bs.numFATs), 1, fp);

    // Seek and read total sectors
    fseek(fp, 32, SEEK_SET);
    fread(&bs.totalSectors, sizeof(bs.totalSectors), 1, fp);
    // Read sectors per FAT
    fseek(fp, 36, SEEK_SET);
    fread(&bs.FATSize, sizeof(bs.FATSize), 1, fp);
    // Read root cluster
    fseek(fp, 44, SEEK_SET);
    fread(&bs.rootCluster, sizeof(bs.rootCluster), 1, fp);

    // Calculating the first data sector
    bs.firstDataSector = bs.reservedSectors + (bs.numFATs * bs.FATSize);
    currentDirectoryCluster = bs.rootCluster;

    return 0;
}

void printInfo()
{
    uint32_t totalDataSectors = bs.totalSectors - (bs.reservedSectors + (bs.FATSize * bs.numFATs * bs.sectorsPerCluster));
    uint64_t totalClusters = totalDataSectors / bs.sectorsPerCluster;

    printf("Bytes Per Sector: %d\n", bs.bytesPerSector);
    printf("Sectors Per Cluster: %d\n", bs.sectorsPerCluster);
    printf("Root Cluster: %d\n", bs.rootCluster);
    printf("Total # of Clusters in Data Region: %llu\n", totalClusters);
    printf("# of Entries in One FAT: %d\n", bs.FATSize * (bs.bytesPerSector / 4)); // Assuming 4 bytes per FAT entry
    printf("Size of Image (in bytes): %llu\n", (uint64_t)bs.totalSectors * bs.bytesPerSector);
}

uint32_t clusterToSector(uint32_t cluster)
{
    return ((cluster - 2) * bs.sectorsPerCluster) + bs.firstDataSector;
}

void readCluster(uint32_t clusterNumber, uint8_t *buffer)
{
    uint32_t firstSector = clusterToSector(clusterNumber);
    fseek(fp, firstSector * bs.bytesPerSector, SEEK_SET);
    fread(buffer, bs.bytesPerSector, bs.sectorsPerCluster, fp);
}

uint32_t readFATEntry(uint32_t clusterNumber)
{
    uint32_t fatOffset = clusterNumber * 4; // 4 bytes per FAT32 entry
    uint32_t fatSector = bs.reservedSectors + (fatOffset / bs.bytesPerSector);
    uint32_t entOffset = fatOffset % bs.bytesPerSector;

    uint8_t sectorBuffer[512]; // Temporary buffer for the sector
    fseek(fp, fatSector * bs.bytesPerSector, SEEK_SET);
    fread(sectorBuffer, 1, 512, fp); // Read the sector containing the FAT entry

    uint32_t nextCluster;
    memcpy(&nextCluster, &sectorBuffer[entOffset], sizeof(uint32_t));
    nextCluster &= 0x0FFFFFFF; // Mask to get 28 bits

    return nextCluster;
}

uint32_t findDirectoryCluster(const char *dirName)
{
    uint8_t *buffer = malloc(bs.bytesPerSector * bs.sectorsPerCluster);
    if (!buffer)
    {
        printf("Memory allocation failed\n");
        return 0;
    }
    
    printf("Searching for directory: %s\n", dirName);
    if(strcmp(dirName,"..")==0){
        if (dirStack.size == 1)
        {
            printf("Already at root directory\n");
            free(buffer);
            return 2;
        }
        popDir();
        u_int32_t parent = dirStack.clusterNumber[dirStack.size - 1];
        printf("Parent directory cluster: %d\n", parent);
        free(buffer);
        return parent;
    }

    if(strcmp(dirName,".")==0){
        free(buffer);
        return currentDirectoryCluster;
    }

    char dirNameUpper[12]; // Buffer to store the uppercase version of dirName
    strncpy(dirNameUpper, dirName, 11);
    dirNameUpper[11] = '\0'; // Ensure null-termination
    // Convert dirNameUpper to uppercase
    for (int i = 0; dirNameUpper[i]; i++)
    {
        dirNameUpper[i] = toupper(dirNameUpper[i]);
    }

    uint32_t cluster = currentDirectoryCluster;
    do
    {
        readCluster(cluster, buffer);
        dentry_t *dentry = (dentry_t *)buffer;
        for (int i = 0; i < bs.bytesPerSector * bs.sectorsPerCluster / sizeof(dentry_t); i++, dentry++)
        {
            if (dentry->DIR_Name[0] == 0)
                break; // End of directory entries
            if (dentry->DIR_Name[0] == 0xE5)
                continue; // Skip deleted entry
            if (!(dentry->DIR_Attr & 0x10))
                continue; // Skip if not a directory

            char name[12];
            memset(name, ' ', 11); // Initialize name buffer with spaces
            memcpy(name, dentry->DIR_Name, 11);
            name[11] = '\0'; // Null-terminate the string
            // Uppercase and trim trailing spaces for comparison
            for (int j = 0; j < 11; j++)
            {
                if (name[j] == ' ')
                    name[j] = '\0'; // Stop at first space
                else
                    name[j] = toupper(name[j]);
            }

            if (strcmp(name, dirNameUpper) == 0)
            {
                free(buffer);
                uint32_t clusterNumber = ((uint32_t)dentry->DIR_FstClusHI << 16) | dentry->DIR_FstClusLO;
                printf("Found directory %s at cluster %u\n", dirName, clusterNumber);
                return clusterNumber;
            }
        }
        cluster = readFATEntry(cluster);
    } while (cluster < 0x0FFFFFF8);

    free(buffer);
    return 0;
}

void dbg_print_dentry(dentry_t *dentry)
{
    if (dentry == NULL)
    {
        return;
    }

    printf("DIR_Name: %s\n", dentry->DIR_Name);
    printf("DIR_Attr: 0x%x\n", dentry->DIR_Attr);
    printf("DIR_FstClusHI: 0x%x\n", dentry->DIR_FstClusHI);
    printf("DIR_FstClusLO: 0x%x\n", dentry->DIR_FstClusLO);
    printf("DIR_FileSize: %u\n", dentry->DIR_FileSize);
}

void listDirectory(uint32_t cluster)
{
    uint8_t *buffer = malloc(bs.bytesPerSector * bs.sectorsPerCluster);
    if (!buffer)
    {
        printf("Failed to allocate memory for directory listing.\n");
        return;
    }

    // Keep reading clusters until the end of the directory or until the end-of-chain marker is found
    printf("cluster: %d\n", cluster);
    while (cluster < 0x0FFFFFF8)
    {
        readCluster(cluster, buffer);
        dentry_t *entry = (dentry_t *)buffer;
        for (int i = 0; i < (bs.bytesPerSector * bs.sectorsPerCluster) / sizeof(dentry_t); i++, entry++)
        {
            if (entry->DIR_Name[0] == 0)
                break; // No more entries
            if (entry->DIR_Name[0] == 0xE5)
                continue; // Entry is free (deleted file)
            if ((entry->DIR_Attr & 0x0F) == 0x0F)
                continue; // Skip long name entries
            if (!(entry->DIR_Attr & 0x10))
                continue;

            // Print the directory entry name
            char name[12];
            memcpy(name, entry->DIR_Name, 11);
            name[11] = '\0'; // Null-terminate the string
            printf("%s\n", name);
        }

        // Move to the next cluster in the chain
        cluster = readFATEntry(cluster);
        printf("cluster: %d\n", cluster);
    }

    free(buffer);
}

void processCommand(tokenlist *tokens)
{
    if (tokens->size == 0)
        return;

    if (strcmp(tokens->items[0], "info") == 0)
    {
        printInfo();
    }
    else if (strcmp(tokens->items[0], "cd") == 0 && tokens->size > 1)
    {
        uint32_t newDirCluster = findDirectoryCluster(tokens->items[1]);
        printf("newDirCluster: %d\n", newDirCluster);
        if (newDirCluster)
        {
            currentDirectoryCluster = newDirCluster;
            printf("Changed directory to %s\n", tokens->items[1]);
            if (strcmp(tokens->items[1], "..") != 0 && strcmp(tokens->items[1], ".") != 0)
            {
                pushDir(tokens->items[1],newDirCluster);
            }
        }
        else
        {
            printf("Directory not found: %s\n", tokens->items[1]);
        }
    }
    else if (strcmp(tokens->items[0], "ls") == 0)
    {
        listDirectory(currentDirectoryCluster);
        printf("Current directory cluster: %d\n", currentDirectoryCluster);
    }

    else if (strcmp(tokens->items[0], "exit") == 0)
    {
        // Exit handling code here
    }
    else
    {
        printf("Unknown command.\n");
    }
}