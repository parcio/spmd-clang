#include <mpi.h>
#include <stddef.h>
#include <stdlib.h>

// Invalid code
// This will deadlock because the recv never gets a message
int main(int argc, char **argv)
{
    int rank, size;
    MPI_Init(NULL, NULL);

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if(rank == 1) {
        int res = 0;
        MPI_Recv(&res, 500, MPI_INT, 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    } 

    MPI_Finalize();

    return 0;
}
