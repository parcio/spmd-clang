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

#ifndef SPMD_DATA_H
#define SPMD_DATA_H

#include "clang/AST/Decl.h"
#include <clang/AST/Expr.h>
#include <clang/AST/ASTContext.h>
#include "clang/Analysis/CFG.h"

#include <memory>
#include<vector>

//TODO make namespace?
/// Lists the currently supported MPI functions
enum MPIFuncs
{
    Unknown,
    Init,
    Finalize,
    CommRank,
    CommSize,
    Send,
    Recv,
    SendInit,
    RecvInit,
    Start,
    Wait,
    RequestFree,
    WaitAll,
    StartAll,
    Allreduce,
    Reduce,
    Barrier,
    Wtime,
};

/// The initial declaration of a SPMD variable that was annotated by the user 
/// and the corresponding call to an initialization function such as MPI_Comm_rank/size
struct SPMDVar
{
    const clang::VarDecl *var_decl;
    const clang::CallExpr *init_call;
};

/// Categorized MPI function call in the Code
struct MPICall
{
    const clang::CallExpr *call_expr;
    const clang::FunctionDecl *func_decl;
    MPIFuncs category;
};

/// TODO
struct FuncCFG
{
    std::unique_ptr<clang::CFG> cfg;
    const clang::FunctionDecl *func;
};

/// Holds all needed global analytical data about the SPMD behaviours of the code
/// and provides functions to access and filter information about the SPMD usage
class SPMDData
{
  public:
    SPMDData() = default;
    SPMDData(const SPMDData &) = default;
    SPMDData(SPMDData &&) = default;
    SPMDData &operator=(const SPMDData &) = default;
    SPMDData &operator=(SPMDData &&) = default;

    /// Simple member function for adding and categorizing uses of SPMD variables
    /// or SPMD functions in the code
    void add_rank_var(const clang::VarDecl *var_decl);
    void add_size_var(const clang::VarDecl *var_decl);
    void add_mpi_call(const clang::CallExpr *call_expr, const clang::FunctionDecl *func_decl);
    /// Adds an SPMD var initialization call such as MPI_Comm_rank to the corresponding
    /// SPMDVar if it exists.
    /// Returns false if the variable used in the function call is not known as an SPMD var
    bool add_rank_var_init(const clang::VarDecl *var_decl, const clang::CallExpr *call_expr);
    bool add_size_var_init(const clang::VarDecl *var_decl, const clang::CallExpr *call_expr);

    /// Return all function calls to SPMD initialization functions that do not use known SPMD vars
    std::vector<const clang::CallExpr*> get_unmatched_rank_inits();
    std::vector<const clang::CallExpr*> get_unmatched_size_inits();

    /// Return all SPMD vars for which no initialization has been found
    std::vector<const clang::VarDecl*> get_uninitialized_rank_vars();
    std::vector<const clang::VarDecl*> get_uninitialized_size_vars();

    /// Return the corresponding MPICall object for the given call expression
    MPICall* find_mpi_call(const clang::CallExpr *call_expr);

    /// TODO
    void add_cfg(std::unique_ptr<clang::CFG> &&cfg, const clang::FunctionDecl *func);

    /// Print out well formated version of the codes CFG
    void print_cfg(clang::ASTContext *ctx);

    void set_context(clang::ASTContext *ctx);

    void read_config();

    const std::vector<MPICall>& mpi_calls() const;

    const std::vector<FuncCFG>& cfg_parts() const;

    // TODO MAKE PRIVATE AGAIN
    clang::ASTContext *_ctx;

    const int process_count() const { return _process_count; }
    const int max_loop_iterations() const { return _max_loop_iterations; }

  private:
    /// Helper functions that search through data of member variables
    SPMDVar* find_rank_var(const clang::VarDecl *var_decl);
    SPMDVar* find_size_var(const clang::VarDecl *var_decl);
    std::vector<const clang::CallExpr*> get_mpi_calls_by_category(MPIFuncs category);

    std::vector<SPMDVar> _rank_vars;
    std::vector<SPMDVar> _size_vars;
    std::vector<MPICall> _mpi_calls;
    std::vector<FuncCFG> _cfg_parts;

    int _process_count;
    int _max_loop_iterations;
};

#endif
