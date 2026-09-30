#include <stdlib.h>
#include <stdio.h>
#include <mpi.h>

#define BUF_SIZE 100

// Valid code
// A standard ring communication pattern running in a fixed iterations loop
// Uses rather verbose if statements to set target and source
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
    if (rank < size-1){
        target = rank+1;
    } else {
        target = 0;
    }
    if (rank > 0){
        source = rank-1;
    } else {
        source = size-1;
    }

    sprintf(message, "Message from rank %d", rank);

    for (int i = 0; i < 3; i++) {
        if (rank % 2 == 0) {
            MPI_Ssend(message, BUF_SIZE, MPI_CHAR, target, 0, MPI_COMM_WORLD);
            MPI_Recv(buf, BUF_SIZE, MPI_CHAR, source, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        } else {
            MPI_Recv(buf, BUF_SIZE, MPI_CHAR, source, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            MPI_Ssend(message, BUF_SIZE, MPI_CHAR, target, 0, MPI_COMM_WORLD);
        }
        printf("Iteration %d | Rank %d: %s\n",i, rank, buf);
    }

    MPI_Finalize();
}
