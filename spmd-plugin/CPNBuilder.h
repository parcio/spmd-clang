/*
 *  SPMDClang - Clang plugin for correctness checking of MPI programs
 *  Copyright (C) 2026-2026  Michael Blesel
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef CPN_BUILDER_H
#define CPN_BUILDER_H

#include "CGContext.h"
#include "CPNNet.h"
#include <unordered_map>
#include <vector>

class CPNBuilder {
public:
    CPNBuilder() {}
    CPNBuilder(CPNBuilder &&) = default;
    CPNBuilder(const CPNBuilder &) = default;
    CPNBuilder &operator=(CPNBuilder &&) = default;
    CPNBuilder &operator=(const CPNBuilder &) = default;
    ~CPNBuilder() {}

    CPNNet build(const int mpi_size, const std::unordered_map<int, std::vector<SPMDCall>> &operations);

private:

    bool is_func_supported(const std::string &func_name) const;

    CPNExpression to_marker_expression(const SPMDCall op, const int transition_id) const;

    void add_operation(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const;
    void mpi_finalize(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const;
    void mpi_ssend(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const;
    void mpi_recv(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const;
    void mpi_barrier(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const;
    void mpi_bsend(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const;
    void mpi_send(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const;
    void mpi_rsend(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const;
    void mpi_isend(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const;
    void mpi_wait(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const;
    void mpi_irecv(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const;
    // Collectives
    void mpi_allgather(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const;
    void mpi_allreduce(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const;
    void mpi_alltoall(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const;
    void mpi_bcast(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const;
    void mpi_gather(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const;
    void mpi_reduce(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const;
    void mpi_scan(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const;
    void mpi_exscan(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const;
    void mpi_scatter(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const;
    // Non-blocking collectives
    void mpi_ibarrier(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const;
    void mpi_iallgather(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const;
    void mpi_iallreduce(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const;
    void mpi_ialltoall(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const;
    void mpi_ibcast(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const;
    void mpi_igather(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const;
    void mpi_ireduce(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const;
    void mpi_iscan(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const;
    void mpi_iexscan(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const;
    void mpi_iscatter(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const;

};

#endif
