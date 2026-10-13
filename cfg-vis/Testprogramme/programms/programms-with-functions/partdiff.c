/*
 * partdiff - PDE solver for Gauß-Seidel and Jacobi methods
 * Copyright (C) 1997 Thomas Ludwig
 * Copyright (C) 1997 Thomas A. Zochler
 * Copyright (C) 1997 Andreas C. Schmidt
 * Copyright (C) 2007-2010 Julian M. Kunkel
 * Copyright (C) 2010-2021 Michael Kuhn
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* ************************************************************************ */
/* Include standard header file.                                            */
/* ************************************************************************ */
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <math.h>
#include <malloc.h>
#include <string.h>
#include <sys/time.h>
#include <mpi.h>

/* ************* */
/* Some defines. */
/* ************* */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define MAX_INTERLINES    10240
#define MAX_ITERATION     200000
#define MAX_THREADS       1024
#define METH_GAUSS_SEIDEL 1
#define METH_JACOBI       2
#define FUNC_F0           1
#define FUNC_FPISIN       2
#define TERM_PREC         1
#define TERM_ITER         2

struct calculation_arguments
{
	uint64_t N;            /* number of spaces between lines (lines=N+1) */
	uint64_t num_matrices; /* number of matrices */
	double   h;            /* length of a space between two lines */
	double*  M;            /* two matrices with real values */
};

struct calculation_results
{
	uint64_t m;
	uint64_t stat_iteration; /* number of current iteration */
	double   stat_precision; /* actual precision of all slaves in iteration */
};

struct options
{
	uint64_t number;         /* Number of threads */
	uint64_t method;         /* Gauss Seidel or Jacobi method of iteration */
	uint64_t interlines;     /* matrix size = interlines*8+9 */
	uint64_t inf_func;       /* inference function */
	uint64_t termination;    /* termination condition */
	uint64_t term_iteration; /* terminate if iteration number reached */
	double   term_precision; /* terminate if precision reached */
};

struct process_data
{
    int rank;
    int size;
    uint64_t chunk_size;
    uint64_t from;
    uint64_t to;
};
/* ************************************************************************ */
/* Global variables                                                         */
/* ************************************************************************ */

/* time measurement variables */
struct timeval start_time; /* time when program started */
struct timeval comp_time;  /* time when calculation completed */

static void
usage(char* name)
{
	printf("Usage: %s [num] [method] [lines] [func] [term] [prec/iter]\n", name);
	printf("\n");
	printf("  - num:       number of threads (1 .. %d)\n", MAX_THREADS);
	printf("  - method:    calculation method (1 .. 2)\n");
	printf("                 %1d: Gauss-Seidel\n", METH_GAUSS_SEIDEL);
	printf("                 %1d: Jacobi\n", METH_JACOBI);
	printf("  - lines:     number of interlines (0 .. %d)\n", MAX_INTERLINES);
	printf("                 matrixsize = (interlines * 8) + 9\n");
	printf("  - func:      interference function (1 .. 2)\n");
	printf("                 %1d: f(x,y) = 0\n", FUNC_F0);
	printf("                 %1d: f(x,y) = 2 * pi^2 * sin(pi * x) * sin(pi * y)\n", FUNC_FPISIN);
	printf("  - term:      termination condition ( 1.. 2)\n");
	printf("                 %1d: sufficient precision\n", TERM_PREC);
	printf("                 %1d: number of iterations\n", TERM_ITER);
	printf("  - prec/iter: depending on term:\n");
	printf("                 precision:  1e-4 .. 1e-20\n");
	printf("                 iterations:    1 .. %d\n", MAX_ITERATION);
	printf("\n");
	printf("Example: %s 1 2 100 1 2 100 \n", name);
}

