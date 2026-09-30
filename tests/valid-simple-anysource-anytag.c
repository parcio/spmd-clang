#include <mpi.h>
#include <stddef.h>
#include <stdio.h>

// Valid code
// Process 0 receiving messages in any order from the other processes
int main(int argc, char **argv)
{
    int rank, size;

    MPI_Init(NULL, NULL);

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (rank == 0)
    {
        for(int i =0; i < size-1; i++) {
            int buf = 0;
            MPI_Recv(&buf, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            printf("Received message from rank %d\n", buf);
        }
    }
    else
    {
        MPI_Send(&rank, 1, MPI_INT, 0, rank, MPI_COMM_WORLD);
    }

    MPI_Finalize();

    return 0;
}
