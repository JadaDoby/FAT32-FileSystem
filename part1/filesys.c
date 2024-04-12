#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include "lexer.h"  
#include <ctype.h>



#define MAX_STACK_SIZE 128
#define END_OF_CHAIN 0x0FFFFFFF
#define DIR_ATTR_DIRECTORY 0x10



typedef struct {
    uint16_t bytesPerSector;
    uint8_t sectorsPerCluster;
    uint32_t totalSectors;
    uint32_t FATSize; // Sectors per FAT
    uint32_t rootCluster;
    uint16_t reservedSectors; // Number of reserved sectors
    uint8_t numFATs;          // Number of FATs
    uint32_t firstDataSector; // Calculated first data sector
} FAT32BootSector;

typedef struct __attribute__((packed)) directory_entry {
    char DIR_Name[11];
    uint8_t DIR_Attr;
    char padding_1[8]; // Placeholder for unused fields
    uint16_t DIR_FstClusHI;
    char padding_2[4]; // Placeholder for unused fields
    uint16_t DIR_FstClusLO;
    uint32_t DIR_FileSize;
} dentry_t;

// Global FAT32 boot sector
FAT32BootSector bs;

// Global variable to store the current directory cluster
uint32_t currentDirectoryCluster;

// File pointer to the FAT32 image
FILE *fp;

// Function prototypes
int mountImage(const char *imageName);
void printInfo();
uint32_t clusterToSector(uint32_t cluster);
void readCluster(uint32_t clusterNumber, uint8_t *buffer);
uint32_t readFATEntry(uint32_t clusterNumber);
void dbg_print_dentry(dentry_t *dentry);
uint32_t findDirectoryCluster(const char *dirName);
void processCommand(tokenlist *tokens);
//part 3 
void makeDirectory(const char* dirName);
int directoryExists(const char* name, uint32_t parentCluster);
uint32_t findFreeCluster();
void updateFAT(uint32_t cluster, uint32_t nextCluster);
uint32_t simulateAllocateCluster(void);
int simulateCreateDirectoryEntry(const char* dirName, uint32_t cluster, uint32_t parentCluster);
void simulateInitializeDirectory(uint32_t newCluster, uint32_t parentCluster);
void createDotEntries(uint32_t cluster, uint32_t parentCluster);
void printInfo();
uint32_t findFreeCluster();
void updateFAT(uint32_t cluster, uint32_t value);
int writeToCluster(uint32_t cluster, const void* buffer, size_t size);
int  allocateNewClusterForDirectory(uint32_t parentCluster);
int findEmptyEntryInDirectory(uint32_t cluster, dentry_t* entry);
void to83Filename(const char* input, char* output);
size_t safeFwrite(const void *ptr, size_t size, size_t count, FILE *stream);
int createDirectoryEntry(uint32_t parentCluster, const char *dirName, uint32_t newCluster);
int initializeDirectoryCluster(uint32_t cluster, uint32_t parentCluster);
int writeDirectoryEntry(uint32_t parentCluster, const dentry_t* newEntry);




// Directory stack for navigating directories
typedef struct {
    char *directoryPath[MAX_STACK_SIZE];
    int size;
    uint32_t clusterNumber[MAX_STACK_SIZE];
} DirectoryStack;

DirectoryStack dirStack;

// Initialize the directory stack
void initDirStack() {
    dirStack.size = 0;
    for (int i = 0; i < MAX_STACK_SIZE; ++i) {
        dirStack.directoryPath[i] = NULL;
    }
}

// Push a directory onto the stack
void pushDir(const char *dir, uint32_t clusternum) {
    if (dirStack.size < MAX_STACK_SIZE) {
        dirStack.directoryPath[dirStack.size] = strdup(dir); // Copy the string
        dirStack.clusterNumber[dirStack.size] = clusternum;
        dirStack.size++;
    }
}

// Pop a directory from the stack
char *popDir() {
    if (dirStack.size > 0) {
        dirStack.size--;
        return dirStack.directoryPath[dirStack.size];
    }
    return NULL;
}

// Free resources allocated for the stack
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


