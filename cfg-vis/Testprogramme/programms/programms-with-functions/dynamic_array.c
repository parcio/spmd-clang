#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char* array;
    int numElems;
    int size;
} Dyn_array_t;


void initArray(Dyn_array_t* array, int desSize) {
    array->array = (char*) malloc(desSize * sizeof(char));
    if (array->array == NULL) {
        printf("Fehler: Speicher konnte nicht reserviert werden.");
        exit(1);
    }
    array->numElems = 0;
    array->size = desSize;
}

void insert(Dyn_array_t* array, char elem) {
    if (array->numElems >= array->size) {
        // Größe verdoppeln
        int new_size = array->size * 2;
        char* new_array = (char*) realloc(array->array, new_size * sizeof(char));
        if (new_array == NULL) {
            printf("Fehler: Speicher konnte nicht reserviert werden.");
            exit(1);
        }
        array->array = new_array;
        array->size = new_size;
    }
    array->array[array->numElems] = elem;
    array->numElems++;
}

void destroyArray(Dyn_array_t* array) {
    free(array->array);
    array->array = NULL;
    array->size = 0;
    array->numElems = 0;
}

int main() {
    Dyn_array_t test;
    initArray(&test, 2);
    insert(&test, 'P');
    insert(&test, 'G');
    insert(&test, 'P');

    char insertItems[19] = {" Uebung macht Spas"};
    for (int i = 0; i < 19; i++) {
        insert(&test, insertItems[i]);
        int n = test.size;
        if (n >= i + 3) {
            char c = test.array[i + 3];
            printf(c == insertItems[i]);
        } else {
            printf(n);
        }
    }

    // TEST_CHECK_(strcmp(test.array, "PGP") || strcmp(test.array, "PGP\0"), "after inserting the chars \"PGP\" the string should be that but was \"%s\"", test.array);
    // free
    destroyArray(&test);
    return 0;
}
