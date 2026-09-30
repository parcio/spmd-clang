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

#include "CGContext.h"
#include "CGNode.h"
#include "CGValue.h"
#include "debug.h"
#include "utils.h"
#include <clang/AST/Expr.h>
#include <clang/Basic/LLVM.h>
#include <llvm/Support/raw_ostream.h>
#include <optional>


CGContext::~CGContext() 
{
    //TODO
}

CGContext CGContext::clean_copy() const
{
    auto new_ctx = *this;
    new_ctx.mpi_calls.clear();
    new_ctx._expressions.clear();
    return new_ctx;
}

void CGContext::dump() const
{
    llvm::errs() << "MPI Rank: ";
    _mpi_rank.dump();
    llvm::errs() << "\n";
    llvm::errs() << "MPI Size: ";
    _mpi_size.dump();
    llvm::errs() << "\n";

    llvm::errs() << "Declarations:\n";
    for(const auto &decl : _declarations) {
        llvm::errs() << "    " << decl.first->getNameAsString() << " -> ";
        decl.second.dump();
        llvm::errs() << "\n";
    }
    llvm::errs() << "Expressions:\n";
    for(const auto &expr : _expressions) {
        expr.first->dumpColor();
        llvm::errs() << "    -> ";
        expr.second.dump();
        llvm::errs() << "\n";
    }
    llvm::errs() << "Function Parameters:\n";
    for(const auto &parm : _func_parameters) {
        llvm::errs() << "    " << parm.first->getNameAsString() << " -> ";
        parm.second.dump();
        llvm::errs() << "\n";
    }
    llvm::errs() << "Function Returns:\n";
    for(const auto &ret : _func_returns) {
        ret.first->dumpColor();
        llvm::errs() << "    -> ";
        ret.second.dump();
        llvm::errs() << "\n";
    }
    llvm::errs() << "MPI Calls:\n";
    for(const auto &call : mpi_calls) {
        llvm::errs() << call.expr->getDirectCallee()->getNameAsString() << "\n";
        for(const auto &arg : call.args) {
            llvm::errs() << "ARG:\n";
            arg.first->dump();
            llvm::errs() << "    -> ";
            arg.second.dump();
            llvm::errs() << "\n";
        }
        llvm::errs() << "\n";
    }
    llvm::errs() << "Loop Iterations:\n";
    for(const auto &node : _loop_iterations) {
        llvm::errs() << "   " << node.first->header->name() << " : " << node.second << "\n";
    }
    llvm::errs() << "Loop Starts:\n";
    for(const auto &node : _loop_starts) {
        // TODO BUG figure out why there are nullptrs here
        if(node.first) {
            llvm::errs() << "   " << node.first->header->name() << " : " << node.second << "\n";
        }
    }
}

void CGContext::bind(const clang::ValueDecl *decl, CGValue value)
{
    const bool DEBUG = CGCONTEXT_DEBUG;
    
    auto iter = _declarations.find(decl);
    if(iter == _declarations.end()) {
        if(DEBUG) {
            llvm::errs() << "Binding Declaration " << decl->getNameAsString() << " to Value ";
            value.dump();
            llvm::errs() << "\n";
        }
        _declarations.insert({decl, value});
    } else {
        if(DEBUG) {
            llvm::errs() << "Overwriting Value of " << decl->getNameAsString() << " with ";
            value.dump();
            llvm::errs() << "\n";
        }
        iter->second = value;
    }   
}


void CGContext::bind(const clang::Expr *expr, CGValue value) 
{
    const bool DEBUG = CGCONTEXT_DEBUG;
    
    auto iter = _expressions.find(expr);
    if(iter == _expressions.end()) {
        if(DEBUG) {
            llvm::errs() << "Binding Expression " << expr << " to value ";
            value.dump();
            llvm::errs() << "\n";
        }
        _expressions.insert({expr, value});
    } else {
        if(DEBUG) {
            llvm::errs() << "Overwriting Value of " << expr << " with ";
            value.dump();
            llvm::errs() << "\n";
        }
        iter->second = value;
    }
}