char *get_input(void) {
    char *buffer = NULL;
    size_t bufsize = 0;
    char line[5];
    while (fgets(line, sizeof(line), stdin)) {
        size_t len = strcspn(line, "\n");
        if (len < sizeof(line) - 1) {
            buffer = realloc(buffer, bufsize + len + 1);
            memcpy(buffer + bufsize, line, len);
            buffer[bufsize + len] = '\0';
            break;
        } else {
            buffer = realloc(buffer, bufsize + sizeof(line) - 1);
            memcpy(buffer + bufsize, line, sizeof(line) - 1);
            bufsize += sizeof(line) - 1;
        }
    }
    return buffer;
}

tokenlist *get_tokens(char *input) {
    char *buf = strdup(input);
    tokenlist *tokens = new_tokenlist();
    char *tok = strtok(buf, " ");
    while (tok) {
        add_token(tokens, tok);
        tok = strtok(NULL, " ");
    }
    free(buf);
    return tokens;
}

tokenlist *new_tokenlist(void) {
    tokenlist *tokens = malloc(sizeof(tokenlist));
    tokens->size = 0;
    tokens->items = malloc(sizeof(char *));
    tokens->items[0] = NULL;
    return tokens;
}

void add_token(tokenlist *tokens, char *item) {
    int i = tokens->size;
    tokens->items = realloc(tokens->items, (i + 2) * sizeof(char *));
    tokens->items[i] = strdup(item);
    tokens->items[i + 1] = NULL;
    tokens->size++;
}

void free_tokens(tokenlist *tokens) {
    for (int i = 0; i < tokens->size; i++) {
        free(tokens->items[i]);
    }
    free(tokens->items);
    free(tokens);
}

int mountImage(const char *imageName) {
    fp = fopen(imageName, "rb");
    if (!fp) {
        perror("Error opening image file");
        return -1;
    }

    fseek(fp, 11, SEEK_SET);
    fread(&bs.bytesPerSector, sizeof(bs.bytesPerSector), 1, fp);
    fseek(fp, 13, SEEK_SET);
    fread(&bs.sectorsPerCluster, sizeof(bs.sectorsPerCluster), 1, fp);
    fseek(fp, 14, SEEK_SET);
    fread(&bs.reservedSectors, sizeof(bs.reservedSectors), 1, fp);
    fseek(fp, 16, SEEK_SET);
    fread(&bs.numFATs, sizeof(bs.numFATs), 1, fp);
    fseek(fp, 32, SEEK_SET);
    fread(&bs.totalSectors, sizeof(bs.totalSectors), 1, fp);
    fseek(fp, 36, SEEK_SET);
    fread(&bs.FATSize, sizeof(bs.FATSize), 1, fp);
    fseek(fp, 44, SEEK_SET);
    fread(&bs.rootCluster, sizeof(bs.rootCluster), 1, fp);

    bs.firstDataSector = bs.reservedSectors + (bs.numFATs * bs.FATSize);
    currentDirectoryCluster = bs.rootCluster;
    return 0;
}

void printInfo() {
    printf("Bytes Per Sector: %d\n", bs.bytesPerSector);
    printf("Sectors Per Cluster: %d\n", bs.sectorsPerCluster);
    printf("Root Cluster: %d\n", bs.rootCluster);
    uint64_t totalClusters = (bs.totalSectors - bs.firstDataSector) / bs.sectorsPerCluster;
    printf("Total # of Clusters in Data Region: %llu\n", totalClusters);
    printf("Size of Image (in bytes): %llu\n", (uint64_t)bs.totalSectors * bs.bytesPerSector);
}

uint32_t clusterToSector(uint32_t cluster) {
    return ((cluster - 2) * bs.sectorsPerCluster) + bs.firstDataSector;
}

void readCluster(uint32_t clusterNumber, uint8_t *buffer) {
    uint32_t sectorNumber = clusterToSector(clusterNumber);
    fseek(fp, sectorNumber * bs.bytesPerSector, SEEK_SET);
    fread(buffer, bs.sectorsPerCluster, bs.bytesPerSector, fp);
}

