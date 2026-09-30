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

#ifndef CPNTRANSITION_H
#define CPNTRANSITION_H

#include "CPNGuard.h"
#include "CPNArc.h"
#include <clang/AST/Expr.h>
#include <optional>
#include <set>
#include <string>
#include <vector>

class CPNMarking;
class CPNNet;

// TODO simplify the terribe type definitions in here
class CPNTransition {
public:
    CPNTransition(std::string name, CPNGuard guard, std::string place, const clang::CallExpr *ast_expr, std::int64_t request_id = -1);
    CPNTransition(CPNTransition &&) = default;
    CPNTransition(const CPNTransition &) = default;
    CPNTransition &operator=(CPNTransition &&) = default;
    CPNTransition &operator=(const CPNTransition &) = default;
    ~CPNTransition() {}

    const std::string &name() const { return _name; }
    int id() const { return _id; }
    const std::int64_t& request_id() const { return _request_id; }
    const std::optional<CPNGuard>& guard() const { return _guard; }
    const std::vector<CPNArcIn>& in_arcs() const { return _in_arcs; }
    const std::vector<CPNArcOut>& out_arcs() const { return _out_arcs; }
    std::vector<CPNArcOut>& out_arcs() { return _out_arcs; }

    void add_in_arc(CPNArcIn arc);
    void add_out_arc(CPNArcOut arc);

    const std::map<std::string, std::vector<const clang::CallExpr*>>& ast_expr() const { return _ast_expr; }
    void add_ast_expr(std::string place, const clang::CallExpr *expr) { _ast_expr[place].push_back(expr); }

    bool could_fire(const CPNMarking &marking) const;
    bool can_fire(const CPNMarking &marking, const CPNNet &net) const;
    bool place_can_fire(const CPNMarking &marking, const CPNNet &net, const std::string &place) const;
    std::optional<CPNMarking> fire(const CPNMarking &marking, const CPNNet &net) const;

private:
    std::set<std::pair<std::map<std::string, std::pair<CPNArcIn, CPNToken>>, CPNEvaluationArgs>> create_valid_combinations(const CPNMarking &marking, const CPNNet &net) const;
    std::string _name;
    int _id;
    // TODO is this placed optimally here? its already included in the guard expression
    std::int64_t _request_id;
    std::optional<CPNGuard> _guard;
    std::vector<CPNArcIn> _in_arcs;
    std::vector<CPNArcOut> _out_arcs;
    std::map<std::string, std::vector<const clang::CallExpr*>> _ast_expr;

    static inline int _ID_COUNTER;
};


#endif
