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

#ifndef CPNSTATEGRAPH_H
#define CPNSTATEGRAPH_H


#include "CPNMarking.h"
#include "CPNNet.h"
#include <clang/AST/ASTContext.h>

class CPNStateGraph {
public:
    CPNStateGraph(const CPNNet &net, const clang::ASTContext &ctx);
    CPNStateGraph(CPNStateGraph &&) = default;
    CPNStateGraph(const CPNStateGraph &) = default;
    CPNStateGraph &operator=(CPNStateGraph &&) = delete;
    CPNStateGraph &operator=(const CPNStateGraph &) = delete;
    ~CPNStateGraph() = default;

    const int order() const { return _states.size(); }
    const int size() const { return _edges.size(); }
    const std::vector<CPNMarking> &terminal_states() const { return _terminal_states; }

    const bool terminates() const { return _terminal_states.size() > 0; }

    void compute(const CPNMarking &initial_marking);

    void display_statistics() const;

    void detect_errors() const;

    void output_sg(const std::string &path) const;


    // TODO WIP cpn-vis
    void output_reactflow(const std::string &path) const;
    void output_reactflow_sg(const std::string &path) const;



private:
    std::set<std::pair<CPNMarking, std::string>> compute_marking(const CPNMarking &marking);

    const CPNNet &_net;
    const clang::ASTContext &_ast_ctx;
    // TODO implement the hash based version for markings so comparisons are faster
    std::set<CPNMarking> _states;
    std::set<std::pair<CPNMarking, CPNMarking>> _edges;
    std::vector<CPNMarking> _terminal_states;
    // TODO make this something other than strings?
    std::set<std::string> _dead_transitions;
    std::set<std::string> _viable_transitions;


    // TODO WIP cpn-vis
    std::string escape_json_string(const std::string& str) const;
    const CPNMarking& find_state_by_id(const std::map<CPNMarking, int>& state_ids, int id) const;
    std::vector<std::string> split_string(const std::string& str, char delimiter) const;
};

#endif
