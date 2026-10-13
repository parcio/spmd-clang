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

#include "CPNStateGraph.h"
#include "CPNGuard.h"
#include "debug.h"
#include "utils.h"
#include <llvm/Support/raw_ostream.h>
#include <regex>
#include <string>

#include <llvm/Support/TimeProfiler.h>


CPNStateGraph::CPNStateGraph(const CPNNet &net, const clang::ASTContext &ctx) : _net(net), _ast_ctx(ctx)
{
    for(const auto &transition : _net.transitions()) {
        _viable_transitions.insert(transition.name());
    }
}

void CPNStateGraph::compute(const CPNMarking &initial_marking)
{
    llvm::TimeTraceScope TimeScope("SPMDClang-CPN-Simulation");
    const bool DEBUG = CPNSTATEGRAPH_COMPUTE_DEBUG;
    _states.clear();
    _states.insert(initial_marking);

    int counter = 0;
    std::vector<CPNMarking> states = { initial_marking };
    if(DEBUG) {
        llvm::errs() << "Initial CPN state created: cpn_" << std::to_string(counter) << "\n";
        initial_marking.output_cpn("cpn" + std::to_string(counter) + ".gv", _net);
        counter++;
    }

    while(!states.empty()) {
        auto state = states.back();
        states.pop_back();

        if(DEBUG) {
            llvm::errs() << "--------------------------------------------------------\n";
        }

        auto next_states = compute_marking(state);

        for(const auto &result : next_states) {
            auto [next_state, transition] = result;

            if(_edges.insert({state, next_state}).second) {
                if(DEBUG) {
                    llvm::errs() << "New edge created in StateGraph\n";
                }
            }
            
            // remove state from terminal_states because it produced new states
            _terminal_states.erase(std::remove(_terminal_states.begin(), _terminal_states.end(), state), _terminal_states.end());

            auto s = next_state;
            // figure out if the state is already in the graph
            if(_states.insert(s).second) {
                states.push_back(next_state);
                _terminal_states.push_back(next_state);
                if(DEBUG) {
                    llvm::errs() << "New state created: cpn_" << std::to_string(counter) << "\n";
                    next_state.output_cpn("cpn" + std::to_string(counter) + ".gv", _net);
                    counter++;
                    next_state.dump();
                }
            } 
        }
        if(DEBUG) {
            llvm::errs() << "=======\n";
        }
    }

    // Find out possible dead transitions in the StateGraph
    std::set<std::string> alive_transitions;
    for(const auto &terminal: _terminal_states) {
        for(const auto &[transition, count] : terminal.transitions_fired()) {
            if(count) {
                alive_transitions.insert(transition);
            }
        }
    }
    for(const auto &transition : _net.transitions()) {
        if(!alive_transitions.count(transition.name())) {
            _dead_transitions.insert(transition.name());
        }
    }

    // TODO WIP cpn-vis
    output_reactflow("net_reactflow.json");

}

void CPNStateGraph::display_statistics() const
{
    bool DEBUG = CPNSTATEGRAPH_DISPLAY_STATISTICS_DEBUG;
    llvm::TimeTraceScope TimeScope("detect errors");
    llvm::errs() << "Produced a StateGraph with " << order() << " states and " << size() << " edges\n";
    llvm::errs() << "The CPN has " << _viable_transitions.size() << " Transitions\n";

    if(!_dead_transitions.empty()) {
        llvm::errs() << "The following transitions never fire:\n";
        for(const auto &t : _dead_transitions) {
            llvm::errs() << t << "\n";
        }
    }

    if(terminates()) {
        llvm::errs() << "The CPN terminates (has " << _terminal_states.size() << " terminal states)\n";
        int counter = 0;
        for(const auto &state : _terminal_states) {
            state.dump_ext();
            if(DEBUG) {
                llvm::errs() << "Writing out final state to cpn_terminal_" << std::to_string(counter) << ".gv\n";
                state.output_cpn("cpn_terminal_" + std::to_string(counter) + ".gv", _net);
                counter++;
            }
        }
    } else {
        llvm::errs() << "The CPN does not terminate (has no terminal state)\n";
    }

    detect_errors();
}

