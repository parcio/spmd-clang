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

#include "CPNArc.h"
#include "utils.h"
#include "CPNMarking.h"
#include <llvm/Support/raw_ostream.h>

bool operator<(const CPNEvaluationArgs &lhs, const CPNEvaluationArgs &rhs)
{
    return lhs.type < rhs.type;
}

std::pair<std::string, CPNToken> CPNArcOut::generate_token(CPNEvaluationArgs args, CPNMarking &marking) const
{
    switch(_expression.kind()) {
        case ExpressionKind::Simple: {
            return {this->place(), _expression.token().value()};
            break;
        }
        case ExpressionKind::Or: {
            if((args.type & CPNEvaluationArgType::CreateContinue)) {
                auto tokens = _expression.or_tokens().value();
                for(const auto &token : tokens) {
                    if(token.kind() == TokenKind::ContinueMarker) {
                        if(!args.source.has_value() && !args.tag.has_value()) {
                            // This the regular case
                            return {this->place(), token };
                        } else {
                            // TODO WIP
                            // Special case for dealing with MPI_ANY_SOURCE and MPI_ANY_TAG
                            std::vector<int64_t> continue_args;
                            if(args.source.has_value()) {
                                continue_args.push_back(args.source.value());
                            } else {
                                continue_args.push_back(token.args().at(0));
                            }
                            continue_args.push_back(token.args().at(1));
                            if(args.tag.has_value()) {
                                continue_args.push_back(args.tag.value());
                            } else {
                                continue_args.push_back(token.args().at(2));
                            }
                            continue_args.push_back(token.args().at(3));
                            auto new_token = CPNToken(TokenKind::ContinueMarker, -1, continue_args);
                            return {this->place(), new_token };
                        }
                    }
                }
                error_print("No ContinueMarker found in this ArcOut expression\n");
            } else if(args.type & CPNEvaluationArgType::None) {
                auto tokens = _expression.or_tokens().value();
                for(const auto &token : tokens) {
                    if(token.kind() == TokenKind::EmptyMarker) {
                        return {this->place(), token };
                    }
                }
                error_print("No EmptyMarker found in this ArcOut expression\n");
            } else if(args.type & CPNEvaluationArgType::CreateRequest) {
                auto tokens = _expression.or_tokens().value();
                for(const auto &token : tokens) {
                    if(token.kind() == TokenKind::RequestMarker) {
                        // Need to fill in the correct request id
                        // this can only be deduced at runtime from the MessageMarker that was consumed by a receive
                        auto new_token = CPNToken(TokenKind::RequestMarker, -1, { args.request_id.value() });
                        return {this->place(), new_token };
                    }
                }
                error_print("No RequestMarker found in this ArcOut expression\n");
            }

            break;
        }
        case ExpressionKind::Queue: {
            auto token = _expression.get_queue_token(marking.transition_fired_count(this->transition()));
            if(!token.has_value()) {
                error_print("Queue is empty in ArcOut expression\n");
            } else {
                return {this->place(), token.value()};
            }
            break;
        }
        default:
            llvm::errs() << "Invalid ExpressionKind in CPNArcOut::generate_token\n";
            break;
    }

    error_print("Unhandled evaluation args. Returning empty token\n");
    return {this->place(), CPNToken(TokenKind::EmptyMarker, -1, {})};

    // if(_expression.kind() == ExpressionKind::Simple) {
    // } else {
    //     switch(args) {
    //         case CPNEvaluationArgs::CreateContinue: {
    //             auto tokens = _expression.tokens().value();
    //             if(tokens.first.kind() == TokenKind::ContinueMarker) {
    //                 return {this->place(), tokens.first };
    //             } else if(tokens.second.kind() == TokenKind::ContinueMarker) {
    //                 return {this->place(), tokens.second };
    //             } else {
    //                 error_print("No ContinueMarker found in this ArcOut expression\n");
    //             }
    //             break;
    //         }
    //         case CPNEvaluationArgs::None: {
    //             auto tokens = _expression.tokens().value();
    //             if(tokens.first.kind() == TokenKind::EmptyMarker) {
    //                 return {this->place(), tokens.first };
    //             } else if(tokens.second.kind() == TokenKind::EmptyMarker) {
    //                 return {this->place(), tokens.second };
    //             } else {
    //                 error_print("No EmptyMarker found in this ArcOut expression\n");
    //             }
    //             break; 
    //         }
    //         case CPNEvaluationArgs::QueueExpression: {
    //             auto token = _expression.get_queue_token(marking.transition_fired_count(this->transition()));
    //             if(!token.has_value()) {
    //                 error_print("Queue is empty in ArcOut expression\n");
    //             } else {
    //                 return {this->place(), token.value()};
    //             }
    //             break;
    //         }
    //         default:
    //             error_print("Unhandled evaluation args. Returning empty token\n");
    //     }
    //     return {this->place(), CPNToken(TokenKind::EmptyMarker, -1, {})};
    // }
}

bool operator<(const CPNArcIn &lhs, const CPNArcIn &rhs)
{
    return lhs.place() < rhs.place();
}
