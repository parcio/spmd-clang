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

#include "SPMDData.h"
#include "config.h"
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/raw_ostream.h>

using namespace clang;

void SPMDData::add_rank_var(const VarDecl *var_decl)
{
    _rank_vars.push_back(SPMDVar{var_decl, nullptr});
}

void SPMDData::add_size_var(const VarDecl *var_decl)
{
    _size_vars.push_back(SPMDVar{var_decl, nullptr});
}

void SPMDData::add_mpi_call(const CallExpr *call_expr, const FunctionDecl *func_decl)
{
    MPIFuncs category;
    if(func_decl->getNameAsString() == "MPI_Init") {
        category = MPIFuncs::Init;
    } else if(func_decl->getNameAsString() == "MPI_Finalize") {
        category = MPIFuncs::Finalize;
    } else if(func_decl->getNameAsString() == "MPI_Comm_rank") {
        category = MPIFuncs::CommRank;
    } else if(func_decl->getNameAsString() == "MPI_Comm_size") {
        category = MPIFuncs::CommSize;
    } else if(func_decl->getNameAsString() == "MPI_Send") {
        category = MPIFuncs::Send;
    } else if(func_decl->getNameAsString() == "MPI_Recv") {
        category = MPIFuncs::Recv;
    } else if(func_decl->getNameAsString() == "MPI_Send_init") {
        category = MPIFuncs::SendInit;
    } else if(func_decl->getNameAsString() == "MPI_Recv_init") {
        category = MPIFuncs::RecvInit;
    } else if(func_decl->getNameAsString() == "MPI_Start") {
        category = MPIFuncs::Start;
    } else if(func_decl->getNameAsString() == "MPI_Wait") {
        category = MPIFuncs::Wait;
    } else if(func_decl->getNameAsString() == "MPI_Request_free") {
        category = MPIFuncs::RequestFree;
    } else if(func_decl->getNameAsString() == "MPI_Waitall") {
        category = MPIFuncs::RequestFree;
    } else if(func_decl->getNameAsString() == "MPI_Startall") {
        category = MPIFuncs::RequestFree;
    // TODO newly added 19.07.23
    } else if(func_decl->getNameAsString() == "MPI_Allreduce") {
        category = MPIFuncs::Allreduce;
    } else if(func_decl->getNameAsString() == "MPI_Reduce") {
        category = MPIFuncs::Reduce;
    } else if(func_decl->getNameAsString() == "MPI_Barrier") {
        category = MPIFuncs::Barrier;
    } else if(func_decl->getNameAsString() == "MPI_Wtime") {
        category = MPIFuncs::Wtime;
    } else {
        category = MPIFuncs::Unknown;
    }
    _mpi_calls.push_back({call_expr, func_decl, category});
}

bool SPMDData::add_rank_var_init(const VarDecl *var_decl, const CallExpr *call_expr)
{
    auto var = find_rank_var(var_decl);
    if(var != nullptr){
        var->init_call = call_expr;
        return true;
    } else {
        return false;
    }
}

bool SPMDData::add_size_var_init(const VarDecl *var_decl, const CallExpr *call_expr)
{
    auto var = find_size_var(var_decl);
    if(var != nullptr){
        var->init_call = call_expr;
        return true;
    } else {
        return false;
    }
}

std::vector<const CallExpr*> SPMDData::get_unmatched_rank_inits()
{
    std::vector<const CallExpr*> unmatched_calls;
    auto rank_calls = get_mpi_calls_by_category(MPIFuncs::CommRank);
    for(auto call : rank_calls) {
        bool matched = false;
        for(auto rank_var : _rank_vars) {
            if(rank_var.init_call == call) {
                matched = true;
                break;
            }
        }
        if(!matched) {
            unmatched_calls.push_back(call);
        }
    }
    return unmatched_calls;
}

std::vector<const CallExpr*> SPMDData::get_unmatched_size_inits()
{
    std::vector<const CallExpr*> unmatched_calls;
    auto size_calls = get_mpi_calls_by_category(MPIFuncs::CommSize);
    for(auto call : size_calls) {
        bool matched = false;
        for(auto size_var : _size_vars) {
            if(size_var.init_call == call) {
                matched = true;
                break;
            }
        }
        if(!matched) {
            unmatched_calls.push_back(call);
        }
    }
    return unmatched_calls;
}

std::vector<const VarDecl*> SPMDData::get_uninitialized_rank_vars()
{
    std::vector<const VarDecl*> uninitialized;
    for(auto rank_var : _rank_vars) {
        if(rank_var.init_call == nullptr) {
            uninitialized.push_back(rank_var.var_decl);
        }
    }
    return uninitialized;
}

