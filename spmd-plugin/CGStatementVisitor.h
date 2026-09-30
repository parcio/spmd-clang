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

#ifndef CGSTATEMENT_VISITOR_H
#define CGSTATEMENT_VISITOR_H

#include "CGContext.h"
#include "CGValue.h"
#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/AST/Expr.h>
#include <clang/AST/OperationKinds.h>
#include <clang/AST/ParentMapContext.h>
#include <clang/AST/StmtVisitor.h>
#include <clang/Basic/IdentifierTable.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/raw_ostream.h>
#include <utility>
#include "utils.h"
#include "debug.h"

class CGStatementVisitor: public clang::ConstStmtVisitor<CGStatementVisitor> 
{
public:
    CGStatementVisitor(CGContext &context, clang::ASTContext *ctx) : _context(context), _ctx(ctx) {}

    static CGValue evaluate_operation(const clang::BinaryOperatorKind opcode, const CGValue &lhs, const CGValue &rhs)
    {
        switch(opcode) {
            case clang::BO_LT:
                return lhs < rhs;
            case clang::BO_GT:
                return lhs > rhs;
            case clang::BO_LE:
                return lhs <= rhs;
            case clang::BO_GE:
                return lhs >= rhs;
            case clang::BO_EQ:
                return lhs == rhs;
            case clang::BO_NE:
                return lhs != rhs;
            case clang::BO_Add:
                return lhs + rhs;
            case clang::BO_Sub:
                return lhs - rhs;
            case clang::BO_Mul:
                return lhs * rhs;
            case clang::BO_Div:
                return lhs / rhs;
            case clang::BO_Rem:
                return lhs % rhs;
            case clang::BO_Shl:
                return lhs << rhs;
            case clang::BO_Shr:
                return lhs >> rhs;
            case clang::BO_And:
                return lhs & rhs;
            case clang::BO_Xor:
                return lhs ^ rhs;
            case clang::BO_Or:
                return lhs | rhs;
            case clang::BO_LAnd:
                return lhs && rhs;
            case clang::BO_LOr:
                return lhs || rhs;
            case clang::BO_MulAssign:
                return lhs * rhs;
            case clang::BO_DivAssign:
                return lhs / rhs;
            case clang::BO_RemAssign:
                return lhs % rhs;
            case clang::BO_AddAssign: 
                return lhs + rhs;
            case clang::BO_SubAssign:
                return lhs - rhs;
            case clang::BO_ShlAssign:
                return lhs << rhs;
            case clang::BO_ShrAssign:
                return lhs >> rhs;
            case clang::BO_AndAssign:
                return lhs & rhs;
            case clang::BO_XorAssign:
                return lhs ^ rhs;
            case clang::BO_OrAssign: 
                return lhs | rhs;

            default:
                error_print("This operation type is not yet implemented");
                return CGValue::top();
        }
    }

    static CGValue evaluate_operation(const clang::UnaryOperatorKind opcode, const CGValue value )
    {
        switch(opcode) {
            case clang::UO_PostInc:
            case clang::UO_PreInc: 
                return value + CGValue::integer(1);
            case clang::UO_PostDec:
            case clang::UO_PreDec: 
                return value - CGValue::integer(1);
            case clang::UO_Plus:
                return +value;
            case clang::UO_Minus:
                return -value;
            case clang::UO_Not:
                return ~value;
            case clang::UO_LNot: 
                return !value;
            default: {
                error_print("This operation type is not yet implemented");
                return CGValue::top();
            }
        }
    }
    
