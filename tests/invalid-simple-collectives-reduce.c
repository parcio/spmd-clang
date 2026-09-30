#include <mpi.h>
#include <mpi_proto.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

// Invalid code
// Test of a deadlock caused by invalid order of collectives in different ranks
int main(int argc, char **argv)
{
    int rank, size;
    MPI_Init(NULL, NULL);

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int in = 0;

    if (rank == 0) {
        MPI_Reduce(&rank, &in, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_Barrier(MPI_COMM_WORLD);
    } else {
        MPI_Barrier(MPI_COMM_WORLD);
        MPI_Reduce(&rank, &in, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
    }

    MPI_Finalize();

    return 0;
}
