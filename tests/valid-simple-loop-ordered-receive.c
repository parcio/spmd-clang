#include "mpi.h"
#include <mpi_proto.h>
#include <stdio.h>
#include <stdlib.h>

int main()
{
    MPI_Init(NULL,NULL);
    int rank; 
    int size; 
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int *bsend_buffer = (int*)malloc(500 * sizeof(int));
    MPI_Buffer_attach(bsend_buffer, 500 * sizeof(int));

    if(rank > 0) {
        int x = 42;
        MPI_Bsend(&x, 1, MPI_INT, 0, rank, MPI_COMM_WORLD);
        MPI_Barrier(MPI_COMM_WORLD);
    } else {
        MPI_Barrier(MPI_COMM_WORLD);
        for(int i = 1; i < size; i++) {
            int r = 0;
            MPI_Recv(&r, 1, MPI_INT, i, i, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            printf("From rank %d: %d\n", i, r);
        }
    }
    MPI_Finalize();
}
