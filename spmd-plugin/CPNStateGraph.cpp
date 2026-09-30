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
