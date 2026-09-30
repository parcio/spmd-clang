#include <mpi.h>
#include <stddef.h>
#include <stdio.h>

// Valid code
// test of a barrier call that can be reached via different code paths
int main(int argc, char **argv)
{
    int rank, size;
    MPI_Init(NULL, NULL);

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (rank == 0) {
        MPI_Barrier(MPI_COMM_WORLD);
    } else if (rank == 1) {
        MPI_Barrier(MPI_COMM_WORLD);
    } else {
        MPI_Barrier(MPI_COMM_WORLD);
    }

    MPI_Finalize();

    return 0;
}
