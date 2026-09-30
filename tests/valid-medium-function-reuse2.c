#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

void sending(int target, int tag, double value)
{
    MPI_Bsend(&value, 1, MPI_DOUBLE, target, tag, MPI_COMM_WORLD);
}

void receiving(int rank, int source, int tag)
{
    double x;
    MPI_Recv(&x, 1, MPI_DOUBLE, source, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    printf("Rank %d received %f\n", rank, x);
}

int get_tag(int tag) {
    return tag;
}


int main()
{
    MPI_Init(NULL, NULL);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int *bsend_buffer = (int*)malloc(1000 * sizeof(int));
    MPI_Buffer_attach(bsend_buffer, 1000 * sizeof(int));

    int next = (rank + 1) % size;
    int prev = (rank - 1 + size) % size;

    double message = 2.0;
    sending(next, get_tag(1), message);
    message = 1.0;
    sending(next, get_tag(0), message);

    int tag1;
    int tag2;
    tag1 = get_tag(1);
    tag2 = get_tag(0);
    receiving(rank, prev, tag2);
    receiving(rank, prev, tag1);

    MPI_Barrier(MPI_COMM_WORLD);

    free(bsend_buffer);
    MPI_Finalize();
}