uint32_t readFATEntry(uint32_t clusterNumber) {
    uint32_t fatSectorNumber = bs.reservedSectors + (clusterNumber * 4 / bs.bytesPerSector);
    uint32_t entryOffset = (clusterNumber * 4) % bs.bytesPerSector;
    fseek(fp, (fatSectorNumber * bs.bytesPerSector) + entryOffset, SEEK_SET);
    uint32_t nextCluster;
    fread(&nextCluster, sizeof(nextCluster), 1, fp);
    nextCluster &= 0x0FFFFFFF; // Last 28 bits
    return nextCluster;
}

uint32_t findDirectoryCluster(const char *dirName) {
    uint8_t *clusterBuffer = malloc(bs.bytesPerSector * bs.sectorsPerCluster);
    if (!clusterBuffer) {
        printf("Memory allocation failed for cluster buffer.\n");
        return 0;
    }

    printf("Searching for directory: %s\n", dirName);
    if (strcmp(dirName, "..") == 0) {
        // Handle ".." to navigate to the parent directory
        if (dirStack.size > 1) {
            uint32_t parentCluster = dirStack.clusterNumber[dirStack.size - 2];
            free(clusterBuffer);
            return parentCluster;
        } else {
            printf("Already at root, cannot go back further.\n");
            free(clusterBuffer);
            return bs.rootCluster; // Root directory's cluster
        }
    } else if (strcmp(dirName, ".") == 0) {
        // Handle "." to stay in the current directory
        free(clusterBuffer);
        return currentDirectoryCluster;
    }

    // Convert dirName to uppercase for comparison
    char dirNameUpper[12];
    strncpy(dirNameUpper, dirName, 11);
    dirNameUpper[11] = '\0';
    for (char *p = dirNameUpper; *p; ++p) *p = toupper(*p);

    uint32_t currentCluster = currentDirectoryCluster;
    while (currentCluster < 0x0FFFFFF8) {
        readCluster(currentCluster, clusterBuffer);
        dentry_t *dirEntry = (dentry_t *)clusterBuffer;
        for (int i = 0; i < bs.bytesPerSector * bs.sectorsPerCluster / sizeof(dentry_t); i++, dirEntry++) {
            if (dirEntry->DIR_Name[0] == 0) break; // End of directory entries

            if (dirEntry->DIR_Name[0] == 0xE5 || (dirEntry->DIR_Attr & 0x0F) == 0x0F) continue; // Skip deleted entries and LFN entries

            // Format the directory name for comparison
            char formattedName[12];
            memset(formattedName, ' ', 11);
            formattedName[11] = '\0';
            for (int j = 0; j < 11; j++) {
                if (dirEntry->DIR_Name[j] == ' ') break; // Stop at first space
                formattedName[j] = toupper(dirEntry->DIR_Name[j]);
            }

            if (strncmp(formattedName, dirNameUpper, 11) == 0) {
                uint32_t foundCluster = ((uint32_t)dirEntry->DIR_FstClusHI << 16) | dirEntry->DIR_FstClusLO;
                printf("Found directory %s at cluster %u.\n", dirName, foundCluster);
                free(clusterBuffer);
                return foundCluster;
            }
        }

        // Move to the next cluster in the directory's cluster chain
        currentCluster = readFATEntry(currentCluster);
    }

    free(clusterBuffer);
    return 0; // Directory not found
}

void dbg_print_dentry(dentry_t *dentry) {
    if (!dentry) return;
    printf("DIR_Name: %.11s\n", dentry->DIR_Name);
    printf("DIR_Attr: %02X\n", dentry->DIR_Attr);
    printf("DIR_FstClusHI: %04X\n", dentry->DIR_FstClusHI);
    printf("DIR_FstClusLO: %04X\n", dentry->DIR_FstClusLO);
    printf("DIR_FileSize: %u\n", dentry->DIR_FileSize);
}

