#include <mpi.h>
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

    int *in = calloc(size, sizeof(int));

    MPI_Request req;
    if (rank == 0) {
        MPI_Igather(&rank, 1, MPI_INT, in, 1, MPI_INT, 0, MPI_COMM_WORLD, &req);
        MPI_Wait(&req, MPI_STATUS_IGNORE);
    } else {
        MPI_Igather(&rank, 1, MPI_INT, in, 1, MPI_INT, 1, MPI_COMM_WORLD, &req);
        MPI_Wait(&req, MPI_STATUS_IGNORE);
    }

    MPI_Finalize();

    return 0;
}