    void VisitCallExpr(const clang::CallExpr *S)
    {
        const bool DEBUG = CGSTATEMENT_VISITOR_DEBUG;

        const clang::FunctionDecl *callee = S->getDirectCallee();
        if(!callee) {
            llvm::errs() << "Error: Only direct callees are currently supported\n";
            error_print("Only direct callees are currently supported");
            llvm::errs() << "   ";
            S->dump();
            return;
        }

        clang::IdentifierInfo *identifier = callee->getIdentifier();
        if(!identifier) {
            error_print("No identifiert information found for function call");
            llvm::errs() << "   ";
            S->dump();
            return;
        }

        if(identifier->getName().starts_with("MPI_")) {
            if(identifier->getName().equals_insensitive("MPI_Comm_rank")) {
                // TODO how to check for MPI_COMM_WORLD in a good way?
                const clang::Expr *rank_arg = S->getArg(1);

                // TODO make this less of an abomination?
                // also make it a reusable function
                if(auto *decl_ref = llvm::dyn_cast<clang::DeclRefExpr>(
                        llvm::dyn_cast<clang::UnaryOperator>(rank_arg->getExprStmt())
                        ->getSubExpr())) {
                    _context.bind(decl_ref->getDecl(), _context.mpi_rank());
                } else {
                    error_print("Could not find a rank variable ValueDecl for this MPI_Comm_rank call");
                    return;
                }
            } else if (identifier->getName().equals_insensitive("MPI_Comm_size")) {
                // TODO how to check for MPI_COMM_WORLD in a good way?
                const clang::Expr *rank_arg = S->getArg(1);

                // TODO make this less of an abomination?
                // also make it a reusable function
                if(auto *decl_ref = llvm::dyn_cast<clang::DeclRefExpr>(
                        llvm::dyn_cast<clang::UnaryOperator>(rank_arg->getExprStmt())
                        ->getSubExpr())) {
                    _context.bind(decl_ref->getDecl(), _context.mpi_size());
                } else {
                    error_print("Could not find a size variable ValueDecl for this MPI_Comm_size call");
                    return;
                }
            } else {
                if(DEBUG) {
                    llvm::errs() << "Adding MPI Call to: " << S->getDirectCallee()->getNameAsString() << "\n";
                }
                SPMDCall call = { .expr = S, .rank = _context.mpi_rank() };
                for(const auto &arg : S->arguments()) {
                    if(DEBUG) {
                        llvm::errs() << "--> Arg: ";
                        arg->dump();
                    }
                    this->Visit(arg);
                    if(auto value = _context.get(arg); value.has_value()) {
                        // TODO HACK make this nicer
                        // MPI_Request arguments need to be treated in a special way
                        if(const auto *unary_op = llvm::dyn_cast<clang::UnaryOperator>(arg)) {
                            if(const auto *decl = llvm::dyn_cast<clang::DeclRefExpr>(unary_op->getSubExpr())) {
                                if(decl->getType().getAsString() == "MPI_Request") {
                                    const auto req_val = _context.get(decl->getDecl());
                                    if(req_val) {
                                        call.args.push_back(std::make_pair(arg, req_val.value()));
                                    } else {
                                        error_print("No CGValue found for this MPI_Request");
                                        arg->dump();
                                        call.args.push_back(std::make_pair(arg, CGValue::top()));
                                    }
                                    continue;
                                }
                            }
                        } 
                        // Make sure that all other unare operator expressions still get handled normally
                        call.args.push_back(std::make_pair(arg, value.value()));
                    } else {
                        error_print("No CGValue found for argument:");
                        arg->dump();
                    }
                }
                _context.mpi_calls.push_back(call);
            }
        } else {
            if(DEBUG) {
                llvm::errs() << "\nAdding arguments from function call to: " << S->getDirectCallee()->getNameAsString() << "\n";
            }
            // Bind arguments of this function call to the ParmVarDecls of the called function
            // TODO BUG In case of a variadic functions currently only the first argument is evaluated here 
            for(int idx = 0; const auto &parm: S->getDirectCallee()->parameters()) {
                this->Visit(S->getArg(idx));
                _context.bind(parm, S->getArg(idx));
                idx++;
            }

            return;
        }
    }

