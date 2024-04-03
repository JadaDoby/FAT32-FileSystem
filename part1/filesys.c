#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

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
    fread(&bs.sectorsPerCluster, sizeof(bs.sectorsPerCluster), 1, fp);
    // Read reserved sectors
    fread(&bs.reservedSectors, sizeof(bs.reservedSectors), 1, fp);
    // Read number of FATs
    fread(&bs.numFATs, sizeof(bs.numFATs), 1, fp);

    // Seek and read total sectors
    fseek(fp, 32, SEEK_SET);
    fread(&bs.totalSectors, sizeof(bs.totalSectors), 1, fp);
    // Read sectors per FAT
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

void cleanup() {
    // Placeholder for cleanup operations
}

void startShell(const char* imageName) {
    char command[256];
    printf("%s/> ", imageName);
    while (scanf("%255s", command) != EOF) {
        if (strcmp(command, "info") == 0) {
            printInfo();
        } else if (strcmp(command, "exit") == 0) {
            cleanup();
            printf("Exiting...\n");
            break;
        } else {
            printf("Unknown command.\n");
        }
        printf("%s/> ", imageName);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <FAT32 image file>\n", argv[0]);
        return 1;
    }

    if (mountImage(argv[1]) != 0) {
        return 1;
    }

    startShell(argv[1]);

    return 0;
}
