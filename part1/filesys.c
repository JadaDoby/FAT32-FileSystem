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
#define MAX_CLUSTERS  
typedef struct { 
    uint16_t bytesPerSector;
    uint8_t sectorsPerCluster;
    uint32_t totalSectors;
    uint32_t FATSize; // Sectors per FAT
    uint32_t rootCluster;
    uint16_t reservedSectors; // Added: Number of reserved sectors
    uint8_t numFATs;          // Added: Number of FATs
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

FAT32BootSector bs;
uint32_t currentDirectoryCluster; // Global variable to store the current directory cluster
int fd;  // File descriptor for the FAT32 image

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
void formatNameToFAT(const char *inputName, char *formattedName);
int writeDirectoryEntry(uint32_t parentCluster, const char* name, uint32_t cluster, uint8_t attr);
int writeEntryToDisk(uint32_t parentCluster, const uint8_t* entry);
void writeFATEntry(uint32_t clusterNumber, uint32_t value);
int initDirectoryCluster(uint32_t newCluster, uint32_t parentCluster);
int updateParentDirectory(uint32_t parentCluster, const char *dirName, uint32_t newCluster);
void clearCluster(uint32_t clusterNumber);
uint32_t clusterToSector(uint32_t cluster);
bool is_8_3_format_directory(const char* name);
int createFile(const char* filename);
void formatEntryName(const char rawName[11], char formattedName[13]);
void formatFAT32Name(const char* inputName, char* formattedName);
int linkClusterToDirectory(uint32_t lastCluster, uint32_t newCluster);
int addDirectoryEntry(uint32_t parentCluster, const char *entryName, uint8_t attr);
int isDirectoryFull(uint32_t cluster);

typedef struct {
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
    pread(fd, buffer, bs.bytesPerSector * bs.sectorsPerCluster, firstSector * bs.bytesPerSector);
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

    while (cluster < 0x0FFFFFF8) {  // End of chain marker for FAT32
        readCluster(cluster, buffer);
        dentry_t *entry = (dentry_t *)buffer;

        for (int i = 0; i < (bs.bytesPerSector * bs.sectorsPerCluster) / sizeof(dentry_t); i++, entry++) {
            if (entry->DIR_Name[0] == 0x00) {
                break;  // No more entries
            }
            if (entry->DIR_Name[0] == 0xE5) {
                continue;  // Entry is free (deleted file)
            }
            if ((entry->DIR_Attr & 0x0F) == 0x0F) {
                continue;  // Skip long name entries
            }

            // Format the file/directory name correctly
            char name[13];
            formatEntryName(entry->DIR_Name, name);
            printf("%s\n", name);
        }

        // Read the next cluster number from the FAT
        cluster = readFATEntry(cluster);
    }

    free(buffer);
}

void formatEntryName(const char rawName[11], char formattedName[13]) {
    int nameLength = 0;
    for (int i = 0; i < 8; i++) {  // Format the main part of the filename
        if (rawName[i] != ' ') {
            formattedName[nameLength++] = rawName[i];
        } else {
            break;
        }
    }
    if (rawName[8] != ' ') {  // Check if there is an extension
        formattedName[nameLength++] = '.';
        for (int i = 8; i < 11; i++) {
            if (rawName[i] != ' ') {
                formattedName[nameLength++] = rawName[i];
            } else {
                break;
            }
        }
    }
    formattedName[nameLength] = '\0';  // Null terminate the string
}
int isDirectoryFull(uint32_t cluster) {
    uint8_t buffer[bs.bytesPerSector * bs.sectorsPerCluster];
    uint32_t nextCluster = cluster;
    int entriesPerCluster = (bs.bytesPerSector * bs.sectorsPerCluster) / ENTRY_SIZE;

    while (nextCluster < 0x0FFFFFF8) {
        if (pread(fd, buffer, sizeof(buffer), clusterToSector(nextCluster) * bs.bytesPerSector) != sizeof(buffer)) {
            perror("Failed to read cluster");
            return -1; // Error reading the cluster
        }

        for (int i = 0; i < entriesPerCluster; i++) {
            if (buffer[i * ENTRY_SIZE] == 0x00 || buffer[i * ENTRY_SIZE] == 0xE5) {
                return 0; // Found a free entry
            }
        }

        nextCluster = readFATEntry(nextCluster); // Get the next cluster in the chain
    }

    return 1; // No free entries found, directory is full
}
int addDirectoryEntry(uint32_t parentCluster, const char *entryName, uint8_t attr) {
    if (isDirectoryFull(parentCluster)) {
        // Try to allocate a new cluster for this directory
        uint32_t newCluster = allocateCluster();
        if (newCluster == 0) {
            printf("No free clusters available to expand the directory.\n");
            return -2; // No clusters available
        }
        if (!linkClusterToDirectory(parentCluster, newCluster)) {
            printf("Failed to link new cluster to directory.\n");
            return -3; // Failed to link new cluster
        }
        parentCluster = newCluster; // Use the new cluster for the entry
    }

    // Proceed to add the entry to the identified cluster
    return writeDirectoryEntry(parentCluster, entryName, attr, 0);
}

int linkClusterToDirectory(uint32_t lastCluster, uint32_t newCluster) {
    printf("Linking cluster %u to %u\n", lastCluster, newCluster);
    writeFATEntry(lastCluster, newCluster);  // Link the new cluster
    writeFATEntry(newCluster, 0x0FFFFFFF);  // Mark as the last cluster
    return 1;
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

void formatNameToFAT(const char* inputName, char* formattedName) {
    int nameLen = 0, extLen = 0;
    const char* ext = strchr(inputName, '.');

    if (ext != NULL) {
        nameLen = ext - inputName;
        extLen = strlen(ext + 1);
    } else {
        nameLen = strlen(inputName);
    }

    int i;
    for (i = 0; i < 8; i++) {
        if (i < nameLen) {
            formattedName[i] = toupper(inputName[i]);
        } else {
            formattedName[i] = ' ';
        }
    }
    for (i = 8; i < 11; i++) {
        if (i - 8 < extLen) {
            formattedName[i] = toupper(ext[i - 7]);
        } else {
            formattedName[i] = ' ';
        }
    }
    formattedName[11] = '\0'; // Null-terminate for safety
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

// void clearCluster(uint32_t clusterNumber) {
//     uint32_t sector = clusterToSector(clusterNumber);
//     uint8_t buffer[bs.bytesPerSector * bs.sectorsPerCluster];
//     memset(buffer, 0, sizeof(buffer));
//     for (int i = 0; i < bs.sectorsPerCluster; i++) {
//         pwrite(fd, buffer, bs.bytesPerSector, (sector + i) * bs.bytesPerSector);
//     }
// }
void processCommand(tokenlist *tokens) {
    if (tokens->size == 0) return;

    if (strcmp(tokens->items[0], "info") == 0) {
        printInfo();
    } else if (strcmp(tokens->items[0], "cd") == 0 && tokens->size > 1) {
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
    } else if (strcmp(tokens->items[0], "ls") == 0) {
        listDirectory(currentDirectoryCluster);
    } else if (strcmp(tokens->items[0], "mkdir") == 0 && tokens->size > 1) {
        int result = createDirectory(tokens->items[1]);
        if (result == 0) {
            printf("Directory created: %s\n", tokens->items[1]);
        } else if (result == -1) {
            printf("File already exists: %s\n", tokens->items[1]);
        } else if (result == -2) {
            printf("Directory is full and cannot add new directory: %s\n", tokens->items[1]);
        } else if (result == -3) {
            printf("Failed to write directory entry for unknown reasons: %s\n", tokens->items[1]);
        } else {
            printf("An unexpected error occurred: %s\n", tokens->items[1]);
        }
    } else if (strcmp(tokens->items[0], "creat") == 0 && tokens->size > 1) {
        int result = createFile(tokens->items[1]);
        if (result == 0) {
            printf("File created: %s\n", tokens->items[1]);
        } else if (result == -1) {
            printf("File already exists: %s\n", tokens->items[1]);
        } else if (result == -2) {
            printf("Directory is full and cannot add new file: %s\n", tokens->items[1]);
        } else if (result == -3) {
            printf("Failed to write directory entry for unknown reasons: %s\n", tokens->items[1]);
        } else {
            printf("An unexpected error occurred: %s\n", tokens->items[1]);
        }
    } else if (strcmp(tokens->items[0], "exit") == 0) {
        printf("Exiting program.\n");
        exit(0);  // Terminate the program cleanly
    } else {
        printf("Unknown command.\n");
    }
}

int createDirectory(const char *dirName) {
    if (!is_8_3_format_directory(dirName)) {
        printf("Error: Directory name '%s' is not in FAT32 8.3 format.\n", dirName);
        return -1;
    }
    uint32_t existingCluster = findDirectoryCluster(dirName);
    if (existingCluster != 0) {
        printf("Error: Directory '%s' already exists.\n", dirName);
        return -1;  // Indicate failure
    }
    uint32_t newCluster = allocateCluster();
    if (newCluster == 0) {
        printf("Error: No free clusters available.\n");
        return -1;  // Indicate failure
    }
    if (initDirectoryCluster(newCluster, currentDirectoryCluster) != 0) {
        printf("Error: Failed to initialize the new directory cluster.\n");
        return -1;  // Indicate failure
    }
    if (updateParentDirectory(currentDirectoryCluster, dirName, newCluster) != 0) {
        printf("Error: Failed to update the parent directory.\n");
        return -1;  // Indicate failure
    }
    printf("Directory '%s' created successfully.\n", dirName);
    return 0;  // Success
}

bool is_8_3_format_directory(const char* name) {
    if (!name) return false;
    size_t len = strlen(name);
    if (len > 11) return false;
    for (size_t i = 0; i < len; i++) {
        if (!(isalnum(name[i]) || name[i] == '_' || name[i] == '-')) return false;
    }
    return true;
}

int initDirectoryCluster(uint32_t newCluster, uint32_t parentCluster) {
    // Clear the new cluster
    clearCluster(newCluster);
    // Create '.' and '..' entries
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
            break;
        }
    }
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
    for (clusterNumber = 2; clusterNumber < bs.totalSectors / bs.sectorsPerCluster; clusterNumber++) {
        nextCluster = readFATEntry(clusterNumber);
        if (nextCluster == 0) {  // 0 indicates a free cluster
            writeFATEntry(clusterNumber, 0x0FFFFFFF);  // Mark the cluster as end of chain
            printf("Allocating new cluster: %u\n", clusterNumber);
            return clusterNumber;
        }
    }
    return 0;  // Return 0 if no free cluster found
}

void formatFAT32Name(const char* inputName, char* formattedName) {
    memset(formattedName, ' ', 11);  // Fill with spaces
    int nameLen = 0, extLen = 0;
    const char* ext = strchr(inputName, '.');  // Find extension

    if (ext != NULL) {
        nameLen = ext - inputName;
        extLen = strlen(ext + 1);
    } else {
        nameLen = strlen(inputName);
    }

    // Copy the name part
    for (int i = 0; i < nameLen && i < 8; i++) {
        formattedName[i] = toupper((unsigned char)inputName[i]);
    }
    // Copy the extension part
    for (int i = 0; i < extLen && i < 3; i++) {
        formattedName[8 + i] = toupper((unsigned char)ext[i + 1]);
    }
    formattedName[11] = '\0';  // Null-termination is not necessary but safe for debugging
}

uint32_t findDirectoryCluster(const char *dirName) {
    char formattedName[12]; // Array to hold the FAT32 formatted name
    formatNameToFAT(dirName, formattedName); // Ensure the name is formatted to the FAT32 8.3 standard

    uint8_t *buffer = malloc(bs.bytesPerSector * bs.sectorsPerCluster);
    if (!buffer) {
        printf("Memory allocation failed\n");
        return 0;
    }

    printf("Searching for directory/file: %s\n", formattedName);
    uint32_t cluster = currentDirectoryCluster;
    do {
        readCluster(cluster, buffer);
        dentry_t *dentry = (dentry_t *)buffer;
        for (int i = 0; i < bs.bytesPerSector * bs.sectorsPerCluster / sizeof(dentry_t); i++, dentry++) {
            if (dentry->DIR_Name[0] == 0) {
                printf("Reached end of directory entries without finding '%s'.\n", dirName);
                break; // End of directory entries
            }
            if (dentry->DIR_Name[0] == 0xE5) continue; // Skip deleted entry

            if (strncmp(dentry->DIR_Name, formattedName, 11) == 0) {
                free(buffer);
                uint32_t foundCluster = ((uint32_t)dentry->DIR_FstClusHI << 16) | dentry->DIR_FstClusLO;
                printf("Found %s at cluster %u\n", dirName, foundCluster);
                return foundCluster;
            }
        }
        cluster = readFATEntry(cluster);
    } while (cluster < 0x0FFFFFF8);

    free(buffer);
    return 0; // No such directory/file
}


//createFile function
int createFile(const char* filename) {
    uint32_t parentCluster = currentDirectoryCluster;

    // Check if directory is full and try to expand it
    if (isDirectoryFull(parentCluster)) {
        uint32_t newCluster = allocateCluster();
        if (newCluster == 0) {
            printf("No free clusters available.\n");
            return -2;
        }
        if (!linkClusterToDirectory(parentCluster, newCluster)) {
            printf("Failed to link new cluster.\n");
            return -3;
        }
        parentCluster = newCluster;  // Use the new cluster for further operations
    }

    // Now try to create the file in the (possibly new) cluster
    uint32_t fileCluster = allocateCluster();
    if (fileCluster == 0) {
        printf("No free clusters available to create the file.\n");
        return -2;
    }
    int result = writeDirectoryEntry(parentCluster, filename, fileCluster, 0x20);
    if (result == 0) {
        printf("File '%s' created successfully.\n", filename);
        return 0;
    } else {
        printf("Failed to write directory entry for '%s'.\n", filename);
        return result;  // Propagate the specific error from writeDirectoryEntry
    }
}






int writeDirectoryEntry(uint32_t parentCluster, const char* name, uint32_t cluster, uint8_t attr) {
    uint32_t sector = clusterToSector(parentCluster);
    uint8_t buffer[bs.bytesPerSector * bs.sectorsPerCluster];
    pread(fd, buffer, sizeof(buffer), sector * bs.bytesPerSector);

    for (int i = 0; i < sizeof(buffer); i += ENTRY_SIZE) {
        if (buffer[i] == 0x00 || buffer[i] == 0xE5) {  // Check for a free or deleted entry
            printf("Writing new directory entry at cluster %u, index %d\n", parentCluster, i);
            memset(&buffer[i], 0, ENTRY_SIZE);  // Clear the entry space
            formatNameToFAT(name, (char*)&buffer[i]);
            buffer[i + 11] = attr;
            uint16_t hi = (cluster >> 16) & 0xFFFF;
            uint16_t lo = cluster & 0xFFFF;
            memcpy(buffer + i + 20, &hi, sizeof(hi));
            memcpy(buffer + i + 26, &lo, sizeof(lo));
            pwrite(fd, buffer, sizeof(buffer), sector * bs.bytesPerSector);
            return 0;  // Success
        }
    }
    return -1;  // No space in directory entry table
}

