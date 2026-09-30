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

#ifndef COMMUNICATION_GRAPH2_H
#define COMMUNICATION_GRAPH2_H

#include "CGInstance.h"
#include "SPMDData.h"
#include "CGNode.h"
#include "Loop.h"
#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/Analysis/CFG.h>
#include <deque>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

struct FuncCG
{
    CGNode *root = nullptr;
    CGNode *exit = nullptr;
    std::vector<CGNode*> nodes;
    const clang::FunctionDecl *func;
};


class CommunicationGraph {
public:
    CommunicationGraph(const SPMDData &spmd_data);
    CommunicationGraph(CommunicationGraph &&) = delete;
    CommunicationGraph(const CommunicationGraph &) = delete;
    CommunicationGraph &operator=(CommunicationGraph &&) = delete;
    CommunicationGraph &operator=(const CommunicationGraph &) = delete;
    ~CommunicationGraph() = default;

    void output_cg(const std::string &path) const;
    void output_func_cg(const std::string &path, const FuncCG &fcg) const;

private:
    void construct_graph(); 
    void construct_graph_rec(FuncCG &fcg, clang::CFGBlock &block, CGNode *pred_node, BranchingCondition *pred_cond, std::vector<clang::CFGBlock*> &known_blocks);

    void combine_partial_graphs();
    void combine_partial_graphs_rec(CGNode *node, std::vector<const FuncCG*> &funcs_to_add, std::vector<CGNode*> &known_nodes, std::set<CGNode*> &nodes_to_remove);

    void instantiate_func_uses();
    std::vector<std::unique_ptr<CGNode>> copy_node_range(const CGNode &start, const CGNode &end, CGNode &start_pred, CGNode &end_succ);
    void copy_node_range_rec(CGNode &node, CGNode &pred_node, BranchingCondition &pred_cond, std::set<const CGNode*> &known_nodes, std::vector<std::unique_ptr<CGNode>> &new_nodes, const CGNode &end, CGNode &end_succ);

    void validate() const;
    std::vector<const CGNode*> get_node_range(const CGNode &start) const;

    void prune_graph();

    void detect_loops();
    Loop analyze_loop(const std::vector<CGNode*> &loop, const int loop_level);
    std::vector<std::vector<CGNode*>> tarjan(std::vector<std::unique_ptr<CGNode>> &nodes);
    std::vector<std::vector<CGNode*>> tarjan(std::vector<CGNode*> &nodes, const CGNode *ignore);
    void tarjan_strongconnect(CGNode &node, long &index, std::deque<CGNode*> &stack, std::map<const CGNode*, std::pair<long, long>> &index_list, std::vector<std::vector<CGNode*>> &loops, const CGNode *ignore);

    void analysis(const int mpi_size) const;
    std::vector<CGInstance> create_cg_instances(const int mpi_size) const;
    void output_cgi(const std::string &path, const std::vector<CGInstance> &cg_instances) const;

    std::string create_node_name(const clang::CFGBlock &block, const clang::FunctionDecl &func);

    const FuncCG* get_fcg(const std::string &func_name) const;

    void dump() const;
    void dump(const FuncCG &fcg) const;

    const Loop* get_parent_loop(const CGNode &node);

    // Member Variables
    const SPMDData &_spmd_data;
    clang::ASTContext &_ast_ctx;
    // TODO check lifetimes of these. ownership of elements should be transferred to _nodes eventually
    std::vector<FuncCG> _func_cgs;
    CGNode *_root;
    std::vector<std::unique_ptr<CGNode>> _nodes;
    /// All loops in the global CG
    std::vector<Loop> _loops;
    llvm::MapVector<const CGNode*, const Loop*> _loop_mappings;

};

#endif // !COMMUNICATION_GRAPH2_H
