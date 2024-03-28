#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Correctly defined FAT32BootSector structure
typedef struct {
    uint16_t bytesPerSector;
    uint8_t sectorsPerCluster;
    uint32_t totalSectors; // Assuming this represents totalSectors32
    uint32_t fatSize; 
    uint32_t rootCluster;
} FAT32BootSector; // Ensure this definition is terminated with a semicolon

FILE *fat32Img;
FAT32BootSector bs; // Only need to declare this once

// Function to mount the image
int mountImage(const char *imageName) {
    FILE *fp = fopen(imageName, "rb"); 
    if (!fp) {
        perror("Error opening image file");
        return -1;
    }

    if (fread(&bs, sizeof(FAT32BootSector), 1, fp) != 1) {
        perror("Error reading boot sector");
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 0;
}

// Function to print information
void printInfo() {
    printf("Bytes Per Sector: %d\n", bs.bytesPerSector);
    printf("Sectors Per Cluster: %d\n", bs.sectorsPerCluster);
    printf("Total Sectors: %d\n", bs.totalSectors); // Direct use of totalSectors
    printf("FAT Size (Sectors): %d\n", bs.fatSize);
    printf("Root Cluster: %d\n", bs.rootCluster);
    // Calculate the size of the image (in bytes) directly using totalSectors
    printf("Size of Image: %llu bytes\n", (unsigned long long)bs.totalSectors * bs.bytesPerSector);
}

void startShell() {
    char command[256];

    while (1) {
        printf("FAT32/> ");
        scanf("%s", command);

        if (strcmp(command, "info") == 0) {
            printInfo();
        } else if (strcmp(command, "exit") == 0) {
            break;
        } else {
            printf("Unknown command.\n");
        }
    }
}

void cleanup() {
    if (fat32Img) {
        fclose(fat32Img);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s [FAT32 image file]\n", argv[0]);
        return 1;
    }

    if (mountImage(argv[1]) != 0) {
        return 1; // Exit if the image could not be mounted
    }

    startShell();
    cleanup();

    return 0;
}
