#include <mpi.h>
#include <mpi_proto.h>
#include <stddef.h>
#include <stdio.h>

// Valid code
// Basic 2 process non-blocking message exchange
int main(int argc, char **argv)
{
    int rank, size;

    MPI_Init(NULL, NULL);

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    MPI_Request req1;
    MPI_Request req2;

    if (rank == 0)
    {
        int value = 5000;
        MPI_Irecv(&value, 1, MPI_INT, rank+1, 0, MPI_COMM_WORLD, &req2);
        MPI_Wait(&req2, MPI_STATUS_IGNORE);
        MPI_Isend(&value, 1, MPI_INT, rank+1, 0, MPI_COMM_WORLD, &req1);
        MPI_Wait(&req1, MPI_STATUS_IGNORE);
        printf("Received value: %d\n", value);
    }
    else if (rank == 1)
    {
        int value = 0;
        MPI_Irecv(&value, 1, MPI_INT, rank-1, 0, MPI_COMM_WORLD, &req2);
        MPI_Wait(&req2, MPI_STATUS_IGNORE);
        MPI_Isend(&value, 1, MPI_INT, rank-1, 0, MPI_COMM_WORLD, &req1);
        MPI_Wait(&req1, MPI_STATUS_IGNORE);
        printf("Received value: %d\n", value);
    }

    MPI_Finalize();

    return 0;
}
