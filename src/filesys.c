#include "filesysFunc.h"

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




