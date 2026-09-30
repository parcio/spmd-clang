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

    int *in_buf = calloc(size, sizeof(int));

    MPI_Allgather(&rank, 1, MPI_INT, in_buf, 1, MPI_INT, MPI_COMM_WORLD);
    for (int i = 0; i < size; i++) {
        printf("%d ", in_buf[i]);
    }


    MPI_Finalize();

    return 0;
}
