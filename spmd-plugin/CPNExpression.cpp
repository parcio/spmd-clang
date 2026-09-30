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

#include "CPNExpression.h"
#include "debug.h"
#include "utils.h"

CPNExpression::CPNExpression(ExpressionKind kind, CPNToken t1)
{
    _kind = kind;
    switch(kind) {
        case ExpressionKind::Simple:
            _token = t1;
            break;
        case ExpressionKind::Queue: {
            _queue = std::vector<CPNToken>({t1});
            break;
        }
        case ExpressionKind::Or:
            error_print("Or expression does not work with this constructor\n");
            break;
        default:
            error_print("Invalid ExpressionKind in CPNExpression constructor\n");
            break;
    }
}

std::string CPNExpression::to_string() const
{
    switch(_kind) {
        case ExpressionKind::Simple:
            return _token.value().to_string();
            break;
        case ExpressionKind::Or: {
            std::string result = "";
            for(int i = 0; i < _or_tokens.value().size(); i++) {
                result += _or_tokens.value()[i].to_string();
                if(i < _or_tokens.value().size() - 1) {
                    result += " or ";
                }
            }
            return result;
            break;
        }
        case ExpressionKind::Queue: {
            std::string result = "";
            for(const auto &token : _queue.value()) {
                result += token.to_string();
                if(!(token == _queue.value().back())) {
                    result += " before ";
                }
            }
            return result;
            break;
        }
            
        default:
            return "InvalidExpression";
    }
}

void CPNExpression::extend_queue(CPNToken token)
{
    const bool DEBUG = CPNEXPRESSION_BUILDING_DEBUG;
    if(_kind == ExpressionKind::Queue) {
        _queue.value().push_back(token);
    } else {
        if(DEBUG) {
            llvm::errs() << "This expression is not a queue\n";
            llvm::errs() << "Transforming it into a queue expression\n";
        }
        _kind = ExpressionKind::Queue;
        _queue = std::vector<CPNToken>({_token.value(), token});
        _token = std::nullopt;
    }
}

std::optional<CPNToken> CPNExpression::get_queue_token(const int idx) const
{
    if(_kind == ExpressionKind::Queue) {
        if(idx < _queue.value().size()) {
            return _queue.value()[idx];
        }
    } 
    return std::nullopt;
}