// TODO special warning for root argument mismatch in collectives
void CPNStateGraph::detect_errors() const
{
    const bool DEBUG = CPNSTATEGRAPH_DETECT_ERRORS_DEBUG;

    std::vector<CPNMarking> deadlock_states;
    std::vector<CPNMarking> remaining_token_states;

    for(const auto &state : _terminal_states) {
        if(state.marking().at("End").size() == 0 ) {
            deadlock_states.push_back(state);
            continue;
        }
        for(const auto &[place, tokens] : state.marking()) {
            if(place != "End" && tokens.size() > 0) {
                remaining_token_states.push_back(state);
                continue;
            }
        }
    }

    if(!deadlock_states.empty()) {
        for(const auto &state : deadlock_states) {
            if(DEBUG) { 
                llvm::errs() << "Analyzing deadlock state:\n";
                state.dump_ext(); 
            }

            for(const auto &[place, tokens] : state.marking()) {
                if(place == "Start" || place == "End" || place == "Signals") {
                    // TODO are there deadlocks that contain tokens in these places?
                    if(tokens.size() > 0) {
                        error_print("Unexpected type of deadlock. Analysis not supported yet");
                    }
                    continue;
                } else if(place == "MessageBuffer") {
                    if(tokens.size() > 0) {
                        for(const auto &token : tokens) {
                            if(token.kind() == TokenKind::MessageMarker) {
                                // TODO create helper functions in token to get specific args
                                std::string source_place = "P" + std::to_string(token.args()[0]);
                                const auto *transition = _net.find_transition(token.transition_id());

                                // TODO maybe combine the below cases
                                if(state.marking().at(source_place).size() == 0 && transition->guard().value().kind() == CPNGuardKind::Ssend) {
                                    int idx = state.transition_fired_count(transition->name())-1;
                                    auto *call_expr = transition->ast_expr().at(source_place).at(idx);

                                    // TODO make better custom diagnostics for all error types
                                    auto &D = _ast_ctx.getDiagnostics();
                                    const auto diag = D.getCustomDiagID(clang::DiagnosticsEngine::Warning, "SPMDClang detected an error: Deadlock detected. Rank %0 stuck at:");
                                    D.Report(call_expr->getBeginLoc(), diag).AddString(source_place);
                                } else if(state.marking().at(source_place).size() == 0 && transition->guard().value().kind() == CPNGuardKind::Send) {
                                    int idx = state.transition_fired_count(transition->name())-1;
                                    auto *call_expr = transition->ast_expr().at(source_place).at(idx);

                                    // TODO make better custom diagnostics for all error types
                                    auto &D = _ast_ctx.getDiagnostics();
                                    const auto diag = D.getCustomDiagID(clang::DiagnosticsEngine::Warning, "SPMDClang detected an error: Potential Deadlock detected (Depending on Message Size). Rank %0 stuck at:");
                                    D.Report(call_expr->getBeginLoc(), diag).AddString(source_place);
                                } else if(state.marking().at(source_place).size() == 0 && transition->guard().value().kind() == CPNGuardKind::Rsend) {
                                    int idx = state.transition_fired_count(transition->name())-1;
                                    auto *call_expr = transition->ast_expr().at(source_place).at(idx);

                                    // TODO make better custom diagnostics for all error types
                                    auto &D = _ast_ctx.getDiagnostics();
                                    const auto diag = D.getCustomDiagID(clang::DiagnosticsEngine::Warning, "SPMDClang detected an error: Potential Deadlock detected (Depending on Message Size). Rank %0 stuck at:");
                                    D.Report(call_expr->getBeginLoc(), diag).AddString(source_place);

                                }
                            }
                        }
                    }
                } else {
                    // Case where token is stuck in a Process place
                    if(tokens.size() > 0) {
                        for(const auto &transition : _net.transitions()) {
                            // TODO fix this case
                            if(transition.name() == "MPI_Init") {
                                continue;
                            }
                            // TODO BUG apparently clang only show the source same source code once if there are multiple diagnostics that have the same source location
                            // figure out how to make this look nicer
                            // maybe collect all diagnostics and only call Report in the end?
                            if(transition.place_can_fire(state, _net, place)) {
                                int idx = state.transition_fired_count(transition.name());
                                auto *call_expr = transition.ast_expr().at(place).at(idx);
                                // TODO make better custom diagnostics for all error types
                                auto &D = _ast_ctx.getDiagnostics();
                                const auto diag = D.getCustomDiagID(clang::DiagnosticsEngine::Warning, "SPMDClang detected an error: Deadlock detected. Rank %0 stuck at:");
                                D.Report(call_expr->getBeginLoc(), diag).AddString(place);
                            }
                        }
                    }
                }
            }
        }
    } else if(!remaining_token_states.empty()) {
        for(const auto &state : remaining_token_states) {
            if(DEBUG) {
                llvm::errs() << "Analyzing remaining token state:\n";
                state.dump_ext();
            }

            for(const auto &[place, tokens] : state.marking()) {
                // if(place == "Start" || place == "End" || place == "Signals") {
                if(place == "Start") {
                    // TODO are there deadlocks that contain tokens in these places?
                    if(tokens.size() > 0) {
                        error_print("Unexpected type of remaining tokens. Analysis not supported yet");
                    }
                } else if(place == "MessageBuffer") {
                    if(tokens.size() > 0) {
                        for(const auto &token : tokens) {
                            if(token.kind() == TokenKind::MessageMarker) {
                                // TODO create helper functions in token to get specific args
                                std::string source_place = "P" + std::to_string(token.args()[0]);
                                const auto *transition = _net.find_transition(token.transition_id());
                                // TODO somehow add info to sendmarker from which transition_fired_count it was produced so that we can pinpoint the exact send operation in source code
                                for(const auto *call_expr : transition->ast_expr().at(source_place)) {
                                    // TODO make better custom diagnostics for all error types
                                    auto &D = _ast_ctx.getDiagnostics();
                                    const auto diag = D.getCustomDiagID(clang::DiagnosticsEngine::Warning, "SPMDClang detected an error: Unreceived message detected. Message from rank %0 might never be received");
                                    D.Report(call_expr->getBeginLoc(), diag).AddString(source_place);
                                }
                            }
                        }
                    }
                } else if(place == "Signals") {
                    // Check for missing MPI_Wait calls for started collective operations
                    if(tokens.size() > 0) {
                        for(const auto &token : tokens) {
                            if(token.kind() == TokenKind::RequestMarker) {
                                bool found = false;
                                for(const auto &transition : _net.transitions()) {
                                    // Find the mpi operation that started this request
                                    if(transition.request_id() == token.args().front()) {
                                        // TODO create helper functions in token to get specific args
                                        found = true;
                                        std::string source_place = "";
                                        for(const auto &arc : transition.in_arcs()) {
                                            if(arc.place().starts_with("P")) {
                                                source_place = arc.place();
                                                break;
                                            }
                                        }
                                        // TODO somehow add info to sendmarker from which transition_fired_count it was produced so that we can pinpoint the exact send operation in source code
                                        for(const auto *call_expr : transition.ast_expr().at(source_place)) {
                                            // TODO make better custom diagnostics for all error types
                                            auto &D = _ast_ctx.getDiagnostics();
                                            const auto diag = D.getCustomDiagID(clang::DiagnosticsEngine::Warning, "SPMDClang detected an error: Missing MPI_Wait detected. Rank %0 never makes sure to complete this non-blocking operation");
                                            D.Report(call_expr->getBeginLoc(), diag).AddString(source_place);
                                        }
                                    }
                                }
                                if(!found) {
                                    error_print("Could not find the non-blocking operation beloning to a remaining RequestMarker in the Signals place. can't produce correct warning about missing MPI_Wait" );
                                }
                            } else {
                                error_print("Unexpected type of remaining tokens. Analysis not supported yet");
                            }
                        }
                    }
                } else {
                    if(place != "End" && tokens.size() > 0) {
                        error_print("Remaining token in a Process place while analyzing remaining token state");
                    }
                }
            }
        }
    } else {
        llvm::errs() << "No errors detected in communication pattern\n";
    }

    // llvm::errs() << "+++++++++++++++++\n";
    // for(const auto &transition : _net.transitions()) {
    //     llvm::errs() << "Transition " << transition.name() << "\n";
    //     for(const auto &[key, value] : transition.ast_expr()) {
    //         llvm::errs() << "    " << key << ":\n";
    //         value.front()->dump();
    //     }
    //     llvm::errs() << "---\n";
    // }
}

