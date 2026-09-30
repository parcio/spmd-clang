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

#include "CPNMarking.h"
#include "utils.h"
#include <llvm/Support/raw_ostream.h>
#include <regex>
#include <string>

CPNMarking::CPNMarking(const CPNNet &net) 
{
    for(const auto &place : net.places()) {
        _marking[place.name()] = std::set<CPNToken>();
    }
    create_string();
}

void CPNMarking::dump() const
{
    llvm::errs() << "Marking:\n";
    for(const auto &[place, tokens] : _marking) {
        llvm::errs() << "    " << place << ": ";
        for(const auto &token : tokens) {
            llvm::errs() << token.to_string() << ", ";
        }
        if(_duplicate_tokens.count(place) > 0) {
            for(const auto &[kind, q] : _duplicate_tokens.at(place)) {
                auto queue = q;
                while(!queue.empty()) {
                    llvm::errs() << queue.front().to_string() << ", ";
                    queue.pop();
                }
            }
        }
        llvm::errs() << "\n";
    }
}

void CPNMarking::dump_ext() const
{
    llvm::errs() << to_string();
}

void CPNMarking::create_string()
{
    std::string result;
    result += "Marking:\n";
    for(const auto &[place, tokens] : _marking) {
        result += "    " + place + ": ";
        for(const auto &token : tokens) {
            result += token.to_string() + ", ";
        }
        if(_duplicate_tokens.count(place) > 0) {
            for(const auto &[kind, q] : _duplicate_tokens.at(place)) {
                auto queue = q;
                while(!queue.empty()) {
                    result +=  queue.front().to_string() + ", ";
                    queue.pop();
                }
            }
        }
        result += "\n";
    }
    result += "Transitions fired:\n";
    for(const auto &[key, value] : _transitions_fired) {
        result += "    " + key + ": " + std::to_string(value) + "\n";
    }
    
    _string = result;
}

std::string CPNMarking::to_string() const
{
    return _string;
}

void CPNMarking::add_token(std::string place, CPNToken token)
{
    if(_marking.find(place) == _marking.end()) {
        error_print("Place " + place + " not found in marking\n");
    } else {
        // This is required to support non-overtaking message semantics from the MPI standard
        if(!_marking[place].insert(token).second) {
            _duplicate_tokens[place][token.to_string()].push(token);
        }
    }
    create_string();
}

void CPNMarking::remove_token(std::string place, CPNToken token)
{
    if(_marking[place].erase(token) == 0) {
        error_print("Token " + token.to_string() + " not found in marking at place " + place + "\n");
    } else {
        if(_duplicate_tokens[place].size() > 0) {
            if(_duplicate_tokens[place][token.to_string()].size() > 0) {
                auto token2 = _duplicate_tokens[place][token.to_string()].front();
                _duplicate_tokens[place][token.to_string()].pop();
                add_token(place, token2);
            }
        }
    }
    create_string();
}

void CPNMarking::transition_fired(const std::string &transition_name)
{
    if(_transitions_fired.count(transition_name)) {
        _transitions_fired[transition_name]++;
    } else {
        _transitions_fired[transition_name] = 1;
    }
    create_string();
}

const int CPNMarking::transition_fired_count(const std::string &transition_name) const
{
    if(_transitions_fired.count(transition_name)) {
        return _transitions_fired.at(transition_name);
    } else {
        return 0;
    }
}

void CPNMarking::output_cpn(const std::string &path, const CPNNet &net) const
{
    std::error_code ec;
    llvm::raw_fd_ostream ofs(path, ec);

    ofs << "digraph G {\n";

    for(const auto &place : net.places()) {
        ofs << "    " << "\"" <<  place.name() << "\"[label=< <B>" << escape_to_html(place.name()) << "</B><BR/>";
        for(const auto &token : _marking.at(place.name())) {
            ofs << token.to_string() << "<BR/>";
        }
        if(_duplicate_tokens.count(place.name()) > 0) {
            for(const auto &[kind, q] : _duplicate_tokens.at(place.name())) {
                auto queue = q;
                while(!queue.empty()) {
                    ofs << queue.front().to_string() << "<BR/>";
                    queue.pop();
                }
            }
        }
        ofs << ">, shape=circle]\n";
    }

    for(const auto &transition : net.transitions()) {
        ofs << "    " << "\"" << transition.name() << "\"[label=< <B>" << escape_to_html(transition.name()) << "</B><BR/>";
        if(transition.guard()) {
            auto guard_string = transition.guard().value().to_string();

            std::regex del("and");
            std::sregex_token_iterator it(guard_string.begin(), guard_string.end(), del, -1);
            std::sregex_token_iterator end;
            std::vector<std::string> substrings;
            while (it != end) {
                substrings.push_back(*it);
                ++it;
            }
            if(substrings.size() == 1) {
                ofs << substrings.at(0) << "<BR/>";
            } else if(substrings.size() % 2 != 0 && substrings.size() != 0) {
                error_print("Guard string has an odd number of substrings. Wrong net visualization");
                llvm::errs() << substrings.size() << "\n";
            }
            for(int i = 0; i < substrings.size()-1; i+=2) {
                ofs << substrings.at(i) << " and " << substrings.at(i+1) << "<BR/>";
            }
        }
        ofs << ">, shape=box]\n";
    }

    for(const auto &transition : net.transitions()) {
        for(const auto &in_arc : transition.in_arcs()) {
            ofs << "    " << "\"" << in_arc.place() << "\" -> \"" << transition.name() << "\"";
            auto label = escape_to_html(in_arc.binding());
            ofs << "[label=\"" << label << "\"]\n";
        }
        for(const auto &out_arc : transition.out_arcs()) {
            ofs << "    " << "\"" << transition.name() << "\" -> \"" << out_arc.place() << "\"";
            auto label = escape_to_html(out_arc.expression().to_string());
            ofs << "[label=\"" << label << "\"]\n";
        }
    }

    ofs << "\n}";
    ofs.flush();
    ofs.close();

}

bool operator==(const CPNMarking &lhs, const CPNMarking &rhs)
{
    // return lhs.marking() == rhs.marking() && lhs.transitions_fired() == rhs.transitions_fired()
    //     && lhs.duplicate_tokens() == rhs.duplicate_tokens();
    return lhs.to_string() == rhs.to_string();
}

bool operator<(const CPNMarking &lhs, const CPNMarking &rhs)
{
    // return (lhs.marking() < rhs.marking()) || (lhs.transitions_fired() < rhs.transitions_fired())
    //     || (lhs.duplicate_tokens() < rhs.duplicate_tokens());
    return lhs.to_string() < rhs.to_string();
}