static void
askParams(struct options* options, int argc, char** argv)
{
	int ret;

	if (argc < 7 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "-?") == 0)
	{
		usage(argv[0]);
		exit(0);
	}

	ret = sscanf(argv[1], "%" SCNu64, &(options->number));

	if (ret != 1 || !(options->number >= 1 && options->number <= MAX_THREADS))
	{
		usage(argv[0]);
		exit(1);
	}

	ret = sscanf(argv[2], "%" SCNu64, &(options->method));

	if (ret != 1 || !(options->method == METH_GAUSS_SEIDEL || options->method == METH_JACOBI))
	{
		usage(argv[0]);
		exit(1);
	}

	ret = sscanf(argv[3], "%" SCNu64, &(options->interlines));

	if (ret != 1 || !(options->interlines <= MAX_INTERLINES))
	{
		usage(argv[0]);
		exit(1);
	}

	ret = sscanf(argv[4], "%" SCNu64, &(options->inf_func));

	if (ret != 1 || !(options->inf_func == FUNC_F0 || options->inf_func == FUNC_FPISIN))
	{
		usage(argv[0]);
		exit(1);
	}

	ret = sscanf(argv[5], "%" SCNu64, &(options->termination));

	if (ret != 1 || !(options->termination == TERM_PREC || options->termination == TERM_ITER))
	{
		usage(argv[0]);
		exit(1);
	}

	if (options->termination == TERM_PREC)
	{
		ret = sscanf(argv[6], "%lf", &(options->term_precision));

		options->term_iteration = MAX_ITERATION;

		if (ret != 1 || !(options->term_precision >= 1e-20 && options->term_precision <= 1e-4))
		{
			usage(argv[0]);
			exit(1);
		}
	}
	else
	{
		ret = sscanf(argv[6], "%" SCNu64, &(options->term_iteration));

		options->term_precision = 0;

		if (ret != 1 || !(options->term_iteration >= 1 && options->term_iteration <= MAX_ITERATION))
		{
			usage(argv[0]);
			exit(1);
		}
	}
}

/* ************************************************************************ */
/* initVariables: Initializes some global variables                         */
/* ************************************************************************ */
static void
initVariables(struct calculation_arguments* arguments, struct calculation_results* results, struct options const* options, struct process_data* proc_data)
{
	arguments->N            = (options->interlines * 8) + 9 - 1;
	arguments->num_matrices = (options->method == METH_JACOBI) ? 2 : 1;
	arguments->h            = 1.0 / arguments->N;

	results->m              = 0;
	results->stat_iteration = 0;
	results->stat_precision = 0;

    uint64_t div = (arguments->N-1) / proc_data->size;
    uint64_t rest = (arguments->N-1) % proc_data->size;
    if((uint64_t)proc_data->rank < rest)
    {
        proc_data->chunk_size = div+3;
        proc_data->from = proc_data->rank * (div+1) + 1;
        proc_data->to = proc_data->from + div ;
    }
    else
    {
        proc_data->chunk_size = div+2;
        proc_data->from = proc_data->rank * (div) + rest + 1;
        proc_data->to = proc_data->from + div - 1;
    }
    /* printf("rank %d, chunk_size: %d, from %d, to %d\n", proc_data->rank, proc_data->chunk_size, proc_data->from, proc_data->to); */
}

/* ************************************************************************ */
/* freeMatrices: frees memory for matrices                                  */
/* ************************************************************************ */
static void
freeMatrices(struct calculation_arguments* arguments)
{
	free(arguments->M);
}

/* ************************************************************************ */
/* allocateMemory ()                                                        */
/* allocates memory and quits if there was a memory allocation problem      */
/* ************************************************************************ */
static void*
allocateMemory(size_t size)
{
	void* p;

	if ((p = malloc(size)) == NULL)
	{
		printf("Speicherprobleme! (%" PRIu64 " Bytes angefordert)\n", size);
		exit(1);
	}

	return p;
}

/* ************************************************************************ */
/* allocateMatrices: allocates memory for matrices                          */
/* ************************************************************************ */
static void
allocateMatrices(struct calculation_arguments* arguments)
{
	uint64_t const N = arguments->N;

	arguments->M = allocateMemory(arguments->num_matrices * (N + 1) * (N + 1) * sizeof(double));
}

