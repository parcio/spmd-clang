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

#ifndef CPNNET_H
#define CPNNET_H

#include "CPNPlace.h"
#include "CPNTransition.h"
#include "CPNExpression.h"
#include <llvm/Support/raw_ostream.h>
#include <string>

class CPNNet {
public:
    CPNNet(const int mpi_size) : _mpi_size(mpi_size), _barrier(nullptr) {}
    CPNNet(CPNNet &&) = default;
    CPNNet(const CPNNet &) = default;
    CPNNet &operator=(CPNNet &&) = default;
    CPNNet &operator=(CPNNet &) = default;
    ~CPNNet() {}

    void dump() const;
    void output_cpn(const std::string &path) const;

    const int mpi_size() const { return _mpi_size; }
    const std::vector<CPNPlace>& places() const { return _places; }
    const std::vector<CPNTransition>& transitions() const { return _transitions; }
    std::vector<CPNTransition>& transitions() { return _transitions; }
    const CPNTransition* find_transition(const int id) const;
    const CPNTransition* find_transition(std::string name) const;
    CPNTransition* get_barrier() const { return _barrier; }

    void add_place(CPNPlace place); 
    void add_transition(CPNTransition transition);
    void add_in_arc(std::string from, std::string to, std::string binding);
    void add_out_arc(std::string from, std::string to, CPNExpression expression);

private:
    CPNPlace* find_place(std::string name);
    CPNTransition* find_transition(std::string name);
    int _mpi_size;
    std::vector<CPNPlace> _places;
    std::vector<CPNTransition> _transitions;
    // TODO allow for multiple barriers depending on the used communicator?
    // TODO handle this better than having a member for each possible collective
    // TODO are these actually needed? look how it works without this for Ibarrier e.g.
    CPNTransition *_barrier;
    CPNTransition *_allgather;
    CPNTransition *_allreduce;
    CPNTransition *_alltoall;
    CPNTransition *_bcast;
    CPNTransition *_gather;
    CPNTransition *_reduce;
    CPNTransition *_scatter;
};


#endif
