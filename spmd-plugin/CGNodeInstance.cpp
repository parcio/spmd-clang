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

#include "CGNodeInstance.h"
#include "CGContext.h"
#include "CGNode.h"
#include "CGStatementVisitor.h"
#include "debug.h"
#include "utils.h"
#include "config.h"
#include <clang/AST/ASTContext.h>
#include <clang/AST/Expr.h>
#include <clang/AST/Stmt.h>
#include <clang/Analysis/CFG.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/raw_ostream.h>
#include <string>
#include <vector>

// TODO remove _loops from this class
CGNodeInstance::CGNodeInstance(const CGContext &initial, const CGNode &original, clang::ASTContext *ctx) : 
    CGNode("P" + std::to_string(initial.mpi_rank().get_as_integer().value()) + "_" + original.name(), *original.func), _context(initial), _visitor(_context, ctx)

{
    const bool DEBUG = CGNODEINSTANCE_DEBUG;
    this->attr = original.attr;
    // _context.add_node_visit(this->name());
    if(this->attr.loop_entry) {
        auto *loop = this->attr.parent_loop;
        _context.increment_loop_iteration(loop);
    }

    if(DEBUG) {
        llvm::errs() << "- Creating CGContext for this node: [" << original.name() << "] -\n";
    }
    for(const auto &elem : original.elements()) {
        this->append_element(elem);
    }

    if(DEBUG) {
        _context.dump();
    }

    // handle maximum loop iterations to be simulated
    // MAX_LOOP_ITERATIONS == 0 means no limit
    // TODO check if this approach is a problem for loops that use break statements
    // TODO remove the MAX_LOOP_ITERATIONS and always have a default value in case of infinite loops?
    // Check if this node is the loop header
    // TODO FIXME make the limit work correctly with the config file
    if(this->attr.loop_exit && MAX_LOOP_ITERATIONS_RUNTIME > 0) {
        if(_context.get_loop_iteration(this->attr.parent_loop) > MAX_LOOP_ITERATIONS_RUNTIME) {
            // Find the correct successor that leaves the loop
            const BranchingCondition *successor = nullptr;
            for(const auto &succ : original.successors()) {
                if(succ.successor->attr.loop_level < this->attr.loop_level) {
                    successor = &succ;
                }
            }

            if(!successor) {
                error_print("Could not find a successor that leaves the loop");
                return;
            }

            this->append_successor(*successor);
            // resetting the loop header visit counter makes iterations limits for nested loops work 
            _context.reset_loop_iterations(this->attr.parent_loop);
            
            if(DEBUG) {
                llvm::errs() << "Reached maximum loop iterations for this loop.";
                llvm::errs() << "Successor for this block will be: " << successor->successor->name() << "\n";
                llvm::errs() << "---\n";
            }
            return;
        }
    }

    if(const auto &terminator = original.get_terminator()) {
        const auto *cond_stmt = terminator.value().getAs<clang::CFGStmt>()->getStmt();
        if(DEBUG) {
            llvm::errs() << "Visiting terminator statement:\n";
            cond_stmt->dump();
        }
        _visitor.Visit(cond_stmt);

        // handle function calls
        if(auto call_expr = llvm::dyn_cast<clang::CallExpr>(cond_stmt)) {
            if(original.successors().size() != 1) {
                error_print("Node " + this->name() + "calls a function but has not exactly one successor");
                call_expr->dump();
            } else {
                const auto &succ = original.successors().front();
                this->append_successor(succ);
                if(DEBUG) {
                    llvm::errs() << "Successor for this block will be: " << succ.successor->name() << "\n";
                }
            }
        } else if(auto evaluated_condition = _context.get(llvm::dyn_cast<clang::Expr>(cond_stmt))) {

            // WIP TODO handle undecidable control flow conditions
            if(evaluated_condition->is_top()) {
                if(DEBUG) {
                    llvm::errs() << "Undecidable control flow condition found for node " << this->name() << "\n";
                    llvm::errs() << "Both control flow branches will be followed\n";
                }
                for(const auto &succ : original.successors()) {
                    if(DEBUG) {
                        llvm::errs() << "A successor for this block will be: " << succ.successor->name() << "\n";
                    }
                    BranchingCondition cond = { .cfg_elem = terminator, .condition_state = succ.condition_state, .successor = succ.successor };
                    this->append_successor(cond);

                    // NOTE TODO
                    // The resetting of the node visit counter for loops needs to be handled
                    // when creating the next node in this case, because one branch will stay in the loop
                }
            } else {
                for(const auto &succ : original.successors()) {
                    if(evaluated_condition->get_as_boolean() == succ.condition_state) {
                        if(DEBUG) {
                            llvm::errs() << "Successor for this block will be: " << succ.successor->name() << "\n";
                        }
                        BranchingCondition cond = { .cfg_elem = terminator, .condition_state = succ.condition_state, .successor = succ.successor };
                        this->append_successor(cond);
                        // TODO BUG in nested loops where the loop exit is not the loop header this might
                        // lead to problems when MAX_LOOP_ITERATIONS is set because the loop header
                        // visits are still increase by each iteration of the outer loop
                        // TODO maybe move this to the creation of the next node instead?
                        // That might also make it easier to reset the counters for the whole loop
                        if(cond.successor->attr.loop_level < this->attr.loop_level) {
                            _context.reset_loop_iterations(this->attr.parent_loop);
                        }
                    } else {
                        if(DEBUG) {
                            llvm::errs() << succ.successor->name() << " will not be the successor for this block\n";
                        }
                    }
                }
            }
        } else {
            error_print("Unexprected case encountered in deciding successor for node " + this->name());
        }
    } else {
        if(original.successors().size() > 1) {
            error_print( "Error: Node " + this->name() + " has multiple successors but not terminator expression!");
        } else if(original.successors().size() == 0) {
            if(DEBUG) {
                llvm::errs() << "Node " << this->name() <<  " has no successors\n";
            }
        } else {
            const auto &succ = original.successors().front();
            this->append_successor(succ);
            if(DEBUG) {
                llvm::errs() << "Successor for this block will be: " << succ.successor->name() << "\n";
            }
        }
    }

    if(DEBUG) {
        llvm::errs() << "---\n";
    }
}


