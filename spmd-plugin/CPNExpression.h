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

#ifndef CPNEXPRESSION_H
#define CPNEXPRESSION_H

#include "CPNToken.h"
#include <llvm/Support/raw_ostream.h>
#include <optional>
#include <queue>
#include <string>
#include <vector>

class CPNMarking;

enum class ExpressionKind
{
    Simple,
    Or,
    Queue,
};

class CPNExpression {
public:
    // TODO refactor the super specific constructors into something more generic
    CPNExpression(CPNToken token) : _kind(ExpressionKind::Simple), _token(token) {}
    CPNExpression(ExpressionKind kind, std::vector<CPNToken> or_tokens ) : _kind(kind), _or_tokens(or_tokens) {}
    // Queue constructor
    CPNExpression(ExpressionKind kind, CPNToken t1); 
    CPNExpression(CPNExpression &&) = default;
    CPNExpression(const CPNExpression &) = default;
    CPNExpression &operator=(CPNExpression &&) = default;
    CPNExpression &operator=(const CPNExpression &) = default;
    ~CPNExpression() {}

    std::string to_string() const;
    const ExpressionKind& kind() const { return _kind; }
    const std::optional<CPNToken>& token() const { return _token; }
    const std::optional<std::vector<CPNToken>>& or_tokens() const { return _or_tokens; }

    void extend_queue(CPNToken token);
    std::optional<CPNToken> get_queue_token(const int idx) const;


private:
    ExpressionKind _kind;
    std::optional<CPNToken> _token;
    // TODO rename
    std::optional<std::vector<CPNToken>> _or_tokens;
    std::optional<std::vector<CPNToken>> _queue;

};

#endif
