#include <mpi.h>
#include <stddef.h>
#include <stdio.h>

// Valid code
// Basic exchange between two processes repeated twice
int main(int argc, char **argv)
{
    int rank, size;

    MPI_Init(NULL, NULL);

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (rank == 0)
    {
    int value = 5000;
    MPI_Ssend(&value, 1, MPI_INT, rank+1, 0, MPI_COMM_WORLD);
    }
    else if (rank == 1)
    {
    int value = 0;
    MPI_Recv(&value, 1, MPI_INT, rank-1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    printf("Received value: %d\n", value);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    if (rank == 0)
    {
    int value = 5000;
    MPI_Ssend(&value, 1, MPI_INT, rank+1, 0, MPI_COMM_WORLD);
    }
    else if (rank == 1)
    {
    int value = 0;
    MPI_Recv(&value, 1, MPI_INT, rank-1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    printf("Received value: %d\n", value);
    }


    MPI_Finalize();

    return 0;
}
