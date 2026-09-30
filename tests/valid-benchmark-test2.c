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

    char message[BUF_SIZE];
    char buf[BUF_SIZE];

    for (int i = 0; i < 100; i++) {
        if (rank == 0) {
            for (int j = 1; j < size; j++) {
                MPI_Recv(buf, BUF_SIZE, MPI_CHAR, MPI_ANY_SOURCE, i, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }
        } else {
            MPI_Ssend(message, BUF_SIZE, MPI_CHAR, 0, i, MPI_COMM_WORLD);
        }
    }





    MPI_Finalize();
}
