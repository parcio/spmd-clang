#include <stdlib.h>
#include <stdio.h>
#include <mpi.h>

int main(int argc, char* argv[]) 
{
    int rank, size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int iterations = 10;

    int next = (rank + 1) % size;
    int prev = (rank - 1 + size) % size;

    while (iterations > 0) {
        for (int i = 0; i < 5; i++) {
                double msg = 42.0 + i;
                MPI_Ssend(&msg, 1, MPI_DOUBLE, next, 0, MPI_COMM_WORLD);
                double recv = 0.0;
                MPI_Recv(&recv, 1, MPI_DOUBLE, prev, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                printf("Rank %d : %f\n", rank, recv);
        }
        MPI_Barrier(MPI_COMM_WORLD);
        iterations--;
    }
    

    MPI_Finalize();
    return 0;
}