static void
allocateMatricesMpi(struct calculation_arguments* arguments, struct process_data* proc_data)
{
	uint64_t const N = arguments->N;

	arguments->M = allocateMemory(arguments->num_matrices * proc_data->chunk_size * (N + 1) * sizeof(double));
}

/* ************************************************************************ */
/* initMatrices: Initialize matrix/matrices and some global variables       */
/* ************************************************************************ */
static void
initMatrices(struct calculation_arguments* arguments, struct options const* options)
{
	uint64_t g, i, j; /* local variables for loops */

	uint64_t const N = arguments->N;
	double const   h = arguments->h;

	typedef double(*matrix)[N + 1][N + 1];

	matrix Matrix = (matrix)arguments->M;

	/* initialize matrix/matrices with zeros */
	for (g = 0; g < arguments->num_matrices; g++)
	{
		for (i = 0; i <= N; i++)
		{
			for (j = 0; j <= N; j++)
			{
				Matrix[g][i][j] = 0.0;
			}
		}
	}

	/* initialize borders, depending on function (function 2: nothing to do) */
	if (options->inf_func == FUNC_F0)
	{
		for (g = 0; g < arguments->num_matrices; g++)
		{
			for (i = 0; i <= N; i++)
			{
				Matrix[g][i][0] = 1.0 - (h * i);
				Matrix[g][i][N] = h * i;
				Matrix[g][0][i] = 1.0 - (h * i);
				Matrix[g][N][i] = h * i;
			}

			Matrix[g][N][0] = 0.0;
			Matrix[g][0][N] = 0.0;
		}
	}
}

static void
initMatricesMpi(struct calculation_arguments* arguments, struct options const* options, struct process_data* proc_data)
{
	uint64_t g, i, j; /* local variables for loops */

	uint64_t const N = arguments->N;
	double const   h = arguments->h;

    uint64_t chunk_size = proc_data->chunk_size;

	typedef double(*matrix)[chunk_size][N + 1];

	matrix Matrix = (matrix)arguments->M;

	/* initialize matrix/matrices with zeros */
	for (g = 0; g < arguments->num_matrices; g++)
	{
		for (i = 0; i < chunk_size; i++)
		{
			for (j = 0; j <= N; j++)
			{
				Matrix[g][i][j] = 0.0;
			}
		}
	}

	/* initialize borders, depending on function (function 2: nothing to do) */
	if (options->inf_func == FUNC_F0)
	{
		for (g = 0; g < arguments->num_matrices; g++)
		{
			for (i = 0; i < chunk_size; i++)
			{
				Matrix[g][i][0] = 1.0 - (h * (proc_data->from + i - 1));
				Matrix[g][i][N] = h * (proc_data->from + i - 1);
			}

            if(proc_data->rank == 0)
            {
                for (i = 0; i <= N; i++)
                {
                    Matrix[g][0][i] = 1.0 - (h * i);
                }
                Matrix[g][0][N] = 0.0;
            }
            else if(proc_data->rank == proc_data->size-1)
            {
                for (i = 0; i <= N; i++)
                {
                    Matrix[g][chunk_size-1][i] = h * i;
                }
                Matrix[g][chunk_size-1][0] = 0.0;
            }
		}
	}
}

