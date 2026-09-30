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

#ifndef CGNODE_H
#define CGNODE_H

#include "Loop.h"
#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/Analysis/CFG.h>
#include <llvm/Support/raw_ostream.h>
#include <optional>
#include <vector>

class CGNode;


struct BranchingCondition 
{
    std::optional<clang::CFGElement> cfg_elem;
    bool condition_state;
    CGNode *successor;
};

struct CGNodeAttributes
{
    bool func_entry = false;
    bool func_exit = false;
    bool loop_entry = false;
    bool loop_exit = false;
    bool loop = false;
    int loop_level = 0;
    const Loop *parent_loop = nullptr;
};

bool operator==(const BranchingCondition &lhs, const BranchingCondition &rhs);

class CGNode {
public:
    CGNode(const std::string &name, const clang::FunctionDecl &func);
    CGNode(CGNode &&) = default;
    CGNode(const CGNode &) = default;
    CGNode &operator=(CGNode &&) = default;
    CGNode &operator=(const CGNode &) = default;
    virtual ~CGNode() {}

    const clang::FunctionDecl *func;
    CGNodeAttributes attr;


    virtual void dump() const;
    const std::string &name() const;
    void set_name(const std::string &name);


    const std::vector<clang::CFGElement>& elements() const { return _elements; }
    virtual void append_element(const clang::CFGElement &element);

    const std::vector<CGNode*>& predecessors() const { return _predecessors; }
    void append_predecessor(CGNode *node);
    void pop_predecessor() { _predecessors.pop_back(); }
    void replace_predecessor(CGNode &old, CGNode &replacement);

    const std::vector<BranchingCondition> &successors() const; 
    void append_successor(BranchingCondition condition);
    void pop_successor() { _successors.pop_back(); }
    // void replace_successor(const BranchingCondition &old, const BranchingCondition &replacement);
    BranchingCondition* replace_successor(const BranchingCondition &old, const BranchingCondition &replacement);
    std::optional<clang::CFGElement> get_terminator() const;

private:
    std::string _name;

    std::vector<clang::CFGElement> _elements;
    std::vector<clang::CFGElement> _skipped_elements;
    std::vector<CGNode*> _predecessors;
    std::vector<BranchingCondition> _successors;
    
};

#endif
