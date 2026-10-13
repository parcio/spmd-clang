#include <stdio.h>
#include <stdlib.h>

void print_tree_char(int height, char sym) {
    if (height <= 0) {
        return;
    }
    for (int h = 0; h < height - 1; h++) {
        for (int i = 0; i < height - 1 - h; i++) {
            printf(" ");
        }
        for (int i = 0; i < (h * 2) + 1; i++) {
            printf("%c",sym);
        }
        printf("\n");
    }
    for (int i = 0; i < height - 1; i++) {
        printf(" ");
    }
    printf("%c",'*');
}

void print_tree(int height) {
    print_tree_char(height, '*');
}

int main() {
    char* test = (char *)malloc(4);
    free(test);
    print_tree_char(10,'x');
    printf("\n");
    return 0;
}
