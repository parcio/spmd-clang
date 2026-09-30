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

#include "CPNTransition.h"
#include <clang-c/Index.h>
#include <llvm/Support/raw_ostream.h>
#include <optional>
#include <utility>
#include "CPNArc.h"
#include "CPNMarking.h"
#include "CPNNet.h"
#include "debug.h"
#include "utils.h"

CPNTransition::CPNTransition(std::string name, CPNGuard guard, std::string place, const clang::CallExpr *ast_expr, std::int64_t request_id) : _name(name + std::to_string(_ID_COUNTER)), _request_id(request_id), _guard(guard)
{ 
    _id = _ID_COUNTER++;
    _ast_expr[place].push_back(ast_expr);
}


void CPNTransition::add_in_arc(CPNArcIn arc)
{
    if(std::find_if(
        _in_arcs.begin(),
        _in_arcs.end(),
        [&](const CPNArcIn &a) { return a.binding() == arc.binding(); }
    ) != _in_arcs.end()) {
        llvm::errs() << "Incoming arc with same binding already exists\n";
        return;
    }

    // if(std::find_if(
    //     _in_arcs.begin(),
    //     _in_arcs.end(),
    //     [&](const CPNArcIn &a) { return a.place() == arc.place(); }
    // ) != _in_arcs.end()) {
    //     llvm::errs() << "There already exist an incoming arc from the same place\n";
    //     return;
    // }

    _in_arcs.push_back(arc);
}

void CPNTransition::add_out_arc(CPNArcOut arc)
{
    // TODO Check if removing this check caused problems somewhere
    // multiple out_arcs to the same place make some things easier in modeling MPI operations
    // if(std::find_if(
    //     _out_arcs.begin(),
    //     _out_arcs.end(),
    //     [&](const CPNArc &a) { return a.place() == arc.place(); }
    // ) != _out_arcs.end()) {
    //     llvm::errs() << "There already exists an outgoing arc to the same place\n";
    //     return;
    // }
    _out_arcs.push_back(arc);
}

// check if the transition can fire, but **does not** check the transition guard!
bool CPNTransition::could_fire(const CPNMarking &marking) const
{
    if(_in_arcs.empty() && _out_arcs.empty()) {
        return false;
    }

    for(const auto &arc : _in_arcs) {
        if(marking.get(arc.place()).empty()) {
            return false;
        }
    }

    return true;
}

inline std::vector<std::vector<std::pair<CPNArcIn, CPNToken>>> multi_cartesian_product(std::vector<std::set<std::pair<CPNArcIn, CPNToken>>> &sets)
{
    using List = std::vector<std::vector<std::pair<CPNArcIn, CPNToken>>>;
    List result = {{}};
    for(const auto &set : sets) {
        List new_result;
        for(const auto &prefix : result) {
            for(auto t : set) {
                auto extend = prefix;
                extend.push_back(t);
                new_result.push_back(extend);
            }
        }
        result = new_result;
    }
    return result;
}

bool CPNTransition::can_fire(const CPNMarking &marking, const CPNNet &net) const
{
    return !create_valid_combinations(marking, net).empty();
}

bool CPNTransition::place_can_fire(const CPNMarking &marking, const CPNNet &net, const std::string &place) const
{
    std::set<std::pair<CPNArcIn, CPNToken>> token_candidates; 
    for(const auto &arc : _in_arcs) {
        if(arc.place() == place) {
            for(const auto &token : marking.get(arc.place())) {
                token_candidates.insert({arc, token});
            }
        }
    }

    for(const auto &token : token_candidates) {
        if(_guard.value().to_string().empty()) {
            return true;
        } else {
            if(_guard.value().evaluate_partial(token.first.binding(), token)) {
                return true;
            }
        }
    }
    return false;
}

