#include <stdlib.h>
#include <stdio.h>
#include <mpi.h>

#define BUF_SIZE 100

// Valid code
// A standard ring communication pattern in a fixed size loop
// Uses MPI_Bsend for a ring communication pattern
int main()
{
    int rank = 0;
    int size = 0;

    MPI_Init(NULL,NULL);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int source = (rank - 1 + size) % size;
    int target = (rank + 1) % size;

    char message[BUF_SIZE];
    char buf[BUF_SIZE];

    for (int i = 0; i < 100; i++) {
        MPI_Allreduce(&rank, &buf, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
        MPI_Barrier(MPI_COMM_WORLD);
        MPI_Reduce(&rank, &buf, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_Barrier(MPI_COMM_WORLD);
        MPI_Bcast(&buf, 1, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Barrier(MPI_COMM_WORLD);
    }





    MPI_Finalize();
}
