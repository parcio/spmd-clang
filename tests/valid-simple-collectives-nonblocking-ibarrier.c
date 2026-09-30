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


    MPI_Request req;
    MPI_Ibarrier(MPI_COMM_WORLD, &req);
    MPI_Wait(&req, MPI_STATUS_IGNORE);


    MPI_Finalize();

    return 0;
}
