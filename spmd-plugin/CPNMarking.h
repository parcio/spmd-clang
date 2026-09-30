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

#ifndef CPNMARKING_H
#define CPNMARKING_H

#include "CPNNet.h"
#include "CPNToken.h"
#include <clang/AST/Type.h>
#include <map>
#include <set>

class CPNMarking {
public:
    CPNMarking(const CPNNet &net);
    CPNMarking(CPNMarking &&) = default;
    CPNMarking(const CPNMarking &m) = default;
    CPNMarking &operator=(CPNMarking &&) = default;
    CPNMarking &operator=(const CPNMarking &m) = default;
    ~CPNMarking() = default;

    void dump() const;
    void dump_ext() const;
    std::string to_string() const;

    const std::map<std::string, std::set<CPNToken>> &marking() const { return _marking; }

    const std::map<std::string, std::map<std::string, std::queue<CPNToken>>> &duplicate_tokens() const { return _duplicate_tokens; }

    void output_cpn(const std::string &path, const CPNNet &net) const;

    void add_token(std::string place, CPNToken token);
    void remove_token(std::string place, CPNToken token);

    std::set<CPNToken> get(const std::string &place) const { return _marking.at(place); }

    void transition_fired(const std::string &transition_name);
    const std::map<std::string, int>& transitions_fired() const { return _transitions_fired; }
    const int transition_fired_count(const std::string &transition_name) const;

private:
    // This needs to be called at the end of every modification to the marking
    // the string is used by the comparison operators
    void create_string();
    std::map<std::string, std::set<CPNToken>> _marking;
    // TODO BUG this currently only supports having having one kind of duplicate token per place at a time
    std::map<std::string, std::map<std::string, std::queue<CPNToken>>> _duplicate_tokens;
    std::map<std::string, int> _transitions_fired;
    std::string _string;
};

bool operator==(const CPNMarking &lhs, const CPNMarking &rhs);

bool operator<(const CPNMarking &lhs, const CPNMarking &rhs);

#endif 