void listDirectory(uint32_t cluster) {
    printf("Listing directory at cluster: %u\n", cluster);
    uint8_t *clusterBuffer = malloc(bs.bytesPerSector * bs.sectorsPerCluster);
    if (!clusterBuffer) {
        printf("Failed to allocate memory for directory listing.\n");
        return;
    }

    while (cluster < 0x0FFFFFF8) {
        readCluster(cluster, clusterBuffer);
        dentry_t *dirEntry = (dentry_t *)clusterBuffer;
        for (int i = 0; i < bs.bytesPerSector * bs.sectorsPerCluster / sizeof(dentry_t); i++, dirEntry++) {
            if (dirEntry->DIR_Name[0] == 0) break; // End of directory entries

            if (dirEntry->DIR_Name[0] == 0xE5 || (dirEntry->DIR_Attr & 0x0F) == 0x0F) continue; // Skip deleted entries and LFN entries

            // Format the directory name for printing
            char formattedName[12];
            memset(formattedName, ' ', 11);
            formattedName[11] = '\0';
            for (int j = 0; j < 11; j++) {
                if (dirEntry->DIR_Name[j] == ' ') break; // Stop at first space
                formattedName[j] = dirEntry->DIR_Name[j];
            }

            printf("%s\n", formattedName);
        }

        // Move to the next cluster in the directory's cluster chain
        cluster = readFATEntry(cluster);
    }

    free(clusterBuffer);
}

uint32_t findFreeCluster() {
    // Calculate the total number of clusters in the filesystem
    uint64_t totalClusters = bs.totalSectors / bs.sectorsPerCluster;

    // Start at cluster 2 (the first data cluster)
    for (uint32_t cluster = 2; cluster < totalClusters; cluster++) {
        uint32_t fatEntry = readFATEntry(cluster);
        if (fatEntry == 0x00000000) { // Check if the cluster is free
            return cluster; // Free cluster found
        }
    }
    return 0; // No free cluster found, return 0 as an error indicator
}

void updateFAT(uint32_t cluster, uint32_t value) {
    // Calculate the FAT entry's byte offset
    uint32_t fatOffset = bs.reservedSectors * bs.bytesPerSector + cluster * 4;
    fseek(fp, fatOffset, SEEK_SET);
    fwrite(&value, sizeof(uint32_t), 1, fp);
}
uint32_t simulateAllocateCluster() {
    uint32_t freeCluster = findFreeCluster();
    if (freeCluster != 0) {
        updateFAT(freeCluster, 0x0FFFFFFF); // Mark the cluster as used by setting it to the end-of-chain marker
        return freeCluster;
    } else {
        printf("No free clusters available.\n");
        return 0;
    }
}

int allocateNewClusterForDirectory(uint32_t parentCluster) {
    uint32_t newCluster = findFreeCluster();
    if (newCluster == 0) {
        printf("No free clusters available.\n");
        return 0; // Failure
    }
    updateFAT(newCluster, 0x0FFFFFFF); // Mark the new cluster as the end of the directory
    updateFAT(parentCluster, newCluster); // Link the parent cluster to the new one
    return newCluster;
}

int findEmptyEntryInDirectory(uint32_t cluster, dentry_t* entry) {
    uint8_t buffer[bs.bytesPerSector * bs.sectorsPerCluster];
    while (cluster < 0x0FFFFFF8) { // Valid clusters
        readCluster(cluster, buffer);
        dentry_t* entries = (dentry_t*)buffer;
        int entriesPerCluster = (bs.bytesPerSector * bs.sectorsPerCluster) / sizeof(dentry_t);
        for (int i = 0; i < entriesPerCluster; i++) {
            if (entries[i].DIR_Name[0] == 0x00 || entries[i].DIR_Name[0] == 0xE5) { // Empty or deleted entry
                memcpy(entry, &entries[i], sizeof(dentry_t));
                return cluster;
            }
        }
        cluster = readFATEntry(cluster); // Move to the next cluster in the chain
    }
    return 0; // No empty entry found
}

void createDirectory(const char* dirName) {
    if (directoryExists(dirName, currentDirectoryCluster)) {
        printf("Error: Directory %s already exists.\n", dirName);
        return;
    }

    uint32_t cluster = findFreeCluster();
    if (cluster == 0) {
        printf("Error: No free clusters available.\n");
        return;
    }

    simulateCreateDirectoryEntry(dirName, cluster, currentDirectoryCluster);
    printf("Directory %s created successfully.\n", dirName);
}

