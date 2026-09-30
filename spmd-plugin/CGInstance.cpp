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

#include "CGInstance.h"
#include "CGContext.h"
#include "CGNode.h"
#include "CGNodeInstance.h"
#include "debug.h"
#include "utils.h"
#include <clang/AST/ASTContext.h>
#include <clang/Analysis/CFG.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/raw_ostream.h>
#include <memory>
#include <queue>
#include <set>
#include <string>


CGInstance::CGInstance(const std::int64_t mpi_rank, const std::int64_t mpi_size, const CGNode &root, clang::ASTContext *ctx, std::vector<Loop> loops)
    : mpi_rank(mpi_rank), mpi_size(mpi_size), _ctx(ctx), _loops(loops)
{
    // const bool DEBUG = CGINSTANCE_DEBUG;

    // CGContext initial(CGValue::integer(mpi_rank), CGValue::integer(mpi_size));
    // _nodes.push_back(std::make_unique<CGNodeInstance>(initial, root, _ctx));
    // auto *new_node = _nodes.back().get();
    //
    // std::set<const CGNode*> known_nodes;
    // known_nodes.insert(&root);
    //
    // for(const auto &succ : new_node->successors()) {
    //     BranchingCondition cond_new = { .cfg_elem = succ.cfg_elem, .condition_state = succ.condition_state, .successor = nullptr };
    //     construct_graph_rec(*succ.successor, *new_node, cond_new, known_nodes);
    //     new_node->replace_successor(succ, cond_new);
    // }
    
    construct_graph_loop(root, mpi_rank, mpi_size);
}

