#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>  // Include for open
#include <ctype.h>
#include "lexer.h"

#define MAX_STACK_SIZE 128
#define ATTR_DIRECTORY 0x10
#define ENTRY_SIZE 32  
#define MAX_OPEN_FILES 16  


typedef struct { 
    uint16_t bytesPerSector;
    uint8_t sectorsPerCluster;
    uint32_t totalSectors;
    uint32_t FATSize; 
    uint32_t rootCluster;
    uint16_t reservedSectors; 
    uint8_t numFATs;         
    uint32_t firstDataSector; 
} FAT32BootSector;

typedef struct __attribute__((packed)) directory_entry {
    char DIR_Name[11];
    uint8_t DIR_Attr;
    char padding_1[8]; 
    uint16_t DIR_FstClusHI;
    char padding_2[4]; 
    uint16_t DIR_FstClusLO;
    uint32_t DIR_FileSize;
} dentry_t;

FAT32BootSector bs;
uint32_t currentDirectoryCluster; // Global variable 
int fd;  

typedef struct {
    char filename[12];  
    char mode[4];       
    int offset;         
    int isOpeninuse;         
} OpenFile;

OpenFile openFiles[MAX_OPEN_FILES];  //Array of open files


// Function prototypes
int mountImage(const char *imageName);
void printInfo();
uint32_t clusterToSector(uint32_t cluster);
void readCluster(uint32_t clusterNumber, uint8_t *buffer);
uint32_t readFATEntry(uint32_t clusterNumber);
void dbg_print_dentry(dentry_t *dentry);
uint32_t findDirectoryCluster(const char *dirName);
void processCommand(tokenlist *tokens);
uint32_t allocateCluster();
int initDirectoryCluster(uint32_t newCluster, uint32_t parentCluster);
int updateParentDirectory(uint32_t parentCluster, const char *dirName, uint32_t newCluster);
int createDirectory(const char *dirName);
void formatNameToFAT(const char* name, uint8_t* entryBuffer);
int writeDirectoryEntry(uint32_t parentCluster, const char* name, uint32_t cluster, uint8_t attr);
int writeEntryToDisk(uint32_t parentCluster, const uint8_t* entry);
void writeFATEntry(uint32_t clusterNumber, uint32_t value);
int initDirectoryCluster(uint32_t newCluster, uint32_t parentCluster);
int updateParentDirectory(uint32_t parentCluster, const char *dirName, uint32_t newCluster);
void clearCluster(uint32_t clusterNumber);
uint32_t clusterToSector(uint32_t cluster);
bool is_8_3_format_directory(const char* name);
bool isDirectoryFull(uint32_t parentCluster);
int linkClusterToDirectory(uint32_t currentDirectoryCluster, uint32_t newCluster);
int addDirectory(uint32_t parentCluster, const char* dirName);
int createFile(const char *fileName);
bool is_8_3_format_filename(const char *name);
bool fileExists(const char* filename);
void toUpperCase(char *str);
int expandDirectory(uint32_t parentCluster);  
void rightTrim(char *str);
int openFile(const char *filename, const char *mode);
void initOpenFiles();

typedef struct{
    char *directoryPath[MAX_STACK_SIZE];
    int size;
    uint32_t clusterNumber[MAX_STACK_SIZE];
} DirectoryStack;

DirectoryStack dirStack;

void initDirStack() {
    dirStack.size = 0;
    for (int i = 0; i < MAX_STACK_SIZE; ++i) {
        dirStack.directoryPath[i] = NULL;
    }
}

void pushDir(const char *dir, uint32_t clusternum) {
    if (dirStack.size < MAX_STACK_SIZE) {
        dirStack.directoryPath[dirStack.size] = strdup(dir); // Copies the string
        dirStack.clusterNumber[dirStack.size] = clusternum;
        dirStack.size++;
    }
}

char *popDir() {
    if (dirStack.size > 0) {
        dirStack.size--;
        return dirStack.directoryPath[dirStack.size];
    }
    return NULL;
}

void freeDirStack() {
    for (int i = 0; i < dirStack.size; ++i) {
        free(dirStack.directoryPath[i]);
    }
}

