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
} FAT32BootSector;

FAT32BootSector bs;

int mountImage(const char *imageName) {
    FILE *fp = fopen(imageName, "rb");
    if (!fp) {
        perror("Error opening image file");
        return -1;
    }

    // Example offsets may need adjustment based on your FAT32 structure
    fseek(fp, 11, SEEK_SET); fread(&bs.bytesPerSector, sizeof(bs.bytesPerSector), 1, fp);
    fseek(fp, 13, SEEK_SET); fread(&bs.sectorsPerCluster, sizeof(bs.sectorsPerCluster), 1, fp);
    fseek(fp, 32, SEEK_SET); fread(&bs.totalSectors, sizeof(bs.totalSectors), 1, fp);
    fseek(fp, 36, SEEK_SET); fread(&bs.FATSize, sizeof(bs.FATSize), 1, fp);
    fseek(fp, 44, SEEK_SET); fread(&bs.rootCluster, sizeof(bs.rootCluster), 1, fp);

    fclose(fp);
    return 0;
}

void printInfo() {
    printf("Bytes Per Sector: %d\n", bs.bytesPerSector);
    printf("Sectors Per Cluster: %d\n", bs.sectorsPerCluster);
    printf("Root Cluster: %d\n", bs.rootCluster);
    printf("Total # of Clusters in Data Region: %llu\n", (uint64_t)bs.totalSectors / bs.sectorsPerCluster);
    printf("# of Entries in One FAT: %d\n", bs.FATSize * (bs.bytesPerSector / 4)); // 4 bytes per FAT entry assumption
    printf("Size of Image (in bytes): %llu\n", (uint64_t)bs.totalSectors * bs.bytesPerSector);
}

void cleanup() {
    // In this example, there's no dynamic memory to free, but if you allocate memory, free it here.
    // e.g., if you had dynamically allocated memory, you would free it like so:
    // free(your_allocated_resource);
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
