#include <mpi.h>
#include <stddef.h>
#include <stdio.h>

// Valid code
// Basic 2 process blocking communication test
int main(int argc, char **argv)
{
    int rank, size;

    MPI_Init(NULL, NULL);

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int counter = 0;

    for(int k = 0; k < 5; k++) {
        for (int i = 1; i < 40; i++) {
            if (rank == 0) {
                counter++;
                MPI_Ssend(&counter, 1, MPI_INT, rank+1, 0, MPI_COMM_WORLD);
            } else if (rank == 1) {
                int res = 0;
                MPI_Recv(&res, 1, MPI_INT, rank-1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                printf("Received value: %d\n", res);
            }

            if(i == 30) {
                break;
            }
        }
    }

    MPI_Finalize();

    return 0;
}
