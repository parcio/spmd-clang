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

#include "CommunicationGraph.h"
#include "CGContext.h"
#include "CGInstance.h"
#include "CGNode.h"
#include "CPNBuilder.h"
#include "CPNMarking.h"
#include "CPNNet.h"
#include "CPNStateGraph.h"
#include "CPNTransition.h"
#include "debug.h"
#include "utils.h"
#include <algorithm>
#include <clang/AST/Expr.h>
#include <clang/Analysis/CFG.h>
#include <clang/Basic/LLVM.h>
#include <clang/Basic/SourceManager.h>
#include <deque>
#include <iterator>
#include <llvm/Support/Casting.h>
#include <llvm/Support/raw_ostream.h>
#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <set>
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

#include "llvm/Support/TimeProfiler.h"


CommunicationGraph::CommunicationGraph(const SPMDData &spmd_data) : _spmd_data(spmd_data),  _ast_ctx(*spmd_data._ctx)
{
    {
        llvm::TimeTraceScope TimeScope("CGCreation");
        construct_graph();
        validate();
        prune_graph();
        // TODO collect_comm_operations
        output_cg("cg.gv");

        // TODO WIP cfg-vis
        output_reactflow(make_output_filename("_cfg.json"));

        // TODO handle loops inside loops
        detect_loops();
    }

    analysis(_spmd_data.process_count());
}


// cfg-vis function; TODO: probably move to own function
std::string CommunicationGraph::node_to_json(const CGNode& node, std::string parentId) {

    int lineNo = 0;
    std::string ret = "\t\t{\n\t\t\t\"id\":\"" + node.name() + "\",\n\t\t\t\"type\": \"cfg-node\",\n\t\t\t\"hidden\": false,\n\t\t\t\"data\": {\n" +
                        "\t\t\t\t\"label\": \"" + node.name() + "\",\n\t\t\t\t\"text\": [\n";
    
    for(auto stmt = node.elements().begin(); stmt != node.elements().end(); stmt++)
    {
        auto *statement = stmt->getAs<clang::CFGStmt>()->getStmt();
        auto lineLoc = statement->getSourceRange().getBegin().printToString(_spmd_data._ctx->getSourceManager());
        std::size_t pos1 = lineLoc.find(":")+1;
        std::size_t pos2 = lineLoc.substr(pos1).find(":");
        lineNo = std::stoi(lineLoc.substr(pos1, pos2));

        ret.append("\t\t\t\t{\n\t\t\t\t\t\"no\": " + std::to_string(lineNo) + ",\n\t\t\t\t\t\"content\": \"" +
                    escape_to_html(cfgelement_to_str(stmt.base())) + "\"\n\t\t\t\t},\n");
    }

    if(node.elements().size() > 0) {
        ret.resize(ret.size()-2);         // delete last , in text-Array
        ret.append("\n");
    }

    ret.append("\t\t\t\t]\n\t\t\t},\n");
    if(parentId != "main") {
        ret.append("\t\t\t\"parentId\": \"" + parentId + "\",\n\t\t\t\"extent\": \"parent\",\n");
    }
    return ret.append("\t\t\t\"position\": { \"x\": 0, \"y\": 0 }\n\t\t},\n");
}

// cfg-vis function; TODO: probably move to own function
std::string CommunicationGraph::edge_to_json(const CGNode& node, const BranchingCondition& succ) {

    std::string srcId = node.name();
    std::string tarId = succ.successor->name();
    
    std::string ret = "\t\t{\n\t\t\t\"id\": \"edg-f-" + srcId + "-t-" + tarId + "\",\n\t\t\t\"source\": \"" + srcId + "\",\n\t\t\t\"target\": \"" + tarId + 
            "\",\n\t\t\t\"label\": \"";

    if(succ.cfg_elem) {
        if(!succ.condition_state) {
            ret.append("!");
        }
        const clang::CFGElement* cfg_el = &succ.cfg_elem.value();
        std::string temp = cfgelement_to_str(cfg_el);
        if(temp.find('"') != std::string::npos ) {
            
            std::string output;
            for(auto c : temp) {
                if(c == '"') {
                    output.append("\\\"");
                }
                else { output.push_back(c); }
            }
            temp = output;
        }
        ret.append(temp);
    } 

    return ret.append("\",\n\t\t\t\"type\": \"labled-directed-edge\",\n\t\t\t\"markerEnd\": {\n\t\t\t\t\"type\": \"arrowclosed\"\n\t\t\t},\n\t\t\t\"animated\": false\n\t\t},\n"); 
}

