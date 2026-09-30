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

#ifndef CGContext_H
#define CGContext_H

#include "CGValue.h"
#include "Loop.h"
#include <clang/AST/Decl.h>
#include <clang/AST/Expr.h>
#include <clang/AST/Stmt.h>
#include <clang/Analysis/CFG.h>
#include <clang/Basic/CodeGenOptions.h>
#include <llvm/ADT/MapVector.h>
#include <optional>
#include <utility>
#include <vector>


struct SPMDCall
{
    const clang::CallExpr* expr;
    std::vector<std::pair<const clang::Expr*, CGValue>> args;
    CGValue rank;
};

class CGContext
{
public:
    CGContext(const CGValue mpi_rank, const CGValue mpi_size): _mpi_rank(mpi_rank), _mpi_size(mpi_size) {}
    CGContext(CGContext &&) = default;
    CGContext(const CGContext &) = default;
    CGContext &operator=(CGContext &&) = default;
    CGContext &operator=(const CGContext &) = default;
    ~CGContext();

    std::vector<SPMDCall> mpi_calls;

    CGContext clean_copy() const;

    const CGValue &mpi_rank() const { return _mpi_rank; }
    const CGValue &mpi_size() const { return _mpi_size; }
    const llvm::MapVector<const clang::ValueDecl*, CGValue> &declarations() const { return _declarations; }
    const llvm::MapVector<const clang::Expr*, CGValue> &expressions() const { return _expressions; }
    const llvm::MapVector<const clang::ParmVarDecl*, CGValue> &func_parameters() const { return _func_parameters; }
    const llvm::MapVector<const clang::ReturnStmt*, CGValue> &func_returns() const { return _func_returns; }
    // const std::map<std::string, int> &visited_nodes() const { return _visited_nodes; }
    const std::map<const Loop*, int> &loop_iterations() const { return _loop_iterations; }
    const std::map<const Loop*, int> &loop_starts() const { return _loop_starts; }

    void dump() const;

    void bind(const clang::ValueDecl *decl, CGValue value);
    void bind(const clang::Expr *expr, CGValue value);
    void bind(const clang::ParmVarDecl *parm, CGValue value);
    void bind(const clang::ReturnStmt *ret, CGValue value);

    void bind(const clang::ValueDecl *decl, const clang::Expr *expr);
    void bind(const clang::Expr *expr1, const clang::Expr *expr2);
    void bind(const clang::ParmVarDecl *parm, const clang::Expr *expr);
    void bind(const clang::ReturnStmt *ret, const clang::Expr *expr);

    std::optional<CGValue> get(const clang::ValueDecl *decl) const;
    std::optional<CGValue> get(const clang::Expr *expr) const;
    std::optional<CGValue> get(const clang::ParmVarDecl *parm) const;
    std::optional<CGValue> get(const clang::ReturnStmt *parm) const;

    // void add_node_visit(const std::string name);
    // void reset_node_visits(const std::string name);
    // int get_node_visits(const std::string name) const;
    int get_loop_iteration(const Loop *loop) const;
    void increment_loop_iteration(const Loop *loop);
    void reset_loop_iterations(const Loop *loop);
    int get_loop_starts(const Loop *loop) const;
    void increment_loop_starts(const Loop *loop);
    


private:
    // TODO make const and public
    CGValue _mpi_rank;
    CGValue _mpi_size;
    llvm::MapVector<const clang::ValueDecl*, CGValue> _declarations;
    llvm::MapVector<const clang::Expr*, CGValue> _expressions;
    llvm::MapVector<const clang::ParmVarDecl*, CGValue> _func_parameters;
    llvm::MapVector<const clang::ReturnStmt*, CGValue> _func_returns;
    // Used for maximum loop iterations
    std::map<const Loop*, int> _loop_iterations;
    std::map<const Loop*, int> _loop_starts;
};

bool operator==(const CGContext &lhs, const CGContext &rhs);    

CGContext operator+(const CGContext &lhs, const CGContext &rhs);


#endif 
