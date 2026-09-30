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

#ifndef CPNARC_H
#define CPNARC_H

#include <string>
#include "CPNExpression.h"

enum CPNEvaluationArgType
{
    None = 1,
    CreateContinue = 1 << 1,
    QueueExpression = 1 << 2,
    CreateRequest = 1 << 3,
};

struct CPNEvaluationArgs {
    CPNEvaluationArgType type; 
    std::optional<int> request_id;
    std::optional<int> source;
    std::optional<int> tag;
};

bool operator<(const CPNEvaluationArgs &lhs, const CPNEvaluationArgs &rhs);

inline CPNEvaluationArgType operator|(CPNEvaluationArgType a, CPNEvaluationArgType b)
{
    return static_cast<CPNEvaluationArgType>(static_cast<int>(a) | static_cast<int>(b));
}

inline CPNEvaluationArgType operator&(CPNEvaluationArgType a, CPNEvaluationArgType b)
{
    return static_cast<CPNEvaluationArgType>(static_cast<int>(a) & static_cast<int>(b));
}

class CPNArc {
public:
    CPNArc(std::string place, std::string transition) : _place(place), _transition(transition) {}
    CPNArc(CPNArc &&) = default;
    CPNArc(const CPNArc &) = default;
    CPNArc &operator=(CPNArc &&) = default;
    CPNArc &operator=(const CPNArc &) = default;
    ~CPNArc() {}

    const std::string& place() const { return _place; }
    const std::string& transition() const { return _transition; }

private:
    std::string _place;
    std::string _transition;
};

class CPNArcIn : public CPNArc
{
public:
    CPNArcIn(std::string from, std::string to, std::string binding) : CPNArc(from, to), _binding(binding) {}
    CPNArcIn(CPNArcIn &&) = default;
    CPNArcIn(const CPNArcIn &) = default;
    CPNArcIn &operator=(CPNArcIn &&) = default;
    CPNArcIn &operator=(const CPNArcIn &) = default;
    ~CPNArcIn() {}

    const std::string& binding() const { return _binding; }

private:
    std::string _binding; 
};

class CPNArcOut : public CPNArc
{
public:
    CPNArcOut(std::string from, std::string to, CPNExpression expression) : CPNArc(to, from), _expression(expression) {}
    CPNArcOut(CPNArcOut &&) = default;
    CPNArcOut(const CPNArcOut &) = default;
    CPNArcOut &operator=(CPNArcOut &&) = default;
    CPNArcOut &operator=(const CPNArcOut &) = default;
    ~CPNArcOut() {}

    const CPNExpression& expression() const { return _expression; }
    CPNExpression& expression() { return _expression; }
    
    std::pair<std::string, CPNToken> generate_token(CPNEvaluationArgs args, CPNMarking &marking) const;

private:
   CPNExpression _expression;
};

bool operator<(const CPNArcIn &lhs, const CPNArcIn &rhs);


#endif
