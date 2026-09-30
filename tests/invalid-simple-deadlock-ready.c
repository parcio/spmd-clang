#include <mpi.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

// Invalid code
// Basic 2 process example of a deadlock
// Both processes should get stuck at their Ssends

#define SIZE 10000

int main(int argc, char **argv)
{
    int rank, size;

    MPI_Init(NULL, NULL);

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (rank == 0)
    {
        double *values = calloc(SIZE, sizeof(double));    
        double *recv = calloc(SIZE, sizeof(double));    
       
        MPI_Rsend(values, SIZE, MPI_DOUBLE, rank+1, 0, MPI_COMM_WORLD);
        MPI_Recv(recv, SIZE, MPI_DOUBLE, rank+1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        printf("Rank %d, Received value: %f\n", rank, recv[0]);
    }
    else if (rank == 1)
    {
        double *values = calloc(SIZE, sizeof(double));    
        double *recv = calloc(SIZE, sizeof(double));    
       
        MPI_Rsend(values, SIZE, MPI_DOUBLE, rank-1, 0, MPI_COMM_WORLD);
        MPI_Recv(recv, SIZE, MPI_DOUBLE, rank-1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
     
        printf("Rank %d, Received value: %f\n", rank, recv[0]);
    }

    MPI_Finalize();

    return 0;
}
