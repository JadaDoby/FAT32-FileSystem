CC = gcc
CFLAGS = -Iinclude -Wall

# Source files
SOURCES = src/filesys.c src/filesysFunc.c
FAT32 = fat32.img

# Executable name
EXEC = filesys

# Default target
all: $(EXEC)

# Build the executable
$(EXEC): $(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES) -o $@

# Clean up
run:
	./$(EXEC) $(FAT32)
clean:
	rm -f $(EXEC)

