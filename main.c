#include <stdio.h>

void runGraphVisualizer(const char *filename);

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: ./sim <file_name>\n");
        return 1;
    }
    runGraphVisualizer(argv[1]);
    return 0;
}