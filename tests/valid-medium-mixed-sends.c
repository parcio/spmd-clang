#include <mpi.h>
#include <mpi_proto.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

// Valid code
// test for multiple different send operations 
int main(int argc, char **argv)
{
    int rank, size;
    MPI_Init(NULL, NULL);

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int *bsend_buffer = (int*)malloc(1000 * sizeof(int));
    MPI_Buffer_attach(bsend_buffer, 1000 * sizeof(int));

    if(rank == 0) {
        int val = 42 + rank;
        int res = 0;
        MPI_Bsend(&val, 1, MPI_INT, rank+1, 0, MPI_COMM_WORLD);
        MPI_Recv(&res, 1, MPI_INT, rank+1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        printf("rank %d: received %d\n", rank, res);

        val++;
        MPI_Ssend(&val, 1, MPI_INT, rank+1, 0, MPI_COMM_WORLD);
        MPI_Recv(&res, 1, MPI_INT, rank+1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        printf("rank %d: received %d\n", rank, res);

        val++;
        MPI_Bsend(&val, 1, MPI_INT, rank+1, 0, MPI_COMM_WORLD);
        MPI_Recv(&res, 1, MPI_INT, rank+1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        printf("rank %d: received %d\n", rank, res);
    } else if(rank == 1) {
        int val = 42 + rank;
        int res = 0;
        MPI_Bsend(&val, 1, MPI_INT, rank-1, 0, MPI_COMM_WORLD);
        MPI_Recv(&res, 1, MPI_INT, rank-1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        printf("rank %d: received %d\n", rank, res);

        val++;
        MPI_Recv(&res, 1, MPI_INT, rank-1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Ssend(&val, 1, MPI_INT, rank-1, 0, MPI_COMM_WORLD);
        printf("rank %d: received %d\n", rank, res);

        val++;
        MPI_Bsend(&val, 1, MPI_INT, rank-1, 0, MPI_COMM_WORLD);
        MPI_Recv(&res, 1, MPI_INT, rank-1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        printf("rank %d: received %d\n", rank, res);
    }

    free(bsend_buffer);

    MPI_Finalize();

    return 0;
}
