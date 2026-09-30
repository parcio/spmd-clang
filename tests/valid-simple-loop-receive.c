#include "mpi.h"
#include <stdio.h>

int main()
{
    MPI_Init(NULL,NULL);
    int rank; 
    int size; 
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if(rank > 0) {
        int x = 42;
        MPI_Ssend(&x, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
    } else {
        for(int i = 1; i < size; i++) {
            int r = 0;
            MPI_Recv(&r, 1, MPI_INT, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            printf("From rank %d: %d\n", i, r);
        }
    }
    MPI_Finalize();
}