void CGInstance::construct_graph_rec(const CGNode &node, CGNodeInstance &pred_node, BranchingCondition &pred_cond, std::set<const CGNode*> &known_nodes)
{
    const bool DEBUG = CGINSTANCE_DEBUG;

    if(!known_nodes.insert(&node).second) {
        // TODO can this go wrong? Is there code where the loop entry can have multiple predecessors?
        // TODO how to avoid infinite loops?
        // TODO what to do when the loop condition can not be evaluated?
        // TODO BUG probably doesnt work correctly when there is undecidable control flow in the loop
        // because in that case the extra logic from this case also needs to be applied to the node inside the loop
        if(!node.attr.loop) {
            // find the already existing CGNodeInstance for this CGNode
            std::string name = "P" + std::to_string(this->mpi_rank) + "_" + node.name(); 
            auto iter = std::find_if(
                _nodes.begin(),
                _nodes.end(),
                [&](std::unique_ptr<CGNodeInstance> &n) { return name == n->name(); }
            );
            CGNodeInstance *cur_node = (*iter).get();

            // if there are multiple predecessors that means that we need to merge their CGContexts
            // If it turns out that a known CGValue is different in the predecessors we need to 
            // reevaluate all the following nodes in the graph with the merged values
            auto old_ctx = ((CGNodeInstance*)cur_node->predecessors().front())->context().clean_copy();
            for(const auto &pred : cur_node->predecessors()) {
                if(old_ctx != ((CGNodeInstance*)pred)->context().clean_copy()) {
                    old_ctx = old_ctx +  ((CGNodeInstance*)pred)->context().clean_copy();
                }
            }
            auto new_ctx = pred_node.context().clean_copy();

            if(old_ctx == new_ctx) {
                if(DEBUG) {
                    llvm::errs() << "Contexts are equal\n";
                }
                // In case the CGContexts from all predecessors don't have any missmatches
                // we don't need to do anything here
                pred_cond.successor = iter->get();
                (*iter)->append_predecessor(&pred_node);
                return;
            } else {
                auto merged_ctx = old_ctx + new_ctx;
                if(DEBUG) {
                    llvm::errs() << "Contexts are not equal\n";
                    // llvm::errs() << "Merging contexts into:\n";
                    // merged_ctx.dump();
                }

                _nodes.push_back(std::make_unique<CGNodeInstance>(merged_ctx, node, _ctx));
                auto *new_node = _nodes.back().get();
                for(const auto &pred : cur_node->predecessors()) {
                    new_node->append_predecessor(pred);
                }
                new_node->append_predecessor(&pred_node);
                pred_cond.successor = new_node;
                //also replace successor of other pred nodes because we are recreating the current node
                for(const auto &pred : cur_node->predecessors()) {
                    for(const auto &succ : pred->successors()) {
                        if(succ.successor == cur_node) {
                            BranchingCondition cond_new = { .cfg_elem = succ.cfg_elem, .condition_state = succ.condition_state, .successor = new_node };
                            pred->replace_successor(succ, cond_new);
                        }
                    }
                }

                // Since the initial context for this nodes has change by merging we need to
                // recompute all following nodes that were already created based on the previous
                // predecessor's context of this node
                // Therefore we need to delete the already created successors from the graph
                // and known_nodes
                // TODO put this into a function?
                std::set<const CGNodeInstance*> added_nodes;
                std::queue<const CGNodeInstance*> queue;
                queue.push(cur_node);
                while(!queue.empty()) {
                    const auto *node = queue.front();
                    queue.pop();
                    if(added_nodes.insert(node).second) {
                        for(const auto &succ : node->successors()) {
                            queue.push((CGNodeInstance*)succ.successor);
                        }
                    }
                }
                // TODO can this be done in a nicer way?
                for(const auto &node : added_nodes) {
                    for(const auto &n : known_nodes) {  
                        std::string target = "P" + std::to_string(this->mpi_rank) + "_" + n->name(); 
                        if(target == node->name()) {
                            if(DEBUG) {
                                llvm::errs() << "Found node to be deleted from known nodes" << n->name() << "\n";
                            }
                            known_nodes.erase(n);
                            break;
                        }
                    }
                    for(const auto &n : _nodes) {
                        if(n.get() == node) {
                            if(DEBUG) {
                                llvm::errs() << "Found node to be deleted" << n->name() << "\n";
                            }
                            std::erase(_nodes, n);
                            break;
                        }

                    }
                }
                // add back the current node to known nodes
                known_nodes.insert(&node);

                // Now recompute the successors of this node with the newly merged CGContext
                for(const auto &succ : new_node->successors()) {
                    BranchingCondition cond_new = { .cfg_elem = succ.cfg_elem, .condition_state = succ.condition_state, .successor = nullptr };
                    construct_graph_rec(*succ.successor, *new_node, cond_new, known_nodes);
                    new_node->replace_successor(succ, cond_new);
                }

                return;
            }
        } else {
            // TODO WIP how to handle undecidable control flow inside of loops?
            
            // find the already existing CGNodeInstance for this CGNode
            // TODO BUG This while process does not work correctly for loops
            // will lead to infinite evaluation chains because in the next interation 
            // it will again start with the LHS of the branch in control flow
            // TODO Figure out a different solution
            // TODO maybe if this whole function is reworked to not work recursively but with queue instead
            // it could work. The requirement is that the undecidable control flow inside a loop need to be 
            // handled before the next loop iteration is started...
            std::string name = "P" + std::to_string(this->mpi_rank) + "_" + node.name(); 
            auto iter = std::find_if(
                _nodes.begin(),
                _nodes.end(),
                [&](std::unique_ptr<CGNodeInstance> &n) { return name == n->name(); }
            );
            CGNodeInstance *cur_node = (*iter).get();

            // TODO WIP check if the loop_entry condition could lead to probles somewhere
            // could there be a case where the loop entry nodes has multiple predecessors
            // from inside the loop?
            // TODO try a while loop with counter increments in two control flow branches at the end
            if((cur_node->predecessors().size() == 1 && cur_node->predecessors().front()->name() == pred_node.name()) ) {
                // This should be the regular loop case where no undecidable control flow is present
                llvm::errs() << "xxxxxxxxxxxxxxxxxxxxxxxxx  Found regular loop case xxxxxxxxxxxxxxxxxxxxxxxxx\n";
            } else if(cur_node->attr.loop_entry) {
                // This should be the regular loop case where no undecidable control flow is present
                llvm::errs() << "xxxxxxxxxxxxxxxxxxxxxxxxx  Found loop case with loop entry xxxxxxxxxxxxxxxxxxxxxxxxx\n";
            } else {
                llvm::errs() << "xxxxxxxxxxxxxxxxxxxxxxxxx  Found loop case with undecidable control flow xxxxxxxxxxxxxxxxxxxxxxxxx\n";

                // if there are multiple predecessors that means that we need to merge their CGContexts
                // If it turns out that a known CGValue is different in the predecessors we need to 
                // reevaluate all the following nodes in the graph with the merged values
                auto old_ctx = ((CGNodeInstance*)cur_node->predecessors().front())->context().clean_copy();
                for(const auto &pred : cur_node->predecessors()) {
                    if(old_ctx != ((CGNodeInstance*)pred)->context().clean_copy()) {
                        old_ctx = old_ctx +  ((CGNodeInstance*)pred)->context().clean_copy();
                    }
                }
                auto new_ctx = pred_node.context().clean_copy();

                if(old_ctx == new_ctx) {
                    if(DEBUG) {
                        llvm::errs() << "Contexts are equal\n";
                    }
                    // In case the CGContexts from all predecessors don't have any missmatches
                    // we don't need to do anything here
                    pred_cond.successor = iter->get();
                    (*iter)->append_predecessor(&pred_node);
                    return;
                } else {
                    auto merged_ctx = old_ctx + new_ctx;
                    if(DEBUG) {
                        llvm::errs() << "Contexts are not equal\n";
                        llvm::errs() << "Merging contexts into:\n";
                        merged_ctx.dump();
                    }

                    _nodes.push_back(std::make_unique<CGNodeInstance>(merged_ctx, node, _ctx));
                    auto *new_node = _nodes.back().get();
                    for(const auto &pred : cur_node->predecessors()) {
                        new_node->append_predecessor(pred);
                    }
                    new_node->append_predecessor(&pred_node);
                    pred_cond.successor = new_node;
                    //also replace successor of other pred nodes because we are recreating the current node
                    for(const auto &pred : cur_node->predecessors()) {
                        for(const auto &succ : pred->successors()) {
                            if(succ.successor == cur_node) {
                                BranchingCondition cond_new = { .cfg_elem = succ.cfg_elem, .condition_state = succ.condition_state, .successor = new_node };
                                pred->replace_successor(succ, cond_new);
                            }
                        }
                    }

                    // Since the initial context for this nodes has change by merging we need to
                    // recompute all following nodes that were already created based on the previous
                    // predecessor's context of this node
                    // Therefore we need to delete the already created successors from the graph
                    // and known_nodes
                    // TODO put this into a function?
                    std::set<const CGNodeInstance*> added_nodes;
                    std::queue<const CGNodeInstance*> queue;
                    queue.push(cur_node);
                    while(!queue.empty()) {
                        const auto *node = queue.front();
                        queue.pop();
                        if(added_nodes.insert(node).second) {
                            for(const auto &succ : node->successors()) {
                                queue.push((CGNodeInstance*)succ.successor);
                            }
                        }
                    }
                    // TODO can this be done in a nicer way?
                    for(const auto &node : added_nodes) {
                        // for(const auto &n : known_nodes) {  
                        //     std::string target = "P" + std::to_string(this->mpi_rank) + "_" + n->name(); 
                        //     if(target == node->name()) {
                        //         if(DEBUG) {
                        //             llvm::errs() << "Found node to be deleted from known nodes" << n->name() << "\n";
                        //         }
                        //         known_nodes.erase(n);
                        //         break;
                        //     }
                        // }
                        for(const auto &n : _nodes) {
                            if(n.get() == node) {
                                if(DEBUG) {
                                    llvm::errs() << "Found node to be deleted" << n->name() << "\n";
                                }
                                std::erase(_nodes, n);
                                break;
                            }

                        }
                    }
                    // // add back the current node to known nodes
                    // known_nodes.insert(&node);

                    // Now recompute the successors of this node with the newly merged CGContext
                    for(const auto &succ : new_node->successors()) {
                        BranchingCondition cond_new = { .cfg_elem = succ.cfg_elem, .condition_state = succ.condition_state, .successor = nullptr };
                        construct_graph_rec(*succ.successor, *new_node, cond_new, known_nodes);
                        new_node->replace_successor(succ, cond_new);
                    }

                    return;
                }
            }

        }
    }


    auto new_context = pred_node.context().clean_copy();
    _nodes.push_back(std::make_unique<CGNodeInstance>(new_context, node, _ctx));
    auto *new_node = _nodes.back().get();
    new_node->append_predecessor(&pred_node);
    pred_cond.successor = new_node;


    for(const auto &succ : new_node->successors()) {
        BranchingCondition cond_new = { .cfg_elem = succ.cfg_elem, .condition_state = succ.condition_state, .successor = nullptr };
        construct_graph_rec(*succ.successor, *new_node, cond_new, known_nodes);
        new_node->replace_successor(succ, cond_new);
    }
}

