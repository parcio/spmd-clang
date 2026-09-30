#include <mpi.h>
#include <stddef.h>
#include <stdio.h>

// Valid code
// Basic test with only MPI_Init and MPI_Finalize
int main(int argc, char **argv)
{
    int rank, size;
    MPI_Init(NULL, NULL);

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    MPI_Finalize();

    return 0;
}