void createDotEntries(uint32_t cluster, uint32_t parentCluster) {
    dentry_t dotEntry, dotDotEntry;

    // Initialize the "." entry
    memset(&dotEntry, 0, sizeof(dotEntry));
    strcpy(dotEntry.DIR_Name, ".          "); // 11 spaces to fill the rest
    dotEntry.DIR_Attr = 0x10; // Directory attribute
    dotEntry.DIR_FstClusHI = (cluster >> 16) & 0xFFFF;
    dotEntry.DIR_FstClusLO = cluster & 0xFFFF;
    dotEntry.DIR_FileSize = 0;

    // Initialize the ".." entry
    memset(&dotDotEntry, 0, sizeof(dotDotEntry));
    strcpy(dotDotEntry.DIR_Name, "..         "); // 11 spaces to fill the rest
    dotDotEntry.DIR_Attr = 0x10; // Directory attribute
    dotDotEntry.DIR_FstClusHI = (parentCluster >> 16) & 0xFFFF;
    dotDotEntry.DIR_FstClusLO = parentCluster & 0xFFFF;
    dotDotEntry.DIR_FileSize = 0;

    // Write these entries to the start of the cluster allocated for the new directory
    uint8_t buffer[bs.bytesPerSector * bs.sectorsPerCluster];
    memset(buffer, 0, sizeof(buffer));
    memcpy(buffer, &dotEntry, sizeof(dotEntry));
    memcpy(buffer + sizeof(dotEntry), &dotDotEntry, sizeof(dotDotEntry));

    writeToCluster(cluster, buffer, bs.bytesPerSector * bs.sectorsPerCluster);
}

int writeToCluster(uint32_t cluster, const void* buffer, size_t size) {
    uint32_t sectorNumber = clusterToSector(cluster);
    fseek(fp, sectorNumber * bs.bytesPerSector, SEEK_SET); // Navigate to the correct sector
    fwrite(buffer, size, 1, fp); // Write the buffer to the disk image

    // Ensure changes are written immediately
    fflush(fp);
}

int simulateCreateDirectoryEntry(const char* dirName, uint32_t cluster, uint32_t parentCluster) {
    dentry_t newDirEntry;
    memset(&newDirEntry, 0, sizeof(newDirEntry));
    char formattedName[12];
    to83Filename(dirName, formattedName); // Convert to FAT 8.3 format
    memcpy(newDirEntry.DIR_Name, formattedName, 11);
    newDirEntry.DIR_Attr = 0x10; // Directory attribute
    newDirEntry.DIR_FstClusHI = (cluster >> 16) & 0xFFFF;
    newDirEntry.DIR_FstClusLO = cluster & 0xFFFF;
    newDirEntry.DIR_FileSize = 0; // Directory size is 0

    // Initialize emptyEntry to be passed to findEmptyEntryInDirectory
    dentry_t emptyEntry;
    uint32_t directoryCluster = findEmptyEntryInDirectory(parentCluster, &emptyEntry);
    
    // If no empty entry found in any of the clusters, attempt to allocate a new cluster
    if (directoryCluster == 0) { 
        directoryCluster = allocateNewClusterForDirectory(parentCluster);
        // If failed to allocate new cluster, return indicating failure
        if (directoryCluster == 0) return 1; // Indicate failure
    }

    // If an empty entry was found or a new cluster was successfully allocated,
    // assume writeToCluster writes data to the specified cluster successfully.
    writeToCluster(directoryCluster, &newDirEntry, sizeof(newDirEntry));

    // If the code reaches this point, it means the directory entry was successfully created.
    return 0; // Indicate success
}

