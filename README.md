# project-3-group-11
# [FAT32 File System]

[Purpose of this project is to familiarize you with basic file-system design and implementation. You will need to understand various aspects of the FAT32 file system, such as cluster-based storage, FAT tables, sectors, and directory structure.]

## Group Members
- **[Jada Doby]**: [jdd20a@fsu.edu]
- **[Shelley Bercy]**: [sb22bg@fsu.edu]
- **[Rood Vilmot]**: [rkv20@fsu.edu]
## Division of Labor

### Part 1: [Mount the Image File]
- **Responsibilities**: [This application effectively simulates a filesystem environment within a FAT32 image, offering direct interaction through a dedicated command-line interface, making it suitable for educational purposes or as a tool for managing FAT32 disk images in a controlled environment.]
- **Assigned to**: [Jada Doby, Rood Vilmont]

### Part 2: [Navigation]
- **Responsibilities**: [The project requires implementing `cd` and `ls` commands to navigate and display directory contents within a FAT32 filesystem command-line application, complete with error handling and state tracking.]
- **Assigned to**: [Shelley Bercy, Rood Vilmont]
### Part 3: [Create ]
- **Responsibilities**: [create commands that will allow the user to create files and directories.]
- **Assigned to**: [Jada Doby, Shelley Bercy]
  ### Part 4: [Read]
- **Responsibilities**: [ The project requires implementing file management commands (`open`, `close`, `lsof`, `lseek`, `read`) in a command-line application to handle opening, closing, listing, seeking, and reading operations on files within a FAT32 filesystem, using a structured approach to track file states, modes, and offsets.]
- **Assigned to**: [Rood Vilmont, Shelley Bercy]
  ### Part 5: [Update]
- **Responsibilities**: [The project requires implementing a `write` command that allows users to append text to a file within a FAT32 filesystem, updating the file's offset accordingly and extending the file's size if necessary, with error handling for non-existent files, directories, or files not opened for writing. ]
- **Assigned to**: [Jada Doby, Rood Vilmont]
  ### Part 6: [Delete]
- **Responsibilities**: [The project requires implementing `rm` and `rmdir` commands to delete files and remove empty directories within a FAT32 filesystem, including error handling for non-existent entries, incorrect types, or cases where the target is still in use.]
- **Assigned to**: [Jada Doby, Shelley Bercy]

## File Listing
 FAT 32 Filesystem/
├── include/
│   ├── filesysFunc.h
│   └── lexer.h
├── src/
│   ├── filesys.c
│   └── filesysFunc.c
├── Makefile
└── README.md



## How to Compile & Execute

### Requirements
- **Compiler**:`gcc` for C/C++
- **Dependencies**:<stdio.h>, <stdlib.h>, <stdbool.h>,<unistd.h> <string.h>,<sys/types.h>,<sys/wait.h>, <sys/stat.h>,<fcntl.h>

### Compilation
For a C/C++ example:
```bash
make
```
This will build the executable in ...
### Execution
```To make:
     make
```
```To clean :
      make clean
```
```To run:
    make run
```

## Bugs
- **Bug 1**: This is bug 1.
- **Bug 2**: This is bug 2.
- **Bug 3**: This is bug 3.

## Considerations
[Description]
