#include <mpi.h>
#include <mpi_proto.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

// Valid code
// test of all the supported collectives being used in a valid order
int main(int argc, char **argv)
{
    int rank, size;
    MPI_Init(NULL, NULL);

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int *out = calloc(size, sizeof(int));
    for (int i = 0; i < size; i++) {
        out[i] = 42 + i;
    }
    int in = 0;

    MPI_Request req;
    MPI_Iscatter(out, 1, MPI_INT, &in, 1, MPI_INT, 0, MPI_COMM_WORLD, &req);
    MPI_Wait(&req, MPI_STATUSES_IGNORE);

    printf("rank %d: %d\n", rank, in);


    MPI_Finalize();

    return 0;
}