// cfg-vis function; TODO: probably move to own function
void CommunicationGraph::output_reactflow(std::string path) 
{
    std::error_code ec;
    llvm::raw_fd_stream ofs(path, ec);
    std::string edges = "";
    std::string nodes = "";

    std::unordered_map<std::string, std::string> functionNodes;
    std::unordered_map<std::string, std::string> functionParents;

    // 1. Collect information about functions and nested functions. 

    // find all functions parents
    for(std::unique_ptr<CGNode>& node : _nodes) {

        // search for function entry nodes
        if (node->attr.func_entry) {
            std::string funcName = node->func->getNameInfo().getAsString();

            // handle _copy suffix
            std::size_t pos = node->name().find("_copy");
            if (pos != std::string::npos)
                funcName.append(node->name().substr(pos));

            // find parent nodes of functions by the entry nodes predecessors parent (predecessor contains function call)
            for (auto &pred : node->predecessors()) {
                std::string parentName = pred->func->getNameInfo().getAsString();

                std::size_t pos2 = pred->name().find("_copy");
                if (pos2 != std::string::npos)
                    parentName.append(pred->name().substr(pos2));
                if (parentName != funcName)
                    functionParents[funcName] = parentName;   
            }
        }
    }

    // create json-Output for every function node
    for(std::unique_ptr<CGNode>& node : _nodes) {
        std::string funcName = node->func->getNameInfo().getAsString();

        if(funcName == "main") {
            continue;
        }

        std::size_t pos = node->name().find("_copy");
        if (pos != std::string::npos)
            funcName.append(node->name().substr(pos));

        if (!functionNodes.contains(funcName)) {

            std::string json = "\t\t{\n\t\t\t\"id\": \"" + funcName + "\",\n\t\t\t\"type\": \"function-node\",\n\t\t\t\"hidden\": false,\n\t\t\t\"data\": {\n" +
            "\t\t\t\t\"label\": \"" + funcName + "()\",\n\t\t\t\t\"text\": []\n\t\t\t},\n\t\t\t\"position\": { \"x\": 0, \"y\": 0 }";

            if (functionParents.contains(funcName)) {
                if (functionParents[funcName] != "main") {
                    json += ", \"parentId\": \"" + functionParents[funcName] + "\", \"extent\": \"parent\"";
                }
            }
            json += " },\n";

            functionNodes[funcName] = json;
        }
    }

    // append function-nodes to export
    for (auto &[name, json] : functionNodes)
        nodes.append(json);

    // 2. Collect information about (normal) nodes and edges. 
    
    // collect node and edge information and save it in a flat structure (-> no hierarchy-structure in json-file)
    for(std::unique_ptr<CGNode>& node : _nodes) {

        // check if node belongs to a function
        std::string functionNodeId = node->func->getNameInfo().getAsString();

        // check if function is called multiple times
        std::size_t pos = node->name().find("_copy");
        if(pos != std::string::npos) {
            functionNodeId.append(node->name().substr(pos));
        }

        nodes.append(node_to_json(*node, functionNodeId));

        // save edges to all successors of each node
        for(auto &succ : node->successors()) {
            edges.append(std::string(edge_to_json(*node, succ)));
        }
    }

    // check if output was successfull and delete trailing commas
    if(nodes.size() < 1) {
        llvm::errs() << "Could not export nodes of CFG to JSON." << "\n";
    }
    else {
        nodes.resize(nodes.size()-2);
    }

    if(edges.size() < 1) {
        llvm::errs() << "Could not export edges of CFG to JSON." << "\n";
    }
    else {
        edges.resize(edges.size()-2);
    }

    ofs << "{ \"nodes\": [\n" << nodes << "\n\t],\n\"edges\": [\n" << edges << "\n\t]\n}";
    ofs.flush();
    ofs.close();
}
// cfg-vis function; TODO: probably move to own function
void CommunicationGraph::output_reactflow_cgi(const std::string &path, const std::vector<CGInstance> &cg_instances) const
{

    std::error_code ec;
    llvm::raw_fd_stream ofs(path, ec);

    ofs << "{\n  \"processes\": [\n";

    bool first = true;
    for (const auto &cg_instance : cg_instances) {
        if (!first) {
            ofs << ",\n";
        }
        first = false;
        // CGInstance::output_cgi_rf returns a JSON object string for the process
        ofs << cg_instance.output_cgi_rf();
    }

    ofs << "\n\t]\n}";
    ofs.flush();
    ofs.close();
}
// cfg-vis function; TODO: probably move to own function
std::string CommunicationGraph::make_output_filename(const std::string &suffix) const
{
    std::string basename = _spmd_data.source_file_basename();
    if(basename.empty()) {
        basename = "program";
    }
    else {
        std::size_t dotPos = basename.rfind('.');
        if(dotPos != std::string::npos) {
            basename = basename.substr(0, dotPos);
        } 
    }

    return basename + suffix;
}



