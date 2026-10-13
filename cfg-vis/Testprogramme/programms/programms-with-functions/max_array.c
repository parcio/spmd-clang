#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/** copied from my attemp to do the AdventOfCode2022 in C
 * https://github.com/AdmiralLuke/AdventOfCode2022/
 * @brief Get the File object
 * 
 * @param path relative path to the File
 * @return FILE* if file existed: File, else: Null
 */
FILE* getFile(char* path) {
    FILE* fp = fopen(path, "r");
    if (fp == NULL) {
        printf("Error: Couldnt open file %s", path);
        return NULL;
    }
    return fp;
} 

/**
 * @brief Get the Next Line object
 * 
 * @param fp File with read rights
 * @param MAX_LENGTH Max length of bytes which are read per line
 * @return char* line content with length max_length
 */
char* getNextLine(FILE* fp, const unsigned MAX_LENGTH) {
    char* buffer = malloc(sizeof(char) * MAX_LENGTH);
    if (fgets(buffer, MAX_LENGTH, fp)) {
        return buffer;
    } else {
        return NULL;
    }
}

int max_sum(int* a, int n) {
    int sum_pos = 0;
    int sum_neg = 0;
    int sum_max = 0;
    for (int i = 0; i < n; i++) {
        if (a[i] < 0) {
            if (sum_pos + sum_neg + a[i] > 0) {
                sum_neg += a[i];
            } else {
                if (sum_pos > sum_max) {
                    sum_max = sum_pos;
                }
                sum_pos = 0;
            }
        } else {
            sum_pos = sum_pos == 0 ? 0 : sum_pos + sum_neg;
            sum_neg = 0;
            sum_pos += a[i];
        } 
    }
    return sum_pos > sum_max ? sum_pos : sum_max;
}

int main() {
    int* tmp = (int *)malloc(sizeof(int) * 7);
    tmp[0] = -24;
    tmp[1] = 4;
    tmp[2] = -1;
    tmp[3] = -1;
    tmp[4] = 8;
    tmp[5] = 3;
    tmp[6] = -32;
    
    int* tmp2 = (int *)malloc(sizeof(int) * 10);
    tmp2[0] = 10;
    tmp2[1] = -1169;
    tmp2[2] = -795;
    tmp2[3] = -116;
    tmp2[4] = -1078;
    tmp2[5] = 1378;
    tmp2[6] = -1221;
    tmp2[7] = -1317;
    tmp2[8] = -1090;
    tmp2[9] = -257;
    
    // {-24, 4, -1, -1,  8, 3, -32};
    // 10 -1169 -795 -116 -1078 1378 -1221 -1097 -1317 -1090 -257

    // printf("%d \n", max_sum(tmp2, 10));

    FILE* input = getFile("sample_10000.txt");
    char* line = getNextLine(input, 256);

    int n = atoi(line);
    int i = 0;
    int* arr = (int *)malloc(sizeof(int) * n);

    free(line);
    line = getNextLine(input, 256);

    while (line != NULL) {   
        
        arr[i] = atoi(line);
        i++;
        free(line);
        line = getNextLine(input, 256);
    } 

    printf("%d\n", max_sum(arr, n));

    free(arr);

    free(tmp);
    free(tmp2);

    return EXIT_SUCCESS;
}