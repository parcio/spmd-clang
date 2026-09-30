#include <mpi.h>
#include <stddef.h>
#include <stdio.h>

// Valid code
// Process 0 receiving messages with different tags and this leads to a deadlock
// FIXME: figure out if this falls under the nonovertaking message semantics or not...  If yes then SPMDClang needs to be modified for anytag and anysource in this regard
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
        printf("Received message with tag %d\n", buf);
        MPI_Recv(&buf, 1, MPI_INT, 1, 42, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        printf("Received message with tag %d\n", buf);
    }
    else if (rank == 1)
    {
        MPI_Request req;
        int tag = 42;
        MPI_Isend(&tag, 1, MPI_INT, 0, tag, MPI_COMM_WORLD, &req);
        MPI_Send(&rank, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
        MPI_Wait(&req, MPI_STATUS_IGNORE);
    }

    MPI_Finalize();

    return 0;
}