    void VisitDeclStmt(const clang::DeclStmt *S)
    {
        const bool DEBUG = CGSTATEMENT_VISITOR_DEBUG;

        if(!S->isSingleDecl()) {
            error_print("DeclGroup Statements not supported yet");
            llvm::errs() << "   ";
            S->dump();
        }

        const auto var_decl = llvm::cast<clang::VarDecl>(S->getSingleDecl());
        if(var_decl->hasInit()) {
            if(const auto *call_expr = llvm::dyn_cast<clang::CallExpr>(var_decl->getInit())) {
                _context.bind(var_decl, call_expr);
            } else {
                this->Visit(var_decl->getInit());
                _context.bind(var_decl, var_decl->getInit());
            }
        } else {
            // TODO WIP
            if(DEBUG) {
                llvm::errs() << "Variable " << var_decl->getNameAsString() << " has no initializer\n";
            }
            // Figure out if this is a MPI_Request declaration
            if(const auto *td = llvm::dyn_cast<clang::TypedefType>(var_decl->getType())) {
                if(td->getDecl()->getNameAsString() == "MPI_Request") {
                    llvm::errs() << "Variable " << var_decl->getNameAsString() << " is a MPI_Request\n";
                    _context.bind(var_decl, CGValue::integer(_request_id++));
                    return;
                }
            }

            _context.bind(var_decl, CGValue::top());
        }
    }