void CPNStateGraph::output_sg(const std::string &path) const
{
    llvm::TimeTraceScope TimeScope("print SG");
    // states need to be identifiable
    std::map<CPNMarking, int> states;
    int counter = 0;
    for(const auto &state : _states) {
        states.insert({state, counter});
        counter++;
    }


    std::error_code ec;
    llvm::raw_fd_stream ofs(path, ec);

    ofs << "digraph G {\n";

    for(const auto &[state, id] : states) {
        ofs << "    " << "\"" << id << "\"" << "[label=<";
        auto content = std::regex_replace(state.to_string(), std::regex("\n"), "<BR/>");
        content = std::regex_replace(content, std::regex("Marking:"), "<B>Marking:</B>");
        content = std::regex_replace(content, std::regex("Transitions fired:"), "<B>Transitions fired:</B>");
        ofs << content;
        ofs << ">, shape=box]\n";
    }

    for(const auto &[s1, s2] : _edges) {
        ofs << "    " << "\"" << states.at(s1) << "\"" << "->" << "\"" << states.at(s2) << "\"\n";
    }


    ofs << "}";
    ofs.flush();
    ofs.close();
}

std::set<std::pair<CPNMarking, std::string>> CPNStateGraph::compute_marking(const CPNMarking &marking)
{
    const bool DEBUG = CPNSTATEGRAPH_COMPUTE_MARKING_DEBUG;
    // Fire each possible transition and return a new marking for every one
    std::set<std::pair<CPNMarking, std::string>> next_states;
    for(auto &transition : _net.transitions()) {
        if(transition.could_fire(marking)) {
            if(DEBUG) {
                llvm::errs() << transition.name() << "could fire\n";
            }
            auto result = transition.fire(marking, _net);

            if(result.has_value()) {
                next_states.insert( {result.value(), transition.name()} );
            }
        }
        if(DEBUG) {
            llvm::errs() << "===\n";
        }
    }
    return next_states;
}


