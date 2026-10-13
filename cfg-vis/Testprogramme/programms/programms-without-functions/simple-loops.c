#include <mpi.h>
#include <stdio.h>

int main(int argc, char** argv) 
{
    int size;
    int rank;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    printf("%d: initialized %d processes\n", rank, size);
    MPI_Status status;

    int a = 1;
    int b = 2;
    int result_a, result_b;

    if(rank == 0)
    {
        int res = 1;
        for(int i=0; i<10; i++) {
            res = res + a;
        }
        result_a = res;
        MPI_Recv((void *)&result_b, 1, MPI_INT, 1, 1, MPI_COMM_WORLD, &status);
        printf("Calculated result_a=%d and got result_b=%.");
    }
    else if(rank == 1) 
    {
        int res = 1;
        for(int i=0; i<10; i++) {
            res = res + b;
        }
        result_b = res;
        MPI_Send((void *)&result_b, 1, MPI_INT, 0, 1, MPI_COMM_WORLD);
    }
    else {
        printf("Nothing to do ... ");
    }

    MPI_Finalize();
    return 0;
}

