#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_BLK_SIZE 1200
int MATRIX_ROWS, MATRIX_COLUMNS;
int TAG_TO_USE = 0;
MPI_Request COMM_REQUESTS[4];
double **MATRIX_DATA;



double **matrix_allocate(int x, int y)
{
    int i;

    double *data_layer = (double *) malloc(x * y * sizeof(double));
    double **resultmatrix = (double **) malloc(x * sizeof(double*));

    if(data_layer == NULL || resultmatrix == NULL) {
        printf("ERROR ALLOCATING MEMORY\n");
        exit(1);
    }

    for(i = 0; i < x; i++) {
        resultmatrix[i] = data_layer + i * y;
    }

    return resultmatrix;
}

void matrix_calculate_num_local_rows(int global_rows, int global_columns, int rank, int numtasks)
{
    int local_cols = 0, local_rows = 0;

    local_cols = global_columns;
    local_rows = global_rows / numtasks;
    if(rank < global_rows % numtasks) {
        ++local_rows;
    }
    
    MATRIX_ROWS = local_rows;
    MATRIX_COLUMNS = local_cols;
}

void matrix_init_communication(const int rank, const int numtasks)
{
    int other = MPI_PROC_NULL;
    if(rank != 0) {
        other = rank - 1;
    }

    MPI_Send_init(MATRIX_DATA[1], MATRIX_COLUMNS, MPI_DOUBLE, other, TAG_TO_USE, MPI_COMM_WORLD, &COMM_REQUESTS[0]);
    MPI_Recv_init(MATRIX_DATA[0], MATRIX_COLUMNS, MPI_DOUBLE, other, TAG_TO_USE, MPI_COMM_WORLD, &COMM_REQUESTS[1]);

    other = MPI_PROC_NULL;
    if(rank != numtasks -1) {
        other = rank + 1;
    }

    MPI_Send_init(MATRIX_DATA[MATRIX_ROWS], MATRIX_COLUMNS, MPI_DOUBLE, other, TAG_TO_USE, MPI_COMM_WORLD, &COMM_REQUESTS[2]);
    MPI_Recv_init(MATRIX_DATA[MATRIX_ROWS+1], MATRIX_COLUMNS, MPI_DOUBLE, other, TAG_TO_USE, MPI_COMM_WORLD, &COMM_REQUESTS[3]);
    
    TAG_TO_USE++;
}

void matrix_init(int rank, int numtasks, double inner_value) 
{
    int i;

    for(i = 0; i < MATRIX_ROWS + 1 + 1; i++) {
        for(int j = 0; j < MATRIX_COLUMNS + 1 + 1; j++) {
            MATRIX_DATA[i][j] = inner_value;
        }
    }

    for(i = 0; i < MATRIX_ROWS + 1; i++) {
        MATRIX_DATA[i][0] = 100.0;
        MATRIX_DATA[i][MATRIX_COLUMNS + 1] = 100.0;
    }

    if(rank == numtasks - 1) {
        for(i = 0; i < MATRIX_COLUMNS + 1 + 1; i++) {
            MATRIX_DATA[MATRIX_ROWS + 1][i] = 100.0;
        }
    }

    if(rank == 0) {
        for(i = 0; i < MATRIX_COLUMNS + 1 + 1; i++) {
            MATRIX_DATA[0][i] = 0.0;
        }
    }
}

void matrix_construct(int global_rows, int global_columns, double inner_value)
{
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    matrix_calculate_num_local_rows(global_rows, global_columns, rank, size);
    MATRIX_DATA = matrix_allocate(MATRIX_ROWS + 2, MATRIX_COLUMNS + 2);

    matrix_init_communication(rank, size);
    matrix_init(rank, size, inner_value);
}


int main(int argc, char *argv[]) 
#define M 16
#define N 16
#define EPSILON 0.005
{
    int rank, numtasks;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);

    double mean = N * 100 + N * 0 + M * 100 + M * 100 - 4 * 100;
    mean = mean / (double) (2 * M + 2 * N - 4);
    if (rank == 0) {
        printf("\n");
        printf("  MEAN = %f \n", mean);
    }

    {
        matrix_construct(M, N, mean);

        MPI_Barrier(MPI_COMM_WORLD);
        double wtime = MPI_Wtime();
        double epsilon = EPSILON;
        calculate();
        wtime = MPI_Wtime() - wtime;
    }
}