void CGContext::bind(const clang::ParmVarDecl *parm, CGValue value)
{
    const bool DEBUG = CGCONTEXT_DEBUG;

    auto iter = _func_parameters.find(parm);
    if(iter == _func_parameters.end()) {
        if(DEBUG) {
            llvm::errs() << "Binding Parameter " << parm->getNameAsString() << " to value ";
            value.dump();
            llvm::errs() << "\n";
        }
        _func_parameters.insert({parm, value});
    } else {
        if(DEBUG) {
            llvm::errs() << "Overwriting Parameter " << parm->getNameAsString() << " with ";
            value.dump();
            llvm::errs() << "\n";
        }
        iter->second = value;
    }
}

void CGContext::bind(const clang::ReturnStmt *ret, CGValue value)
{
    const bool DEBUG = CGCONTEXT_DEBUG;

    auto iter = _func_returns.find(ret);
    if(iter == _func_returns.end()) {
        if(DEBUG) {
            llvm::errs() << "Binding return statement to value ";
            value.dump();
            llvm::errs() << "\n";
        }
        _func_returns.insert({ret, value});
    } else {
        if(DEBUG) {
            llvm::errs() << "Overwriting Value of return statement with ";
            value.dump();
            llvm::errs() << "\n";
        }
        iter->second = value;
    }
}


void CGContext::bind(const clang::ValueDecl *decl, const clang::Expr *expr)
{
    const bool DEBUG = CGCONTEXT_DEBUG;

    CGValue value = CGValue::top();
    if(auto v = this->get(expr)) {
        value = v.value();
    } else {
        value = CGValue::top();
        if(DEBUG) {
            llvm::errs() << "No value for expression found: " << expr << "\n";
            llvm::errs() << "Setting value to top\n";
        }
    }
    bind(decl, value);
}

void CGContext::bind(const clang::Expr *expr1, const clang::Expr *expr2)
{
    const bool DEBUG = CGCONTEXT_DEBUG;

    CGValue value = CGValue::top();
    if(auto v = this->get(expr2)) {
        value = v.value();
    } else {
        value = CGValue::top();
        if(DEBUG) {
            llvm::errs() << "No value for expression found: " << expr2 << "\n";
            llvm::errs() << "Setting value to top\n";
        }
    }
    bind(expr1, value);
}

void CGContext::bind(const clang::ParmVarDecl *parm, const clang::Expr *expr)
{
    const bool DEBUG = CGCONTEXT_DEBUG;

    CGValue value = CGValue::top();
    if(auto v = this->get(expr)) {
        value = v.value();
    } else {
        value = CGValue::top();
        if(DEBUG) {
            llvm::errs() << "No value for expression found: " << expr << "\n";
            llvm::errs() << "Setting value to top\n";
        }
    }
    bind(parm, value);
}

void CGContext::bind(const clang::ReturnStmt *ret, const clang::Expr *expr)
{
    const bool DEBUG = CGCONTEXT_DEBUG;

    CGValue value = CGValue::top();
    if(auto v = this->get(expr)) {
        value = v.value();
    } else {
        value = CGValue::top();
        if(DEBUG) {
            llvm::errs() << "No value for expression found: " << expr << "\n";
            llvm::errs() << "Setting value to top\n";
        }
    }
    bind(ret, value);
}


std::optional<CGValue> CGContext::get(const clang::ValueDecl *decl) const 
{
    if(!decl) {
        warning_print("Tried to call CGContext::get with nullptr");
        return std::nullopt;
    }
    auto iter = _declarations.find(decl);
    if(iter == _declarations.end()) {
        return std::nullopt;
    }
    return iter->second;
}

