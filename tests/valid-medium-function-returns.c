#include <mpi.h>
#include <stdio.h>

int get_rank()
{
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    return rank;
}

int get_size()
{
    int size;
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    return size;
}

void exchange(int message)
{
    int res = 0;
    if (get_rank() == 0) {
        MPI_Ssend(&message, 1, MPI_INT, get_rank()+1, 0, MPI_COMM_WORLD);
        MPI_Recv(&res, 1, MPI_INT, get_rank()+1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        printf("Rank %d Result: %d\n", get_rank(), res);
    } else if (1 == get_rank()) {
        MPI_Recv(&res, 1, MPI_INT, get_rank()-1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Ssend(&message, 1, MPI_INT, get_rank()-1, 0, MPI_COMM_WORLD);
        printf("Rank %d Result: %d\n", get_rank(), res);
    }
}

int main()
{
    MPI_Init(NULL, NULL);

    int rank, size;
    rank = get_rank();
    size = get_size();

    if (rank == 0) {
        double res = 0.0;
        MPI_Recv(&res, 1, MPI_DOUBLE, rank+1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        printf("Rank %d Result: %f\n", rank, res);
        exchange(42);
    } else if (rank == 1) {
        double msg = 42.0;
        MPI_Ssend(&msg, 1, MPI_DOUBLE, rank-1, 0, MPI_COMM_WORLD);
        exchange(46);
    }


    MPI_Finalize();
}
