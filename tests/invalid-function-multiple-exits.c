#include<stdio.h>
#include<mpi.h>

// Invalid code if called with more than two processes
// Will deadlock process rank > 1 will get stuck in the MPI_Recv
void test(int rank)
{
    printf("FUNCTION CALL WITH %d\n", rank);

    if(rank ==  1) {
        int msg = 42;
        MPI_Ssend(&msg, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
        return;
    }

    int res = 0;
    MPI_Recv(&res, 1, MPI_INT, 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    printf("Rank 0 received: %d\n", res);
    return;
}


int main()
{
    int rank;
    int size;

    MPI_Init(NULL, NULL);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    test(rank);

    printf("Program end\n");

    MPI_Finalize();
}
