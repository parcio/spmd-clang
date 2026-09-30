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

#ifndef CPNTOKEN_H
#define CPNTOKEN_H

#include <clang/AST/DependenceFlags.h>
#include <cstdint>
#include <string>
#include <vector>

enum class TokenKind
{
    EmptyMarker,
    InitMarker,
    FinalizeMarker,
    MessageMarker,
    SsendMarker,
    BsendMarker,
    SendMarker,
    RsendMarker,
    RecvMarker,
    ContinueMarker,
    IsendMarker,
    WaitMarker,
    IrecvMarker,
    CompleteMarker,
    RequestMarker,
    BarrierMarker,
    AllgatherMarker,
    AllreduceMarker,
    AlltoallMarker,
    BcastMarker,
    GatherMarker,
    ReduceMarker,
    ScanMarker,
    ExscanMarker,
    ScatterMarker,
    IbarrierMarker,
    IbarrierCompleteMarker,
    IallgatherMarker,
    IallgatherCompleteMarker,
    IallreduceMarker,
    IallreduceCompleteMarker,
    IalltoallMarker,
    IalltoallCompleteMarker,
    IbcastMarker,
    IbcastCompleteMarker,
    IgatherMarker,
    IgatherCompleteMarker,
    IreduceMarker,
    IreduceCompleteMarker,
    IscanMarker,
    IscanCompleteMarker,
    IexscanMarker,
    IexscanCompleteMarker,
    IscatterMarker,
    IscatterCompleteMarker,
};

class CPNToken {
public:
    CPNToken(TokenKind kind, std::int64_t id, std::vector<std::int64_t> args, std::int64_t request_id = -1);
    CPNToken(CPNToken &&) = default;
    CPNToken(const CPNToken &) = default;
    CPNToken &operator=(CPNToken &&) = default;
    CPNToken &operator=(const CPNToken &) = default;
    ~CPNToken() = default;

    // TODO optional getters for specific args
    // makes it easier in case the order of args for a Token gets changed

    const TokenKind& kind() const { return _kind; }
    const std::int64_t& transition_id() const { return _transition_id; }
    const std::int64_t& request_id() const { return _request_id; }
    const std::vector<std::int64_t>& args() const { return _args; }

    std::string to_string() const { return _token_string; }
    std::string arguments_to_string() const { return _argument_string; }
    std::string kind_to_string() const;

private:
    TokenKind _kind;   
    std::int64_t _transition_id;
    std::int64_t _request_id;
    // argument order for TokenKinds it applies to [src, dest, tag, comm]
    std::vector<std::int64_t> _args;
    std::string _token_string;
    std::string _argument_string;
};

bool operator==(const CPNToken &lhs, const CPNToken &rhs);

bool operator<(const CPNToken &lhs, const CPNToken &rhs);

#endif