std::optional<CGValue> CGContext::get(const clang::Expr *expr) const
{
    if(!expr) {
        warning_print("Tried to call CGContext::get with nullptr");
        return std::nullopt;
    }
    if(const auto *call_expr = llvm::dyn_cast<clang::CallExpr>(expr)) {
        if(const auto return_value = this->get(call_expr->getDirectCallee())) {
            return return_value;
        } else {
            return std::nullopt;
        }
    } else {
        auto iter = _expressions.find(expr);
        if(iter == _expressions.end()) {
            return std::nullopt;
        }
        return iter->second;
    }
}

std::optional<CGValue> CGContext::get(const clang::ParmVarDecl *parm) const
{
    if(!parm) {
        warning_print("Tried to call CGContext::get with nullptr");
        return std::nullopt;
    }
    auto iter = _func_parameters.find(parm);
    if(iter == _func_parameters.end()) {
        return std::nullopt;
    }
    return iter->second;
}

std::optional<CGValue> CGContext::get(const clang::ReturnStmt *ret) const
{
    if(!ret) {
        warning_print("Tried to call CGContext::get with nullptr");
        return std::nullopt;
    }
    auto iter = _func_returns.find(ret);
    if(iter == _func_returns.end()) {
        return std::nullopt;
    }
    return iter->second;
}

// void CGContext::add_node_visit(const std::string name)
// {
//     if(_visited_nodes.count(name)) {
//         _visited_nodes[name]++;
//     } else {
//         _visited_nodes.insert({name, 1});
//     }
// }
//
// void CGContext::reset_node_visits(const std::string name)
// {
//     if(_visited_nodes.count(name)) {
//         _visited_nodes[name] = 0;
//     }
// }
//
// int CGContext::get_node_visits(const std::string name) const
// {
//     if(_visited_nodes.count(name)) {
//         return _visited_nodes.at(name);
//     } else {
//         return 0;
//     }
// }

int CGContext::get_loop_iteration(const Loop *loop) const
{
    if(_loop_iterations.count(loop)) {
        return _loop_iterations.at(loop);
    } else {
        return 0;
    }
}

void CGContext::increment_loop_iteration(const Loop *loop)
{
    if(_loop_iterations.count(loop)) {
        _loop_iterations[loop]++;
    } else {
        _loop_iterations.insert({loop, 1});
    }
}

void CGContext::reset_loop_iterations(const Loop *loop)
{
    if(_loop_iterations.count(loop)) {
        _loop_iterations[loop] = 0;
    }
}

int CGContext::get_loop_starts(const Loop *loop) const
{
    if(_loop_starts.count(loop)) {
        return _loop_starts.at(loop);
    } else {
        return 0;
    }
}

void CGContext::increment_loop_starts(const Loop *loop)
{
    if(_loop_starts.count(loop)) {
        _loop_starts[loop]++;
    } else {
        _loop_starts.insert({loop, 1});
    }
}

