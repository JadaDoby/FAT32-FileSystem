#ifndef FILESYSFUNC_H
#define FILESYSFUNC_H

#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h> // Include for open
#include <ctype.h>

#define MAX_STACK_SIZE 128
#define ATTR_DIRECTORY 0x10
#define ENTRY_SIZE 32
#define MAX_OPEN_FILES 16

typedef struct
{
    uint16_t bytesPerSector;
    uint8_t sectorsPerCluster;
    uint32_t totalSectors;
    uint32_t FATSize;
    uint32_t rootCluster;
    uint16_t reservedSectors;
    uint8_t numFATs;
    uint32_t firstDataSector;
} FAT32BootSector;

typedef struct __attribute__((packed)) directory_entry
{
    char DIR_Name[11];
    uint8_t DIR_Attr;
    char padding_1[8];
    uint16_t DIR_FstClusHI;
    char padding_2[4];
    uint16_t DIR_FstClusLO;
    uint32_t DIR_FileSize;
} dentry_t;

typedef struct
{
    char filename[12];
    char mode[4];
    int offset;
    int isOpeninuse;
    uint32_t cluster;
} OpenFile;

typedef struct
{
    char *directoryPath[MAX_STACK_SIZE];
    int size;
    uint32_t clusterNumber[MAX_STACK_SIZE];
} DirectoryStack;

// Function prototypes
int mountImage(const char *imageName);
void printInfo();
char* popDir();
void pushDir(const char *dirName, uint32_t cluster);
void initDirStack();
void freeDirStack();
const char* getCurrentDirPath();
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
void formatNameToFAT(const char *name, uint8_t *entryBuffer);
int writeDirectoryEntry(uint32_t parentCluster, const char *name, uint32_t cluster, uint8_t attr);
int writeEntryToDisk(uint32_t parentCluster, const uint8_t *entry);
void writeFATEntry(uint32_t clusterNumber, uint32_t value);
int initDirectoryCluster(uint32_t newCluster, uint32_t parentCluster);
int updateParentDirectory(uint32_t parentCluster, const char *dirName, uint32_t newCluster);
void clearCluster(uint32_t clusterNumber);
uint32_t clusterToSector(uint32_t cluster);
bool is_8_3_format_directory(const char *name);
bool isDirectoryFull(uint32_t parentCluster);
int linkClusterToDirectory(uint32_t currentDirectoryCluster, uint32_t newCluster);
int addDirectory(uint32_t parentCluster, const char *dirName);
int createFile(const char *fileName);
bool is_8_3_format_filename(const char *name);
bool fileExists(const char *filename);
void toUpperCase(char *str);
int expandDirectory(uint32_t parentCluster);
void rightTrim(char *str);
int openFile(const char *filename, const char *mode);
void initOpenFiles();
int closeFile(const char *filename);
int writeToFile(const char *filename, const char *data);
uint32_t findClusterByOffset(uint32_t startCluster, uint32_t offset);
uint32_t getDirectoryEntryFileSize(uint32_t cluster);
bool extendFile(uint32_t cluster, uint32_t newSize);
void updateDirectoryEntrySize(uint32_t cluster, uint32_t newSize);
const char *getString(const tokenlist *tokens);
int seekFile(const char *filename, long offset);
void listOpenFiles(void);
#endif