// TODO WIP cpn-vis
void CPNStateGraph::output_reactflow(const std::string &path) const
{
    std::error_code ec;
    llvm::raw_fd_stream ofs(path, ec);

    if(ec) {
        llvm::errs() << "Error opening file: " << ec.message() << "\n";
        return;
    }

    ofs << "{\n";
    ofs << "  \"nodes\": [\n";

    int place_x = 100;
    int place_y = 100;
    int transition_x = 350;
    int transition_y = 150;
    int vertical_spacing = 120;
    int horizontal_spacing = 250;


    const auto& places = _net.places();
    int place_counter = 0;
    for(const auto &place : places) {
        std::string place_id = place.name();

        int row = place_counter / 2;
        int col = place_counter % 2;
        int x = place_x + col * horizontal_spacing;
        int y = place_y + row * vertical_spacing;


        std::string html_label = "<B>" + escape_json_string(place_id) + "</B><BR/>";

        ofs << "    {\n";
        ofs << "      \"id\": \"" << place_id << "\",\n";
        ofs << "      \"type\": \"petriPlace\",\n";
        ofs << "      \"position\": { \"x\": " << x << ", \"y\": " << y << " },\n";
        ofs << "      \"data\": {\n";
        ofs << "        \"type\": \"place\",\n";
        ofs << "        \"id\": \"" << place_id << "\",\n";
        ofs << "        \"label\": \"" << escape_json_string(place_id) << "\",\n";
        ofs << "        \"htmlLabel\": \"" << escape_json_string(html_label) << "\"\n";
        ofs << "      }\n";
        ofs << "    },";
        ofs << "\n";

        place_counter++;
    }

    const auto& transitions = _net.transitions();
    int transition_counter = 0;
    for(const auto &transition : transitions) {
        std::string transition_id = transition.name();

        int row = transition_counter / 2;
        int col = transition_counter % 2;
        int x = transition_x + col * horizontal_spacing;
        int y = transition_y + row * vertical_spacing;

        std::string guard_html = "";
        if(transition.guard()) {
            auto guard_string = transition.guard().value().to_string();
            guard_html = escape_json_string(guard_string);
        }

        std::string html_label = "<B>" + escape_json_string(transition_id) + "</B><BR/>";
        if(!guard_html.empty()) {
            html_label += guard_html + "<BR/>";
        }

        ofs << "    {\n";
        ofs << "      \"id\": \"" << transition_id << "\",\n";
        ofs << "      \"type\": \"petriTransition\",\n";
        ofs << "      \"position\": { \"x\": " << x << ", \"y\": " << y << " },\n";
        ofs << "      \"data\": {\n";
        ofs << "        \"type\": \"transition\",\n";
        ofs << "        \"id\": \"" << transition_id << "\",\n";
        ofs << "        \"label\": \"" << escape_json_string(transition_id) << "\",\n";
        ofs << "        \"htmlLabel\": \"" << escape_json_string(html_label) << "\"\n";
        ofs << "      }\n";
        ofs << "    }";

        if(transition_counter < static_cast<int>(transitions.size()) - 1) {
            ofs << ",";
        }
        ofs << "\n";

        transition_counter++;
    }

    ofs << "  ],\n";
    ofs << "  \"edges\": [\n";

    std::vector<std::string> edge_list;

    for(const auto &transition : transitions) {
        for(const auto &in_arc : transition.in_arcs()) {
            std::string source = in_arc.place();
            std::string target = transition.name();
            std::string edge_id = source + "-" + target;
            std::string label = in_arc.binding();

            std::string edge_json = "    {\n";
            edge_json += "      \"id\": \"" + edge_id + "\",\n";
            edge_json += "      \"source\": \"" + source + "\",\n";
            edge_json += "      \"target\": \"" + target + "\",\n";
            edge_json += "      \"label\": \"" + escape_json_string(label) + "\"\n";
            edge_json += "    }";

            edge_list.push_back(edge_json);
        }

        for(const auto &out_arc : transition.out_arcs()) {
            std::string source = transition.name();
            std::string target = out_arc.place();
            std::string edge_id = source + "-" + target;
            std::string label = out_arc.expression().to_string();

            std::string edge_json = "    {\n";
            edge_json += "      \"id\": \"" + edge_id + "\",\n";
            edge_json += "      \"source\": \"" + source + "\",\n";
            edge_json += "      \"target\": \"" + target + "\",\n";
            edge_json += "      \"label\": \"" + escape_json_string(label) + "\"\n";
            edge_json += "    }";

            edge_list.push_back(edge_json);
        }
    }

    for(size_t i = 0; i < edge_list.size(); ++i) {
        ofs << edge_list[i];
        if(i != edge_list.size() - 1) {
            ofs << ",";
        }
        ofs << "\n";
    }

    ofs << "  ]\n";
    ofs << "}\n";

    ofs.flush();
    ofs.close();
}

