#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include "lexer.h"

typedef struct {
    uint16_t bytesPerSector;
    uint8_t sectorsPerCluster;
    uint32_t totalSectors;
    uint32_t FATSize; // Sectors per FAT
    uint32_t rootCluster;
    uint16_t reservedSectors; // Added: Number of reserved sectors
    uint8_t numFATs; // Added: Number of FATs
} FAT32BootSector;

FAT32BootSector bs;
uint32_t currentDirectoryCluster; // Global variable to store the current directory cluster

// Function prototypes
int mountImage(const char *imageName);
void printInfo();
void changeDirectory(const char* dirName);


int mountImage(const char *imageName) {
    FILE *fp = fopen(imageName, "rb");
    if (!fp) {
        perror("Error opening image file");
        return -1;
    }

    // Read bytes per sector
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

    fclose(fp);
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

int processCommand(tokenlist *tokens)
{
    if (tokens->size == 0)
        return -1;

    if (strcmp(tokens->items[0], "info") == 0)
    {
        printInfo();
    }
    else if (strcmp(tokens->items[0], "cd") == 0 && tokens->size > 1)
    {
       // changeDirectory(tokens->items[1]);
    }
    else
    {
        printf("Unknown command.\n");
    }

    return 0;
}

void changeDirectory(const char *dirName){
    
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

    printf("%s/> ", argv[1]);
    char *input = get_input();
    while (strcmp(input, "exit") != 0)
    {
        tokenlist *tokens = get_tokens(input);
        processCommand(tokens);
        free_tokens(tokens);
        free(input);
        printf("%s/> ", argv[1]);
        input = get_input();
    }
    free(input);
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