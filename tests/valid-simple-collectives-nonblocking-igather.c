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

    int *in = calloc(size, sizeof(int));

    MPI_Request req;
    MPI_Igather(&rank, 1, MPI_INT, in, 1, MPI_INT, 0, MPI_COMM_WORLD, &req);
    MPI_Wait(&req, MPI_STATUS_IGNORE);
    if(rank == 0) {
        for (int i = 0; i < size; i++) {
            printf("rank %d: %d\n", i, in[i]);
        }
    }


    MPI_Finalize();

    return 0;
}
