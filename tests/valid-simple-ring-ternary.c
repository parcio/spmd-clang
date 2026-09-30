#include <stdlib.h>
#include <stdio.h>
#include <mpi.h>

#define BUF_SIZE 100

// Valid code
// A standard ring communication pattern
// Uses ternaty operators to set target and source
// Currently not working with spmdclang (see Bugtracker.md)
int main()
{
    int rank = 0;
    int size = 0;

    MPI_Init(NULL,NULL);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    char message[BUF_SIZE];
    char buf[BUF_SIZE];
    int target, source;
    target = (rank < size-1) ? rank+1 : 0;
    source = (rank > 0) ? rank-1 : size-1;

    sprintf(message, "Message from rank %d", rank);

    if (rank % 2 == 0) {
        MPI_Ssend(message, BUF_SIZE, MPI_CHAR, target, 0, MPI_COMM_WORLD);
        MPI_Recv(buf, BUF_SIZE, MPI_CHAR, source, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    } else {
        MPI_Recv(buf, BUF_SIZE, MPI_CHAR, source, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Ssend(message, BUF_SIZE, MPI_CHAR, target, 0, MPI_COMM_WORLD);
    }

    printf("Rank %d: %s\n",rank, buf);

    MPI_Finalize();
}