void CGNodeInstance::dump() const 
{
    // CGNode::dump();
    
    llvm::errs() << "[" << this->name() << "]\n";
    for(const auto &elem : this->elements()) {
        llvm::errs() << "  ";
        elem.dump();
        llvm::errs() << "  " << get_evaluated_cfg_as_string(elem) << "\n";
    }


    llvm::errs() << "Context: \n";
    _context.dump();
}

void CGNodeInstance::dump2() const 
{
    CGNode::dump();
}


// TODO make function for _contex.get ? get_as_string : UNEVALUATED
std::string CGNodeInstance::get_evaluated_cfg_as_string(const clang::CFGElement &element) const
{
    std::string result = "";
    const auto *stmt = element.getAs<clang::CFGStmt>()->getStmt();
    
    if(auto *call_expr = llvm::dyn_cast<clang::CallExpr>(stmt)) {
        result += "<$ (";
        for(const auto &arg : call_expr->arguments()) {
            result += " " + cgv_to_str(_context.get(arg)) + " ";
        }
        result += ") $>\n";
    } else if(auto *var_decl = llvm::dyn_cast<clang::DeclStmt>(stmt)) {
        if(auto *v2 = llvm::dyn_cast<clang::VarDecl>(var_decl->getSingleDecl())) {
            result += "<$ " + v2->getNameAsString() + " = " + cgv_to_str(_context.get(v2)) + " $>\n";
        }
    } else if (auto *binary_op = llvm::dyn_cast<clang::BinaryOperator>(stmt)) {
        const auto opcode = binary_op->getOpcode();
        switch (opcode) {
            case clang::BO_Assign: {
                auto const *lhs = binary_op->getLHS();
                auto const *rhs = binary_op->getRHS();
                if(const auto *array = llvm::dyn_cast<clang::ArraySubscriptExpr>(lhs)) {
                    if(const auto *base = llvm::dyn_cast<clang::ImplicitCastExpr>(array->getBase())) {
                        if(const auto *decl = llvm::dyn_cast<clang::DeclRefExpr>(base->getSubExpr())) {
                            result += "<$ " + decl->getDecl()->getNameAsString();
                            result += "[" + cgv_to_str(_context.get(array->getIdx())) + "] = ";
                        }
                    }
                } else if(const auto *decl = llvm::dyn_cast<clang::DeclRefExpr>(lhs)->getDecl()) {
                    result += "<$ " + decl->getNameAsString() + " = ";
                }
                result += cgv_to_str(_context.get(rhs)) + " $>\n";
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
                auto const *lhs = binary_op->getLHS();
                // auto const *rhs = binary_op->getRHS();
                if(const auto *array = llvm::dyn_cast<clang::ArraySubscriptExpr>(lhs)) {
                    if(const auto *base = llvm::dyn_cast<clang::ImplicitCastExpr>(array->getBase())) {
                        if(const auto *decl = llvm::dyn_cast<clang::DeclRefExpr>(base->getSubExpr())) {
                            result += "<$ " + decl->getDecl()->getNameAsString();
                            result += "[" + cgv_to_str(_context.get(array->getIdx())) + "] = ";
                        }
                    }
                } else if(const auto *decl = llvm::dyn_cast<clang::DeclRefExpr>(lhs)->getDecl()) {
                    result += "<$ " + decl->getNameAsString() + " = ";
                }
                result += cgv_to_str(_context.get(binary_op)) + " $>\n";
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
                result += "<$ " + cgv_to_str(_context.get(binary_op)) + " $>\n";
                break;
            }
            default: {
                result += "<$ BinaryOperator TODO $>\n";
                break;
            }
        }
    } else if(auto unary_op = llvm::dyn_cast<clang::UnaryOperator>(stmt)) {
        const auto opcode = unary_op->getOpcode();
        switch (opcode) {
            case clang::UO_PostInc: 
            case clang::UO_PostDec:
            case clang::UO_PreInc:
            case clang::UO_PreDec: {
                result += "<$ ";
                if(auto var = llvm::dyn_cast<clang::DeclRefExpr>(unary_op->getSubExpr())) {
                    result += var->getDecl()->getNameAsString() + " = ";
                }
                result += cgv_to_str(_context.get(unary_op)) + " $>\n";
                break;
            }
            case clang::UO_AddrOf:
            case clang::UO_Deref: 
            case clang::UO_Plus:
            case clang::UO_Minus:
            case clang::UO_Not:
            case clang::UO_LNot: {
                result += "<$ " + cgv_to_str(_context.get(unary_op)) + " $>\n";
                break;
            }
            default: {
                result += "<$ UnaryOperator TODO $>\n";
                break;
            }
        }
    } else if(auto *return_stmt = llvm::dyn_cast<clang::ReturnStmt>(stmt)) {
        result += "<$ ";
        if(return_stmt->getRetValue()) {
            result += cgv_to_str(_context.get(return_stmt->getRetValue())) + " $>\n";
        } else {
            result += "UNEVALUATED $>\n";
        }
    } else if(auto *literal = llvm::dyn_cast<clang::IntegerLiteral>(stmt)) {
        result += "<$ ";
        result += cgv_to_str(_context.get(literal)) + " $>\n";
    } else if(auto *cond_op = llvm::dyn_cast<clang::ConditionalOperator>(stmt)) {
        result += "<$ ";
        result += cgv_to_str(_context.get(cond_op)) + " $>\n";
    } else {
        result += "<$ Unknown CFGElement $>\n";
    }

    return result;
}

void CGNodeInstance::append_element(const clang::CFGElement &element) 
{
    const bool DEBUG = CGNODEINSTANCE_APPEND_ELEMENT_DEBUG;
    CGNode::append_element(element);
    if(const auto *stmt = element.getAs<clang::CFGStmt>()->getStmt(); stmt) {
        if(DEBUG) {
            llvm::errs() << "@\nVisiting: ";
            stmt->dump();
            llvm::errs() << "\n";
        }
        _visitor.Visit(stmt);
        if(DEBUG) {
            llvm::errs() << "@\n";
        }
    } else {
        error_print("Unexpected type of CFGElement in creating CGNodeInstance " + this->name());
    }
}

std::string CGNodeInstance::cgv_to_str(std::optional<CGValue> value) const
{
    if(value.has_value()) {
        return value.value().get_as_string();
    } else {
        return "UNEVALUATED";
    }
}
