#include <mpi.h>
#include <stddef.h>
#include <stdio.h>

// Valid code
// Process 0 receiving messages in any order from the other processes
int main(int argc, char **argv)
{
    int rank, size;

    MPI_Init(NULL, NULL);

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (rank == 0)
    {
        MPI_Request req;
        MPI_Request req2;
        int buf;
        MPI_Irecv(&buf, 1, MPI_INT, 1, 0, MPI_COMM_WORLD, &req);
        MPI_Wait(&req, MPI_STATUS_IGNORE);
        printf("Received message from rank %d\n", buf);
        MPI_Irecv(&buf, 1, MPI_INT, 1, 0, MPI_COMM_WORLD, &req2);
        MPI_Wait(&req2, MPI_STATUS_IGNORE);
        // for(int i =0; i < size-1; i++) {
        //     int buf = 0;
        //     MPI_Irecv(&buf, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &req);
        //     MPI_Wait(&req, MPI_STATUS_IGNORE);
        //     printf("Received message from rank %d\n", buf);
        // }
    }
    else
    {
        MPI_Request req;
        MPI_Isend(&rank, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &req);
        MPI_Send(&rank, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
        MPI_Wait(&req, MPI_STATUS_IGNORE);
    }

    MPI_Finalize();

    return 0;
}
