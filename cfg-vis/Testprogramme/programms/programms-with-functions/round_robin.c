#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>


void round_robin(int rank, int size);


int main(int argc, char** argv)
{
    //int size;
    //int rank;

    MPI_Init(&argc, &argv);

    __attribute__((spmd("rank"))) int rank;
    __attribute__((spmd("size"))) int size;

    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    printf("%d: initialized %d processes\n", rank, size);
    round_robin(rank, size);
    printf("%d: terminates \n", rank);

    MPI_Finalize();
    return 0;
}

void round_robin(int rank, int size) 
{
    // Every process passes random number to his sucessor in a circle of four. 
    long int rand_cur, rand_prev;
    int rank_suc = (rank+1) % size;
    int rank_prev = rank == 0 ? size - 1 : rank - 1;
    MPI_Status status;

    srandom(time(NULL) + rank);
    rand_cur = random() / (RAND_MAX / 100);
    printf("%d: Processes random number is %ld\n", rank , rand_cur);

    // Processes with even ranks send first, odd ranks receive first.
    if (rank % 2)
    {
        printf("%d: Sending %ld to %d\n", rank, rand_cur, rank_suc);
        MPI_Send((void *)&rand_cur, 1, MPI_LONG, rank_suc, 1, MPI_COMM_WORLD);

        printf("%d: receiving from %d\n", rank, rank_prev);
        MPI_Recv((void *)&rand_prev, 1, MPI_LONG, rank_prev, 1, MPI_COMM_WORLD, &status);
    }
    else {
        printf("%d: receiving from %d\n", rank, rank_prev);
        MPI_Recv((void *)&rand_prev, 1, MPI_LONG, rank_prev, 1, MPI_COMM_WORLD, &status);

        printf("%d: Sending %ld to %d\n", rank, rand_cur, rank_suc);
        MPI_Send((void *)&rand_cur, 1, MPI_LONG, rank_suc, 1, MPI_COMM_WORLD);
    }

    printf("%d: I had %ld, and got %ld from %d\n", rank, rand_cur, rand_prev, rank_prev);
}