/* ************************************************************************ */
/* calculate: solves the equation                                           */
/* ************************************************************************ */
static void
calculate(struct calculation_arguments const* arguments, struct calculation_results* results, struct options const* options)
{
	int    i, j;        /* local variables for loops */
	int    m1, m2;      /* used as indices for old and new matrices */
	double star;        /* four times center value minus 4 neigh.b values */
	double residuum;    /* residuum of current iteration */
	double maxresiduum; /* maximum residuum value of a slave in iteration */

	int const    N = arguments->N;
	double const h = arguments->h;

	double pih    = 0.0;
	double fpisin = 0.0;

	int term_iteration = options->term_iteration;

	typedef double(*matrix)[N + 1][N + 1];

	matrix Matrix = (matrix)arguments->M;

	/* initialize m1 and m2 depending on algorithm */
	if (options->method == METH_JACOBI)
	{
		m1 = 0;
		m2 = 1;
	}
	else
	{
		m1 = 0;
		m2 = 0;
	}

	if (options->inf_func == FUNC_FPISIN)
	{
		pih    = M_PI * h;
		fpisin = 0.25 * (2 * M_PI * M_PI) * h * h;
	}

	while (term_iteration > 0)
	{
		maxresiduum = 0;

		/* over all rows */
		for (i = 1; i < N; i++)
		{
			double fpisin_i = 0.0;

			if (options->inf_func == FUNC_FPISIN)
			{
				fpisin_i = fpisin * sin(pih * (double)i);
			}

			/* over all columns */
			for (j = 1; j < N; j++)
			{
				star = 0.25 * (Matrix[m2][i - 1][j] + Matrix[m2][i][j - 1] + Matrix[m2][i][j + 1] + Matrix[m2][i + 1][j]);

				if (options->inf_func == FUNC_FPISIN)
				{
					star += fpisin_i * sin(pih * (double)j);
				}

				if (options->termination == TERM_PREC || term_iteration == 1)
				{
					residuum    = Matrix[m2][i][j] - star;
					residuum    = fabs(residuum);
					maxresiduum = (residuum < maxresiduum) ? maxresiduum : residuum;
				}

				Matrix[m1][i][j] = star;
			}
		}

		results->stat_iteration++;
		results->stat_precision = maxresiduum;

		/* exchange m1 and m2 */
		i  = m1;
		m1 = m2;
		m2 = i;

		/* check for stopping calculation depending on termination method */
		if (options->termination == TERM_PREC)
		{
			if (maxresiduum < options->term_precision)
			{
				term_iteration = 0;
			}
		}
		else if (options->termination == TERM_ITER)
		{
			term_iteration--;
		}
	}

	results->m = m2;
}

