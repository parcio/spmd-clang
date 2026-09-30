#include <mpi.h>
#include <stddef.h>
#include <stdio.h>

// Valid code
// Process 0 receiving messages from any source but non deterministic order can lead to deadlock
int main(int argc, char **argv)
{
    int rank, size;

    MPI_Init(NULL, NULL);

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (rank == 0)
    {
        int buf;
        MPI_Recv(&buf, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        printf("Received message with tag %d\n", buf);
        MPI_Recv(&buf, 1, MPI_INT, 2, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        printf("Received message with tag %d\n", buf);
    }
    else if (rank == 1)
    {
        int tag = 42;
        MPI_Send(&tag, 1, MPI_INT, 0, tag, MPI_COMM_WORLD);
    } else if (rank == 2) {
        int tag = 0;
        MPI_Send(&tag, 1, MPI_INT, 0, tag, MPI_COMM_WORLD);
    }

    MPI_Finalize();

    return 0;
}
