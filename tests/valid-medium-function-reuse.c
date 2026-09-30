#include <mpi.h>
#include <stdio.h>

void sending(int target, double value)
{
    MPI_Ssend(&value, 1, MPI_DOUBLE, target, 0, MPI_COMM_WORLD);
}

double receiving(int source)
{
    double x;
    MPI_Recv(&x, 1, MPI_DOUBLE, source, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    return x;
}

int exchange(int rank)
{
    if(rank == 0) {
        sending(1, 42.0);
    } else if (rank == 1) {
        double x = receiving(0);
        printf("Rank %d, received: %f\n", rank, x);
    }

    return 1;
}

int main()
{
    MPI_Init(NULL, NULL);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int result = exchange(rank);
    if(rank == 0) {
        printf("Exchange function result: %d\n", result);
    }

    if(rank == 0) {
        double x = receiving(1);
        printf("Rank %d, received: %f\n", rank, x);
        x = receiving(1);
        printf("Rank %d, received: %f\n", rank, x);
    } else if (rank == 1) {
        sending(0, 43.0);
        sending(0, 44.0);
    }

    MPI_Finalize();

}