    void VisitBinaryOperator(const clang::BinaryOperator *S)
    {
        const auto opcode = S->getOpcode();
        
        switch (opcode) {
            case clang::BO_Assign: {
                // TODO WIP Check what else needs to be done to support array correctly
                if(auto *array = llvm::dyn_cast<clang::ArraySubscriptExpr>(S->getLHS())) {
                    warning_print("Array assignments might not work correctly yet");
                    this->Visit(array);
                    this->Visit(S->getRHS());
                    _context.bind(array, S->getRHS());
                } else if(auto *decl = llvm::dyn_cast<clang::DeclRefExpr>(S->getLHS())->getDecl()) {
                    if(const auto *call_expr = llvm::dyn_cast<clang::CallExpr>(S->getRHS())) {
                        _context.bind(decl, call_expr);
                    } else {
                        this->Visit(S->getRHS());
                        _context.bind(decl, S->getRHS());
                    }
                } else {
                    error_print("No LHS for assign operation found");
                    S->dump();
                }
                break;           
            }
            case clang::BO_MulAssign:
            case clang::BO_DivAssign:
            case clang::BO_RemAssign:
            case clang::BO_AddAssign: 
            case clang::BO_SubAssign:
            case clang::BO_ShlAssign:
            case clang::BO_ShrAssign:
            case clang::BO_AndAssign:
            case clang::BO_XorAssign:
            case clang::BO_OrAssign: {
                auto rhs = _context.get(S->getRHS());
                if(!rhs) {
                    if(const auto *call_expr = llvm::dyn_cast<clang::CallExpr>(S->getRHS())) {
                        rhs = _context.get(call_expr);
                    } else {
                        this->Visit(S->getRHS());
                        rhs = _context.get(S->getRHS());
                    }
                }
                if(!rhs.has_value()) {
                    rhs = CGValue::top();
                    warning_print("No value found for RHS. Setting value to Top");
                    S->getRHS()->dump();
                }

                // TODO WIP Check what else needs to be done to support array correctly
                if(auto *array = llvm::dyn_cast<clang::ArraySubscriptExpr>(S->getLHS())) {
                    warning_print("Array assignments might not work correctly yet");
                    this->Visit(array);
                    this->Visit(S->getRHS());
                    auto lhs = _context.get(array);
                    if(!lhs.has_value()) {
                        lhs = CGValue::top();
                        warning_print("No value found for LHS. Setting value to Top");
                        S->getLHS()->dump();
                    }
                    _context.bind(array, evaluate_operation(opcode, lhs.value(), rhs.value()));
                    _context.bind(S, evaluate_operation(opcode, lhs.value(), rhs.value()));
                } else if(const auto *decl = llvm::dyn_cast<clang::DeclRefExpr>(S->getLHS())->getDecl()) {
                    auto lhs = _context.get(decl);
                    if(!lhs) {
                        this->Visit(S->getLHS());
                        lhs = _context.get(S->getLHS());
                    }
                    if(!lhs.has_value()) {
                        lhs = CGValue::top();
                        warning_print("No value found for LHS. Setting value to Top");
                        S->getLHS()->dump();
                    }
                    _context.bind(decl, evaluate_operation(opcode, lhs.value(), rhs.value()));
                    _context.bind(S, evaluate_operation(opcode, lhs.value(), rhs.value()));
                }
                break;
            }
            case clang::BO_LE:
            case clang::BO_LT:
            case clang::BO_GE:
            case clang::BO_GT:
            case clang::BO_EQ:
            case clang::BO_NE:
            case clang::BO_Add:
            case clang::BO_Sub:
            case clang::BO_Mul:
            case clang::BO_Div:
            case clang::BO_Rem:
            case clang::BO_Shl:
            case clang::BO_Shr:
            case clang::BO_And:
            case clang::BO_Xor:
            case clang::BO_Or:
            case clang::BO_LAnd:
            case clang::BO_LOr: {
                auto lhs = _context.get(S->getLHS());
                if(!lhs) {
                    if(const auto *call_expr = llvm::dyn_cast<clang::CallExpr>(S->getLHS())) {
                        lhs = _context.get(call_expr);
                    } else {
                        this->Visit(S->getLHS());
                        lhs = _context.get(S->getLHS());
                    }
                }
                auto rhs = _context.get(S->getRHS());
                if(!rhs) {
                    if(const auto *call_expr = llvm::dyn_cast<clang::CallExpr>(S->getRHS())) {
                        rhs = _context.get(call_expr);
                    } else {
                        this->Visit(S->getRHS());
                        rhs = _context.get(S->getRHS());
                    }
                }
                if(!lhs.has_value()) {
                    lhs = CGValue::top();
                    warning_print("No value found for LHS. Setting value to Top");
                    S->getLHS()->dump();
                }
                if(!rhs.has_value()) {
                    rhs = CGValue::top();
                    warning_print("No value found for RHS. Setting value to Top");
                    S->getRHS()->dump();
                }
                _context.bind(S, evaluate_operation(opcode, lhs.value(), rhs.value()));
                break;
            }
            default: {
                // TODO other cases
                error_print("This binary operator is not yet implemented");
                llvm::errs() << "   ";
                S->dump();
                break;
            }
        }
    }

    void VisitUnaryOperator(const clang::UnaryOperator *S)
    {
        const bool DEBUG = CGSTATEMENT_VISITOR_DEBUG;

        const auto opcode = S->getOpcode();
        
        switch (opcode) {
            case clang::UO_PostInc: 
            case clang::UO_PostDec:
            case clang::UO_PreInc:
            case clang::UO_PreDec: {
                auto value = _context.get(S->getSubExpr());
                if(!value) {
                    this->Visit(S->getSubExpr());
                    value = _context.get(S->getSubExpr());
                }
                if(!value.has_value()) {
                    value = CGValue::top();
                    warning_print("No value found for obejct of UnaryOperation. Setting value to Top");
                    S->getSubExpr()->dump();
                }
                _context.bind(S, evaluate_operation(opcode, value.value()));
                // Also overwrite the value of the variable declaration 
                if(auto *decl = llvm::dyn_cast<clang::DeclRefExpr>(S->getSubExpr())) {
                    _context.bind(decl->getDecl(), S);
                }
                break;
            }
            case clang::UO_AddrOf:
            case clang::UO_Deref: {
                if(DEBUG) {
                    llvm::errs() << "Pointer operations cannot be evaluated. Setting value of expression to Top\n"; 
                    S->dump();
                }
                _context.bind(S, CGValue::top());
                break;
            }
            case clang::UO_Plus:
            case clang::UO_Minus:
            case clang::UO_Not:
            case clang::UO_LNot: {
                auto value = _context.get(S->getSubExpr());
                if(!value) {
                    this->Visit(S->getSubExpr());
                    value = _context.get(S->getSubExpr());
                }
                if(!value.has_value()) {
                    value = CGValue::top();
                    warning_print("No value found for obejct of UnaryOperation. Setting value to Top");
                    S->getSubExpr()->dump();
                }
                _context.bind(S, evaluate_operation(opcode, value.value()));
                break;
            }
            default: {
                error_print("This unary operator is not yet implemented");
                llvm::errs() << "   ";
                S->dump();
                break;
            }
                
        }

    }