std::vector<const VarDecl*> SPMDData::get_uninitialized_size_vars()
{
    std::vector<const VarDecl*> uninitialized;
    for(auto size_var : _size_vars) {
        if(size_var.init_call == nullptr) {
            uninitialized.push_back(size_var.var_decl);
        }
    }
    return uninitialized;
}

MPICall* SPMDData::find_mpi_call(const CallExpr *call_expr)
{
    for(auto &call : _mpi_calls) {
        if(call.call_expr == call_expr) {
            return &call;
        }
    }
    return nullptr;
}

void SPMDData::add_cfg(std::unique_ptr<CFG> &&cfg, const FunctionDecl *func)
{
    _cfg_parts.push_back(FuncCFG{std::move(cfg), func});
}

void SPMDData::print_cfg(ASTContext *ctx)
{
    llvm::errs() << "=====\n";
    llvm::errs() << "Printing CFG:\n";
    llvm::errs() << "CFG Count: " << _cfg_parts.size() << "\n";
    for(auto &cfg : _cfg_parts) {
        if(cfg.cfg == nullptr) {
            llvm::errs() << "NULL\n";
        }
        if(cfg.func->isGlobal()) {
            llvm::errs() << "IS GLOBAL\n";
        }
        if(cfg.func->isInlineBuiltinDeclaration()) {
            llvm::errs() << "IS INLINE BUILTIN\n";
        }
        llvm::errs() << llvm::dyn_cast<DeclContext>(cfg.func)->getDeclKindName() << "\n";
        // cfg.func->dump();
        cfg.cfg->print(llvm::errs(), ctx->getLangOpts(), false);
        llvm::errs() << "---\n";
    }

}

void SPMDData::set_context(ASTContext *ctx)
{
    _ctx = ctx;
}

const std::vector<MPICall>& SPMDData::mpi_calls() const
{
    return _mpi_calls;
}

const std::vector<FuncCFG>& SPMDData::cfg_parts() const
{
    return _cfg_parts;
}

SPMDVar* SPMDData::find_rank_var(const VarDecl *var_decl)
{
    for(auto &var : _rank_vars) {
        if(var.var_decl == var_decl) {
            return &var;
        }
    }
    return nullptr;
}

SPMDVar* SPMDData::find_size_var(const VarDecl *var_decl)
{
    for(auto &var : _size_vars) {
        if(var.var_decl == var_decl) {
            return &var;
        }
    }
    return nullptr;
}

std::vector<const CallExpr*> SPMDData::get_mpi_calls_by_category(MPIFuncs category)
{
    std::vector<const CallExpr*> calls;
    for(auto &call : _mpi_calls) {
        if(call.category == category){
            calls.push_back(call.call_expr);
        }
    }
    return calls;
}

// TODO HACK implement this in a cleaner way
void SPMDData::read_config() 
{
    auto config_file = llvm::MemoryBuffer::getFile(".spmdclang");
    if(config_file.getError()) {
        llvm::errs() << "No SPMDClang config file .spmdclang found.\n";
        // set config options to default values
        llvm::errs() << "Setting SPMDClang process count to " << PROCESS_COUNT << "\n";
        _process_count = PROCESS_COUNT;
        llvm::errs() << "Setting SPMDClang max loop iterations to " << MAX_LOOP_ITERATIONS << "\n";
        _max_loop_iterations = MAX_LOOP_ITERATIONS;
        // TODO HACK FIXME make this not needed by passing SPMDData to all classes that require it
        // TODO FIXME does not work correctly; the set number is not used
        MAX_LOOP_ITERATIONS_RUNTIME = MAX_LOOP_ITERATIONS;
        return;
    } else {
        llvm::errs() << "Reading SPMDClang config file .spmdclang\n";
        llvm::StringRef content = config_file.get()->getBuffer();
        llvm::SmallVector<llvm::StringRef, 4> lines;
        content.split(lines, '\n', -1, false);

        for(auto line : lines) {
            line = line.trim();
            if(line.empty() || line.starts_with("#")) {
                continue;
            }

            if(line.starts_with("PROCESS_COUNT=")) {
                int count = std::stoi(line.split('=').second.trim().str());
                llvm::errs() << "Setting SPMDClang process count to " << count << "\n";
                _process_count = count;
            } else if(line.starts_with("MAX_LOOP_ITERATIONS=")) {
                int iterations = std::stoi(line.split('=').second.trim().str());
                llvm::errs() << "Setting SPMDClang max loop iterations to " << iterations << "\n";
                _max_loop_iterations = iterations;
                // TODO HACK FIXME make this not needed by passing SPMDData to all classes that require it
                // TODO FIXME does not work correctly; the set number is not used
                MAX_LOOP_ITERATIONS_RUNTIME = iterations;
            }
        }
    }
}
