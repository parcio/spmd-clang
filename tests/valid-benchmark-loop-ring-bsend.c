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
    sprintf(message, "Message from rank %d", rank);

    int *bsend_buffer = (int*)malloc(2000 * sizeof(int));
    MPI_Buffer_attach(bsend_buffer, 2000 * sizeof(int));


    for (int i = 0; i < 100; i++) {
        MPI_Bsend(message, BUF_SIZE, MPI_CHAR, target, 0, MPI_COMM_WORLD);
        MPI_Recv(buf, BUF_SIZE, MPI_CHAR, source, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        printf("Iteration %d | Rank %d: %s\n", i, rank, buf);
        MPI_Barrier(MPI_COMM_WORLD);
    }

    MPI_Finalize();
}