char currentPath[128];
const char *getCurrentDirPath() {
    strcpy(currentPath, ""); 
    for (int i = 0; i < dirStack.size; ++i) {
        strcat(currentPath, dirStack.directoryPath[i]);
        if (i < dirStack.size - 1) {
            strcat(currentPath, "/"); 
        }
    }
    return currentPath;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <FAT32 image file>\n", argv[0]);
        return 1;
    }

    if (mountImage(argv[1]) != 0) {
        return 1;
    }
    initDirStack();

    pushDir(argv[1], 2);

    char *input;
    while (1) {
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

char *get_input(void) {
    char *buffer = NULL;
    int bufsize = 0;
    char line[5];
    while (fgets(line, 5, stdin) != NULL) {
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

tokenlist *new_tokenlist(void) {
    tokenlist *tokens = (tokenlist *)malloc(sizeof(tokenlist));
    tokens->size = 0;
    tokens->items = (char **)malloc(sizeof(char *));
    tokens->items[0] = NULL;
    return tokens;
}

void add_token(tokenlist *tokens, char *item) {
    int i = tokens->size;
    tokens->items = (char **)realloc(tokens->items, (i + 2) * sizeof(char *));
    tokens->items[i] = (char *)malloc(strlen(item) + 1);
    tokens->items[i + 1] = NULL;
    strcpy(tokens->items[i], item);
    tokens->size += 1;
}

tokenlist *get_tokens(char *input) {
    char *buf = (char *)malloc(strlen(input) + 1);
    strcpy(buf, input);
    tokenlist *tokens = new_tokenlist();
    char *tok = strtok(buf, " ");
    while (tok != NULL) {
        add_token(tokens, tok);
        tok = strtok(NULL, " ");
    }
    free(buf);
    return tokens;
}

void free_tokens(tokenlist *tokens) {
    for (int i = 0; i < tokens->size; i++)
        free(tokens->items[i]);
    free(tokens->items);
    free(tokens);
}

int mountImage(const char *imageName) {
    fd = open(imageName, O_RDWR);
    if (fd == -1) {
        perror("Error opening image file");
        return -1;
    }

    // Read from position 11 to get bytes per sector
    pread(fd, &bs.bytesPerSector, sizeof(bs.bytesPerSector), 11);
    // Read sectors per cluster from position 13
    pread(fd, &bs.sectorsPerCluster, sizeof(bs.sectorsPerCluster), 13);
    // Read number of reserved sectors from position 14
    pread(fd, &bs.reservedSectors, sizeof(bs.reservedSectors), 14);
    // Read number of FATs from position 16
    pread(fd, &bs.numFATs, sizeof(bs.numFATs), 16);
    // Read total sectors from position 32
    pread(fd, &bs.totalSectors, sizeof(bs.totalSectors), 32);
    // Read sectors per FAT from position 36
    pread(fd, &bs.FATSize, sizeof(bs.FATSize), 36);
    // Read root cluster from position 44
    pread(fd, &bs.rootCluster, sizeof(bs.rootCluster), 44);

    // Calculate the first data sector
    bs.firstDataSector = bs.reservedSectors + (bs.numFATs * bs.FATSize);
    currentDirectoryCluster = bs.rootCluster;

    return 0;
}

void printInfo() {
    uint32_t totalDataSectors = bs.totalSectors - (bs.reservedSectors + (bs.FATSize * bs.numFATs * bs.sectorsPerCluster));
    uint64_t totalClusters = totalDataSectors / bs.sectorsPerCluster;
    printf("Bytes Per Sector: %d\n", bs.bytesPerSector);
    printf("Sectors Per Cluster: %d\n", bs.sectorsPerCluster);
    printf("Root Cluster: %d\n", bs.rootCluster);
    printf("Total # of Clusters in Data Region: %llu\n", totalClusters);
    printf("# of Entries in One FAT: %d\n", bs.FATSize * (bs.bytesPerSector / 4)); // Assuming 4 bytes per FAT entry
    printf("Size of Image (in bytes): %llu\n", (uint64_t)bs.totalSectors * bs.bytesPerSector);
}

uint32_t clusterToSector(uint32_t cluster) {
    // The cluster number should be at least 2, as cluster numbers start from 2 in FAT32.
    if (cluster < 2) {
        fprintf(stderr, "Invalid cluster number: %u. Cluster numbers should be >= 2.\n", cluster);
        return 0;  // Returning 0 might indicate an error in the context of your application.
    }
    // Calculate the sector number corresponding to the given cluster number.
    uint32_t sector = ((cluster - 2) * bs.sectorsPerCluster) + bs.firstDataSector;
    return sector;
}

void readCluster(uint32_t clusterNumber, uint8_t *buffer) {
    uint32_t firstSector = clusterToSector(clusterNumber);
    ssize_t bytesRead = pread(fd, buffer, bs.bytesPerSector * bs.sectorsPerCluster, firstSector * bs.bytesPerSector);
    if (bytesRead < bs.bytesPerSector * bs.sectorsPerCluster) {
        perror("Failed to read full cluster");
    }
}


uint32_t readFATEntry(uint32_t clusterNumber) {
    uint32_t fatOffset = clusterNumber * 4; // 4 bytes per FAT32 entry
    uint32_t fatSector = bs.reservedSectors + (fatOffset / bs.bytesPerSector);
    uint32_t entOffset = fatOffset % bs.bytesPerSector;
    uint8_t sectorBuffer[512]; // Temporary buffer for the sector
    pread(fd, sectorBuffer, 512, fatSector * bs.bytesPerSector); // Read the sector containing the FAT entry
    uint32_t nextCluster;
    memcpy(&nextCluster, &sectorBuffer[entOffset], sizeof(uint32_t));
    nextCluster &= 0x0FFFFFFF; // Mask to get 28 bits
    return nextCluster;
}

uint32_t findDirectoryCluster(const char *dirName) {
    uint8_t *buffer = malloc(bs.bytesPerSector * bs.sectorsPerCluster);
    if (!buffer) {
        printf("Memory allocation failed\n");
        return 0;
    }
    
    printf("Searching for directory: %s\n", dirName);
    if(strcmp(dirName,"..")==0){
        if (dirStack.size == 1) {
            printf("Already at root directory\n");
            free(buffer);
            return 2;
        }
        popDir();
        uint32_t parent = dirStack.clusterNumber[dirStack.size - 1];
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
    for (int i = 0; dirNameUpper[i]; i++) {
        dirNameUpper[i] = toupper(dirNameUpper[i]);
    }
    uint32_t cluster = currentDirectoryCluster;
    do {
        readCluster(cluster, buffer);
        dentry_t *dentry = (dentry_t *)buffer;
        for (int i = 0; i < bs.bytesPerSector * bs.sectorsPerCluster / sizeof(dentry_t); i++, dentry++) {
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
            for (int j = 0; j < 11; j++) {
                if (name[j] == ' ')
                    name[j] = '\0'; // Stop at first space
                else
                    name[j] = toupper(name[j]);
            }
            if (strcmp(name, dirNameUpper) == 0) {
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

void dbg_print_dentry(dentry_t *dentry) {
    if (dentry == NULL) {
        return;
    }
    printf("DIR_Name: %s\n", dentry->DIR_Name);
    printf("DIR_Attr: 0x%x\n", dentry->DIR_Attr);
    printf("DIR_FstClusHI: 0x%x\n", dentry->DIR_FstClusHI);
    printf("DIR_FstClusLO: 0x%x\n", dentry->DIR_FstClusLO);
    printf("DIR_FileSize: %u\n", dentry->DIR_FileSize);
}
void listDirectory(uint32_t cluster) {
    uint8_t *buffer = malloc(bs.bytesPerSector * bs.sectorsPerCluster);
    if (!buffer) {
        printf("Failed to allocate memory for directory listing.\n");
        return;
    }
    printf("Listing directory at cluster: %d\n", cluster);

    // Read through clusters until end-of-chain or no more entries
    while (cluster < 0x0FFFFFF8) { // 0x0FFFFFF8 is the end-of-chain marker for FAT32
        readCluster(cluster, buffer);
        dentry_t *entry = (dentry_t *)buffer;
        int entryFound = 0;

        for (int i = 0; i < (bs.bytesPerSector * bs.sectorsPerCluster) / sizeof(dentry_t); i++, entry++) {
            if (entry->DIR_Name[0] == 0) break; // No more entries
            if (entry->DIR_Name[0] == 0xE5) continue; // Entry is deleted
            if ((entry->DIR_Attr & 0x0F) == 0x0F) continue; // Skip long name entries

            char name[12];
            memcpy(name, entry->DIR_Name, 11);
            name[11] = '\0'; // Null-terminate the string
            printf("%s\n", name);
            entryFound = 1;
        }

        if (!entryFound) {
            printf("No valid entries found in current directory cluster: %d\n", cluster);
        }

        // Move to the next cluster in the chain if needed
        cluster = readFATEntry(cluster);
    }

    free(buffer);
}








int createDirEntry(uint32_t parentCluster, const char *dirName) {
    // Create the directory entry for `dirName` in `parentCluster`
    uint32_t dirCluster = allocateCluster();
    if (dirCluster == 0) {
        printf("Failed to allocate a new cluster for the directory.\n");
        return -1;
    }
    // Assuming `writeDirectoryEntry()` is a function that writes the entry into the directory
    if (!writeDirectoryEntry(parentCluster, dirName, dirCluster, ATTR_DIRECTORY)) {
        printf("Failed to create directory entry.\n");
        return -1;
    }
    // Create entries for '.' and '..'
    writeDirectoryEntry(dirCluster, ".", dirCluster, ATTR_DIRECTORY);  // Self-reference
    writeDirectoryEntry(dirCluster, "..", parentCluster, ATTR_DIRECTORY);  // Parent reference
    return 0;  // Success
}

void formatNameToFAT(const char* name, uint8_t* entryBuffer) {
    // Clearing space for the name and extension
    memset(entryBuffer, ' ', 11);
    // Copy the base name and extension into the buffer
    int i = 0, j = 0;
    for (; name[i] != '\0' && name[i] != '.' && i < 8; ++i) {
        entryBuffer[j++] = toupper((unsigned char)name[i]); // Copy only the first 8 characters for the name
    }
    if (name[i] == '.') ++i; // Skip the dot
    j = 8; // Move to the extension part
    for (; name[i] != '\0' && j < 11; ++i) {
        entryBuffer[j++] = toupper((unsigned char)name[i]); // Copy up to 3 characters for the extension
    }
}

int updateParentDirectory(uint32_t parentCluster, const char *dirName, uint32_t newCluster) {
    if (writeDirectoryEntry(parentCluster, dirName, newCluster, ATTR_DIRECTORY) != 0) {
        printf("Failed to update parent directory with new directory entry.\n");
        return -1;
    }
    return 0;
}

void writeFATEntry(uint32_t clusterNumber, uint32_t value) {
    uint32_t fatOffset = clusterNumber * 4;
    uint32_t fatSector = bs.reservedSectors + (fatOffset / bs.bytesPerSector);
    uint32_t entOffset = fatOffset % bs.bytesPerSector;
    uint8_t sectorBuffer[bs.bytesPerSector];
    pread(fd, sectorBuffer, bs.bytesPerSector, fatSector * bs.bytesPerSector); // Read the sector containing the FAT entry
    memcpy(&sectorBuffer[entOffset], &value, sizeof(uint32_t));
    pwrite(fd, sectorBuffer, bs.bytesPerSector, fatSector * bs.bytesPerSector); // Write back the modified sector
}



int writeEntryToDisk(uint32_t parentCluster, const uint8_t* entry) {
    uint32_t sectorNumber = clusterToSector(parentCluster);
    int found = 0;
    // Calculate the full sector size to be read/written.
    uint32_t sectorSize = bs.bytesPerSector;
    uint32_t clusterSize = bs.sectorsPerCluster * sectorSize;
    char buffer[clusterSize];  // Buffer for the entire cluster.
    // Read the entire cluster at once.
    if (pread(fd, buffer, clusterSize, sectorNumber * sectorSize) != clusterSize) {
        perror("Error reading sector");
        return -1;
    }
    // Iterate over each entry within the cluster to find a free or deleted entry.
    for (int i = 0; i < clusterSize; i += ENTRY_SIZE) {
        if (buffer[i] == 0x00 || buffer[i] == 0xE5) {  // Check for free (0x00) or deleted (0xE5) entries.
            memcpy(&buffer[i], entry, ENTRY_SIZE);  // Copy the new directory entry to the buffer.
            found = 1;
            break;
        }
    }
    // If a free entry was found, write the entire cluster back to disk.
    if (found) {
        if (pwrite(fd, buffer, clusterSize, sectorNumber * sectorSize) != clusterSize) {
            perror("Error writing sector");
            return -1;
        }
        return 0;
    } else {
        printf("Failed to find a free directory entry.\n");
        return -1;
    }
}
 
uint32_t allocateCluster() {
    uint32_t clusterNumber, nextCluster;
    // Start scanning from cluster 2 (the first data cluster in FAT32)
    for (clusterNumber = 2; clusterNumber < bs.totalSectors / bs.sectorsPerCluster; clusterNumber++) {
        nextCluster = readFATEntry(clusterNumber);
        if (nextCluster == 0) { // 0 indicates a free cluster
            // Mark the cluster as end of chain
            writeFATEntry(clusterNumber, 0x0FFFFFFF);
            return clusterNumber;
        }
    }
    return 0; // Return 0 if no free cluster found
}

int writeDirectoryEntry(uint32_t parentCluster, const char* name, uint32_t cluster, uint8_t attr) {
    while (true) {
        uint32_t sectorNumber = clusterToSector(parentCluster);
        uint32_t clusterSize = bs.sectorsPerCluster * bs.bytesPerSector;
        uint8_t buffer[clusterSize];

        if (pread(fd, buffer, clusterSize, sectorNumber * bs.bytesPerSector) < clusterSize) {
            perror("Error reading cluster");
            return -1;
        }

        for (int i = 0; i < clusterSize; i += ENTRY_SIZE) {
            // Check for free (0x00) or deleted (0xE5) entries
            if (buffer[i] == 0x00 || buffer[i] == 0xE5) {
                // Properly format the filename and extension into the buffer
                formatNameToFAT(name, buffer + i);
                buffer[i + 11] = attr;  // Set attribute (0 for files)
                uint16_t hi = (cluster >> 16) & 0xFFFF; // High word of cluster number
                uint16_t lo = cluster & 0xFFFF;         // Low word of cluster number
                memcpy(buffer + i + 20, &hi, sizeof(hi));
                memcpy(buffer + i + 26, &lo, sizeof(lo));
                memset(buffer + i + 28, 0, 4);          // Set file size to 0 bytes

                // Write the modified buffer back to the disk
                if (pwrite(fd, buffer, clusterSize, sectorNumber * bs.bytesPerSector) < clusterSize) {
                    perror("Failed to write directory entry");
                    return -1;
                }
                return 0; // Entry written successfully
            }
        }

        // No free entry found, try to expand the directory
        int newCluster = expandDirectory(parentCluster);
        if (newCluster == -1) {
            return -1; // Failed to expand directory
        }
        parentCluster = newCluster; // Update parentCluster to newly allocated cluster
    }
}

   

//new
int createDirectory(const char *dirName) {
    printf("Attempting to create directory: %s\n", dirName);
    
    // Validate directory name format
    if (!is_8_3_format_directory(dirName)) {
        printf("Error: Directory name '%s' is not in FAT32 8.3 format.\n", dirName);
        return -1;
    }

    // Check if the directory already exists
    uint32_t existingCluster = findDirectoryCluster(dirName);
    if (existingCluster != 0) {
        printf("Error: Directory '%s' already exists at cluster %u.\n", dirName, existingCluster);
        return -1;
    }

    // Check if current directory is full and try to expand if necessary
    if (isDirectoryFull(currentDirectoryCluster)) {
        uint32_t newCluster = allocateCluster();
        if (newCluster == 0) {
            printf("Error: No free clusters available to extend the directory.\n");
            return -1;
        }

        // Link the new cluster as part of the current directory to extend its capacity
        if (linkClusterToDirectory(currentDirectoryCluster, newCluster) != 0) {
            printf("Error: Failed to link new cluster to extend directory capacity.\n");
            return -1;
        }

        // Attempt to add the directory again with the new space
        return addDirectory(newCluster, dirName);
    }

    // Add the directory to the current directory cluster
    return addDirectory(currentDirectoryCluster, dirName);
}

int initDirectoryCluster(uint32_t newCluster, uint32_t parentCluster) {
    // Clear the new cluster
    clearCluster(newCluster);
    // Create '.' and '..' directory entries
    if (writeDirectoryEntry(newCluster, ".", newCluster, ATTR_DIRECTORY) != 0 ||
        writeDirectoryEntry(newCluster, "..", parentCluster, ATTR_DIRECTORY) != 0) {
        printf("Failed to write '.' or '..' directory entries.\n");
        return -1;
    }
    return 0;
}

void clearCluster(uint32_t clusterNumber) {
    uint32_t sector = clusterToSector(clusterNumber);
    uint8_t buffer[bs.bytesPerSector * bs.sectorsPerCluster];
    memset(buffer, 0, sizeof(buffer));

    for (int i = 0; i < bs.sectorsPerCluster; i++) {
        if (pwrite(fd, buffer, bs.bytesPerSector, (sector + i) * bs.bytesPerSector) != bs.bytesPerSector) {
            perror("Failed to clear cluster");
            // Log the error or handle it as necessary
            // You could set a global error flag or handle it in another way
            break;  // Exit the loop on failure
        }
    }
}

bool isDirectoryFull(uint32_t parentCluster) {
    do {
        uint32_t sectorNumber = clusterToSector(parentCluster);
        uint32_t clusterSize = bs.sectorsPerCluster * bs.bytesPerSector;
        uint8_t buffer[clusterSize];

        if (pread(fd, buffer, clusterSize, sectorNumber * bs.bytesPerSector) != clusterSize) {
            perror("Error reading cluster for full check");
            return true; // Assume full if read fails
        }

        for (int i = 0; i < clusterSize; i += ENTRY_SIZE) {
            if (buffer[i] == 0x00 || buffer[i] == 0xE5) { // Free or deleted entry found
                return false;
            }
        }

        parentCluster = readFATEntry(parentCluster); // Get the next cluster in the chain
    } while (parentCluster < 0x0FFFFFF8);

    return true; // No free entry found in any cluster, directory is full
}
int expandDirectory(uint32_t parentCluster) {
    uint32_t newCluster = allocateCluster();
    if (newCluster == 0) {
        printf("No free clusters available to allocate for new directory.\n");
        return -1;
    }

    if (linkClusterToDirectory(parentCluster, newCluster) != 0) {
        printf("Failed to link new cluster to extend directory capacity.\n");
        return -1;
    }

    return newCluster;
}




int addDirectory(uint32_t parentCluster, const char* dirName) {
    if (isDirectoryFull(parentCluster)) {
        printf("Directory is full. Cannot add new directory.\n");
        return -1;
    }

    uint32_t newCluster = allocateCluster();
    if (newCluster == 0) {
        printf("No free clusters available to allocate for new directory.\n");
        return -1;
    }

    if (writeDirectoryEntry(parentCluster, dirName, newCluster, ATTR_DIRECTORY) != 0) {
        printf("Failed to write new directory entry.\n");
        return -1;
    }

    return initDirectoryCluster(newCluster, parentCluster);
}

int linkClusterToDirectory(uint32_t directoryCluster, uint32_t newCluster) {
    uint32_t lastCluster = directoryCluster;
    uint32_t nextCluster;

    while ((nextCluster = readFATEntry(lastCluster)) != 0x0FFFFFFF) {
        lastCluster = nextCluster; // Follow chain to end
    }

    // Link the new cluster
    writeFATEntry(lastCluster, newCluster);
    writeFATEntry(newCluster, 0x0FFFFFFF); // Mark the new cluster as end of chain

    return 0; // Success
}

void processCommand(tokenlist *tokens) {
    if (tokens->size == 0) return;

    if (strcmp(tokens->items[0], "info") == 0) {
        printInfo();
    } else if (strcmp(tokens->items[0], "cd") == 0) {
        if (tokens->size > 1) {
            uint32_t newDirCluster = findDirectoryCluster(tokens->items[1]);
            if (newDirCluster) {
                currentDirectoryCluster = newDirCluster;
                printf("Changed directory to %s\n", tokens->items[1]);
                if (strcmp(tokens->items[1], "..") != 0 && strcmp(tokens->items[1], ".") != 0) {
                    pushDir(tokens->items[1], newDirCluster);
                }
            } else {
                printf("Directory not found: %s\n", tokens->items[1]);
            }
        }
    } else if (strcmp(tokens->items[0], "ls") == 0) {
        listDirectory(currentDirectoryCluster);
    } else if (strcmp(tokens->items[0], "mkdir") == 0 && tokens->size > 1) {
        if (createDirectory(tokens->items[1]) == 0) {
            printf("Directory created: %s\n", tokens->items[1]);
        } else {
            printf("Failed to create directory: %s\n", tokens->items[1]);
        }
    } else if (strcmp(tokens->items[0], "creat") == 0 && tokens->size > 1) {
        if (createFile(tokens->items[1]) == 0) {
            printf("File '%s' created successfully.\n", tokens->items[1]);
        } else {
            printf("Failed to create file: %s\n", tokens->items[1]);
        }
    } else if (strcmp(tokens->items[0], "mount") == 0 && tokens->size > 1) {
        if (mountImage(tokens->items[1]) == 0) {
            printf("Mounted image: %s\n", tokens->items[1]);
        } else {
            printf("Failed to mount image: %s\n", tokens->items[1]);
        }
    } else if (strcmp(tokens->items[0], "open") == 0 && tokens->size == 3) {
        openFile(tokens->items[1], tokens->items[2]);
    } else if (strcmp(tokens->items[0], "exit") == 0) {
        printf("Exiting program.\n");
        exit(0);  // Terminate the program cleanly
    } else {
        printf("Unknown command.\n");
    }
}


        

bool is_8_3_format_directory(const char* name) {
    if (!name) return false;
    int nameLen = 0, extLen = 0;
    bool dotSeen = false;

    for (int i = 0; name[i] != '\0'; i++) {
        if (name[i] == '.') {
            if (dotSeen) return false; // Only one dot allowed
            dotSeen = true;
            continue;
        }
        if (!isalnum(name[i]) && name[i] != '_') return false; // Allow only alphanumeric and underscore

        if (dotSeen) {
            if (++extLen > 3) return false; // Extension part cannot be more than 3 characters
        } else {
            if (++nameLen > 8) return false; // Name part cannot be more than 8 characters
        }
    }

    return nameLen > 0 && (!dotSeen || extLen > 0); // Name part must exist, extension part is optional but if dot is seen it must have 1-3 characters
}

bool is_8_3_format_filename(const char *name) {
    int nameLen = 0, extLen = 0;
    bool dotSeen = false;

    // Validate input pointer
    if (name == NULL) return false;

    // Iterate through each character in the string
    for (int i = 0; name[i] != '\0'; i++) {
        // Check if the current character is a dot
        if (name[i] == '.') {
            // If a dot is already seen or it appears as the first character, return false
            if (dotSeen || i == 0) return false;
            dotSeen = true;
            continue;
        }

        // Ensure character is alphanumeric or underscore
        if (!isalnum(name[i]) && name[i] != '_') return false;

        // Count characters in the name part or extension part
        if (dotSeen) {
            extLen++;
            // Extension length should not exceed three characters
            if (extLen > 3) return false;
        } else {
            nameLen++;
            // Name length should not exceed eight characters
            if (nameLen > 8) return false;
        }
    }

    // The name must have at least one character and, if a dot is seen, the extension must have 1 to 3 characters
    return nameLen > 0 && (!dotSeen || extLen > 0);
}
void toUpperCase(char *str) {
    while (*str) {
        *str = toupper((unsigned char)*str);
        str++;
    }
}
bool fileExists(const char *filename) {
    char filenameFAT[12]; // Buffer to hold the uppercased and properly formatted filename
    memset(filenameFAT, ' ', 11); // Initialize with spaces to match FAT32 format
    filenameFAT[11] = '\0'; // Ensure null termination

    // Convert input filename to upper case and format it as FAT32 filename
    int nameLen = 0;
    for (int i = 0; i < strlen(filename) && nameLen < 11; i++) {
        if (filename[i] != '.') {
            filenameFAT[nameLen++] = toupper((unsigned char)filename[i]);
        } else {
            break; // Stop at the first dot to handle extension separately
        }
    }
    
    // Handle extension if there is one
    int dotIndex = strlen(filename) - 1;
    while (dotIndex >= 0 && filename[dotIndex] != '.') {
        dotIndex--;
    }

    if (dotIndex != -1) { // There is an extension
        int extIndex = 8; // Extension starts at 8th position in FAT32
        for (int i = dotIndex + 1; i < strlen(filename) && extIndex < 11; i++) {
            filenameFAT[extIndex++] = toupper((unsigned char)filename[i]);
        }
    }


    uint8_t *buffer = malloc(bs.bytesPerSector * bs.sectorsPerCluster);
    if (!buffer) {
        perror("Memory allocation failed");
        return true; // Assume existence to prevent potential data loss.
    }

    uint32_t cluster = currentDirectoryCluster;
    do {
        readCluster(cluster, buffer);
        dentry_t *entry = (dentry_t *)buffer;
        for (int i = 0; i < (bs.bytesPerSector * bs.sectorsPerCluster) / sizeof(dentry_t); i++, entry++) {
            if (entry->DIR_Name[0] == 0x00) break;
            if (entry->DIR_Name[0] == 0xE5) continue;

            if (strncmp(entry->DIR_Name, filenameFAT, 11) == 0) {
                free(buffer);
                return true;
            }
        }
        cluster = readFATEntry(cluster);
    } while (cluster < 0x0FFFFFF8);

    free(buffer);
    return false;
}



int createFile(const char *fileName) {

    if (!is_8_3_format_filename(fileName)) {
        printf("Error: File name '%s' is not in valid FAT32 8.3 format.\n", fileName);
        return -1;
    }

    if (fileExists(fileName)) {
        printf("Error: A file named '%s' already exists.\n", fileName);
        return -1;
    }

    uint32_t fileCluster = allocateCluster();
    if (fileCluster == 0) {
        printf("No free clusters available to create the file.\n");
        return -1;
    }

    if (writeDirectoryEntry(currentDirectoryCluster, fileName, fileCluster, 0) != 0) {
        printf("Failed to write directory entry for the file.\n");
        return -1;
    }

    printf("File '%s' created successfully.\n", fileName);
    return 0;
}

void rightTrim(char *str) {
    int end = strlen(str) - 1;
    while (end >= 0 && str[end] == ' ') {
        str[end] = '\0';
        end--;
    }
}
void initOpenFiles() {
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        openFiles[i].isOpeninuse = 0;  // Mark all entries as unused
    }
}
int openFile(const char *filename, const char *mode) {
    if (!is_8_3_format_filename(filename)) {
        printf("Error: Invalid filename format.\n");
        return -1;
    }
    if (strcmp(mode, "-r") != 0 && strcmp(mode, "-w") != 0 && strcmp(mode, "-rw") != 0 && strcmp(mode, "-wr") != 0) {
        printf("Error: Invalid mode. Use -r, -w, -rw, or -wr.\n");
        return -1;
    }

    if (!fileExists(filename)) {
        printf("Error: File '%s' does not exist.\n", filename);
        return -1;
    }

    // Check if file is already open
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (openFiles[i].isOpeninuse && strncmp(openFiles[i].filename, filename, 11) == 0) {
            printf("Error: File '%s' is already open.\n", filename);
            return -1;
        }
    }

    // Find an empty slot and open the file
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (!openFiles[i].isOpeninuse) {
            strncpy(openFiles[i].filename, filename, 11);
            strcpy(openFiles[i].mode, mode + 1); // Skip the '-' in mode for internal storage
            openFiles[i].offset = 0;
            openFiles[i].isOpeninuse = 1;
            printf("File '%s' opened in mode %s.\n", filename, mode);
            return 0;
        }
    }

    printf("Error: Too many open files.\n");
    return -1;
}

