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

#include "CPNGuard.h"
#include "CPNToken.h"
#include <llvm/Support/raw_ostream.h>

std::string CPNGuard::create_string() const
{
    std::string result = "";

    for(const auto &[key, value] : _expression) {
        result += "type(" + key + ")" + " == "  + value.token().value().kind_to_string() + " and " 
            + key + ".matches" + value.token().value().arguments_to_string() + " and ";
    }
    // Remove last " and "
    if(!result.empty()) {
        result.pop_back();
        result.pop_back();
        result.pop_back();
        result.pop_back();
        result.pop_back();
    }

    return result;
}

// TODO clean this up.  Got a bit messy with the any_source/tag stuff
bool CPNGuard::evaluate(std::map<std::string, std::pair<CPNArcIn, CPNToken>> tokens) const
{
    std::map<std::string, bool> results;
    for(const auto &[guard_binding, guard_expression] : _expression) {
        results[guard_binding] = true;
        if(tokens.count(guard_binding)) {
            auto &[arc, token] = tokens.at(guard_binding);
            if(guard_expression.token().value() == token) {
                continue;
            } else {
                results[guard_binding] = false;
            }
            // Special case for receives with ANY_SOURCE or ANY_TAG
            // TODO make this nicer (and faster)
            if((this->kind() == CPNGuardKind::Recv || this->kind() == CPNGuardKind::Irecv 
                        || this->kind() == CPNGuardKind::Complete) && guard_binding == "s" ) {
                results[guard_binding] = true;
                // MPI_ANY_SOURCE (value of -2); MPI_ANY_TAG (value of -1)
                bool any_source = (guard_expression.token().value().args().at(0) == -2);
                bool any_tag = (guard_expression.token().value().args().at(2) == -1);
                if(any_source || any_tag) {
                    // In this case just ignore the matching of source/tag arguments in a wildcard was used
                    for(int i = 0; i < guard_expression.token().value().args().size(); i++) {
                        if(any_source && i == 0) {
                            continue;
                        } else if(any_tag && i == 2) {
                            continue;
                        } else {
                            if(guard_expression.token().value().args().at(i) != token.args().at(i)) {
                                results[guard_binding] = false;
                                break;
                            }
                        }
                    }
                } else {
                    if(guard_expression.token().value() != token) {
                        results[guard_binding] = false;
                    }
                }
            }
        } else {
            results[guard_binding] = false;
        }
    }

    for(const auto &[key, value] : results) {
        if(!value) {
            return false;
        }
    }
    return true;
}

bool CPNGuard::evaluate_partial(std::string binding, std::pair<CPNArcIn, CPNToken> token) const
{
    for(const auto &[guard_binding, guard_expression] : _expression) {
        if(guard_binding == binding) {
            if(guard_expression.token().value() == token.second) {
                return true;
            } else {
                return false;
            }
        }
    }
    return false;
}