// TODO This function is terrible and needs to be shortened/split-up
void CGInstance::construct_graph_loop(const CGNode &root, const std::int64_t mpi_rank, const std::int64_t mpi_size)
{
    const bool DEBUG = CGINSTANCE_DEBUG;

    std::set<const CGNode*> known_nodes;
    
    CGContext initial(CGValue::integer(mpi_rank), CGValue::integer(mpi_size));
    _nodes.push_back(std::make_unique<CGNodeInstance>(initial, root, _ctx));
    auto *new_node = _nodes.back().get();
    known_nodes.insert(&root);

    std::queue<ConstructGraphData> queue;

    for(const auto &succ : new_node->successors()) {
        BranchingCondition cond_new = { .cfg_elem = succ.cfg_elem, .condition_state = succ.condition_state, .successor = nullptr };
        // TODO make sure that every code path calls pred_node->replace_successor correctly
        queue.push({*succ.successor, new_node, cond_new, &succ});
        if(DEBUG) {
            llvm::errs() << "Added node to queue: " << succ.successor->name() << "\n";
        }
    }

    while(!queue.empty()) {
        auto [orig_node, pred_node, pred_cond, old_pred_cond] = queue.front();
        queue.pop();

        if(DEBUG) {
            llvm::errs() << " ++++++++  NOW WORKING ON NODE: " << orig_node.name() << "\n";
            dump_queue(queue);
        }

        // WIP only start with a new loop iteration if there are no more nodes from this loop in queue
        if(orig_node.attr.loop_entry && !queue.empty()) {
            bool other_loop_header_found = false;
            std::queue<ConstructGraphData> tmp_queue = queue;
            while(!tmp_queue.empty()) {
                if(tmp_queue.front().orig_node.attr.loop_entry) {
                    other_loop_header_found = true;
                    break;
                }
                tmp_queue.pop();
            }

            if(!other_loop_header_found) {
                // Put this element at the end of the queue to first finish all other nodes from this iteration
                queue.push({orig_node, pred_node, pred_cond, old_pred_cond});
                if(DEBUG) {
                    llvm::errs() << "Moved node to end of queue: " << orig_node.name() << "\n";
                    llvm::errs() << " End of loop ";
                    dump_queue(queue);
                }
                continue;
            } else {
                // error_print("Found at least one more loop header in queue. Case not yet implemented. Exiting");
                // return;
                bool skip = false;
                std::queue<ConstructGraphData> tmp_queue = queue;
                while(!tmp_queue.empty()) {
                    if(tmp_queue.front().orig_node.name() == orig_node.name()) {
                        skip = true;
                        break;
                    } 
                    tmp_queue.pop();
                }
                if(skip) {
                    // TODO possible BUG here in case there is a loop header with two predecessors (with undecidable control flow)
                    if(DEBUG) {
                        llvm::errs() << "Skipping evaluation of " << orig_node.name() << " because it appears again later in the queue\n";
                    }
                    continue;
                } else {
                    error_print("Found a different loop header in queue. Case not yet implemented. Exiting");
                    return;
                }
            }
        }

        // if the pred node was deleted due to handling of undecidable control flow we can skip thiis iteration
        if(pred_node == nullptr) {
            if(DEBUG) {
                llvm::errs() << "Skipping evaluation of " << orig_node.name() << " because predecessor does not exist anymore\n";
                llvm::errs() << " End of loop ";
                dump_queue(queue);
            }
            continue;
        }

        if(!(known_nodes.insert(&orig_node).second)) {
            if(!orig_node.attr.loop) {
                // find the already existing CGNodeInstance for this CGNode
                std::string name = "P" + std::to_string(this->mpi_rank) + "_" + orig_node.name(); 
                auto iter = std::find_if(
                    _nodes.begin(),
                    _nodes.end(),
                    [&](std::unique_ptr<CGNodeInstance> &n) { return name == n->name(); }
                );
                CGNodeInstance *cur_node = (*iter).get();

                // if there are multiple predecessors that means that we need to merge their CGContexts
                // If it turns out that a known CGValue is different in the predecessors we need to 
                // reevaluate all the following nodes in the graph with the merged values
                auto old_ctx = ((CGNodeInstance*)cur_node->predecessors().front())->context().clean_copy();
                for(const auto &pred : cur_node->predecessors()) {
                    if(pred->name() != pred_node->name()) {
                        if(old_ctx != ((CGNodeInstance*)pred)->context().clean_copy()) {
                            old_ctx = old_ctx +  ((CGNodeInstance*)pred)->context().clean_copy();
                        }
                    }
                }
                auto new_ctx = pred_node->context().clean_copy();

                if(old_ctx == new_ctx) {
                    if(DEBUG) {
                        llvm::errs() << "Contexts are equal\n";
                    }
                    // In case the CGContexts from all predecessors don't have any missmatches
                    // we don't need to do anything here
                    pred_cond.successor = iter->get();
                    pred_node->replace_successor(*old_pred_cond, pred_cond);
                    (*iter)->append_predecessor(pred_node);
                    if(DEBUG) {
                        llvm::errs() << " End of loop ";
                        dump_queue(queue);
                    }
                    continue;
                } else {
                    auto merged_ctx = old_ctx + new_ctx;
                    if(DEBUG) {
                        llvm::errs() << "Contexts are not equal\n";
                        // llvm::errs() << "Merging contexts into:\n";
                        // merged_ctx.dump();
                    }

                    _nodes.push_back(std::make_unique<CGNodeInstance>(merged_ctx, orig_node, _ctx));
                    auto *new_node = _nodes.back().get();
                    for(const auto &pred : cur_node->predecessors()) {
                        new_node->append_predecessor(pred);
                    }
                    new_node->append_predecessor(pred_node);
                    pred_cond.successor = new_node;
                    pred_node->replace_successor(*old_pred_cond, pred_cond);
                    //also replace successor of other pred nodes because we are recreating the current node
                    for(const auto &pred : cur_node->predecessors()) {
                        for(const auto &succ : pred->successors()) {
                            if(succ.successor == cur_node) {
                                BranchingCondition cond_new = { .cfg_elem = succ.cfg_elem, .condition_state = succ.condition_state, .successor = new_node };
                                pred->replace_successor(succ, cond_new);
                            }
                        }
                    }
                    
                    // Delete this node because we are currently recreating it
                    for(const auto &node : _nodes) {
                        if(node.get() == cur_node) {
                            std::erase(_nodes, node);
                        }
                    }

                    std::set<const CGNodeInstance*> added_nodes;
                    std::queue<const CGNodeInstance*> del_queue;
                    del_queue.push(cur_node);
                    while(!del_queue.empty()) {
                        const auto *node = del_queue.front();
                        del_queue.pop();
                        if(added_nodes.insert(node).second) {
                            for(const auto &succ : node->successors()) {
                                del_queue.push((CGNodeInstance*)succ.successor);
                            }
                        }
                    }
                    // TODO can this be done in a nicer way?
                    // TODO Figure out why this is required in the non-loop case for `undecidable_control_flow_right_sided.c`
                    for(const auto &node : added_nodes) {
                    
                        //  WIP EXPERMIMENTAL make this better
                        std::queue<ConstructGraphData> tmp_queue;
                        while(!queue.empty()) {
                            auto data = queue.front();
                            queue.pop();
                            if(data.orig_node.name() == node->name()) {
                                if(DEBUG) {
                                    llvm::errs() << "REMOVING NODE: " << data.orig_node.name() << " FROM QUEUE\n";
                                }
                                continue;
                            } else {
                                tmp_queue.push(data);
                            }
                        }
                        while(!tmp_queue.empty()) {
                            queue.push(tmp_queue.front());
                            tmp_queue.pop();
                        }
                    }
                    
                    // Now recompute the successors of this node with the newly merged CGContext
                    for(const auto &succ : new_node->successors()) {
                        BranchingCondition cond_new = { .cfg_elem = succ.cfg_elem, .condition_state = succ.condition_state, .successor = nullptr };
                        queue.push({*succ.successor, new_node, cond_new, &succ});
                        if(DEBUG) {
                            llvm::errs() << "Added node to queue: " << succ.successor->name() << "\n";
                        }
                    }
                    if(DEBUG) {
                        llvm::errs() << " End of loop ";
                        dump_queue(queue);
                    }
                    continue;
                }

            } else {
                // WIP TODO Loop case
                
                // TODO WIP Check this,
                // TODO remove code duplication here
                // If the node is the start of a loop we actually need to create a new node and not merge
                if(orig_node.attr.loop_entry) {
                    auto new_context = pred_node->context().clean_copy();
                    _nodes.push_back(std::make_unique<CGNodeInstance>(new_context, orig_node, _ctx));
                    auto *new_node = _nodes.back().get();
                    new_node->append_predecessor(pred_node);
                    pred_cond.successor = new_node;
                    pred_node->replace_successor(*old_pred_cond, pred_cond);

                    // TODO WIP LOOP STARTS
                    if(pred_node->attr.parent_loop != new_node->attr.parent_loop) {
                        // TODO make this a CGNodeInstance function instead so we dont need to get a mutable contect reference here
                        new_node->context().increment_loop_starts(new_node->attr.parent_loop);
                    }

                    for(const auto &succ : new_node->successors()) {
                        BranchingCondition cond_new = { .cfg_elem = succ.cfg_elem, .condition_state = succ.condition_state, .successor = nullptr };
                        queue.push({*succ.successor, new_node, cond_new, &succ});
                        if(DEBUG) {
                            llvm::errs() << "Added node to queue: " << succ.successor->name() << "\n";
                        }
                    }
                    if(DEBUG) {
                        llvm::errs() << " End of loop ";
                        dump_queue(queue);
                    }
                    continue;
                }

                
                // TODO WIP Find the correct already existing node that this is a duplicate of
                std::vector<CGNodeInstance*> existing_nodes;
                CGNodeInstance* cur_node = nullptr;
                for(const auto &n : _nodes) {
                    std::string name = "P" + std::to_string(this->mpi_rank) + "_" + orig_node.name(); 
                    if(n->name() == name) {
                        existing_nodes.push_back(n.get());
                    }
                }
                // TODO combine with previous loop?
                for(const auto &n : existing_nodes) {
                    if(pred_node->attr.parent_loop == orig_node.attr.parent_loop) {
                        // TODO BUG WIP FIXME this does not work for nested loops...
                        // probably fixable with another member in CGContext that stores how often the loop
                        // was entered from outside of itself (predecessort.loop_parent != this.loop_parent)
                        int pred_iteration = pred_node->context().get_loop_iteration(pred_node->attr.parent_loop);
                        int pred_starts = pred_node->context().get_loop_starts(pred_node->attr.parent_loop);
                        if(DEBUG) {
                            if(n->context().get_loop_iteration(n->attr.parent_loop) == pred_iteration && n->context().get_loop_starts(n->attr.parent_loop) == pred_starts) {
                                cur_node = n;
                                llvm::errs() << "Found the correct node to work on\n";
                                llvm::errs() << "   Name: " << cur_node->name() << "\n";
                                llvm::errs() << "   Iteration: " << cur_node->context().get_loop_iteration(cur_node->attr.parent_loop) << "\n";
                                llvm::errs() << "Pred Iteration: " << pred_node->context().get_loop_iteration(cur_node->attr.parent_loop) << "\n";
                                llvm::errs() << "Cur Starts: " << cur_node->context().get_loop_starts(cur_node->attr.parent_loop) << "\n";
                                llvm::errs() << "Pred Starts: " << pred_node->context().get_loop_starts(cur_node->attr.parent_loop) << "\n";
                                // pred_node->dump();
                            }
                        }
                    } else {
                        // TODO check if this case can just be combined withe the above one
                        int pred_iteration = pred_node->context().get_loop_iteration(n->attr.parent_loop);
                        int pred_starts = pred_node->context().get_loop_starts(n->attr.parent_loop);
                        if(n->context().get_loop_iteration(n->attr.parent_loop) == pred_iteration && n->context().get_loop_starts(n->attr.parent_loop) == pred_starts) {
                            cur_node = n;
                        }
                    }
                }

                // We can only have undecidable control flow if this node was already visited this loop iteration
                // Therefore we don't have to do anything special in this case
                // TODO remove code duplication here
                if(cur_node == nullptr) {
                    if(DEBUG) {
                        llvm::errs() << "No already existing node from the same iteration found. Continuing with normal graph creation.\n";
                    }
                    auto new_context = pred_node->context().clean_copy();
                    _nodes.push_back(std::make_unique<CGNodeInstance>(new_context, orig_node, _ctx));
                    auto *new_node = _nodes.back().get();
                    new_node->append_predecessor(pred_node);
                    pred_cond.successor = new_node;
                    pred_node->replace_successor(*old_pred_cond, pred_cond);

                    for(const auto &succ : new_node->successors()) {
                        BranchingCondition cond_new = { .cfg_elem = succ.cfg_elem, .condition_state = succ.condition_state, .successor = nullptr };
                        queue.push({*succ.successor, new_node, cond_new, &succ});
                        if(DEBUG) {
                            llvm::errs() << "Added node to queue: " << succ.successor->name() << "\n";
                        }
                    }
                    if(DEBUG) {
                        llvm::errs() << " End of loop ";
                        dump_queue(queue);
                    }
                    continue;
                }

                // if there are multiple predecessors that means that we need to merge their CGContexts
                // If it turns out that a known CGValue is different in the predecessors we need to 
                // reevaluate all the following nodes in the graph with the merged values
                auto old_ctx = ((CGNodeInstance*)cur_node->predecessors().front())->context().clean_copy();
                for(const auto &pred : cur_node->predecessors()) {
                    if(pred->name() != pred_node->name()) {
                        if(old_ctx != ((CGNodeInstance*)pred)->context().clean_copy()) {
                            old_ctx = old_ctx +  ((CGNodeInstance*)pred)->context().clean_copy();
                        }
                    }
                }
                auto new_ctx = pred_node->context().clean_copy();

                if(old_ctx == new_ctx) {
                    if(DEBUG) {
                        llvm::errs() << "Contexts are equal\n";
                    }
                    // In case the CGContexts from all predecessors don't have any missmatches
                    // we don't need to do anything here
                    pred_cond.successor = cur_node;
                    pred_node->replace_successor(*old_pred_cond, pred_cond);
                    cur_node->append_predecessor(pred_node);
                    if(DEBUG) {
                        llvm::errs() << " End of loop ";
                        dump_queue(queue);
                    }
                    continue;
                } else {
                    // TODO WIP
                    auto merged_ctx = old_ctx + new_ctx;
                    if(DEBUG) {
                        llvm::errs() << "Contexts are not equal\n";
                        llvm::errs() << "Merging contexts into:\n";
                        merged_ctx.dump();
                    }

                    _nodes.push_back(std::make_unique<CGNodeInstance>(merged_ctx, orig_node, _ctx));
                    auto *new_node = _nodes.back().get();
                    for(const auto &pred : cur_node->predecessors()) {
                        new_node->append_predecessor(pred);
                    }
                    new_node->append_predecessor(pred_node);
                    pred_cond.successor = new_node;
                    pred_node->replace_successor(*old_pred_cond, pred_cond);

                    // TODO WIP LOOP STARTS
                    if(pred_node->attr.parent_loop != new_node->attr.parent_loop) {
                        // TODO make this a CGNodeInstance function instead so we dont need to get a mutable contect reference here
                        new_node->context().increment_loop_starts(new_node->attr.parent_loop);
                    }

                    //also replace successor of other pred nodes because we are recreating the current node
                    for(const auto &pred : cur_node->predecessors()) {
                        for(const auto &succ : pred->successors()) {
                            if(succ.successor == cur_node) {
                                BranchingCondition cond_new = { .cfg_elem = succ.cfg_elem, .condition_state = succ.condition_state, .successor = new_node };
                                pred->replace_successor(succ, cond_new);
                            }
                        }
                    }

                    // Delete this node because we are currently recreating it
                    for(const auto &node : _nodes) {
                        if(node.get() == cur_node) {
                            std::erase(_nodes, node);
                        }
                    }

                    // Now recompute the successors of this node with the newly merged CGContext
                    for(const auto &succ : new_node->successors()) {
                        BranchingCondition cond_new = { .cfg_elem = succ.cfg_elem, .condition_state = succ.condition_state, .successor = nullptr };
                        queue.push({*succ.successor, new_node, cond_new, &succ});
                        if(DEBUG) {
                            llvm::errs() << "Added node to queue: " << succ.successor->name() << "\n";
                        }
                    }
                    if(DEBUG) {
                        llvm::errs() << " End of loop ";
                        dump_queue(queue);
                    }
                    continue;

                }
            }
        }

        auto new_context = pred_node->context().clean_copy();
        _nodes.push_back(std::make_unique<CGNodeInstance>(new_context, orig_node, _ctx));
        auto *new_node = _nodes.back().get();
        new_node->append_predecessor(pred_node);
        pred_cond.successor = new_node;
        pred_node->replace_successor(*old_pred_cond, pred_cond);

        // TODO WIP LOOP STARTS
        if(pred_node->attr.parent_loop != new_node->attr.parent_loop) {
            // TODO make this a CGNodeInstance function instead so we dont need to get a mutable contect reference here
            new_node->context().increment_loop_starts(new_node->attr.parent_loop);
        }

        for(const auto &succ : new_node->successors()) {
            BranchingCondition cond_new = { .cfg_elem = succ.cfg_elem, .condition_state = succ.condition_state, .successor = nullptr };
            queue.push({*succ.successor, new_node, cond_new, &succ});
            if(DEBUG) {
                llvm::errs() << "Added node to queue: " << succ.successor->name() << "\n";
            }
        }
        if(DEBUG) {
            llvm::errs() << " End of loop ";
            dump_queue(queue);
        }
    }
}