bool operator==(const CGContext &lhs, const CGContext &rhs) 
{
    const bool DEBUG = CGCONTEXT_DEBUG;
    if(DEBUG) {
        llvm::errs() << "Comparing CGContexts\n";
    }

    bool equal = true;

    if(lhs.mpi_rank().get_as_integer() != rhs.mpi_rank().get_as_integer() 
        || lhs.mpi_size().get_as_integer() != rhs.mpi_size().get_as_integer()) {
        equal = false;
    }



    for(const auto &[decl, value] : lhs.declarations()) {
        if(rhs.declarations().contains(decl)) {
            if((value.get_as_integer() != rhs.declarations().find(decl)->second.get_as_integer())) {
                equal = false;
                if(DEBUG) {
                    llvm::errs() << "Declaration " << decl->getNameAsString() << " differs\n";
                    llvm::errs() << "LHS: ";
                    value.dump();
                    llvm::errs() << "\n";
                    llvm::errs() << "RHS: ";
                    rhs.declarations().find(decl)->second.dump();
                    llvm::errs() << "\n";
                }
            }
        }
    }


    for(const auto &[expr, value] : lhs.expressions()) {
        if(rhs.expressions().contains(expr)) {
            if((value.get_as_integer() != rhs.expressions().find(expr)->second.get_as_integer())) {
                equal = false;
                if(DEBUG) {
                    llvm::errs() << "Expression " << expr << " differs\n";
                    llvm::errs() << "LHS: ";
                    value.dump();
                    llvm::errs() << "\n";
                    llvm::errs() << "RHS: ";
                    rhs.expressions().find(expr)->second.dump();
                    llvm::errs() << "\n";
                }
            }
        }
    }

    for(const auto &[parm, value] : lhs.func_parameters()) {
        if(rhs.func_parameters().contains(parm)) {
            if((value.get_as_integer() != rhs.func_parameters().find(parm)->second.get_as_integer())) {
                equal = false;
                if(DEBUG) {
                    llvm::errs() << "Parameter " << parm->getNameAsString() << " differs\n";
                    llvm::errs() << "LHS: ";
                    value.dump();
                    llvm::errs() << "\n";
                    llvm::errs() << "RHS: ";
                    rhs.func_parameters().find(parm)->second.dump();
                    llvm::errs() << "\n";
                }
            }
        }
    }   

    for(const auto &[ret, value] : lhs.func_returns()) {
        if(rhs.func_returns().contains(ret)) {
            if((value.get_as_integer() != rhs.func_returns().find(ret)->second.get_as_integer())) {
                equal = false;
                if(DEBUG) {
                    llvm::errs() << "Return statement " << ret << " differs\n";
                    llvm::errs() << "LHS: ";
                    value.dump();
                    llvm::errs() << "\n";
                    llvm::errs() << "RHS: ";
                    rhs.func_returns().find(ret)->second.dump();
                    llvm::errs() << "\n";
                }
            }
        }
    }

    return equal;
}

CGContext operator+(const CGContext &lhs, const CGContext &rhs)
{
    CGContext new_ctx(lhs.mpi_rank(), lhs.mpi_size());

    for(const auto &[decl, value] : lhs.declarations()) {
        if(rhs.declarations().contains(decl)) {
            if((value.get_as_integer() != rhs.declarations().find(decl)->second.get_as_integer())) {
                new_ctx.bind(decl, CGValue::top());
            } else {
                new_ctx.bind(decl, value);
            }
        }
    }

    for(const auto &[parm, value] : lhs.func_parameters()) {
        if(rhs.func_parameters().contains(parm)) {
            if((value.get_as_integer() != rhs.func_parameters().find(parm)->second.get_as_integer())) {
                new_ctx.bind(parm, CGValue::top());
            } else {
                new_ctx.bind(parm, value);
            }
        }
    }   

    for(const auto &[ret, value] : lhs.func_returns()) {
        if(rhs.func_returns().contains(ret)) {
            if((value.get_as_integer() != rhs.func_returns().find(ret)->second.get_as_integer())) {
                new_ctx.bind(ret, CGValue::top());
            } else {
                new_ctx.bind(ret, value);
            }
        }
    }

    for(const auto &[loop, count] : lhs.loop_iterations()) {
        // TODO make this nicer
        for(int i = 0; i < count; i++) {
            new_ctx.increment_loop_iteration(loop);
        }
    }
    for(const auto &[loop, count] : rhs.loop_iterations()) {
        if(!lhs.loop_iterations().contains(loop)) {
            // TODO make this nicer
            for(int i = 0; i < count; i++) {
                new_ctx.increment_loop_iteration(loop);
            }
        }
    }

    for(const auto &[loop, count] : lhs.loop_starts()) {
        // TODO make this nicer
        for(int i = 0; i < count; i++) {
            new_ctx.increment_loop_starts(loop);
        }
    }
    for(const auto &[loop, count] : rhs.loop_starts()) {
        if(!lhs.loop_starts().contains(loop)) {
            // TODO make this nicer
            for(int i = 0; i < count; i++) {
                new_ctx.increment_loop_starts(loop);
            }
        }
    }

    return new_ctx;
}