static void
calculate_jacobi_mpi(struct calculation_arguments const* arguments, struct calculation_results* results, struct options const* options, struct process_data* proc_data)
{
	int    i, j;        /* local variables for loops */
	int    m1, m2;      /* used as indices for old and new matrices */
	double star;        /* four times center value minus 4 neigh.b values */
	double residuum;    /* residuum of current iteration */
	double maxresiduum; /* maximum residuum value of a slave in iteration */

	int const    N = arguments->N;
	double const h = arguments->h;

	double pih    = 0.0;
	double fpisin = 0.0;

	int term_iteration = options->term_iteration;

    int rank = proc_data->rank;
    int size = proc_data->size;
    uint64_t from = proc_data->from;
    uint64_t chunk_size = proc_data->chunk_size;

	typedef double(*matrix)[chunk_size][N + 1];

	matrix Matrix = (matrix)arguments->M;

    m1 = 0;
    m2 = 1;

	if (options->inf_func == FUNC_FPISIN)
	{
		pih    = M_PI * h;
		fpisin = 0.25 * (2 * M_PI * M_PI) * h * h;
	}

	while (term_iteration > 0)
	{
		maxresiduum = 0;

        MPI_Request req_up, req_down;

        if(rank < size-1)
        {
            MPI_Isend(&Matrix[m2][chunk_size-2][0], N+1, MPI_DOUBLE, rank+1, 0, MPI_COMM_WORLD, &req_up);
            MPI_Recv(&Matrix[m2][chunk_size-1][0], N+1, MPI_DOUBLE, rank+1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        if(rank > 0 )
        {
            MPI_Isend(&Matrix[m2][1][0], N+1, MPI_DOUBLE, rank-1, 0, MPI_COMM_WORLD, &req_down);
            MPI_Recv(&Matrix[m2][0][0], N+1, MPI_DOUBLE, rank-1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }

		/* over all rows */
		for (i = 1; (uint64_t)i < chunk_size-1; i++)
		{
			double fpisin_i = 0.0;

			if (options->inf_func == FUNC_FPISIN)
			{
				fpisin_i = fpisin * sin(pih * (double)(i+from-1));
			}

			/* over all columns */
			for (j = 1; j < N; j++)
			{
				star = 0.25 * (Matrix[m2][i - 1][j] + Matrix[m2][i][j - 1] + Matrix[m2][i][j + 1] + Matrix[m2][i + 1][j]);

				if (options->inf_func == FUNC_FPISIN)
				{
					star += fpisin_i * sin(pih * (double)j);
				}

				if (options->termination == TERM_PREC || term_iteration == 1)
				{
					residuum    = Matrix[m2][i][j] - star;
					residuum    = fabs(residuum);
					maxresiduum = (residuum < maxresiduum) ? maxresiduum : residuum;
				}

				Matrix[m1][i][j] = star;
			}
		}

		results->stat_iteration++;
		results->stat_precision = maxresiduum;

        if (options->termination == TERM_PREC || term_iteration == 1)
        {
            MPI_Allreduce(MPI_IN_PLACE, &maxresiduum, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
            results->stat_precision = maxresiduum;
        }

        if(rank < size-1)
        {
            MPI_Wait(&req_up, MPI_STATUS_IGNORE);
        }
        if(rank > 0)
        {
            MPI_Wait(&req_down, MPI_STATUS_IGNORE);
        }

		/* exchange m1 and m2 */
		i  = m1;
		m1 = m2;
		m2 = i;

		/* check for stopping calculation depending on termination method */
		if (options->termination == TERM_PREC)
		{
			if (maxresiduum < options->term_precision)
			{
				term_iteration = 0;
			}
		}
		else if (options->termination == TERM_ITER)
		{
			term_iteration--;
		}
	}

	results->m = m2;
}

static void
calculate_gauss_mpi(struct calculation_arguments const* arguments, struct calculation_results* results, struct options const* options, struct process_data* proc_data)
{
	int    i, j;        /* local variables for loops */
	int    m1, m2;      /* used as indices for old and new matrices */
	double star;        /* four times center value minus 4 neigh.b values */
	double residuum;    /* residuum of current iteration */
	double maxresiduum; /* maximum residuum value of a slave in iteration */

	int const    N = arguments->N;
	double const h = arguments->h;

	double pih    = 0.0;
	double fpisin = 0.0;

	int term_iteration = options->term_iteration;

    int rank = proc_data->rank;
    int size = proc_data->size;
    uint64_t from = proc_data->from;
    uint64_t chunk_size = proc_data->chunk_size;
    int last_iter = 0;
    int prec_reached = 0;

    MPI_Request fini_recv_req;
    MPI_Request up_recv_req;
    MPI_Request down_send_req;
    MPI_Request up_send_req;
    MPI_Request res_send_req;
    MPI_Request stop_send_req;

	typedef double(*matrix)[chunk_size][N + 1];

	matrix Matrix = (matrix)arguments->M;

    m1 = 0;
    m2 = 0;

	if (options->inf_func == FUNC_FPISIN)
	{
		pih    = M_PI * h;
		fpisin = 0.25 * (2 * M_PI * M_PI) * h * h;
	}

    if (options->termination == TERM_PREC && rank == 0)
    {
        MPI_Irecv(&prec_reached, 1, MPI_INT, size-1, 42, MPI_COMM_WORLD, &fini_recv_req);
    }

    if(rank > 0)
    {
        MPI_Send(&Matrix[m2][1][0], N+1, MPI_DOUBLE, rank-1, 0, MPI_COMM_WORLD);
    }

	while (term_iteration > 0)
	{
		maxresiduum = 0;

        if(rank > 0)
        {
            MPI_Recv(&Matrix[m2][0][0], N+1, MPI_DOUBLE, rank-1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            MPI_Recv(&maxresiduum, 1, MPI_DOUBLE, rank-1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            MPI_Recv(&last_iter, 1, MPI_INT, rank-1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        if(rank < size-1)
        {
            MPI_Irecv(&Matrix[m2][chunk_size-1][0], N+1, MPI_DOUBLE, rank+1, 0, MPI_COMM_WORLD, &up_recv_req);
        }

		/* over all rows */
		for (i = 1; (uint64_t)i < chunk_size-1; i++)
		{
			double fpisin_i = 0.0;

			if (options->inf_func == FUNC_FPISIN)
			{
				fpisin_i = fpisin * sin(pih * (double)(i+from-1));
			}

            if(rank < size-1 && (uint64_t)i == chunk_size-2)
            {
                MPI_Wait(&up_recv_req, MPI_STATUS_IGNORE);
            }

			/* over all columns */
			for (j = 1; j < N; j++)
			{
				star = 0.25 * (Matrix[m2][i - 1][j] + Matrix[m2][i][j - 1] + Matrix[m2][i][j + 1] + Matrix[m2][i + 1][j]);

				if (options->inf_func == FUNC_FPISIN)
				{
					star += fpisin_i * sin(pih * (double)j);
				}

				if (options->termination == TERM_PREC || term_iteration == 1)
				{
					residuum    = Matrix[m2][i][j] - star;
					residuum    = fabs(residuum);
					maxresiduum = (residuum < maxresiduum) ? maxresiduum : residuum;
				}

				Matrix[m1][i][j] = star;
			}

            if(rank > 0 && i == 1 && last_iter == 0)
            {
                MPI_Isend(&Matrix[m2][1][0], N+1, MPI_DOUBLE, rank-1, 0, MPI_COMM_WORLD, &down_send_req);
            }
            if(rank < size-1 && (uint64_t)i == chunk_size-2)
            {
                MPI_Isend(&Matrix[m2][chunk_size-2][0], N+1, MPI_DOUBLE, rank+1, 0, MPI_COMM_WORLD, &up_send_req);
                MPI_Isend(&maxresiduum, 1, MPI_DOUBLE, rank+1, 0, MPI_COMM_WORLD, &res_send_req);
                if(rank == 0 && prec_reached == 1)
                {
                    last_iter = 1;
                }
                MPI_Isend(&last_iter, 1, MPI_INT, rank+1, 0, MPI_COMM_WORLD, &stop_send_req);
            }
		}

		results->stat_iteration++;
		results->stat_precision = maxresiduum;

		/* exchange m1 and m2 */
		i  = m1;
		m1 = m2;
		m2 = i;

		/* check for stopping calculation depending on termination method */
		if (options->termination == TERM_PREC)
		{
			if (rank == size-1 && maxresiduum < options->term_precision)
			{
                prec_reached = 1;
				MPI_Send(&prec_reached, 1, MPI_INT, 0 , 42, MPI_COMM_WORLD);
			}

            if(last_iter == 1)
            {
                term_iteration = 0;
            }
		}
		else if (options->termination == TERM_ITER)
		{
			term_iteration--;
		}

        if(rank > 0 && last_iter == 0)
        {
            MPI_Wait(&down_send_req, MPI_STATUS_IGNORE);
        }
        if(rank < size-1 )
        {
            MPI_Wait(&up_send_req, MPI_STATUS_IGNORE);
            MPI_Wait(&res_send_req, MPI_STATUS_IGNORE);
            MPI_Wait(&stop_send_req, MPI_STATUS_IGNORE);
        }
            
	}

    if(rank == size-1)
    {
        MPI_Send(&results->stat_precision, 1, MPI_DOUBLE, 0, 43, MPI_COMM_WORLD);
    }
    else if(rank == 0)
    {
        MPI_Recv(&results->stat_precision, 1, MPI_DOUBLE, size-1, 43, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        if(options->termination == TERM_PREC)
        {
            MPI_Wait(&fini_recv_req, MPI_STATUS_IGNORE);
        }
    }

	results->m = m2;
}

/* ************************************************************************ */
/*  displayStatistics: displays some statistics about the calculation       */
/* ************************************************************************ */
static void
displayStatistics(struct calculation_arguments const* arguments, struct calculation_results const* results, struct options const* options)
{
	int    N    = arguments->N;
	double time = (comp_time.tv_sec - start_time.tv_sec) + (comp_time.tv_usec - start_time.tv_usec) * 1e-6;

	printf("Berechnungszeit:    %f s\n", time);
	printf("Speicherbedarf:     %f MiB\n", (N + 1) * (N + 1) * sizeof(double) * arguments->num_matrices / 1024.0 / 1024.0);
	printf("Berechnungsmethode: ");

	if (options->method == METH_GAUSS_SEIDEL)
	{
		printf("Gauss-Seidel");
	}
	else if (options->method == METH_JACOBI)
	{
		printf("Jacobi");
	}

	printf("\n");
	printf("Interlines:         %" PRIu64 "\n", options->interlines);
	printf("Stoerfunktion:      ");

	if (options->inf_func == FUNC_F0)
	{
		printf("f(x,y) = 0");
	}
	else if (options->inf_func == FUNC_FPISIN)
	{
		printf("f(x,y) = 2 * pi^2 * sin(pi * x) * sin(pi * y)");
	}

	printf("\n");
	printf("Terminierung:       ");

	if (options->termination == TERM_PREC)
	{
		printf("Hinreichende Genaugkeit");
	}
	else if (options->termination == TERM_ITER)
	{
		printf("Anzahl der Iterationen");
	}

	printf("\n");
	printf("Anzahl Iterationen: %" PRIu64 "\n", results->stat_iteration);
	printf("Norm des Fehlers:   %e\n", results->stat_precision);
	printf("\n");
}

/****************************************************************************/
/** Beschreibung der Funktion displayMatrix:                               **/
/**                                                                        **/
/** Die Funktion displayMatrix gibt eine Matrix                            **/
/** in einer "ubersichtlichen Art und Weise auf die Standardausgabe aus.   **/
/**                                                                        **/
/** Die "Ubersichtlichkeit wird erreicht, indem nur ein Teil der Matrix    **/
/** ausgegeben wird. Aus der Matrix werden die Randzeilen/-spalten sowie   **/
/** sieben Zwischenzeilen ausgegeben.                                      **/
/****************************************************************************/
static void
displayMatrix(struct calculation_arguments* arguments, struct calculation_results* results, struct options* options)
{
	int x, y;

	int const interlines = options->interlines;
	int const N          = arguments->N;

	typedef double(*matrix)[N + 1][N + 1];

	matrix Matrix = (matrix)arguments->M;

	printf("Matrix:\n");

	for (y = 0; y < 9; y++)
	{
		for (x = 0; x < 9; x++)
		{
			printf("%7.4f", Matrix[results->m][y * (interlines + 1)][x * (interlines + 1)]);
		}

		printf("\n");
	}

	fflush(stdout);
}

/********************************************************************************************************************************/
/** rank und size sind jeweils die Werte von MPI rank und size                                                                 **/
/** from und to stehen für den globalen(!) Bereich von Zeilen für die dieser Prozess zuständig ist                             **/
/**                                                                                                                            **/
/** Beispiel mit 9 Matrixzeilen und 4 Prozessen:                                                                               **/
/** - rank 0 is verantwortlich fpr Zeilen 1-2, rank 1 für 3-4 rank 2 für 5-6 und rank 3 für 7.                                 **/
/**   Zeilen 0 und 8 sind nicht inkludiert weil sie nicht berechnet werden.                                                    **/
/** - Jeder Prozess speicher zwei Randzeilen in seiner Matrix                                                                  **/
/** - Zum Beispiel: Rank 2 hat vier Zeilen 0-3 aber berechnet nur 1-2 weil 0 und 3 beide Randzeilen für andere Prozesse sind.  **/
/**   Rank 2 ist verantwortlich für die globalen Zeilen 5-6.                                                                   **/
/********************************************************************************************************************************/
static
void
DisplayMatrixMpi (struct calculation_arguments* arguments, struct calculation_results* results, struct options* options, int rank, int size, int from, int to)
{
  int const elements = 8 * options->interlines + 9;

  int x, y;

  typedef double(*matrix)[to - from + 3][arguments->N + 1];
  matrix Matrix = (matrix)arguments->M;
  int m = results->m;
  
  MPI_Status status;

  /* first line belongs to rank 0 */
  if (rank == 0)
    from--;

  /* last line belongs to rank size - 1 */
  if (rank + 1 == size)
    to++;

  if (rank == 0)
    printf("Matrix:\n");

  for (y = 0; y < 9; y++)
  {
    int line = y * (options->interlines + 1);

    if (rank == 0)
    {
      /* check whether this line belongs to rank 0 */
      if (line < from || line > to)
      {
        /* use the tag to receive the lines in the correct order
         * the line is stored in Matrix[0], because we do not need it anymore */
        MPI_Recv(Matrix[m][0], elements, MPI_DOUBLE, MPI_ANY_SOURCE, 42 + y, MPI_COMM_WORLD, &status);
      }
    }
    else
    {
      if (line >= from && line <= to)
      {
        /* if the line belongs to this process, send it to rank 0
         * (line - from + 1) is used to calculate the correct local address */
        MPI_Send(Matrix[m][line - from + 1], elements, MPI_DOUBLE, 0, 42 + y, MPI_COMM_WORLD);
      }
    }

    if (rank == 0)
    {
      for (x = 0; x < 9; x++)
      {
        int col = x * (options->interlines + 1);
        if (line >= from && line <= to)
        {
          /* this line belongs to rank 0 */
          printf("%7.4f", Matrix[m][line][col]);
        }
        else
        {
          /* this line belongs to another rank and was received above */
          printf("%7.4f", Matrix[m][0][col]);
        }
      }

      printf("\n");
    }
  }

  fflush(stdout);
}

/* ************************************************************************ */
/*  main                                                                    */
/* ************************************************************************ */
int
main(int argc, char** argv)
{
	struct options               options;
	struct calculation_arguments arguments;
	struct calculation_results   results;
    struct process_data proc_data;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &proc_data.rank);
    MPI_Comm_size(MPI_COMM_WORLD, &proc_data.size);

	askParams(&options, argc, argv);

	initVariables(&arguments, &results, &options, &proc_data);

    if(proc_data.size == 1 || (uint64_t)proc_data.size >= arguments.N-1)
    {
        if(proc_data.rank == 0)
        {
            allocateMatrices(&arguments);
            initMatrices(&arguments, &options);

            gettimeofday(&start_time, NULL);
            calculate(&arguments, &results, &options);
            gettimeofday(&comp_time, NULL);

            displayStatistics(&arguments, &results, &options);
            displayMatrix(&arguments, &results, &options);

            freeMatrices(&arguments);
        }
    }
    else if(options.method == METH_JACOBI)
    {
        allocateMatricesMpi(&arguments, &proc_data);
        initMatricesMpi(&arguments, &options, &proc_data);
        gettimeofday(&start_time, NULL);
        calculate_jacobi_mpi(&arguments, &results, &options, &proc_data);
        gettimeofday(&comp_time, NULL);
        if(proc_data.rank == 0)
        {
            displayStatistics(&arguments, &results, &options);
        }
        MPI_Barrier(MPI_COMM_WORLD);
        DisplayMatrixMpi(&arguments, &results, &options, proc_data.rank, proc_data.size, proc_data.from, proc_data.to);

        freeMatrices(&arguments);
    }
    else if(options.method == METH_GAUSS_SEIDEL)
    {
        allocateMatricesMpi(&arguments, &proc_data);
        initMatricesMpi(&arguments, &options, &proc_data);
        gettimeofday(&start_time, NULL);
        calculate_gauss_mpi(&arguments, &results, &options, &proc_data);
        gettimeofday(&comp_time, NULL);
        if(proc_data.rank == 0)
        {
            displayStatistics(&arguments, &results, &options);
        }
        MPI_Barrier(MPI_COMM_WORLD);
        DisplayMatrixMpi(&arguments, &results, &options, proc_data.rank, proc_data.size, proc_data.from, proc_data.to);

        freeMatrices(&arguments);
    }

    MPI_Finalize();
	return 0;
}