int directoryExists(const char* name, uint32_t parentCluster) {
    // This is a simplified version. Actual implementation will depend on how you read directories.
    // Assuming you have a function to read a cluster and provide directory entries.
    dentry_t* entries; // Assume this is populated with entries from the parentCluster
    int entryCount ;

    for (int i = 0; i < entryCount; i++) {
        // Assuming dentry_t has fields like DIR_Name and DIR_Attr and a directory attribute constant DIR_ATTR_DIRECTORY
        if ((entries[i].DIR_Attr & DIR_ATTR_DIRECTORY) && strcmp(entries[i].DIR_Name, name) == 0) {
            return 1; // Found directory
        }
    }

    return 0; // Directory not found
}
void simulateInitializeDirectory(uint32_t newCluster, uint32_t parentCluster) {
    dentry_t dot, dotDot;

    memset(&dot, 0, sizeof(dot));
    strcpy(dot.DIR_Name, ".          ");
    dot.DIR_Attr = DIR_ATTR_DIRECTORY;
    dot.DIR_FstClusLO = newCluster & 0xFFFF;
    dot.DIR_FstClusHI = (newCluster >> 16) & 0xFFFF;

    memset(&dotDot, 0, sizeof(dotDot));
    strcpy(dotDot.DIR_Name, "..         ");
    dotDot.DIR_Attr = DIR_ATTR_DIRECTORY;
    dotDot.DIR_FstClusLO = parentCluster & 0xFFFF;
    dotDot.DIR_FstClusHI = (parentCluster >> 16) & 0xFFFF;

    // Write these entries to the start of the newCluster. You'll need a function to write to a cluster.
    // This is pseudocode, as actual implementation depends on your project structure.
    writeToCluster(newCluster, &dot, sizeof(dot));
    writeToCluster(newCluster, &dotDot, sizeof(dotDot) /*, Offset might be needed here */);
}
void to83Filename(const char* input, char* output) {
    memset(output, ' ', 11);
    int nameLen = 0;
    for (; input[nameLen] && input[nameLen] != '.' && nameLen < 8; nameLen++) {
        output[nameLen] = toupper(input[nameLen]);
    }
    if (input[nameLen] == '.') {
        int extLen = 0;
        nameLen++; // Skip the dot
        for (; input[nameLen + extLen] && extLen < 3; extLen++) {
            output[8 + extLen] = toupper(input[nameLen + extLen]);
        }
    }
}

void makeDirectory(const char *dirName) {
    // Check if directory already exists in the current directory cluster
    if (directoryExists(dirName, currentDirectoryCluster)) {
        printf("Directory %s already exists.\n", dirName);
        return;
    }

    // Find a free cluster for the new directory
    uint32_t newCluster = findFreeCluster();
    if (newCluster == 0) {
        printf("Unable to find a free cluster for the new directory.\n");
        return;
    }

    // Create the directory entry in the current directory
    if (!createDirectoryEntry(currentDirectoryCluster, dirName, newCluster)) {
        printf("Failed to create directory entry for %s.\n", dirName);
        return;
    }

    // Initialize the new directory cluster (e.g., setting up '.' and '..' entries)
    if (!initializeDirectoryCluster(newCluster, currentDirectoryCluster)) {
        printf("Failed to initialize the new directory cluster for %s.\n", dirName);
        return;
    }

    printf("Directory %s created successfully.\n", dirName);
}

int createDirectoryEntry(uint32_t parentCluster, const char *dirName, uint32_t newCluster) {
    // Convert dirName to FAT32 8.3 format
    char fatName[11];
    to83Filename(dirName, fatName); // Implement this function based on FAT32 naming conventions

    dentry_t newEntry; // Assuming dentry_t is your directory entry structure
    memset(&newEntry, 0, sizeof(newEntry));
    memcpy(newEntry.DIR_Name, fatName, 11); // Copy the formatted name
    newEntry.DIR_Attr = 0x10; // Directory attribute
    newEntry.DIR_FstClusLO = newCluster & 0xFFFF;
    newEntry.DIR_FstClusHI = (newCluster >> 16) & 0xFFFF;
    newEntry.DIR_FileSize = 0; // Directory entries have a file size of 0

    // Find a free directory entry in the parent cluster and write the new entry
    // This part depends on your implementation of reading and writing clusters
    // For simplicity, we're assuming a function 'writeDirectoryEntry' exists
    if (!writeDirectoryEntry(parentCluster, &newEntry)) {
        return 0; // Failure
    }
    return 1; // Success
}
int writeDirectoryEntry(uint32_t parentCluster, const dentry_t* newEntry) {
    // Placeholder for writing a directory entry to the disk.
    // This should interact with the disk I/O functions to place `newEntry` into `parentCluster`.
    // You'll need to calculate the exact location within the cluster to write the entry.

    // Example: Writing to the first available or specified position within the cluster
    uint8_t *buffer = malloc(bs.bytesPerSector * bs.sectorsPerCluster);
    if (!buffer) {
        perror("Memory allocation failed for buffer");
        return 0;
    }

    readCluster(parentCluster, buffer);  // Read the current content of the cluster
    memcpy(buffer /* + offset */, newEntry, sizeof(dentry_t));  // Copy new entry into the buffer at the correct offset
    int result = writeToCluster(parentCluster, buffer, bs.bytesPerSector * bs.sectorsPerCluster); // Write back to the cluster

    free(buffer);
    return result; // Return 1 for success, 0 for failure
}


