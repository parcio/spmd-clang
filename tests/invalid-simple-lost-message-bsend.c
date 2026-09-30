#include <mpi.h>
#include <stddef.h>
#include <stdlib.h>

// Invalid code
// This will complete but the message from rank 0 can never be received
int main(int argc, char **argv)
{
    int rank, size;
    MPI_Init(NULL, NULL);

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int *bsend_buffer = (int*)malloc(500 * sizeof(int));
    MPI_Buffer_attach(bsend_buffer, 500 * sizeof(int));

    if(rank == 0) {
        int val = 42;
        MPI_Bsend(&val, 1, MPI_INT, rank+1, 0, MPI_COMM_WORLD);

    } 

    free(bsend_buffer);

    MPI_Finalize();

    return 0;
}