void CommunicationGraph::output_cg(const std::string &path) const
{
    std::error_code ec;
    llvm::raw_fd_stream ofs(path, ec);

    ofs << "digraph G {\n";

    // The label text is formatted in HTML
    for(const auto &node : _nodes) {
        auto block_name = node->name();
        // Contents of the Node box
        ofs << "    " << "\"" <<  block_name << "\"" << "[label=< <B>" << escape_to_html(block_name) << "</B><BR/>";
        for(const auto &stmt : node->elements()) {
            auto stmt_string = escape_to_html(cfgelement_to_str(&stmt));
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
    ofs << "}";
    ofs.flush();
    ofs.close();
}


void CommunicationGraph::output_func_cg(const std::string &path, const FuncCG &fcg) const
{
    std::error_code ec;
    llvm::raw_fd_stream ofs(path, ec);

    ofs << "digraph G {\n";

    // The label text is formatted in HTML
    for(const auto &node : fcg.nodes) {
        auto block_name = node->name();
        // Contents of the Node box
        ofs << "    " << "\"" <<  block_name << "\"" << "[label=< <B>" << escape_to_html(block_name) << "</B><BR/>";
        for(const auto &stmt : node->elements()) {
            auto stmt_string = escape_to_html(cfgelement_to_str(&stmt));
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
    ofs << "}";
    ofs.flush();
    ofs.close();
}



void CommunicationGraph::construct_graph()
{
    const bool DEBUG = CONSTRUCT_GRAPH_DEBUG;
    if(DEBUG) {
        llvm::errs() << "==========================\n";
        llvm::errs() << "= Constructing global CG =\n";
        llvm::errs() << "==========================\n";
    }

    for(const auto &func_cfg : _spmd_data.cfg_parts()) {
        auto first_block = func_cfg.cfg->getEntry().succ_begin()->getReachableBlock();

        if(DEBUG) {
            llvm::errs() << "Working on following function:\n";
            func_cfg.cfg->dump(_ast_ctx.getLangOpts(), true);
        }

        std::vector<clang::CFGBlock*> known_blocks;
        FuncCG fcg = { .func = func_cfg.func };
        construct_graph_rec(fcg, *first_block, nullptr, nullptr, known_blocks);

        auto exit_node = std::find_if(
            fcg.nodes.begin(),
            fcg.nodes.end(),
            [&](CGNode *n) { return n->successors().empty(); }
        );
        if(exit_node != fcg.nodes.end()) {
            fcg.exit = *exit_node;
            (*exit_node)->attr.func_exit = true;
            fcg.root->attr.func_entry = true;
        } else {
            warning_print("No function exit node could be found");
        }

        if(DEBUG) {
            llvm::errs() << "Final Function CG:\n";
            dump(fcg);
            llvm::errs() << "---\n";
        }

        _func_cgs.push_back(fcg);
    }

    combine_partial_graphs();
    instantiate_func_uses();
}

// TODO repplace vecotr with set
void CommunicationGraph::construct_graph_rec(FuncCG &fcg, clang::CFGBlock &block, CGNode *pred_node, BranchingCondition *pred_cond, std::vector<clang::CFGBlock*> &known_blocks)
{
    // Since there can be recursion or multiple predecessors for one node we need to check if this node was already handled
    // TODO Test this
    if(std::find(known_blocks.begin(), known_blocks.end(), &block) != known_blocks.end()) {
        auto node_name = create_node_name(block, *fcg.func);
        auto iter = std::find_if(
            fcg.nodes.begin(),
            fcg.nodes.end(),
            [&](CGNode *n) { return (n->name() == node_name); }
        );
        if(iter != fcg.nodes.end()) {
            pred_cond->successor = *iter;
            (*iter)->append_predecessor(pred_node);
        }
        return;
    } else {
        known_blocks.push_back(&block);
    }
    
    _nodes.push_back(std::make_unique<CGNode>(create_node_name(block, *fcg.func), *fcg.func));
    auto new_node = _nodes.back().get();
    fcg.nodes.push_back(new_node);
    // No predecessor means this is the first node of the FuncCG
    if(!pred_node) {
        fcg.root = new_node;
    } else {
        // Add the edge from the previous node
        new_node->append_predecessor(pred_node);
        pred_cond->successor = new_node;
    }

    // Add all but the last CFGElements from the original block to the current node
    // The last CFGElement can be the branching condition if there is any branching
    for(auto elem = block.begin(); elem != block.end(); elem++) {
        // In most cases there are either one or two successors to a clang CFG node
        // If we have two they need to be ordered correctly and stored with
        // information about the branching condition
        if(block.succ_size() == 2 && elem == block.end()-1) {
            bool true_reachable = block.succ_begin()->isReachable();
            bool false_reachable = (block.succ_begin()+1)->isReachable();

            if(true_reachable && false_reachable) {
                BranchingCondition cond_true = { *elem, true, nullptr };
                construct_graph_rec(fcg, **block.succ_begin(), new_node, &cond_true, known_blocks);
                new_node->append_successor(cond_true);

                BranchingCondition cond_false = { *elem, false, nullptr };
                construct_graph_rec(fcg, **(block.succ_begin()+1), new_node, &cond_false, known_blocks);
                new_node->append_successor(cond_false);
                
            } else {
                if(!true_reachable && !false_reachable) {
                    warning_print("Encountered block with only unreachable successors.");
                } else if (true_reachable) {
                    BranchingCondition cond_empty = { *elem, true, nullptr };
                    construct_graph_rec(fcg, **block.succ_begin(), new_node, &cond_empty, known_blocks);
                    new_node->append_successor(cond_empty);
                } else if (false_reachable) {
                    BranchingCondition cond_empty = { *elem, false, nullptr };
                    construct_graph_rec(fcg, **(block.succ_begin()+1), new_node, &cond_empty, known_blocks);
                    new_node->append_successor(cond_empty);
                }
            }
        } else if(block.succ_size() > 2 && elem == block.end()-1) {
            // Not sure if this case is possible
            error_print("More than 2 successots found. This case is not supported yet");
        } else {
            new_node->append_element(*elem);
        }
    }
    // If there is no branching the condition is left empty
    if(block.succ_size() == 1) {
        BranchingCondition cond_empty = { .cfg_elem = std::nullopt, .condition_state = true, .successor = nullptr };
        construct_graph_rec(fcg, **block.succ_begin(), new_node, &cond_empty, known_blocks);
        new_node->append_successor(cond_empty);
    }

}

void CommunicationGraph::combine_partial_graphs()
{
    const bool DEBUG = COMBINE_PARTIAL_GRAPHS_DEBUG;
    if(DEBUG) {
        llvm::errs() << "================================\n";
        llvm::errs() << "= Combining function based CGs =\n";
        llvm::errs() << "================================\n";
    }

    // DISS DEBUG TODO REMOVE
    // for(const auto &fcg : _func_cgs) {
    //     output_func_cg(fcg.func->getNameAsString() + ".gv", fcg);
    // }

    if(const auto *main_cg = get_fcg("main")) {
        _root = main_cg->root;
        std::vector<const FuncCG*> funcs_to_add;
        std::vector<CGNode*> known_nodes;
        std::set<CGNode*> nodes_to_remove;

        combine_partial_graphs_rec(main_cg->root, funcs_to_add, known_nodes, nodes_to_remove);

        // After the main function has been worked on we add the remaining functions to the CG
        std::set<const FuncCG*> added_funcs;
        while(!funcs_to_add.empty()) {
            auto *func_cg = funcs_to_add.back();
            funcs_to_add.pop_back();
            // In case of a function being called multiple times in the code we only need to add it once here
            if(added_funcs.insert(func_cg).second) {
                if(DEBUG) {
                    llvm::errs() << "Integrating \'" << func_cg->func->getNameAsString() << "\' into the CG\n";
                }
                combine_partial_graphs_rec(func_cg->root, funcs_to_add, known_nodes, nodes_to_remove);
            } else {
                if(DEBUG) {
                    llvm::errs() << "Function \'" << func_cg->func->getNameAsString() << "\' already added\n";
                }
            }
        }
        
        for(const auto &node : nodes_to_remove) {
            _nodes.erase(std::find_if(
                _nodes.begin(),
                _nodes.end(),
                [&](std::unique_ptr<CGNode>& n) { return n.get() == node; }
            ));
        }

        if(DEBUG) {
            llvm::errs() << "=====\n";
            this->dump();
        }
    }


    // TODO DISS DEBUG REMOVE
    // output_cg("cg_phase1.gv");
}

    void CommunicationGraph::combine_partial_graphs_rec(CGNode *node, std::vector<const FuncCG*> &funcs_to_add, std::vector<CGNode*> &known_nodes, std::set<CGNode*> &nodes_to_remove)
{
    const bool DEBUG = COMBINE_PARTIAL_GRAPHS_REC_DEBUG;
    // If the node has been worked on before we do not need to do it again
    if(std::find(known_nodes.begin(), known_nodes.end(), node) != known_nodes.end()) {
        return;
    } else {
        known_nodes.push_back(node);
    }

    bool was_split = false;
    for(auto elem = node->elements().begin(); elem != node->elements().end(); elem++) {
        const auto stmt = elem->getAs<clang::CFGStmt>()->getStmt();

        if(auto *call_expr = clang::dyn_cast<clang::CallExpr>(stmt)) {
            if(const auto *func_cg = get_fcg(call_expr->getDirectCallee()->getNameAsString())) {
                if(DEBUG) {
                    llvm::errs() << "Following block needs to be split at following statement:\n";
                    node->dump();
                    elem->dump();
                    llvm::errs() << "---\n";
                }

                // The contents of the function will be worked on later
                funcs_to_add.push_back(func_cg);

                // TODO figure out how to deal with functions that have return values
                // The Stmts for that in the CFGBlock are in 2 parts (func call -> return value handling)
                _nodes.push_back(std::make_unique<CGNode>(node->name() + "_s", *node->func));
                auto start_node = _nodes.back().get();
                start_node->attr.func_entry = node->attr.func_entry;
                // The first split gets all elements up to the function call
                for(auto iter = node->elements().begin(); iter != elem; iter++) {
                    start_node->append_element(*iter);
                }
                _nodes.push_back(std::make_unique<CGNode>(node->name() + "_e", *node->func));
                auto end_node = _nodes.back().get();
                end_node->attr.func_exit = node->attr.func_exit;
                for(auto iter = elem+1; iter != node->elements().end(); iter++) {
                    end_node->append_element(*iter);
                }
                for(const auto &succ : node->successors()) {
                    end_node->append_successor(succ);
                }

                // Add an edge to the beginning of the called function
                BranchingCondition cond_call = { .cfg_elem = *elem, .condition_state = true, .successor = func_cg->root };
                start_node->append_successor(cond_call);
                for(auto *pred : node->predecessors()) {
                    start_node->append_predecessor(pred);
                }
                if(_root == node) {
                    _root = start_node;
                }

                // Replace uses of the original node in predecessors with the new node
                for(auto *pred : node->predecessors()) {
                    for(const auto &pred_succ : pred->successors()) {
                        if(pred_succ.successor == node) {
                            BranchingCondition cond_new = { .cfg_elem = pred_succ.cfg_elem, .condition_state = pred_succ.condition_state, .successor = start_node };
                            pred->replace_successor(pred_succ, cond_new);
                        }
                    }
                }
                // Replace use of the original node in all successors
                for(const auto &succ : node->successors()) {
                    for(auto *pred : succ.successor->predecessors()) {
                        if(pred == node) {
                            succ.successor->replace_predecessor(*node, *end_node);
                        }
                    }
                }
                func_cg->root->append_predecessor(start_node);
                // Find the exit node of the called function and add an edge from it to the new end_node
                // TODO check if return values need to be handled here
                BranchingCondition cond_return = { .cfg_elem = std::nullopt, .condition_state = true, .successor = end_node };
                func_cg->exit->append_successor(cond_return);
                end_node->append_predecessor(func_cg->exit);

                was_split = true;

                if(DEBUG) {
                    llvm::errs() << "Created Node:\n";
                    start_node->dump();
                    llvm::errs() << "---\n";
                    llvm::errs() << "Created Node:\n";
                    end_node->dump();
                }

                // If there are more than one CallExpr in this node we need
                // to split the newly created end_node again and not the no longer used original node
                nodes_to_remove.insert(node);
                combine_partial_graphs_rec(end_node, funcs_to_add, known_nodes, nodes_to_remove);

                // TODO remove?
                // 
                // _nodes.erase(std::find_if(
                //     _nodes.begin(),
                //     _nodes.end(),
                //     [&](std::unique_ptr<CGNode>& n) { return n.get() == node; }
                // ));
                break;
            }
        }

        // TODO potential code in deprecated CommunicationGraph
        if(elem->getKind() >= elem->DTOR_BEGIN && elem->getKind() <= elem->DTOR_END) {
            error_print("DTOR case in combine partial graphs not yet implemented");
            elem->dump();
        }

        // TODO potential code in deprecated CommunicationGraph
        if(clang::dyn_cast<clang::CXXConstructExpr>(stmt)) {
            error_print("Constructor case in combine partial grapahs not yet implemented");
            elem->dump();
        }
    }

    if(!was_split) {
        for(const auto &succ : node->successors()) {
            // Only go on with CG creation while successors are still in the same function
            // Other function will already be handled from the main combine function
            if(node->func == succ.successor->func) {
                combine_partial_graphs_rec(succ.successor, funcs_to_add, known_nodes, nodes_to_remove);
            }
        }
    }
}


// TODO figure out if multiple function exits can be a problem
void CommunicationGraph::instantiate_func_uses() 
{
    const bool DEBUG = INSTANTIATE_FUNC_USES_DEBUG;
    if(DEBUG) {
        llvm::errs() << "===============================\n";
        llvm::errs() << "= Instantiating function uses =\n";
        llvm::errs() << "===============================\n";
    }

    std::queue<std::pair<CGNode*, CGNode*>> func_uses;

    // TODO worth it to make this not O(n^2) ?
    // TODO multiple function exits?
    // Find all pairs of function entries/exits 
    for(const auto &entry_node : _nodes) {
        if(entry_node->attr.func_entry) {
            for(const auto &exit_node : _nodes) {
                if(exit_node->attr.func_exit && entry_node->func == exit_node->func) {
                    func_uses.push({entry_node.get(), exit_node.get()});
                }
            }
        }
    }

    llvm::errs() << "-----\n";
    while(!func_uses.empty()) {
        auto [entry, exit] = func_uses.front();
        func_uses.pop();

        if(entry->predecessors().size() != exit->successors().size()) {
            error_print("Function use has mismatching number of predecessors to successors!");
            if(DEBUG) {
                entry->dump();
                exit->dump();
            }
        }
        for(const auto &succ : exit->successors()) {
            if(succ.cfg_elem) {
                warning_print("UNEXPECTED CASE! Function use successor has a branching condition!");
            }
        }

        // Each call of a functions gets its own set of nodes in the graph
        while(entry->predecessors().size() > 1) {
            if(DEBUG) {
                llvm::errs() << "Adding multiple instantiations of function use:\n";
                entry->dump();
                exit->dump();
                llvm::errs() << "---\n";
            }

            // Predecessor/Successor lists of the entry/exit nodes are created in the same order
            // therefore we get the correct pair of predecessor/successor this way
            const auto &pred = entry->predecessors().back();
            entry->pop_predecessor();
            const auto &succ = exit->successors().back();
            exit->pop_successor();
            
            // Create the set of new nodes for this function call
            std::vector<std::unique_ptr<CGNode>> node_range = copy_node_range(*entry, *exit, *pred, *succ.successor);
            
            if(DEBUG) {
                llvm::errs() << "Listing Nodes that need to be copied\n";
                for(const auto &n : node_range) {
                    n->dump();
                }
                llvm::errs() << "---\n";
            }

            // New nodes need new names to correctly identify them
            for(auto &n : node_range) {
                n->set_name(n->name() + "_copy" + std::to_string(entry->predecessors().size()));

                // function calls appearing inside the copied range of nodes also need to be handled
                if(n->attr.func_entry && n.get() != &*entry) {
                        auto exit_node = std::find_if(
                            _nodes.begin(),
                            _nodes.end(),
                            [&](const std::unique_ptr<CGNode> &exit) { return (exit->attr.func_exit && (exit->func == n->func ));}
                        );
                    func_uses.push(std::make_pair(n.get(), exit_node->get()));
                } 
            }
            // Move all the newly created nodes into _nodes
            _nodes.insert(
                _nodes.end(),
                std::make_move_iterator(node_range.begin()),
                std::make_move_iterator(node_range.end())
            );
        }
    
    }

}


std::vector<std::unique_ptr<CGNode>> CommunicationGraph::copy_node_range(const CGNode &start, const CGNode &end, CGNode &start_pred, CGNode &end_succ)
{
    std::vector<std::unique_ptr<CGNode>> new_nodes;
    std::set<const CGNode*> known_nodes;

    known_nodes.insert(&start);

    new_nodes.push_back(std::make_unique<CGNode>(start.name(), *start.func));
    auto new_node = new_nodes.back().get();
    new_node->append_predecessor(&start_pred);
    for(const auto &pred_succ : start_pred.successors()) {
        if(pred_succ.successor == &start) {
            BranchingCondition cond_new = { .cfg_elem = pred_succ.cfg_elem, .condition_state = pred_succ.condition_state, .successor = new_node };
            start_pred.replace_successor(pred_succ, cond_new);
        }
    }
    new_node->attr.func_entry = start.attr.func_entry;
    new_node->attr.func_exit = start.attr.func_exit;
    for(const auto &elem : start.elements()) {
        new_node->append_element(elem);
    }

    for(const auto &succ : start.successors()) {
        BranchingCondition cond_new = succ;
        copy_node_range_rec(*succ.successor, *new_node, cond_new, known_nodes, new_nodes, end, end_succ);
        new_node->append_successor(cond_new);
    }

    return new_nodes;
}


void CommunicationGraph::copy_node_range_rec(CGNode &node, CGNode &pred_node, BranchingCondition &pred_cond, std::set<const CGNode*> &known_nodes, std::vector<std::unique_ptr<CGNode>> &new_nodes, const CGNode &end, CGNode &end_succ) 
{
    // If the node has already been visited just connect it to its predecessor
    if(!known_nodes.insert(&node).second) {
        auto n = std::find_if(
            new_nodes.begin(),
            new_nodes.end(),
            [&](std::unique_ptr<CGNode> &n) { return n->name() == node.name(); }
        );
        pred_cond.successor = n->get();
        n->get()->append_predecessor(&pred_node);
        return;
    }

    new_nodes.push_back(std::make_unique<CGNode>(node.name(), *node.func));
    auto new_node = new_nodes.back().get();
    new_node->append_predecessor(&pred_node);
    pred_cond.successor = new_node;
    new_node->attr.func_entry = node.attr.func_entry;
    new_node->attr.func_exit = node.attr.func_exit;
    for(const auto &elem : node.elements()) {
        new_node->append_element(elem);
    }

    if(&node != &end) {
        for(const auto &succ : node.successors()) {
            BranchingCondition cond_new = succ;
            copy_node_range_rec(*succ.successor, *new_node, cond_new, known_nodes, new_nodes, end, end_succ);
            new_node->append_successor(cond_new);
        }
    // If this is the last node in the range to be copied we need to connect it to the rest of the global CG
    } else {
        BranchingCondition cond = { .cfg_elem = std::nullopt, .condition_state = true, .successor = &end_succ };
        new_node->append_successor(cond);
        end_succ.replace_predecessor(node, *new_node);
    }
}

void CommunicationGraph::validate() const
{
    const bool DEBUG = VALIDATE_DEBUG;
    if(DEBUG) {
        llvm::errs() << "====================\n";
        llvm::errs() << "= Validating graph =\n";
        llvm::errs() << "====================\n";
    }

    // Check graph for connectivity
    auto nodes = get_node_range(*_root);
    if(nodes.size() != _nodes.size()) {
        error_print("CommunicationGraph seems to be disconnected!");
        llvm::errs() << nodes.size() << " / " << _nodes.size() << "\n"; 
        if(nodes.size() > _nodes.size()) {
            for(const auto &n1 : nodes) {
                bool duplicate = false;
                for(const auto &n2 : _nodes) {
                    if(n1 == n2.get()) {
                        duplicate = true;
                    }
                }
                if(!duplicate) {
                    llvm::errs() << "Node seems to be disconnected: \n";
                    n1->dump();
                    llvm::errs() << "---\n";
                }
            }
        } else {
            for(const auto &n1 : _nodes) {
                bool duplicate = false;
                for(const auto &n2 : nodes) {
                    if(n1.get() == n2) {
                        duplicate = true;
                    }
                }
                if(!duplicate) {
                    llvm::errs() << "Node seems to be disconnected: \n";
                    n1->dump();
                    llvm::errs() << "---\n";
                }
            }
        }

        for(const auto &n1 : nodes) {
            int counter = 0;
            for(const auto &n2 : nodes) {
                if(n1 == n2) {
                    counter++; 
                }
            }
            if(counter != 1) {
                llvm::errs() << "Duplicate node: (" << std::to_string(counter) << " times)\n";
                n1->dump();
                llvm::errs() << "---\n";
            }
        }

        for(const auto &n1 : _nodes) {
            int counter = 0;
            for(const auto &n2 : _nodes) {
                if(n1.get() == n2.get()) {
                    counter++; 
                }
            }
            if(counter != 1) {
                llvm::errs() << "Duplicate node: (" << std::to_string(counter) << " times)\n";
                n1->dump();
                llvm::errs() << "---\n";
            }
        }
    }
}

std::vector<const CGNode*> CommunicationGraph::get_node_range(const CGNode &start) const
{
    std::set<const CGNode*> added_nodes;
    std::queue<const CGNode*> queue;

    queue.push(&start);
    while(!queue.empty()) {
        const auto *node = queue.front();
        queue.pop();
        if(added_nodes.insert(node).second) {
            for(const auto &succ : node->successors()) {
                queue.push(succ.successor);
            }
        }
    }

    return std::vector<const CGNode*>(added_nodes.begin(), added_nodes.end());
}

// TODO reformat node names and distribute IDs?
void CommunicationGraph::prune_graph()
{
    const bool DEBUG = PRUNE_GRAPH_DEBUG;
    if(DEBUG) {
        llvm::errs() << "=================\n";
        llvm::errs() << "= Pruning graph =\n";
        llvm::errs() << "=================\n";
    }

    std::vector<const CGNode*> empty_nodes;

    for(auto const &node: _nodes) {
        // Only remove simple empty nodes that do not have a condition for successsion
        if(node->elements().empty() && node->predecessors().size() == 1 
            && node->successors().size() == 1 && !node->get_terminator().has_value()) {
            const auto &pred = node->predecessors().back();
            const auto &succ = node->successors().back();
            // Connect predecessor to successor
            for(const auto &ps : pred->successors()) {
                if(ps.successor == node.get()) {
                    BranchingCondition cond_new = { .cfg_elem = ps.cfg_elem, .condition_state = ps.condition_state, .successor = succ.successor };
                    pred->replace_successor(ps, cond_new);
                }
            }
            succ.successor->replace_predecessor(*node.get(), *pred);

            if(node->attr.func_entry && node->func == succ.successor->func) {
                succ.successor->attr.func_entry = true;
            }
            if(node->attr.func_exit && node->func == pred->func) {
                pred->attr.func_exit = true;
            }
            empty_nodes.push_back(node.get());
        }
    }

    // Remove the selected nodes from the global CG
    while(!empty_nodes.empty()) {
        const auto &node = empty_nodes.back();
        empty_nodes.pop_back();
        auto iter = std::remove_if(
            _nodes.begin(),
            _nodes.end(),
            [&](std::unique_ptr<CGNode> &n) { return n.get() == node; }
        );
        _nodes.erase(iter, _nodes.end());
    }
}


void CommunicationGraph::detect_loops()
{
    const bool DEBUG = DETECT_LOOPS_DEBUG;
    if(DEBUG) {
        llvm::errs() << "===================\n";
        llvm::errs() << "= Detecting loops =\n";
        llvm::errs() << "===================\n";
    }

    auto loops = tarjan(_nodes);

    if(loops.empty()) {
        if(DEBUG) {
            llvm::errs() << "No loops detected\n";
        }
        return;
    }

    int loop_level = 1;
    for(const auto &loop : loops) {
        auto l = analyze_loop(loop, loop_level);
        _loops.push_back(l);
    }

    // handle nested loops
    // TODO Refactor?
    while(true) {
        loop_level++;
        std::vector<std::vector<CGNode*>> nested_loops;
        for(const auto &loop : loops) {
            std::vector<CGNode*> nodes;
            const CGNode* ignore = nullptr;
            for(const auto &n : loop) {
                if(n->attr.loop_entry && n->attr.loop_exit) {
                    ignore = n;
                    continue;
                } else {
                    nodes.push_back(n);
                }
            }
            if(nodes.size() > 1) {
                auto loops2 = tarjan(nodes, ignore);
                for(const auto &l : loops2) {
                    auto ll = analyze_loop(l, loop_level);
                    nested_loops.push_back(l);
                    _loops.push_back(ll);
                }
            }
        }
        if(nested_loops.empty()) {
            break;
        } else {
            loops = nested_loops;
        }
    }

    if(DEBUG) {
        for(const auto &loop : _loops) {
            llvm::errs() << "Following Nodes are a loop:\n";
            for(auto &n : loop.nodes) {
                n->dump();
            }
            llvm::errs() << "\nLoop Entry node:\n";
            loop.header->dump();
            for(auto &n : loop.exits) {
                llvm::errs() << "\nLoop Exit node:\n";
                n->dump();
            }
            llvm::errs() << "---\n";
        }
    }

    // Populate the loop_parent attribute for all nodes
    for(auto &node : _nodes) {
        auto *parent_loop = get_parent_loop(*node);
        if(parent_loop) {
            node->attr.parent_loop = parent_loop;
        }
    }
}

// TODO refactor
Loop CommunicationGraph::analyze_loop(const std::vector<CGNode*> &loop, const int loop_level)
{
    // const bool DEBUG = DETECT_LOOPS_DEBUG;

    CGNode *header = nullptr;
    std::vector<CGNode*> exits;    

    for(auto &n : loop) {
        n->attr.loop = true;
        n->attr.loop_level = loop_level;
    }
    for(auto &n : loop) {
        for(const auto &pred : n->predecessors()) {
            if(std::find(loop.begin(), loop.end(), pred) == loop.end()) {
                n->attr.loop_entry = true;
                header = n;
            }
        }
        for(const auto &succ : n->successors()) {
            if(std::find(loop.begin(), loop.end(), succ.successor) == loop.end()) {
                n->attr.loop_exit = true;
                exits.push_back(n);
            }
        }
    }
    return {header, exits, loop};
}

std::vector<std::vector<CGNode*>> CommunicationGraph::tarjan(std::vector<std::unique_ptr<CGNode>> &nodes)
{
    std::vector<std::vector<CGNode*>> sccs;
    std::deque<CGNode*> stack;
    std::map<const CGNode*, std::pair<long, long>> index_list;
    long index = 0;

    for(const auto &node : nodes) {
        if(index_list.count(node.get()) == 0) {
            tarjan_strongconnect(*node.get(), index, stack, index_list, sccs, nullptr);
        }
    }
    return sccs;
}

std::vector<std::vector<CGNode*>> CommunicationGraph::tarjan(std::vector<CGNode*> &nodes, const CGNode *ignore)
{
    std::vector<std::vector<CGNode*>> sccs;
    std::deque<CGNode*> stack;
    std::map<const CGNode*, std::pair<long, long>> index_list;
    long index = 0;

    for(const auto &node : nodes) {
        if(index_list.count(node) == 0) {
            tarjan_strongconnect(*node, index, stack, index_list, sccs, ignore);
        }
    }
    return sccs;
}

void CommunicationGraph::tarjan_strongconnect(CGNode &node, long &index, std::deque<CGNode*> &stack, std::map<const CGNode*, std::pair<long, long>> &index_list, std::vector<std::vector<CGNode*>> &sccs, const CGNode *ignore)
{
    index_list.insert({&node, {index, index}});
    index++;
    stack.push_back(&node);

    for(const auto &succ : node.successors()) {
        // ignore argument is used to find nested loops and points to the loop header of the outer loop
        if(succ.successor != ignore) {
            if(index_list.count(succ.successor) == 0) {
                tarjan_strongconnect(*succ.successor, index, stack, index_list, sccs, ignore);
                index_list.at(&node).second = std::min(index_list.at(&node).second, index_list.at(succ.successor).second);
            } else if(std::find(stack.begin(), stack.end(), succ.successor) != stack.end()) {
                index_list.at(&node).second = std::min(index_list.at(&node).second, index_list.at(succ.successor).first);
            }
        }
    }

    if(index_list.at(&node).second == index_list.at(&node).first) {
        std::vector<CGNode*> scc;
        CGNode* node2;
        do {
            node2 = stack.back();
            stack.pop_back();
            scc.push_back(node2);

        } while (&node != node2);

        if(scc.size() == 1) {
            for(const auto &succ : scc.back()->successors()) {
                if(succ.successor == scc.back()) {
                    sccs.push_back(scc);
                }
            }
        } else {
            sccs.push_back(scc);
        }
    }
}


void CommunicationGraph::analysis(const int mpi_size) const
{
    const bool DEBUG = ANALYSIS;
    if(DEBUG) {
        llvm::errs() << "====================\n";
        llvm::errs() << "= Running Analysis =\n";
        llvm::errs() << "====================\n";
    }

    // CGI SECTION
    // TODO handle branching control flow inside a CGInstance
    // TODO maybe use the before/after logic from the stuff for the paper with Tim
    auto cg_instances = create_cg_instances(mpi_size);
    if(DEBUG) {
        output_cgi("cgi.gv", cg_instances);

        // TODO WIP cfg-vis
        output_reactflow_cgi(make_output_filename("_cgi.json"), cg_instances);
    }

    // Collect MPI Operations for each CGInstance
    // TODO move into CGInstance
    std::unordered_map<int, std::vector<SPMDCall>> operations;
    for(const auto &cg_instance : cg_instances) {
        for(const auto &node : cg_instance.nodes()) {
            for(const auto &call : node->context().mpi_calls) {
                operations[cg_instance.mpi_rank].push_back(call);
            }
        }
    }
    if(DEBUG) {
        llvm::errs() << "Collecting MPI operations:\n";
        for(const auto &[rank, calls] : operations) {
            llvm::errs() << "MPI Calls for rank " << rank << ":\n";
            for(const auto &call : calls) {
                llvm::errs() << "    " << call.expr->getDirectCallee()->getNameAsString() << "(";
                for(auto arg = call.args.begin(); arg != call.args.end(); arg++) {
                    arg->second.dump();
                    if(arg != std::prev(call.args.end())) {
                        llvm::errs() << ", ";
                    }
                }
                llvm::errs() << ")\n";
            }
        }
    }
    if(operations.empty()) {
        warning_print("No MPI operations found in the program.\nExiting\n");
        return;
    }

    // CPN SECTION
    CPNNet net = CPNBuilder().build(mpi_size, operations);
    CPNMarking marking = CPNMarking(net);
    marking.add_token("Start", CPNToken(TokenKind::InitMarker, 0, {}));

    CPNStateGraph state_graph = CPNStateGraph(net, this->_ast_ctx);
    state_graph.compute(marking);
    state_graph.display_statistics(); 
    if(DEBUG) {
        state_graph.output_sg("sg.gv");
    }

    // TODO WIP analyze terminal states for errors in communication patterns
}


std::vector<CGInstance> CommunicationGraph::create_cg_instances(const int mpi_size) const
{
    const bool DEBUG = CREATE_CG_INSTANCE_DEBUG;
    llvm::TimeTraceScope TimeScope("DataFlowAnalysis");
    std::vector<CGInstance> cg_instances;
    for(int i = 0; i < mpi_size; i++) {
        if(DEBUG) {
            llvm::errs() << "=== Creating CG Instance for rank " << i << " size " << mpi_size << ": ===\n";
        }
        cg_instances.push_back(CGInstance(i, mpi_size, *_root, _spmd_data._ctx, _loops));
    }
    return cg_instances;
}

void CommunicationGraph::output_cgi(const std::string &path, const std::vector<CGInstance> &cg_instances) const
{
    std::error_code ec;
    llvm::raw_fd_stream ofs("cgi.gv", ec);
    llvm::errs() << "Outputting cgi graph to " << path << "\n";
    std::string output = "digraph G {\n";
    for(const auto &cg_instance : cg_instances) {
        output.append(cg_instance.output_cg());
    }
    output.append("}");
    ofs << output;
    ofs.flush();
    ofs.close();
}


std::string CommunicationGraph::create_node_name(const clang::CFGBlock &block, const clang::FunctionDecl &func) 
{
    return func.getNameAsString() + std::to_string(block.getBlockID());
}

const FuncCG* CommunicationGraph::get_fcg(const std::string &func_name) const
{
    auto iter = std::find_if(
        _func_cgs.begin(),
        _func_cgs.end(),
        [&](const FuncCG &fcg) { return ( fcg.func->getNameAsString() == func_name); }
    );
    if(iter != _func_cgs.end()) {
        return &*iter;
    } else {
        return nullptr;
    }
}

void CommunicationGraph::dump() const
{
    llvm::errs() << "Dumping CommunicationGraph of the program:\n";
    for(const auto &node : _nodes) {
        node->dump();
        llvm::errs() << "---\n";
    }
}

void CommunicationGraph::dump(const FuncCG &fcg) const
{
    llvm::errs() << "FuncCG of \'" << fcg.func->getNameAsString() <<  "\':\n";
    for(const auto &node : fcg.nodes) {
        node->dump();
        if(fcg.exit == node) {
            llvm::errs() << "IS EXIT NODE\n";
        }
        llvm::errs() << "---\n";
    }
}

const Loop* CommunicationGraph::get_parent_loop(const CGNode &node)
{
    if(_loop_mappings.count(&node)) {
        return _loop_mappings[&node];
    } else {
        const Loop *parent_loop = nullptr;
        int max_level = 0;
        for(const auto &loop : _loops) {
            for (auto *loop_node : loop.nodes) {
                if(loop_node == &node && loop_node->attr.loop_level > max_level) {
                    parent_loop = &loop;
                    max_level = loop.level;
                }
            }
        }
        _loop_mappings[&node] = parent_loop;
        return parent_loop;
    }
}
