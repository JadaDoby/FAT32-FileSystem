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

int main() {
    // Example usage of the functions
    // You would typically check if mountImage succeeds before calling printInfo
    return 0;
}
