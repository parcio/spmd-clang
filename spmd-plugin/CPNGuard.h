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

#ifndef CPNGUARD_H
#define CPNGUARD_H

#include "CPNArc.h"
#include "CPNExpression.h"
#include <map>
#include <string>
#include <unordered_map>

enum class CPNGuardKind 
{
    Init,
    Ssend,
    Bsend,
    Send,
    Rsend,
    Recv,
    Isend,
    Wait,
    Irecv,
    Complete,
    Barrier,
    Finalize,
    Blocking,
    Allgather,
    Allreduce,
    Alltoall,
    Bcast,
    Gather,
    Reduce,
    Scan,
    Exscan,
    Scatter,
    Ibarrier,
    IbarrierComplete,
    Iallgather,
    IallgatherComplete,
    Iallreduce,
    IallreduceComplete,
    Ialltoall,
    IalltoallComplete,
    Ibcast,
    IbcastComplete,
    Igather,
    IgatherComplete,
    Ireduce,
    IreduceComplete,
    Iscan,
    IscanComplete,
    Iexscan,
    IexscanComplete,
    Iscatter,
    IscatterComplete,
};



class CPNGuard {
public:
    CPNGuard(CPNGuardKind kind) : _kind(kind) { _guard_string = create_string(); }
    CPNGuard(CPNGuardKind kind, std::unordered_map<std::string, CPNExpression> expression) : _kind(kind), _expression(expression) { _guard_string = create_string(); }
    CPNGuard(CPNGuard &&) = default;
    CPNGuard(const CPNGuard &) = default;
    CPNGuard &operator=(CPNGuard &&) = default;
    CPNGuard &operator=(const CPNGuard &) = default;
    ~CPNGuard() {}

    const std::string to_string() const { return _guard_string; }
    const CPNGuardKind kind() const { return _kind; }
    const std::unordered_map<std::string, CPNExpression>& expression() const { return _expression; }

    bool evaluate(std::map<std::string, std::pair<CPNArcIn, CPNToken>>) const;
    bool evaluate_partial(std::string binding, std::pair<CPNArcIn, CPNToken>) const;

private:
    std::string create_string() const;

    CPNGuardKind _kind;
    std::unordered_map<std::string, CPNExpression> _expression;
    std::string _expression_string;
    std::string _guard_string;
};


#endif
