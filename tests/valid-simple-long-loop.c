#include <stdio.h>
#include <mpi.h>

int main(int argc, char* argv[]) 
{
    int rank, size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int next = (rank + 1) % size;
    int prev = (rank - 1 + size) % size;

    double val = 0.0;
    int iterations = 0;

    while (iterations < 10000) {
        for (int i = 0; i < 5; i++) {
            val += i;
        }

        if (rank % 2 == 0) {
            MPI_Ssend(&val, 1, MPI_DOUBLE, next, 0, MPI_COMM_WORLD);
            double recv = 0.0;
            MPI_Recv(&recv, 1, MPI_DOUBLE, prev, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            printf("Rank %d : %f\n", rank, recv);
        } else {
            double recv = 0.0;
            MPI_Recv(&recv, 1, MPI_DOUBLE, prev, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            printf("Rank %d : %f\n", rank, recv);
            MPI_Ssend(&val, 1, MPI_DOUBLE, next, 0, MPI_COMM_WORLD);
        }
        MPI_Barrier(MPI_COMM_WORLD);
        iterations++;
    }
    

    MPI_Finalize();
    return 0;
}
