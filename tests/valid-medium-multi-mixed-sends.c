#include <mpi.h>
#include <mpi_proto.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

// Valid code
// test for multiple different send operations with 2 types of envelopes
// Tests that non-overtaking message semantics also work if there are more than one 
// type of identical token in the same place
int main(int argc, char **argv)
{
    int rank, size;
    MPI_Init(NULL, NULL);

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int *bsend_buffer = (int*)malloc(9000 * sizeof(int));
    MPI_Buffer_attach(bsend_buffer, 9000 * sizeof(int));

    if(rank == 0) {
        int val = 42 + rank;
        int val2[2] = {1, 2};

        MPI_Bsend(&val2, 2, MPI_INT, rank+1, 1, MPI_COMM_WORLD);
        MPI_Bsend(&val2, 2, MPI_INT, rank+1, 1, MPI_COMM_WORLD);
        MPI_Bsend(&val, 1, MPI_INT, rank+1, 0, MPI_COMM_WORLD);
        val++;
        MPI_Ssend(&val, 1, MPI_INT, rank+1, 0, MPI_COMM_WORLD);



    } else if(rank == 1) {
        int res = 0;
        int res2[2] = {0, 0};

        MPI_Recv(&res, 1, MPI_INT, rank-1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        printf("rank %d: received %d\n", rank, res);

        MPI_Recv(&res, 1, MPI_INT, rank-1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        printf("rank %d: received %d\n", rank, res);

        MPI_Recv(&res2, 2, MPI_INT, rank-1, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        printf("rank %d: received %d,%d\n", rank, res2[0], res2[1]);

        MPI_Recv(&res2, 2, MPI_INT, rank-1, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        printf("rank %d: received %d,%d\n", rank, res2[0], res2[1]);


    }

    free(bsend_buffer);

    MPI_Finalize();

    return 0;
}