// TODO WIP cpn-vis
void CPNStateGraph::output_reactflow_sg(const std::string &path) const
{
    std::map<CPNMarking, int> state_ids;
    int id_counter = 0;

    for(const auto &state : _states) {
        state_ids.insert({state, id_counter});
        id_counter++;
    }

    std::error_code ec;
    llvm::raw_fd_stream ofs(path, ec);

    if(ec) {
        llvm::errs() << "Error opening file: " << ec.message() << "\n";
        return;
    }

    ofs << "{\n";
    ofs << "  \"states\": [\n";

    int state_index = 0;
    for(const auto &[state, id] : state_ids) {
        ofs << "    {\n";
        ofs << "      \"id\": \"" << id << "\",\n";
        ofs << "      \"marking\": {\n";

        const auto& marking = state.marking();


        std::vector<std::string> place_order = {
            "End", "MessageBuffer", "P0", "P1", "Signals", "Start"
        };

        for(size_t i = 0; i < place_order.size(); ++i) {
            const std::string& place = place_order[i];
            ofs << "        \"" << place << "\": [";

            auto marking_it = marking.find(place);
            if(marking_it != marking.end()) {
                const auto& tokens = marking_it->second;
                int token_count = 0;
                for(const auto &token : tokens) {
                    if(token_count > 0) {
                        ofs << ", ";
                    }
                    ofs << "\"" << escape_json_string(token.to_string()) << "\"";
                    token_count++;
                }
            }

            ofs << "]";
            if(i < place_order.size() - 1) {
                ofs << ",";
            }
            ofs << "\n";
        }

        ofs << "      },\n";
        ofs << "      \"firedTransitions\": [\n";


        const auto& fired = state.transitions_fired();
        std::vector<std::string> fired_list;

        for(const auto &[transition, count] : fired) {
            if(count > 0) {
                fired_list.push_back(transition);
            }
        }


        std::sort(fired_list.begin(), fired_list.end());

        for(size_t i = 0; i < fired_list.size(); ++i) {
            ofs << "        \"" << escape_json_string(fired_list[i]) << "\"";
            if(i < fired_list.size() - 1) {
                ofs << ",";
            }
            ofs << "\n";
        }

        ofs << "      ]\n";
        ofs << "    }";

        if(state_index < static_cast<int>(state_ids.size()) - 1) {
            ofs << ",";
        }
        ofs << "\n";
        state_index++;
    }

    ofs << "  ],\n";
    ofs << "  \"edges\": [\n";


    std::map<CPNMarking, std::vector<std::pair<CPNMarking, std::string>>> marking_cache;


    for(const auto &[source_state, target_state] : _edges) {
        if(marking_cache.find(source_state) == marking_cache.end()) {
            CPNStateGraph* non_const_this = const_cast<CPNStateGraph*>(this);
            auto next_states = non_const_this->compute_marking(source_state);

            std::vector<std::pair<CPNMarking, std::string>> next_states_vec;
            for(const auto &item : next_states) {
                next_states_vec.push_back(item);
            }
            marking_cache[source_state] = next_states_vec;
        }
    }

    int edge_index = 0;
    for(const auto &[source_state, target_state] : _edges) {
        int source_id = state_ids.at(source_state);
        int target_id = state_ids.at(target_state);
        std::string edge_id = std::to_string(source_id) + "-" + std::to_string(target_id);


        std::string transition_label = "";

        auto cache_it = marking_cache.find(source_state);
        if(cache_it != marking_cache.end()) {
            for(const auto &[next_state, transition] : cache_it->second) {
                if(next_state == target_state) {
                    transition_label = transition;
                    break;
                }
            }
        }


        if(transition_label.empty()) {
            CPNStateGraph* non_const_this = const_cast<CPNStateGraph*>(this);
            auto next_states = non_const_this->compute_marking(source_state);
            for(const auto &[next_state, transition] : next_states) {
                if(next_state == target_state) {
                    transition_label = transition;
                    break;
                }
            }
        }

        ofs << "    {\n";
        ofs << "      \"id\": \"" << edge_id << "\",\n";
        ofs << "      \"source\": \"" << source_id << "\",\n";
        ofs << "      \"target\": \"" << target_id << "\",\n";
        ofs << "      \"label\": \"" << escape_json_string(transition_label) << "\"\n";
        ofs << "    }";

        if(edge_index < static_cast<int>(_edges.size()) - 1) {
            ofs << ",";
        }
        ofs << "\n";
        edge_index++;
    }

    ofs << "  ]\n";
    ofs << "}\n";
    ofs.flush();
    ofs.close();
}


// TODO WIP cpn-vis
std::string CPNStateGraph::escape_json_string(const std::string& str) const
{
    std::string result;
    result.reserve(str.length());

    for(char c : str) {
        switch(c) {
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '/':  result += "\\/"; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:   result += c; break;
        }
    }

    return result;
}
