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

#include "CPNNet.h"
#include "utils.h"

void CPNNet::dump() const
{
    llvm::errs() << "Places:\n";
    for(const auto &place : _places) {
        llvm::errs() << "  " << place.name() << "\n";
    }
    llvm::errs() << "Transitions:\n";
    for(const auto &transition : _transitions) {
        llvm::errs() << "  " << transition.name() << "\n";
        llvm::errs() << "  Guard: \n";
        if(transition.guard()) {
            llvm::errs() << "    " << transition.guard().value().to_string() << "\n";
        }
        llvm::errs() << "  In Arcs:\n";
        for(const auto &in_arc : transition.in_arcs()) {
            llvm::errs() << "    " << in_arc.binding() << "\n";
        }
        llvm::errs() << "  Out Arcs:\n";
        for(const auto &out_arc : transition.out_arcs()) {
            llvm::errs() << "    " << out_arc.expression().to_string() << "\n";
        }
        llvm::errs() << "\n";
    }
}

void CPNNet::output_cpn(const std::string &path) const
{
    std::error_code ec;
    llvm::raw_fd_ostream ofs(path, ec);

    ofs << "digraph G {\n";

    for(const auto &place : _places) {
        ofs << "    " << "\"" <<  place.name() << "\"[label=< <B>" << escape_to_html(place.name()) << "</B><BR/>";
        //TODO add tokens
        ofs << ">, shape=circle]\n";
    }

    for(const auto &transition : _transitions) {
        ofs << "    " << "\"" << transition.name() << "\"[label=< <B>" << escape_to_html(transition.name()) << "</B><BR/>";
        if(transition.guard()) {
            ofs << transition.guard().value().to_string() << "<BR/>";
        }
        ofs << ">, shape=box]\n";
    }

    for(const auto &transition : _transitions) {
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


const CPNTransition* CPNNet::find_transition(const int id) const
{
    auto iter =std::find_if(
        _transitions.begin(),
        _transitions.end(),
        [&](const CPNTransition &t) { return t.id() == id; }
    );
    if(iter != _transitions.end()) {
        return &*iter;
    } else {
        return nullptr;
    }
}


void CPNNet::add_place(CPNPlace place) 
{
    if(std::find_if(
        _places.begin(),
        _places.end(),
        [&](const CPNPlace &p) { return p.name() == place.name(); }
    ) != _places.end()) {
        llvm::errs() << "Place " << place.name() <<  " already exists\n";
    } else {
        _places.push_back(place);
    }
}

void CPNNet::add_transition(CPNTransition transition)
{
    if(std::find_if(
        _transitions.begin(),
        _transitions.end(),
        [&](const CPNTransition &t) { return t.name() == transition.name(); }
    ) != _transitions.end()) {
        llvm::errs() << "Transition " << transition.name() <<  " already exists\n";
    } else {
        _transitions.push_back(transition);
        if(transition.guard().value().kind() == CPNGuardKind::Barrier) {
            if(_barrier) {
                error_print("There is already a barrier transition in the net\n");
            } else {
                _barrier = &_transitions.back();
            }
        } else if(transition.guard().value().kind() == CPNGuardKind::Allgather) {
            if(_allgather) {
                error_print("There is already a allgather transition in the net\n");
            } else {
                _allgather = &_transitions.back();
            }
        } else if(transition.guard().value().kind() == CPNGuardKind::Allreduce) {
            if(_allreduce) {
                error_print("There is already a allreduce transition in the net\n");
            } else {
                _allreduce = &_transitions.back();
            }
        } else if(transition.guard().value().kind() == CPNGuardKind::Alltoall) {
            if(_alltoall) {
                error_print("There is already a alltoall transition in the net\n");
            } else {
                _alltoall = &_transitions.back();
            }
        } else if(transition.guard().value().kind() == CPNGuardKind::Bcast) {
            if(_bcast) {
                error_print("There is already a bcast transition in the net\n");
            } else {
                _bcast = &_transitions.back();
            }
        } else if(transition.guard().value().kind() == CPNGuardKind::Gather) {
            if(_gather) {
                error_print("There is already a _gather transition in the net\n");
            } else {
                _gather = &_transitions.back();
            }
        } else if(transition.guard().value().kind() == CPNGuardKind::Reduce) {
            if(_reduce) {
                error_print("There is already a _reduce transition in the net\n");
            } else {
                _reduce = &_transitions.back();
            }
        } else if(transition.guard().value().kind() == CPNGuardKind::Scatter) {
            if(_scatter) {
                error_print("There is already a _scatter transition in the net\n");
            } else {
                _scatter = &_transitions.back();
            }
        }
    }
}

void CPNNet::add_in_arc(std::string from, std::string to, std::string binding)
{
    if(!find_place(from)) {
        error_print("Place " + from + " not found\n");
        return;
    }
    auto transition = find_transition(to);
    if(!transition) {
        error_print("Transition " + to + " not found\n");
        return;
    }
    auto arc = CPNArcIn(from, to, binding);
    transition->add_in_arc(arc);
}

void CPNNet::add_out_arc(std::string from, std::string to, CPNExpression expression)
{
    if(!find_place(to)) {
        error_print("Place " + to + " not found\n");
        return;
    }
    auto transition = find_transition(from);
    if(!transition) {
        error_print("Transition " + from + " not found\n");
        return;
    }
    auto arc = CPNArcOut(from, to, expression);
    transition->add_out_arc(arc);
}

CPNPlace* CPNNet::find_place(std::string name)
{
    auto iter = std::find_if(
        _places.begin(),
        _places.end(),
        [&](const CPNPlace &p) { return p.name() == name; }
    ); 
    if(iter != _places.end()) {
        return &*iter;
    } else {
        return nullptr;
    }
}

const CPNTransition* CPNNet::find_transition(std::string name) const
{
    auto iter = std::find_if(
        _transitions.begin(),
        _transitions.end(),
        [&](const CPNTransition &t) { return t.name() == name; }
    ); 
    if(iter != _transitions.end()) {
        return &*iter;
    } else {
        return nullptr;
    }
}

CPNTransition* CPNNet::find_transition(std::string name)
{
    auto iter = std::find_if(
        _transitions.begin(),
        _transitions.end(),
        [&](const CPNTransition &t) { return t.name() == name; }
    ); 
    if(iter != _transitions.end()) {
        return &*iter;
    } else {
        return nullptr;
    }
}
