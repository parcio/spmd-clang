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

#ifndef CGINSTANCE_H
#define CGINSTANCE_H

#include "CGNode.h"
#include "CGNodeInstance.h"
#include <cstdint>
#include <llvm/ADT/MapVector.h>
#include <memory>
#include <queue>
#include <set>
#include <vector>

struct ConstructGraphData
{
    CGNode &orig_node;
    CGNodeInstance *pred_node;
    BranchingCondition pred_cond;
    const BranchingCondition *old_pred_cond;
};


class CGInstance 
{
public:
    CGInstance(const std::int64_t mpi_rank, const std::int64_t mpi_size, const CGNode &root, clang::ASTContext *ctx, std::vector<Loop> loops);
    CGInstance(CGInstance &&) = default;
    CGInstance(const CGInstance &) = delete;
    CGInstance &operator=(CGInstance &&) = delete;
    CGInstance &operator=(const CGInstance &) = delete;
    ~CGInstance() = default;

    const std::int64_t mpi_rank;
    const std::int64_t mpi_size;

    const std::vector<std::unique_ptr<CGNodeInstance>> &nodes() const { return _nodes; }

    void dump() const;

    std::string output_cg() const;
    

private:
    
    void construct_graph_rec(const CGNode &node, CGNodeInstance &pred_node, BranchingCondition &pred_cond, std::set<const CGNode*> &known_nodes);
    void construct_graph_loop(const CGNode &root, const std::int64_t mpi_rank, const std::int64_t mpi_size);
    void dump_queue(std::queue<ConstructGraphData> &queue) const;
    // Members
    std::vector<std::unique_ptr<CGNodeInstance>> _nodes;
    clang::ASTContext *_ctx;
    // TODO remove this member because not needed?
    std::vector<Loop> _loops;
};

#endif 
