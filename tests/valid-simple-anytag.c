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
        int buf;
        MPI_Recv(&buf, 1, MPI_INT, 1, MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        printf("Received message from rank %d\n", buf);
        MPI_Recv(&buf, 1, MPI_INT, 1, MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        printf("Received message from rank %d\n", buf);
    }
    else if (rank == 1)
    {
        MPI_Send(&rank, 1, MPI_INT, 0, 42, MPI_COMM_WORLD);
        MPI_Send(&rank, 1, MPI_INT, 0, 46, MPI_COMM_WORLD);
    }

    MPI_Finalize();

    return 0;
}
