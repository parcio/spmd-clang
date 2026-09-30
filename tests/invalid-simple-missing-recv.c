#include <mpi.h>
#include <stddef.h>
#include <stdlib.h>

// Invalid code
// This will block because the message from rank 0 is never received
int main(int argc, char **argv)
{
    int rank, size;
    MPI_Init(NULL, NULL);

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if(rank == 0) {
        int val = 42;
        MPI_Ssend(&val, 1, MPI_INT, 1, 0, MPI_COMM_WORLD);
    } 

    MPI_Finalize();

    return 0;
}