int initializeDirectoryCluster(uint32_t cluster, uint32_t parentCluster) {
    dentry_t dotEntry, dotDotEntry;
    memset(&dotEntry, 0, sizeof(dotEntry));
    memset(&dotDotEntry, 0, sizeof(dotDotEntry));

    // Setting up the '.' entry
    strcpy(dotEntry.DIR_Name, ".          ");
    dotEntry.DIR_Attr = 0x10;
    dotEntry.DIR_FstClusLO = cluster & 0xFFFF;
    dotEntry.DIR_FstClusHI = (cluster >> 16) & 0xFFFF;

    // Setting up the '..' entry
    strcpy(dotDotEntry.DIR_Name, "..         ");
    dotDotEntry.DIR_Attr = 0x10;
    dotDotEntry.DIR_FstClusLO = parentCluster & 0xFFFF;
    dotDotEntry.DIR_FstClusHI = (parentCluster >> 16) & 0xFFFF;

    // Assuming 'writeToCluster' writes the given data to the specified cluster
    // You might need to implement this based on how you handle disk I/O
    uint8_t buffer[512]; // Assuming sector size is 512 bytes
    memset(buffer, 0, 512);
    memcpy(buffer, &dotEntry, sizeof(dotEntry));
    memcpy(buffer + sizeof(dotEntry), &dotDotEntry, sizeof(dotDotEntry));

    if (!writeToCluster(cluster, (uint8_t*)buffer, (sizeof(dotEntry) * 2))) {
        return 0; // Failure
    }
    return 1; // Success
}


void processCommand(tokenlist *tokens) {
    if (tokens->size == 0)
        return;

    if (strcmp(tokens->items[0], "info") == 0) {
        printInfo();
    } else if (strcmp(tokens->items[0], "cd") == 0 && tokens->size > 1) {
        uint32_t newDirCluster = findDirectoryCluster(tokens->items[1]);
        if (newDirCluster) {
            currentDirectoryCluster = newDirCluster;
            printf("Changed directory to %s\n", tokens->items[1]);
            if (strcmp(tokens->items[1], "..") != 0 && strcmp(tokens->items[1], ".") != 0) {
                pushDir(tokens->items[1], newDirCluster); // Add directory to stack
            } else if (strcmp(tokens->items[1], "..") == 0) {
                // Handling ".." to move to parent directory
                popDir(); // Remove current directory from stack
            }
            // No need to handle "." since it means stay in the current directory
        } else {
            printf("Directory not found: %s\n", tokens->items[1]);
        }
    } else if (strcmp(tokens->items[0], "ls") == 0) {
        listDirectory(currentDirectoryCluster);
    } else if (strcmp(tokens->items[0], "mkdir") == 0 && tokens->size > 1) {
        // Assumes you have a function makeDirectory that takes a directory name and the current directory cluster
        makeDirectory(tokens->items[1]);
    } else if (strcmp(tokens->items[0], "exit") == 0) {
        exit(0); // Exits the program
    } else {
        printf("Unknown command: %s\n", tokens->items[0]);
    }
}