    void VisitIntegerLiteral(const clang::IntegerLiteral *S)
    {
        CGValue value = CGValue::top();
        if (const auto v = S->getValue().trySExtValue()) {
            value = CGValue::integer(v.value());
        }
        _context.bind(S, value);
    }

    void VisitImplicitCastExpr(const clang::ImplicitCastExpr *S) 
    {
        this->Visit(S->getSubExpr());
        _context.bind(S, S->getSubExpr());
    }

    void VisitDeclRefExpr(const clang::DeclRefExpr *S)
    {
        auto const value = _context.get(S->getDecl());
        if(value) {
            _context.bind(S, value.value());
        } else {
            if(auto parm_var_decl = llvm::dyn_cast<clang::ParmVarDecl>(S->getDecl())) {
                auto const value = _context.get(parm_var_decl);
                _context.bind(S, value.value());
            } else {
                _context.bind(S, CGValue::top());
            }
        }
    }

    void VisitParenExpr(const clang::ParenExpr *S) 
    {
        this->Visit(S->getSubExpr());
        _context.bind(S, S->getSubExpr());
    }

    void VisitCStyleCastExpr(const clang::CStyleCastExpr *S)
    {
        this->Visit(S->getSubExpr());
        _context.bind(S, S->getSubExpr());
    }

    void VisitArraySubscriptExpr(const clang::ArraySubscriptExpr *S)
    {
        this->Visit(S->getIdx());
    }

    void VisitReturnStmt(const clang::ReturnStmt *S)
    {
        // Do nothing for empty returns
        if(!S->getRetValue()) {
            return;
        }
         
        this->Visit(S->getRetValue());
        _context.bind(S, S->getRetValue());

        // Binding the result of the return value to the function declaration so that it can
        // be used in cases where the return value is assigned to a variable
        // TODO how to handle recursion?
        auto parents = _ctx->getParents(*S);
        while(!parents.empty()) {
            if(auto *func_decl = parents[0].get<clang::FunctionDecl>()) {
                _context.bind(func_decl, S->getRetValue());
                break;
            } else {
                parents = _ctx->getParents(parents[0]);
            }
        }
    }

    void VisitConditionalOperator(const clang::ConditionalOperator *S)
    {
        this->Visit(S->getCond());
        const auto cond = _context.get(S->getCond());
        if(!cond) {
            warning_print("Condition of ternary operator could not be evaluated. Setting value to Top");
            _context.bind(S, CGValue::top());
        } else {
            std::optional<CGValue> value;
            if(cond->get_as_boolean().value()) {
                this->Visit(S->getTrueExpr());
                value = _context.get(S->getTrueExpr());
            } else {
                this->Visit(S->getFalseExpr());
                value = _context.get(S->getFalseExpr());
            }
            if(value) {
                _context.bind(S, value.value());
            } else {
                warning_print("Value of ternary operator could not be evaluated. Setting value to Top");
                _context.bind(S, CGValue::top());
            }
        }
    }


private:
    CGContext &_context;
    clang::ASTContext *_ctx;
    inline static int _request_id = 0;
};


#endif 
