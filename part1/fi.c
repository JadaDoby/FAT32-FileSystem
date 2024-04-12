

// void append_dir_entry(int fd, dentry_t *new_dentry, uint32_t clus_num, bpb_t bpb) {
//     uint32_t curr_clus_num = clus_num;
//     uint32_t sectorSize = bpb.BPB_BytsPerSec;
//     uint32_t clusterSize = bpb.BPB_SecPerClus * sectorSize;
//     uint32_t bytesProcessed = 0;

//     while (true) {
//         uint32_t data_offset = convert_clus_num_to_offset_in_data_region(curr_clus_num, bpb);

//         for (bytesProcessed = 0; bytesProcessed < clusterSize; bytesProcessed += sizeof(dentry_t)) {
//             dentry_t dentry;
//             ssize_t rd_bytes = pread(fd, &dentry, sizeof(dentry_t), data_offset + bytesProcessed);
//             if (rd_bytes != sizeof(dentry_t)) {
//                 printf("Failed to read directory entry\n");
//                 return;
//             }

//             if (dentry.DIR_Name[0] == 0x00 || dentry.DIR_Name[0] == (char)0xE5) {
//                 // Write the new directory entry
//                 ssize_t wr_bytes = pwrite(fd, new_dentry, sizeof(dentry_t), data_offset + bytesProcessed);
//                 if (wr_bytes != sizeof(dentry_t)) {
//                     printf("Failed to write directory entry\n");
//                     return;
//                 }

//                 // Write the end-of-directory marker after the new entry
//                 dentry_t end_of_dir_entry = {0};
//                 end_of_dir_entry.DIR_Name[0] = 0x00;
//                 wr_bytes = pwrite(fd, &end_of_dir_entry, sizeof(dentry_t), data_offset + bytesProcessed + sizeof(dentry_t));
//                 if (wr_bytes != sizeof(dentry_t)) {
//                     printf("Failed to write end-of-directory marker\n");
//                     return;
//                 }
//                 return;
//             }
//         }

//         // Check if next cluster is available in the chain
//         uint32_t fat_offset = convert_clus_num_to_offset_in_fat_region(curr_clus_num, bpb);
//         uint32_t next_clus_num;
//         pread(fd, &next_clus_num, sizeof(uint32_t), fat_offset);

//         uint32_t fat32_bad_cluster_min = 0xFFFFFF8;
//         uint32_t fat32_bad_cluster_max = 0xFFFFFFFF;

//         // Check if the cluster number falls within the range of bad clusters or is the end of the file.
//         if ((next_clus_num >= fat32_bad_cluster_min && next_clus_num <= fat32_bad_cluster_max)) {
//             // Allocate a new cluster and link it
//             uint32_t new_clus_num = alloca_cluster(fd, bpb);
//             if (new_clus_num == 0) {
//                 printf("No free cluster available for directory expansion\n");
//                 return;
//             }
//             pwrite(fd, &new_clus_num, sizeof(uint32_t), fat_offset); // Link the new cluster
//             curr_clus_num = new_clus_num;

//             // Initialize the new cluster with an end-of-directory marker
//             dentry_t end_of_dir_entry = {0};
//             end_of_dir_entry.DIR_Name[0] = 0x00;
//             uint32_t new_cluster_offset = convert_clus_num_to_offset_in_data_region(new_clus_num, bpb);
//             ssize_t wr_bytes = pwrite(fd, &end_of_dir_entry, sizeof(dentry_t), new_cluster_offset);
//             if (wr_bytes != sizeof(dentry_t)) {
//                 printf("Failed to initialize new cluster with end-of-directory marker\n");
//                 return;
//             }
//         } else {
//             curr_clus_num = next_clus_num; // Move to next cluster in the chain
//         }
//     }
// }

// void print_boot_sector_info(bpb_t bpb) {
//     printf("Bytes Per Sector: %u\n", bpb.BPB_BytsPerSec);
//     printf("Sectors Per Cluster: %u\n", bpb.BPB_SecPerClus);

//     // Calculate the First Data Sector
//     uint32_t firstDataSector = bpb.BPB_RsvdSecCnt + (bpb.BPB_NumFATs * bpb.BPB_FATSz32);
//     printf("First Data Sector: %u\n", firstDataSector);

//     // Calculate total sectors
//     uint32_t totalSectors = bpb.BPB_TotSec32;

//     // Calculate total clusters in Data Region
//     uint32_t totalDataSectors = totalSectors - firstDataSector;
//     uint32_t totalClusters = totalDataSectors / bpb.BPB_SecPerClus;
//     printf("Total clusters in Data Region: %u\n", totalClusters);

//     // Calculate number of entries in one FAT
//     uint32_t fatSize = bpb.BPB_FATSz32 * bpb.BPB_BytsPerSec; // Total size of one FAT in bytes
//     uint32_t numEntriesInOneFAT = fatSize / sizeof(uint32_t); // Each FAT entry is 4 bytes (uint32_t)
//     printf("# of entries in one FAT: %u\n", numEntriesInOneFAT);