void CGInstance::dump() const
{
    for(const auto &node : _nodes) {
        llvm::errs() << "Created CGNodeInstance: \n";
        node->dump();
        llvm::errs() << "=====\n";
    }
}

// TODO include information about evaluated values and expressions in graph visualization
std::string CGInstance::output_cg() const
{
    std::error_code ec;
    std::ostringstream ofs; // Create an output string stream

    // The label text is formatted in HTML
    for(const auto &node : _nodes) {
        auto block_name = node->name();
        // Contents of the Node box
        ofs << "    " << "\"" <<  block_name << "\"" << "[label=< <B>" << escape_to_html(block_name) << "</B><BR/>";
        for(const auto &stmt : node->elements()) {
            auto stmt_string = escape_to_html(cfgelement_to_str(&stmt));
            stmt_string.append(" | " + escape_to_html(node->get_evaluated_cfg_as_string(stmt)));
            stmt_string.append("<BR/>");
            ofs << stmt_string;
        }
        ofs << ">, shape=box]\n";

        // Specifying the edges
        for(const auto &succ : node->successors()) {
            ofs << "    " << "\"" << block_name << "\"" << "->" << "\"" << succ.successor->name() << "\"";
            if(succ.condition_state && succ.cfg_elem.has_value()) {
                auto condition_string = escape_to_html(cfgelement_to_str(&succ.cfg_elem.value()));
                ofs << "[label=\"" << condition_string << "\"]\n";
            } else if(!succ.condition_state && succ.cfg_elem.has_value()) {
                auto condition_string = escape_to_html(cfgelement_to_str(&succ.cfg_elem.value()));
                ofs << "[label=\"!(" << condition_string << ")\"]\n";
            } else {
                ofs << "\n";
            }
        }
    }
    return ofs.str();
}

void CGInstance::dump_queue(std::queue<ConstructGraphData> &queue) const
{
    llvm::errs() << "Queue: [";
    if(!queue.empty()) {
        std::queue<ConstructGraphData> tmp_queue = queue;
        while(!tmp_queue.empty()) {
            llvm::errs() << tmp_queue.front().orig_node.name() << ", ";
            tmp_queue.pop();
        }
        llvm::errs() << "]\n";
    } else {
        llvm::errs() << "]\n";
    }
}