std::optional<CPNMarking> CPNTransition::fire(const CPNMarking &marking, const CPNNet &net) const
{
    const bool DEBUG = CPNTRANSITION_FIRE_DEBUG;

    auto new_marking = marking;
    auto valid_combinations = create_valid_combinations(marking, net);

    // TODO how to deal with this case?
    if(valid_combinations.empty()) {
        if(DEBUG) {
            llvm::errs() << "No valid token combinations found to fire transition" << _name << "\n";
        }
        return std::nullopt;
    } else if(valid_combinations.size() > 1) {
        // TODO investigate this case
        warning_print("Multiple valid token combinations found to fire transition" + _name + "\n");
        if(DEBUG) {
            llvm::errs() << "Currently only choosing the first one\n";
            for(const auto &combination : valid_combinations) {
                llvm::errs() << "Valid combination:\n";
                for(const auto &[key, value] : combination.first) {
                    auto [arc, token] = value;
                    llvm::errs() << "    " << key << ": " << token.to_string() << "\n";
                }
                llvm::errs() << "---\n";
            }
        }
    }

    auto valid = *valid_combinations.begin();
    for(const auto &[key, value] : valid.first) {
        auto [arc, token] = value;
        if(DEBUG) {
            llvm::errs() << "Removing token from " << arc.place() << "\n";
            llvm::errs() << "    " << token.to_string() << "\n";
        }
        new_marking.remove_token(arc.place(), token);
    }

    for(auto &arc : _out_arcs) {
        auto [place, token] = arc.generate_token(valid.second, new_marking);
        // Will not generate empty tokens (see Blocking send behaviour)
        if(token.kind() != TokenKind::EmptyMarker) {
            new_marking.add_token(place, token);
            if(DEBUG) {
                llvm::errs() << "Created Token: " << token.to_string() << "\n";
            }
        }
    }

    new_marking.transition_fired(_name);

    // TODO WIP special check for Rsend to figure out if a matching Recv was already posted
    // TODO maybe have this check instead be based on the generated tokens?
    // This does not work correctly at the moment because of collective operations (including MPI_Init)
    // They generate the next MPI operations for every process at the same time, therefore it seems like 
    // standard MPI_Recv are already posted even in cases where it is not clear
    if(this->guard().has_value() && this->guard().value().kind() == CPNGuardKind::Rsend) {
        if(DEBUG) {
            llvm::errs() << "Rsend transition is fired here\n";
        }
        // get the destination rank for this Rsend
        auto dest = valid.first.at("s").second.args().at(2);
        // check if the destination rank currently has a Recv pending
        bool recv_found = false;
        for(const auto &tokens : marking.get("P" + std::to_string(dest))) {
            if(tokens.kind() == TokenKind::RecvMarker) {
                recv_found = true;
            }
        }

        if(DEBUG) {
            if(recv_found) {
                llvm::errs() << "@@@@@@ There is a Recv pending for this Rsend\n";
            } else {
                llvm::errs() << "@@@@@@ There is NO Recv pending for this Rsend\n";
            }
        }
    }

    return new_marking;
}

