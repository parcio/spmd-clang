#include <mpi.h>
#include <stddef.h>
#include <stdio.h>

// Invalid code
// Basic 2 process example of a deadlock
// Both processes should get stuck at their Ssends
int main(int argc, char **argv)
{
    int rank, size;

    MPI_Init(NULL, NULL);

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (rank == 0)
    {
        int value = 42;
        int recv = 0;
        MPI_Ssend(&value, 1, MPI_INT, rank+1, 0, MPI_COMM_WORLD);
        MPI_Recv(&recv, 1, MPI_INT, rank+1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        printf("Rank %d, Received value: %d\n", rank, recv);
    }
    else if (rank == 1)
    {
        int value = 42;
        int recv = 0;
        MPI_Ssend(&value, 1, MPI_INT, rank-1, 0, MPI_COMM_WORLD);
        MPI_Recv(&recv, 1, MPI_INT, rank-1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        printf("Rank %d, Received value: %d\n", rank, recv);
    }

    MPI_Finalize();

    return 0;
}