//     printf("Size of Image (bytes): %u\n", totalSectors * bpb.BPB_BytsPerSec);
//     printf("Root Cluster: %u\n", bpb.BPB_RootClus);
// }

// //mounts the fat23 image file
// bpb_t mount_fat32(int img_fd) {
//     bpb_t bpb;

    
//     size_t rd_bytes = pread(img_fd, &bpb, sizeof(bpb_t), 0);
//     if (rd_bytes == -1) {
//         perror("pread failed");
//         close(img_fd);
//         exit(EXIT_FAILURE);
//     }

//     if (rd_bytes != sizeof(bpb_t)) {
//         printf("request %zu bytes, but read %zu bytes\n", sizeof(bpb_t), rd_bytes);
//         close(img_fd);
//         exit(EXIT_FAILURE);
//     }
//     printf("               INITIAL VALUES:\n");
//     printf("===============================================\n");
//     printf("BS_jmpBoot is %.3s\n", bpb.BS_jmpBoot);
//     printf("BS_OEMName is %.8s\n", bpb.BS_OEMName);
//     printf("BPB_BytsPerSec is %u\n", bpb.BPB_BytsPerSec);
//     printf("BPB_SecPerClus is %u\n", bpb.BPB_SecPerClus);
//     printf("BPB_RsvdSecCnt is %u\n", bpb.BPB_RsvdSecCnt);
//     printf("BPB_NumFATs is %u\n", bpb.BPB_NumFATs);
//     printf("BPB_RootEntCnt is %u\n", bpb.BPB_RootEntCnt);
//     printf("BPB_TotSec16 is %u\n", bpb.BPB_TotSec16);
//     printf("BPB_Media is %u\n", bpb.BPB_Media);
//     printf("BPB_FATSz32 is %u\n", bpb.BPB_FATSz32);
//     printf("BPB_SecPerTrk is %u\n", bpb.BPB_SecPerTrk);
//     printf("BPB_NumHeads is %u\n", bpb.BPB_NumHeads);
//     printf("BPB_HiddSec is %u\n", bpb.BPB_HiddSec);
//     printf("BPB_TotSec32 is %u\n", bpb.BPB_TotSec32);
//     printf("BPB_RootClus is %u\n", bpb.BPB_RootClus);
//     printf("===============================================\n");

//     return bpb;
// }

// //main function (loops) (all the functionality is called or written here)
// void main_process(int img_fd, const char* img_path, bpb_t bpb) {
//     while (1) {
//         // 0. print prompt and current working directory
//         printf("%s%s>",img_path, current_path);
//         // 1. get cmd from input.
//         // you can use the parser provided in Project1
//         char *input = get_input();
//         tokenlist *tokens = get_tokens(input);
//         if(tokens->size <= 0)
// 			continue;

//         // 2. if cmd is "exit" break;
//         if(strcmp(tokens->items[0], "exit") == 0 && tokens->size > 1)
//             printf("exit command does not take any arguments.\n");
//         else if(strcmp(tokens->items[0], "exit") == 0)
//         {
//             free_tokens(tokens);
//             break;
//         }
//         else if(strcmp(tokens->items[0], "info") == 0 && tokens->size > 1)
//             printf("info command does not take any arguments.\n");
//         else if (strcmp(tokens->items[0], "info") == 0)
//             print_boot_sector_info(bpb);
//         else if(strcmp(tokens->items[0], "cd") == 0 && tokens->size > 2)
//             printf("cd command does not take more than two arguments.\n");
//         else if(strcmp(tokens->items[0], "cd") == 0 && tokens->size < 2)
//             printf("cd command requires an argument, none was given.\n");
//         else if (strcmp(tokens->items[0], "cd") == 0)
//         {
//             if(!is_valid_path(img_fd,bpb,tokens->items[1]))
//                 printf("%s does not exist.\n", tokens->items[1]);
//         }
//         else if(strcmp(tokens->items[0], "ls") == 0 && tokens->size > 1)
//             printf("ls command does not take any arguments.\n");
//         else if (strcmp(tokens->items[0], "ls") == 0)
//             list_content(img_fd, bpb);
//         else if(strcmp(tokens->items[0], "mkdir") == 0 && tokens->size > 2)
//             printf("mkdir command does not take more than two arguments.\n");
//         else if(strcmp(tokens->items[0], "mkdir") == 0 && tokens->size < 2)
//             printf("mkdir command requires an argument, none was given.\n");
//         else if (strcmp(tokens->items[0], "mkdir") == 0)
//         {
//             int value = is_directory(img_fd, bpb, tokens->items[1]);
//             if(strcmp(tokens->items[1], ".") == 0 || strcmp(tokens->items[1], "..") == 0)
//                 printf("users can not make . or .. directories.\n");
//             else if(value == 0)
//                 new_directory(img_fd, bpb, tokens->items[1]);
//             else if(value == 1)
//                 printf("directory already exists\n");