std::set<std::pair<std::map<std::string, std::pair<CPNArcIn, CPNToken>>, CPNEvaluationArgs>> CPNTransition::create_valid_combinations(const CPNMarking &marking, const CPNNet &net) const
{
    const bool DEBUG = CPNTRANSITION_VALID_COMBINATIONS_DEBUG;

    std::vector<std::set<std::pair<CPNArcIn, CPNToken>>> token_candidates;
    if(DEBUG) {
        llvm::errs() << "checking if transition" << _name << " can fire\n";
    }
    for(const auto &arc : _in_arcs) {
        std::set<std::pair<CPNArcIn, CPNToken>> tokens;
        if(DEBUG) {
            llvm::errs() << "Tokens from place " << arc.place() << " are:\n";
        }
        for(const auto &token : marking.get(arc.place())) {
            tokens.insert({arc, token});
            if(DEBUG) {
                llvm::errs() << arc.binding() << ": " << token.to_string() << "\n";
            }
        }
        token_candidates.push_back(tokens);
    }

    auto cartesian = multi_cartesian_product(token_candidates);
    std::vector<std::map<std::string, std::pair<CPNArcIn, CPNToken>>> token_combinations;
    for(const auto &combination : cartesian) {
        std::map<std::string, std::pair<CPNArcIn, CPNToken>> comb;
        if(DEBUG) {
            llvm::errs() << "Combination:\n";
        }
        for(const auto &token : combination) {
            comb.insert({token.first.binding(), token});
            if(DEBUG) {
                llvm::errs() << token.first.binding() << ": " << token.second.to_string() << "\n";
            }
        }
        token_combinations.push_back(comb);
        if(DEBUG) {
            llvm::errs() << "---\n";
        }
    }


    std::set<std::pair<std::map<std::string, std::pair<CPNArcIn, CPNToken>>, CPNEvaluationArgs>> valid_combinations;
    for(const auto &token_combination : token_combinations) {
        CPNEvaluationArgs evaluation_args = { CPNEvaluationArgType::None, std::nullopt, std::nullopt, std::nullopt };
        // TODO incorporate this into the guard evaluation
        if(_guard.value().to_string().empty()) {
            if(DEBUG) {
                llvm::errs() << "Token combination is valid to fire transition (empty guard expression)" << _name << "\n";
            }
            for(const auto &[key, value] : token_combination) {
                auto [arc, token] = value;
                valid_combinations.insert({token_combination, evaluation_args});
                if(DEBUG) {
                    llvm::errs() << "    " << key << ": " << token.to_string() << "\n";
                }
            }
        } else {
            if(_guard.value().evaluate(token_combination)) {
                if(DEBUG) {
                    llvm::errs() << "Token combination is valid to fire transition" << _name << "\n";
                    for(const auto &[key, value] : token_combination) {
                        auto [arc, token] = value;
                        llvm::errs() << "    " << key << ": " << token.to_string() << "\n";
                    }
                }
                // Special case for Recv transitions 
                if(_guard.value().kind() == CPNGuardKind::Recv || _guard.value().kind() == CPNGuardKind::Complete) {
                    auto &[arc, token] = token_combination.at("s");
                    if(DEBUG) {
                        llvm::errs() << "Transition ID of MessageMarker:" << token.transition_id() << "\n";
                    }
                    if(auto send_transition = net.find_transition(token.transition_id())) {
                        // Find out if the producing transition of the MessageMarker was a non-local send 
                        // TODO HACK
                        if(send_transition->name() == "MPI_Ssend" + std::to_string(token.transition_id())
                                || send_transition->name() == "MPI_Send" + std::to_string(token.transition_id())
                                || send_transition->name() == "MPI_Rsend" + std::to_string(token.transition_id())) {
                            // TODO WIP FIXME make this below nicer...
                            bool any_source = _guard.value().expression().at("s").token().value().args().at(0) == -2;
                            bool any_tag = _guard.value().expression().at("s").token().value().args().at(2) == -1;
                            evaluation_args = { CPNEvaluationArgType::CreateContinue, std::nullopt, std::nullopt, std::nullopt };
                            if(any_source) {
                                evaluation_args.source = token.args().at(0);
                            }
                            if(any_tag) {
                                evaluation_args.tag = token.args().at(2);
                            }
                            if(DEBUG) {
                                llvm::errs() << "Send transition was a blocking send\n";
                            }
                        } else if(send_transition->name() == "MPI_Isend" + std::to_string(token.transition_id())) {
                            // Find out if the producing transition of the MessageMarker was a non-blocking send
                            evaluation_args = { CPNEvaluationArgType::CreateRequest, token.request_id(), std::nullopt, std::nullopt };
                            if(DEBUG) {
                                llvm::errs() << "Send transition was a non-blocking send\n";
                            }
                        }
                    }

                }
                valid_combinations.insert({token_combination, evaluation_args});
                // } else if (_guard.value().kind() == CPNGuardKind::Barrier) {
                //     evaluation_args = CPNEvaluationArgType::QueueExpression;
                // }

                // for(const auto &out_arc : _out_arcs) {
                //     if(out_arc.expression().kind() == ExpressionKind::Queue) {
                //         evaluation_args = evaluation_args | CPNEvaluationArgType::QueueExpression;
                //     }
                // }
            }
        }
    }

    return valid_combinations;
}
