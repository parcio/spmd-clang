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

#include "CGNode.h"
#include "debug.h"
#include "utils.h"
#include <algorithm>
#include <clang/Analysis/CFG.h>
#include <clang/Basic/SourceManager.h>
#include <llvm/Support/raw_ostream.h>
#include <optional>


bool operator==(const BranchingCondition &lhs, const BranchingCondition &rhs)
{
    return (&lhs.cfg_elem == &rhs.cfg_elem &&
            lhs.condition_state == rhs.condition_state &&
            lhs.successor == rhs.successor);
}


CGNode::CGNode(const std::string &name, const clang::FunctionDecl& func) 
    : func(&func), _name(name)
{
}

void CGNode::dump() const
{
    llvm::errs() << "[" << _name << "]\n";
    for(const auto &elem : _elements) {
        llvm::errs() << "  ";
        elem.dump();
    }
    llvm::errs() << "Predecessors: ";
    for(const auto &pred : _predecessors) {
        llvm::errs() << pred->name() << " ";
    }
    llvm::errs() << "\n";
    llvm::errs() << "Successors: ";
    for(const auto &succ : _successors) {
        llvm::errs() << succ.successor->name() << " ";
    }
    llvm::errs() << "\n";
    llvm::errs() << "Condition: ";
    if(!_successors.empty() && _successors.begin()->cfg_elem.has_value()) {
        llvm::errs() << cfgelement_to_str(&_successors.begin()->cfg_elem.value());
    }
    llvm::errs() << "\n";
    
    if(attr.func_entry) {
        llvm::errs() << "IS FUNCTION ENTRY\n";
    }
    if(attr.func_exit) {
        llvm::errs() << "IS FUNCTION EXIT\n";
    }
    if(attr.loop_entry) {
        llvm::errs() << "IS LOOP ENTRY\n";
    }
    if(attr.loop_exit) {
        llvm::errs() << "IS LOOP EXIT\n";
    }
    if(attr.loop) {
        llvm::errs() << "LOOP LEVEL: " << attr.loop_level << "\n";
    }
    if(attr.parent_loop) {
        llvm::errs() << "LOOP PARENT: " << attr.parent_loop->header->name() << "\n";
    }

    if(!_skipped_elements.empty()) {
        for(const auto &elem : _skipped_elements) {
            llvm::errs() << "  ";
            elem.dump();
        }
    }
}


const std::string &CGNode::name() const
{
    return _name;
}

void CGNode::set_name(const std::string &name) 
{
    _name = name;
}

void CGNode::append_element(const clang::CFGElement &element)
{
    const bool DEBUG = APPEND_ELEMENT_DEBUG;
    if(element.getKind() == clang::CFGElement::Kind::Statement) {
        _elements.push_back(element);
    } else {
        // TODO look into this case more
        warning_print("Encountered unhandled type of CFGElement");
        if(DEBUG) {
            llvm::errs() << "Element: ";
            element.dump();
            llvm::errs() << "\n";
        }
        _skipped_elements.push_back(element);
    }
}

void CGNode::append_predecessor(CGNode *node)
{
    _predecessors.push_back(node);
}

void CGNode::replace_predecessor(CGNode &old, CGNode &replacement) 
{
    std::replace(_predecessors.begin(), _predecessors.end(), &old, &replacement);
}

const std::vector<BranchingCondition> &CGNode::successors() const 
{
    return _successors;
}

void CGNode::append_successor(BranchingCondition condition)
{
    _successors.push_back(condition);
}

BranchingCondition* CGNode::replace_successor(const BranchingCondition &old, const BranchingCondition &replacement)
{
    std::replace(_successors.begin(), _successors.end(), old, replacement);
    for(auto &succ : _successors) {
        if(succ.successor == replacement.successor) {
            return &succ;
        }
    }
    error_print("Replace successor failed for node " + _name);
    return nullptr;
}

std::optional<clang::CFGElement> CGNode::get_terminator() const
{
    // TODO check if condition state need to be taken into account for branching control flow nodes
    if(!_successors.empty() && _successors.begin()->cfg_elem) {
        return _successors.begin()->cfg_elem;
    } else {
        return std::nullopt;
    }
}

