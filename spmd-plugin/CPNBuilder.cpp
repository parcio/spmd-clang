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

#include "CPNBuilder.h"
#include "CPNExpression.h"
#include "CPNGuard.h"
#include "debug.h"
#include "utils.h"
#include <llvm/Support/raw_ostream.h>
#include <unordered_map>
#include <vector>
#include <llvm/Support/TimeProfiler.h>

// TODO make sure that this function can return false in case the net is not built so that the plugin can stop before starting the analysis
CPNNet CPNBuilder::build(const int mpi_size, const std::unordered_map<int, std::vector<SPMDCall>> &mpi_operations)
{
    // llvm::TimeTraceScope TimeScope("SPMDClang-CPNBuilder");
    auto net = CPNNet(mpi_size);

    // Add Places
    net.add_place(CPNPlace("Start"));
    net.add_place(CPNPlace("End"));
    net.add_place(CPNPlace("MessageBuffer"));
    net.add_place(CPNPlace("Signals"));
    for(const auto &[rank, ops] : mpi_operations) {
        net.add_place(CPNPlace("P" + std::to_string(rank)));
    }

    // Filter out not supported or irrelevant MPI operations
    auto operations = std::unordered_map<int, std::vector<SPMDCall>>();
    for(const auto &[rank, ops] : mpi_operations) {
        for(auto &op : ops) {
            auto func_name = op.expr->getDirectCallee()->getNameAsString();

            if(is_func_supported(func_name)) {
                operations[rank].push_back(op);
            } else {
                // TODO add an else if case for known operations that are skipped on purpose
                // TODO maybe return an optional here and only build the net if all MPI operations are either supported or explicitly on a 'skip list'
                warning_print("Operation " + func_name + " not yet supported and will be skipped. CPN might be incorrect");
            }
        }
    }

    // Check if all processes have MPI_Init as their first operation
    bool all_init = true;
    for(auto &[rank, ops] : operations) {
        auto op = ops.front();
        auto func_name = op.expr->getDirectCallee()->getNameAsString();
        if(func_name == "MPI_Init") {
            ops.erase(ops.begin());
        } else {
            // TODO Better error message for user
            error_print("MPI_Init not found as first operation in rank " + std::to_string(rank) + "\n");
            all_init = false;
        }
    }
    // TODO implement this with all the other transition types
    if(all_init) {
        auto op = operations.begin()->second.front();
        // TODO add this the correct way in the future
        auto transition = CPNTransition("MPI_Init", CPNGuard(CPNGuardKind::Init, {}), "all", op.expr);
        net.add_transition(transition);
        net.add_in_arc("Start", transition.name(), "i");

        // Add outgoing arcs from MPI_Init for all processes
        for(const auto &[rank, ops] : operations) {
            auto op = ops.front();
            auto expr = to_marker_expression(op, transition.id());
            net.add_out_arc(transition.name(), "P" + std::to_string(rank), expr);
        }
    }

    // Add all but the MPI_Finalize operations for all processes
    for(auto &[rank, ops] : operations) {
        while(ops.size() > 0) {
            auto op = ops.front();
            ops.erase(ops.begin());
            add_operation(net, rank, op, ops);
        }
    }

    // // Check if all processes have MPI_Finalize as their last operation
    // bool all_finalize = true;
    // for(auto &[rank, ops] : operations) {
    //     auto op = ops.front();
    //     auto func_name = op.expr->getDirectCallee()->getNameAsString();
    //     if(func_name != "MPI_Finalize") {
    //         error_print("MPI_Finalize not found as last operation in rank " + std::to_string(rank) + "\n");
    //         all_finalize = false;
    //     }
    // }
    // // TODO implement this with all the other transition types
    // if(all_finalize) {
    //     std::unordered_map<std::string, CPNExpression> guard_expression;
    //     for(const auto &[rank, ops] : operations) {
    //         auto s = "f" + std::to_string(rank);
    //         auto expr = CPNExpression(CPNToken(TokenKind::FinalizeMarker, -1, {}));
    //         guard_expression.insert(std::make_pair(s, expr));
    //     }
    //
    //     auto op = operations.begin()->second.front();
    //     // TODO add this the correct way in the future
    //     auto transition = CPNTransition("MPI_Finalize", CPNGuard(CPNGuardKind::Finalize, guard_expression), "all", op.expr);
    //     net.add_transition(transition);
    //
    //     for(const auto &[rank, ops] : operations) {
    //         net.add_in_arc("P" + std::to_string(rank), transition.name(), "f" + std::to_string(rank));
    //     }
    //
    //     auto expr = CPNExpression(CPNToken(TokenKind::FinalizeMarker, transition.id(), {}));
    //     net.add_out_arc(transition.name(), "End", expr);
    // } 

    return net;
}

bool CPNBuilder::is_func_supported(const std::string &func_name) const
{
    std::vector<std::string> supported_funcs = {
        "MPI_Init", "MPI_Finalize", "MPI_Ssend", "MPI_Recv", "MPI_Barrier", "MPI_Bsend", "MPI_Send",
        "MPI_Allgather", "MPI_Allreduce", "MPI_Alltoall", "MPI_Bcast", "MPI_Gather", "MPI_Reduce", "MPI_Scan", "MPI_Exscan", "MPI_Scatter",
        "MPI_Rsend", "MPI_Isend", "MPI_Wait", "MPI_Irecv", "MPI_Ibarrier", "MPI_Iallgather", "MPI_Iallreduce", "MPI_Ialltoall",
        "MPI_Ibcast", "MPI_Igather", "MPI_Ireduce", "MPI_Iscan", "MPI_Iexscan", "MPI_Iscatter",
    };

    if(std::find(supported_funcs.begin(), supported_funcs.end(), func_name) != supported_funcs.end()) {
        return true;
    }

    return false;
}


CPNExpression CPNBuilder::to_marker_expression(const SPMDCall op, const int transition_id) const
{
    auto func_name = op.expr->getDirectCallee()->getNameAsString();
    if(func_name == "MPI_Finalize") {
        return CPNExpression(CPNToken(TokenKind::FinalizeMarker, transition_id, {}));

    } else if(func_name == "MPI_Ssend") {
        // auto type = op.args[2].second.get_as_integer().value();
        auto src = op.rank.get_as_integer().value();
        auto dest = op.args[3].second.get_as_integer().value();
        auto tag = op.args[4].second.get_as_integer().value();
        auto comm = op.args[5].second.get_as_integer().value();
        return CPNExpression(CPNToken(TokenKind::SsendMarker, transition_id, { src, dest, tag, comm }));
    } else if(func_name == "MPI_Bsend") {
        // auto type = op.args[2].second.get_as_integer().value();
        auto src = op.rank.get_as_integer().value();
        auto dest = op.args[3].second.get_as_integer().value();
        auto tag = op.args[4].second.get_as_integer().value();
        auto comm = op.args[5].second.get_as_integer().value();
        return CPNExpression(CPNToken(TokenKind::BsendMarker, transition_id, { src, dest, tag, comm }));
    } else if(func_name == "MPI_Send") {
        // auto type = op.args[2].second.get_as_integer().value();
        auto src = op.rank.get_as_integer().value();
        auto dest = op.args[3].second.get_as_integer().value();
        auto tag = op.args[4].second.get_as_integer().value();
        auto comm = op.args[5].second.get_as_integer().value();
        return CPNExpression(CPNToken(TokenKind::SendMarker, transition_id, { src, dest, tag, comm }));
    } else if(func_name == "MPI_Rsend") {
        // auto type = op.args[2].second.get_as_integer().value();
        auto src = op.rank.get_as_integer().value();
        auto dest = op.args[3].second.get_as_integer().value();
        auto tag = op.args[4].second.get_as_integer().value();
        auto comm = op.args[5].second.get_as_integer().value();
        return CPNExpression(CPNToken(TokenKind::RsendMarker, transition_id, { src, dest, tag, comm }));
    } else if(func_name == "MPI_Recv") {
        // auto type = op.args[2].second.get_as_integer().value();
        auto src = op.args[3].second.get_as_integer().value();
        auto dest = op.rank.get_as_integer().value();
        auto tag = op.args[4].second.get_as_integer().value();
        auto comm = op.args[5].second.get_as_integer().value();
        return CPNExpression(CPNToken(TokenKind::RecvMarker, transition_id, { src, dest, tag, comm}));
    } else if(func_name == "MPI_Isend") {
        // auto type = op.args[2].second.get_as_integer().value();
        auto src = op.rank.get_as_integer().value();
        auto dest = op.args[3].second.get_as_integer().value();
        auto tag = op.args[4].second.get_as_integer().value();
        auto comm = op.args[5].second.get_as_integer().value();
        auto request = op.args[6].second.get_as_integer().value();
        return CPNExpression(CPNToken(TokenKind::IsendMarker, transition_id, { src, dest, tag, comm, request }));
    } else if(func_name == "MPI_Wait") {
        auto request = op.args[0].second.get_as_integer().value();
        return CPNExpression(CPNToken(TokenKind::WaitMarker, transition_id, { request}));
    } else if(func_name == "MPI_Irecv") {
        // auto type = op.args[2].second.get_as_integer().value();
        auto src = op.args[3].second.get_as_integer().value();
        auto dest = op.rank.get_as_integer().value();
        auto tag = op.args[4].second.get_as_integer().value();
        auto comm = op.args[5].second.get_as_integer().value();
        auto request = op.args[6].second.get_as_integer().value();
        return CPNExpression(CPNToken(TokenKind::IrecvMarker, transition_id, {src, dest, tag, comm, request}));
    } else if(func_name == "MPI_Barrier") {
        auto comm = op.args[0].second.get_as_integer().value();
        return CPNExpression(CPNToken(TokenKind::BarrierMarker, transition_id, {comm}));
    } else if(func_name == "MPI_Allgather") {
        auto comm = op.args[6].second.get_as_integer().value();
        return CPNExpression(CPNToken(TokenKind::AllgatherMarker, transition_id, {comm}));
    } else if(func_name == "MPI_Allreduce") {
        auto comm = op.args[5].second.get_as_integer().value();
        return CPNExpression(CPNToken(TokenKind::AllreduceMarker, transition_id, {comm}));
    } else if(func_name == "MPI_Alltoall") {
        auto comm = op.args[6].second.get_as_integer().value();
        return CPNExpression(CPNToken(TokenKind::AlltoallMarker, transition_id, {comm}));
    } else if(func_name == "MPI_Bcast") {
        auto root = op.args[3].second.get_as_integer().value();
        auto comm = op.args[4].second.get_as_integer().value();
        return CPNExpression(CPNToken(TokenKind::BcastMarker, transition_id, {root, comm}));
    } else if(func_name == "MPI_Gather") {
        auto root = op.args[6].second.get_as_integer().value();
        auto comm = op.args[7].second.get_as_integer().value();
        return CPNExpression(CPNToken(TokenKind::GatherMarker, transition_id, {root, comm}));
    } else if(func_name == "MPI_Reduce") {
        auto root = op.args[5].second.get_as_integer().value();
        auto comm = op.args[6].second.get_as_integer().value();
        return CPNExpression(CPNToken(TokenKind::ReduceMarker, transition_id, {root, comm}));
    } else if(func_name == "MPI_Scan") {
        auto comm = op.args[5].second.get_as_integer().value();
        return CPNExpression(CPNToken(TokenKind::ScanMarker, transition_id, {comm}));
    } else if(func_name == "MPI_Exscan") {
        auto comm = op.args[5].second.get_as_integer().value();
        return CPNExpression(CPNToken(TokenKind::ExscanMarker, transition_id, {comm}));
    } else if(func_name == "MPI_Scatter") {
        auto root = op.args[6].second.get_as_integer().value();
        auto comm = op.args[7].second.get_as_integer().value();
        return CPNExpression(CPNToken(TokenKind::ScatterMarker, transition_id, {root, comm}));
    } else if(func_name == "MPI_Ibarrier") {
        auto comm = op.args[0].second.get_as_integer().value();
        auto req = op.args[1].second.get_as_integer().value();
        return CPNExpression(CPNToken(TokenKind::IbarrierMarker, transition_id, {comm, req}));
    } else if(func_name == "MPI_Iallgather") {
        auto comm = op.args[6].second.get_as_integer().value();
        auto req = op.args[7].second.get_as_integer().value();
        return CPNExpression(CPNToken(TokenKind::IallgatherMarker, transition_id, {comm, req}));
    } else if(func_name == "MPI_Iallreduce") {
        auto comm = op.args[5].second.get_as_integer().value();
        auto req = op.args[6].second.get_as_integer().value();
        return CPNExpression(CPNToken(TokenKind::IallreduceMarker, transition_id, {comm, req}));
    } else if(func_name == "MPI_Ialltoall") {
        auto comm = op.args[6].second.get_as_integer().value();
        auto req = op.args[7].second.get_as_integer().value();
        return CPNExpression(CPNToken(TokenKind::IalltoallMarker, transition_id, {comm, req}));
    } else if(func_name == "MPI_Ibcast") {
        auto root = op.args[3].second.get_as_integer().value();
        auto comm = op.args[4].second.get_as_integer().value();
        auto req = op.args[5].second.get_as_integer().value();
        return CPNExpression(CPNToken(TokenKind::IbcastMarker, transition_id, {root, comm, req}));
    } else if(func_name == "MPI_Igather") {
        auto root = op.args[6].second.get_as_integer().value();
        auto comm = op.args[7].second.get_as_integer().value();
        auto req = op.args[8].second.get_as_integer().value();
        return CPNExpression(CPNToken(TokenKind::IgatherMarker, transition_id, {root, comm, req}));
    } else if(func_name == "MPI_Ireduce") {
        auto root = op.args[5].second.get_as_integer().value();
        auto comm = op.args[6].second.get_as_integer().value();
        auto req = op.args[7].second.get_as_integer().value();
        return CPNExpression(CPNToken(TokenKind::IreduceMarker, transition_id, {root, comm, req}));
    } else if(func_name == "MPI_Iscan") {
        auto comm = op.args[5].second.get_as_integer().value();
        auto req = op.args[6].second.get_as_integer().value();
        return CPNExpression(CPNToken(TokenKind::IscanMarker, transition_id, {comm, req}));
    } else if(func_name == "MPI_Iexscan") {
        auto comm = op.args[5].second.get_as_integer().value();
        auto req = op.args[6].second.get_as_integer().value();
        return CPNExpression(CPNToken(TokenKind::IexscanMarker, transition_id, {comm, req}));
    } else if(func_name == "MPI_Iscatter") {
        auto root = op.args[6].second.get_as_integer().value();
        auto comm = op.args[7].second.get_as_integer().value();
        auto req = op.args[8].second.get_as_integer().value();
        return CPNExpression(CPNToken(TokenKind::IscatterMarker, transition_id, {root, comm, req}));
    } else {
        llvm::errs() << "Not yet supported operation: " << func_name << "\n";
        return CPNExpression(CPNToken(TokenKind::EmptyMarker, transition_id, {}));
    }
}

void CPNBuilder::add_operation(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const
{
    auto func_name = op.expr->getDirectCallee()->getNameAsString();
    if(func_name == "MPI_Finalize") {
        mpi_finalize(net, rank, op, ops);
    } else if(func_name == "MPI_Ssend") {
        mpi_ssend(net, rank, op, ops);
    } else if(func_name == "MPI_Recv") {
        mpi_recv(net, rank, op, ops);
    } else if(func_name == "MPI_Barrier") {
        mpi_barrier(net, rank, op, ops);
    } else if(func_name == "MPI_Bsend") {
        mpi_bsend(net, rank, op, ops);
    } else if(func_name == "MPI_Send") {
        mpi_send(net, rank, op, ops);
    } else if(func_name == "MPI_Rsend") {
        mpi_rsend(net, rank, op, ops);
    } else if(func_name == "MPI_Isend") {
        mpi_isend(net, rank, op, ops);
    } else if(func_name == "MPI_Wait") {
        mpi_wait(net, rank, op, ops);
    } else if(func_name == "MPI_Irecv") {
        mpi_irecv(net, rank, op, ops);
    } else if(func_name == "MPI_Allgather") {
        mpi_allgather(net, rank, op, ops);
    } else if(func_name == "MPI_Allreduce") {
        mpi_allreduce(net, rank, op, ops);
    } else if(func_name == "MPI_Alltoall") {
        mpi_alltoall(net, rank, op, ops);
    } else if(func_name == "MPI_Bcast") {
        mpi_bcast(net, rank, op, ops);
    } else if(func_name == "MPI_Gather") {
        mpi_gather(net, rank, op, ops);
    } else if(func_name == "MPI_Reduce") {
        mpi_reduce(net, rank, op, ops);
    } else if(func_name == "MPI_Scan") {
        mpi_scan(net, rank, op, ops);
    } else if(func_name == "MPI_Exscan") {
        mpi_exscan(net, rank, op, ops);
    } else if(func_name == "MPI_Scatter") {
        mpi_scatter(net, rank, op, ops);
    } else if(func_name == "MPI_Ibarrier") {
        mpi_ibarrier(net, rank, op, ops);
    } else if(func_name == "MPI_Iallgather") {
        mpi_iallgather(net, rank, op, ops);
    } else if(func_name == "MPI_Iallreduce") {
        mpi_iallreduce(net, rank, op, ops);
    } else if(func_name == "MPI_Ialltoall") {
        mpi_ialltoall(net, rank, op, ops);
    } else if(func_name == "MPI_Ibcast") {
        mpi_ibcast(net, rank, op, ops);
    } else if(func_name == "MPI_Igather") {
        mpi_igather(net, rank, op, ops);
    } else if(func_name == "MPI_Ireduce") {
        mpi_ireduce(net, rank, op, ops);
    } else if(func_name == "MPI_Iscan") {
        mpi_iscan(net, rank, op, ops);
    } else if(func_name == "MPI_Iexscan") {
        mpi_iexscan(net, rank, op, ops);
    } else if(func_name == "MPI_Iscatter") {
        mpi_iscatter(net, rank, op, ops);
    } else {
        error_print("Operation " + func_name + " not yet supported and should have been filtered. CPN will be incorrect."); 
    }
}

void CPNBuilder::mpi_finalize(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const
{
    const bool DEBUG = CPNBUILDER_GENERAL_DEBUG;

    std::unordered_map<std::string, CPNExpression> guard_expression;
    for(int i = 0; i < net.mpi_size(); i++) {
        guard_expression.insert(std::make_pair("f" + std::to_string(i), CPNExpression(CPNToken(TokenKind::FinalizeMarker, -1, {}))));
    }
    auto guard = CPNGuard(CPNGuardKind::Finalize, guard_expression);

    // check if there is already a Finalize transition in the net
    // collective operations only get one transition in the whole net
    CPNTransition* existing_transition = nullptr;
    for(auto &t : net.transitions()) {
        if(t.guard().value().kind() == CPNGuardKind::Finalize && t.guard().value().to_string() == guard.to_string()) {
            existing_transition = &t;
        }
    }

    if(!existing_transition) {
        auto transition = CPNTransition("MPI_Finalize", guard, "P" + std::to_string(rank), op.expr);
        net.add_transition(transition);

        std::string place_name = "P" + std::to_string(rank);
        CPNExpression expr = CPNExpression(CPNToken(TokenKind::FinalizeMarker, transition.id(), {rank}));
        net.add_in_arc(place_name, transition.name(), "f" + std::to_string(rank));

        auto expr2 = CPNExpression(CPNToken(TokenKind::FinalizeMarker, transition.id(), {}));
        net.add_out_arc(transition.name(), "End", expr2);
    } else {
        if(DEBUG) {
            llvm::errs() << rank << " adding to existing MPI_Finalize transition\n";
            llvm::errs() << "Adding Finalize arc from " << "P" + std::to_string(rank) << " to " << existing_transition->name() << "\n";
        }

        std::string place_name = "P" + std::to_string(rank);
        net.add_in_arc(place_name, existing_transition->name(), "f" + std::to_string(rank));
        existing_transition->add_ast_expr("P" + std::to_string(rank), op.expr);
    }
}

void CPNBuilder::mpi_ssend(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const
{
    const bool DEBUG = CPNBUILDER_GENERAL_DEBUG;
    // auto type = op.args[2].second.get_as_integer().value();
    auto src = op.rank.get_as_integer().value();
    auto dest = op.args[3].second.get_as_integer().value();
    auto tag = op.args[4].second.get_as_integer().value();
    auto comm = op.args[5].second.get_as_integer().value();

    std::unordered_map<std::string, CPNExpression> guard_expression;
    guard_expression.insert(std::make_pair("s", CPNExpression(CPNToken(TokenKind::SsendMarker, -1, {src, dest, tag, comm}))));
    auto guard = CPNGuard(CPNGuardKind::Ssend, guard_expression);
    
    // Check if there already is a transtition with the same guard for this process
    // TODO make this a function?
    CPNTransition* existing_transition = nullptr;
    for(auto &t : net.transitions()) {
        if(t.guard().value().kind() == CPNGuardKind::Ssend && t.guard().value().to_string() == guard.to_string()) {
            for(const auto &in_arc : t.in_arcs()) {
                if(in_arc.place() == "P" + std::to_string(rank)) {
                    existing_transition = &t;
                    if(DEBUG) {
                        llvm::errs() << " ==> FOUND EQUAL SSEND TRANSITION\n";
                    }
                    break;
                }
            }
        }
    }

    if(!existing_transition) {
        auto transition = CPNTransition("MPI_Ssend", guard, "P" + std::to_string(rank), op.expr);
        net.add_transition(transition);

        std::string place_name = "P" + std::to_string(rank);
        CPNExpression expr = CPNExpression(CPNToken(TokenKind::MessageMarker, transition.id(), {src, dest, tag, comm}));
        net.add_in_arc(place_name, transition.name(), "s");
        net.add_out_arc(transition.name(), "MessageBuffer", expr);

        // Add a blocking state transition so that this process has to wait until the message has been received
        std::unordered_map<std::string, CPNExpression> guard_expression2;
        guard_expression2.insert(std::make_pair("c", CPNExpression(CPNToken(TokenKind::ContinueMarker, -1, { src, dest, tag, comm}))));
        auto transition2 = CPNTransition("Blocking", CPNGuard(CPNGuardKind::Blocking, guard_expression2), "P" + std::to_string(rank), op.expr);
        net.add_transition(transition2);
        net.add_in_arc("Signals", transition2.name(), "c");

        auto expr2 = to_marker_expression(ops.front(), transition2.id());
        net.add_out_arc(transition2.name(), place_name, expr2);
    } else {
        if(DEBUG) {
            llvm::errs() << "Adding to existing MPI_Ssend transition\n";
        }

        // Find matching "blocking" transition
        CPNTransition *blocking_transition = nullptr;
        CPNArcOut *blocking_out_arc = nullptr;
        std::unordered_map<std::string, CPNExpression> guard_expression2;
        guard_expression2.insert(std::make_pair("c", CPNExpression(CPNToken(TokenKind::ContinueMarker, -1, { src, dest, tag, comm}))));
        auto block_guard = CPNGuard(CPNGuardKind::Blocking, guard_expression2);

        for(auto &t : net.transitions()) {
            if(t.guard().value().kind() == CPNGuardKind::Blocking && t.guard().value().to_string() == block_guard.to_string()) {
                for(auto &out_arc : t.out_arcs()) {
                    if(out_arc.place() == "P" + std::to_string(rank)) {
                        blocking_transition = &t;
                        blocking_out_arc = &out_arc;
                        if(DEBUG) {
                            llvm::errs() << " ==> FOUND EQUAL BLOCKING TRANSITION\n";
                        }
                    }
                }
            }
        }

        if(!blocking_transition) {
            error_print("Could not find blocking transition for existing MPI_Ssend");
            return;
        }

        auto expr2 = to_marker_expression(ops.front(), blocking_transition->id());
        blocking_out_arc->expression().extend_queue(expr2.token().value());
        existing_transition->add_ast_expr("P" + std::to_string(rank), op.expr);
        blocking_transition->add_ast_expr("P" + std::to_string(rank), op.expr);
    }
}

void CPNBuilder::mpi_recv(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const
{
    const bool DEBUG = CPNBUILDER_GENERAL_DEBUG;
    // auto type = op.args[2].second.get_as_integer().value();
    auto src = op.args[3].second.get_as_integer().value();
    auto dest = op.rank.get_as_integer().value();
    auto tag = op.args[4].second.get_as_integer().value();
    auto comm = op.args[5].second.get_as_integer().value();

    std::unordered_map<std::string, CPNExpression> guard_expression;
    guard_expression.insert(std::make_pair("s", CPNExpression(CPNToken(TokenKind::MessageMarker, -1, { src, dest, tag, comm}))));
    guard_expression.insert(std::make_pair("r", CPNExpression(CPNToken(TokenKind::RecvMarker, -1, {src, dest, tag, comm}))));
    auto guard = CPNGuard(CPNGuardKind::Recv, guard_expression);

    // check if there already is a transtition with the same guard for this process
    // TODO make this a function?
    CPNTransition* existing_transition = nullptr;
    for(auto &t : net.transitions()) {
        if(t.guard().value().kind() == CPNGuardKind::Recv && t.guard().value().to_string() == guard.to_string()) {
            for(const auto &in_arc : t.in_arcs()) {
                if(in_arc.place() == "P" + std::to_string(rank)) {
                    existing_transition = &t;
                    if(DEBUG) {
                        llvm::errs() << " ==> FOUND EQUAL RECV TRANSITION\n";
                    }
                    break;
                }
            }
        }
    }

    if(!existing_transition) {
        auto transition = CPNTransition("MPI_Recv", guard, "P" + std::to_string(rank), op.expr);
        net.add_transition(transition);

        std::string place_name = "P" + std::to_string(rank);
        net.add_in_arc(place_name, transition.name(), "r");
        net.add_in_arc("MessageBuffer", transition.name(), "s");

        // add continuation marker for blocking sends that might have created the message in the buffer
        // make this an Or expression because the continuemarker should only be created if the sendmarker was from a blocking  operation
        // Also add a potential RequestMarker as output in case the message was sent by a non-blocking send
        auto token = CPNToken(TokenKind::ContinueMarker, transition.id(), { src, dest, tag, comm}); 
        auto token2 = CPNToken(TokenKind::EmptyMarker, transition.id(), {});
        auto token3 = CPNToken(TokenKind::RequestMarker, transition.id(), {});
        auto expr = CPNExpression(ExpressionKind::Or, { token, token2, token3 });
        net.add_out_arc(transition.name(), "Signals", expr);

        auto expr2 = to_marker_expression(ops.front(), transition.id());
        net.add_out_arc(transition.name(), place_name, expr2);
    } else {
        if(DEBUG) {
            llvm::errs() << "Adding to existing MPI_Recv transition\n";
        }
        
        std::string place_name = "P" + std::to_string(rank);
        for(auto &out_arc : existing_transition->out_arcs()) {
            if(out_arc.place() == place_name) {
                auto expr2 = to_marker_expression(ops.front(), existing_transition->id());
                out_arc.expression().extend_queue(expr2.token().value());
                existing_transition->add_ast_expr("P" + std::to_string(rank), op.expr);
                if(DEBUG) {
                    llvm::errs() << "Extending existing MPI_Recv transition\n";
                }
                break;
            }
        }
    }
}


void CPNBuilder::mpi_barrier(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const
{
    const bool DEBUG = CPNBUILDER_GENERAL_DEBUG;

    auto comm = op.args[0].second.get_as_integer().value();

    std::unordered_map<std::string, CPNExpression> guard_expression;
    for(int i = 0; i < net.mpi_size(); i++) {
        guard_expression.insert(std::make_pair("b" + std::to_string(i), CPNExpression(CPNToken(TokenKind::BarrierMarker, -1, {comm}))));
    }
    auto guard = CPNGuard(CPNGuardKind::Barrier, guard_expression);

    // check if there is already a barrier transition in the net
    // collective operations only get one transition in the whole net
    CPNTransition* existing_transition = nullptr;
    for(auto &t : net.transitions()) {
        if(t.guard().value().kind() == CPNGuardKind::Barrier && t.guard().value().to_string() == guard.to_string()) {
            existing_transition = &t;
        }
    }

    if(!existing_transition) {
        auto transition = CPNTransition("MPI_Barrier", guard, "P" + std::to_string(rank), op.expr);
        net.add_transition(transition);

        std::string place_name = "P" + std::to_string(rank);
        CPNExpression expr = CPNExpression(CPNToken(TokenKind::BarrierMarker, transition.id(), {comm}));
        net.add_in_arc(place_name, transition.name(), "b" + std::to_string(rank));
        auto expr2 = to_marker_expression(ops.front(), transition.id());
        net.add_out_arc(transition.name(), place_name, expr2);
    } else {
        if(DEBUG) {
            llvm::errs() << rank << " adding to existing barrier\n";
        }
        bool arc_exists = false;
        for(auto &in_arc : existing_transition->in_arcs()) {        
            if(in_arc.place() == "P" + std::to_string(rank)) {
                arc_exists = true;
            }
        }
        
        if(!arc_exists) {
            if(DEBUG) {
                llvm::errs() << "Adding barrier arc from " << "P" + std::to_string(rank) << " to " << existing_transition->name() << "\n";
            }
            std::string place_name = "P" + std::to_string(rank);
            net.add_in_arc(place_name, existing_transition->name(), "b" + std::to_string(rank));
            auto expr2 = to_marker_expression(ops.front(), existing_transition->id());
            net.add_out_arc(existing_transition->name(), place_name, expr2);
            existing_transition->add_ast_expr("P" + std::to_string(rank), op.expr);
        } else {
            if(DEBUG) {
                llvm::errs() << rank << " adding to queue of barrier out arc\n";
            }
            
            std::string place_name = "P" + std::to_string(rank);
            for(auto &out_arc : existing_transition->out_arcs()) {
                if(out_arc.place() == place_name) {
                    auto expr2 = to_marker_expression(ops.front(), existing_transition->id());
                    out_arc.expression().extend_queue(expr2.token().value());
                    existing_transition->add_ast_expr("P" + std::to_string(rank), op.expr);
                    if(DEBUG) {
                        llvm::errs() << "Extending existing MPI_Barrier transition\n";
                    }
                    break;
                }
            }

        }
    }

}

void CPNBuilder::mpi_bsend(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const
{
    const bool DEBUG = CPNBUILDER_GENERAL_DEBUG;

    // auto type = op.args[2].second.get_as_integer().value();
    auto src = op.rank.get_as_integer().value();
    auto dest = op.args[3].second.get_as_integer().value();
    auto tag = op.args[4].second.get_as_integer().value();
    auto comm = op.args[5].second.get_as_integer().value();

    std::unordered_map<std::string, CPNExpression> guard_expression;
    guard_expression.insert(std::make_pair("s", CPNExpression(CPNToken(TokenKind::BsendMarker, -1, {src, dest, tag, comm}))));
    auto guard = CPNGuard(CPNGuardKind::Bsend, guard_expression);

    // Check if there already is a transtition with the same guard for this process
    // TODO make this a function?
    CPNTransition* existing_transition = nullptr;
    for(auto &t : net.transitions()) {
        if(t.guard().value().kind() == CPNGuardKind::Bsend && t.guard().value().to_string() == guard.to_string()) {
            for(const auto &in_arc : t.in_arcs()) {
                if(in_arc.place() == "P" + std::to_string(rank)) {
                    existing_transition = &t;
                    if(DEBUG) {
                        llvm::errs() << " ==> FOUND EQUAL BSEND TRANSITION\n";
                    }
                    break;
                }
            }
        }
    }

    if(!existing_transition) {
        auto transition = CPNTransition("MPI_Bsend", guard, "P" + std::to_string(rank), op.expr);
        net.add_transition(transition);

        std::string place_name = "P" + std::to_string(rank);
        CPNExpression expr = CPNExpression(CPNToken(TokenKind::MessageMarker, transition.id(), { src, dest, tag, comm}));
        net.add_in_arc(place_name, transition.name(), "s");
        net.add_out_arc(transition.name(), "MessageBuffer", expr);

        auto expr2 = to_marker_expression(ops.front(), transition.id());
        net.add_out_arc(transition.name(), place_name, expr2);
    } else {
        if(DEBUG) {
            llvm::errs() << "Adding to existing MPI_Bsend transition\n";
        }

        std::string place_name = "P" + std::to_string(rank);
        for(auto &out_arc : existing_transition->out_arcs()) {
            if(out_arc.place() == place_name) {
                auto expr2 = to_marker_expression(ops.front(), existing_transition->id());
                out_arc.expression().extend_queue(expr2.token().value());
                existing_transition->add_ast_expr("P" + std::to_string(rank), op.expr);
                if(DEBUG) {
                    llvm::errs() << "Extending existing MPI_Bsend transition\n";
                }
                break;
            }
        }
    }
}

// TODO currently behaves exactly like MPI_Ssend; maybe change that or make it configurable
void CPNBuilder::mpi_send(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const
{
    const bool DEBUG = CPNBUILDER_GENERAL_DEBUG;
    // auto type = op.args[2].second.get_as_integer().value();
    auto src = op.rank.get_as_integer().value();
    auto dest = op.args[3].second.get_as_integer().value();
    auto tag = op.args[4].second.get_as_integer().value();
    auto comm = op.args[5].second.get_as_integer().value();

    std::unordered_map<std::string, CPNExpression> guard_expression;
    guard_expression.insert(std::make_pair("s", CPNExpression(CPNToken(TokenKind::SendMarker, -1, {src, dest, tag, comm}))));
    auto guard = CPNGuard(CPNGuardKind::Send, guard_expression);
    
    // Check if there already is a transtition with the same guard for this process
    // TODO make this a function?
    CPNTransition* existing_transition = nullptr;
    for(auto &t : net.transitions()) {
        if(t.guard().value().kind() == CPNGuardKind::Send && t.guard().value().to_string() == guard.to_string()) {
            for(const auto &in_arc : t.in_arcs()) {
                if(in_arc.place() == "P" + std::to_string(rank)) {
                    existing_transition = &t;
                    if(DEBUG) {
                        llvm::errs() << " ==> FOUND EQUAL SEND TRANSITION\n";
                    }
                    break;
                }
            }
        }
    }

    if(!existing_transition) {
        auto transition = CPNTransition("MPI_Send", guard, "P" + std::to_string(rank), op.expr);
        net.add_transition(transition);

        std::string place_name = "P" + std::to_string(rank);
        CPNExpression expr = CPNExpression(CPNToken(TokenKind::MessageMarker, transition.id(), {src, dest, tag, comm}));
        net.add_in_arc(place_name, transition.name(), "s");
        net.add_out_arc(transition.name(), "MessageBuffer", expr);

        // Add a blocking state transition so that this process has to wait until the message has been received
        std::unordered_map<std::string, CPNExpression> guard_expression2;
        guard_expression2.insert(std::make_pair("c", CPNExpression(CPNToken(TokenKind::ContinueMarker, -1, { src, dest, tag, comm}))));
        auto transition2 = CPNTransition("Blocking", CPNGuard(CPNGuardKind::Blocking, guard_expression2), "P" + std::to_string(rank), op.expr);
        net.add_transition(transition2);
        net.add_in_arc("Signals", transition2.name(), "c");

        auto expr2 = to_marker_expression(ops.front(), transition2.id());
        net.add_out_arc(transition2.name(), place_name, expr2);
    } else {
        if(DEBUG) {
            llvm::errs() << "Adding to existing MPI_Send transition\n";
        }

        // Find matching "blocking" transition
        CPNTransition *blocking_transition = nullptr;
        CPNArcOut *blocking_out_arc = nullptr;
        std::unordered_map<std::string, CPNExpression> guard_expression2;
        guard_expression2.insert(std::make_pair("c", CPNExpression(CPNToken(TokenKind::ContinueMarker, -1, { src, dest, tag, comm}))));
        auto block_guard = CPNGuard(CPNGuardKind::Blocking, guard_expression2);

        for(auto &t : net.transitions()) {
            if(t.guard().value().kind() == CPNGuardKind::Blocking && t.guard().value().to_string() == block_guard.to_string()) {
                for(auto &out_arc : t.out_arcs()) {
                    if(out_arc.place() == "P" + std::to_string(rank)) {
                        blocking_transition = &t;
                        blocking_out_arc = &out_arc;
                        if(DEBUG) {
                            llvm::errs() << " ==> FOUND EQUAL BLOCKING TRANSITION\n";
                        }
                    }
                }
            }
        }

        if(!blocking_transition) {
            error_print("Could not find blocking transition for existing MPI_Send");
            return;
        }

        auto expr2 = to_marker_expression(ops.front(), blocking_transition->id());
        blocking_out_arc->expression().extend_queue(expr2.token().value());
        existing_transition->add_ast_expr("P" + std::to_string(rank), op.expr);
        blocking_transition->add_ast_expr("P" + std::to_string(rank), op.expr);
    }
}

// TODO currently behaves exactly like MPI_Ssend; maybe change that or make it configurable
void CPNBuilder::mpi_rsend(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const
{
    const bool DEBUG = CPNBUILDER_GENERAL_DEBUG;
    // auto type = op.args[2].second.get_as_integer().value();
    auto src = op.rank.get_as_integer().value();
    auto dest = op.args[3].second.get_as_integer().value();
    auto tag = op.args[4].second.get_as_integer().value();
    auto comm = op.args[5].second.get_as_integer().value();

    std::unordered_map<std::string, CPNExpression> guard_expression;
    guard_expression.insert(std::make_pair("s", CPNExpression(CPNToken(TokenKind::RsendMarker, -1, {src, dest, tag, comm}))));
    auto guard = CPNGuard(CPNGuardKind::Rsend, guard_expression);
    
    // Check if there already is a transtition with the same guard for this process
    // TODO make this a function?
    CPNTransition* existing_transition = nullptr;
    for(auto &t : net.transitions()) {
        if(t.guard().value().kind() == CPNGuardKind::Rsend && t.guard().value().to_string() == guard.to_string()) {
            for(const auto &in_arc : t.in_arcs()) {
                if(in_arc.place() == "P" + std::to_string(rank)) {
                    existing_transition = &t;
                    if(DEBUG) {
                        llvm::errs() << " ==> FOUND EQUAL RSEND TRANSITION\n";
                    }
                    break;
                }
            }
        }
    }

    if(!existing_transition) {
        auto transition = CPNTransition("MPI_Rsend", guard, "P" + std::to_string(rank), op.expr);
        net.add_transition(transition);

        std::string place_name = "P" + std::to_string(rank);
        CPNExpression expr = CPNExpression(CPNToken(TokenKind::MessageMarker, transition.id(), { src, dest, tag, comm}));
        net.add_in_arc(place_name, transition.name(), "s");
        net.add_out_arc(transition.name(), "MessageBuffer", expr);

        // Add a blocking state transition so that this process has to wait until the message has been received
        std::unordered_map<std::string, CPNExpression> guard_expression2;
        guard_expression2.insert(std::make_pair("c", CPNExpression(CPNToken(TokenKind::ContinueMarker, -1, { src, dest, tag, comm}))));
        auto transition2 = CPNTransition("Blocking", CPNGuard(CPNGuardKind::Blocking, guard_expression2), "P" + std::to_string(rank), op.expr);
        net.add_transition(transition2);
        net.add_in_arc("Signals", transition2.name(), "c");

        auto expr2 = to_marker_expression(ops.front(), transition2.id());
        net.add_out_arc(transition2.name(), place_name, expr2);
    } else {
        if(DEBUG) {
            llvm::errs() << "Adding to existing MPI_Rsend transition\n";
        }

        // Find matching "blocking" transition
        CPNTransition *blocking_transition = nullptr;
        CPNArcOut *blocking_out_arc = nullptr;
        std::unordered_map<std::string, CPNExpression> guard_expression2;
        guard_expression2.insert(std::make_pair("c", CPNExpression(CPNToken(TokenKind::ContinueMarker, -1, { src, dest, tag, comm}))));
        auto block_guard = CPNGuard(CPNGuardKind::Blocking, guard_expression2);

        for(auto &t : net.transitions()) {
            if(t.guard().value().kind() == CPNGuardKind::Blocking && t.guard().value().to_string() == block_guard.to_string()) {
                for(auto &out_arc : t.out_arcs()) {
                    if(out_arc.place() == "P" + std::to_string(rank)) {
                        blocking_transition = &t;
                        blocking_out_arc = &out_arc;
                        if(DEBUG) {
                            llvm::errs() << " ==> FOUND EQUAL BLOCKING TRANSITION\n";
                        }
                    }
                }
            }
        }

        if(!blocking_transition) {
            error_print("Could not find blocking transition for existing MPI_Rsend");
            return;
        }

        auto expr2 = to_marker_expression(ops.front(), blocking_transition->id());
        blocking_out_arc->expression().extend_queue(expr2.token().value());
        existing_transition->add_ast_expr("P" + std::to_string(rank), op.expr);
        blocking_transition->add_ast_expr("P" + std::to_string(rank), op.expr);
    }
}

// TODO comment
void CPNBuilder::mpi_isend(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const
{
    const bool DEBUG = CPNBUILDER_GENERAL_DEBUG;

    // auto type = op.args[2].second.get_as_integer().value();
    auto src = op.rank.get_as_integer().value();
    auto dest = op.args[3].second.get_as_integer().value();
    auto tag = op.args[4].second.get_as_integer().value();
    auto comm = op.args[5].second.get_as_integer().value();
    auto request = op.args[6].second.get_as_integer().value();

    std::unordered_map<std::string, CPNExpression> guard_expression;
    guard_expression.insert(std::make_pair("s", CPNExpression(CPNToken(TokenKind::IsendMarker, -1, {src, dest, tag, comm, request}))));
    auto guard = CPNGuard(CPNGuardKind::Isend, guard_expression);

    // Check if there already is a transtition with the same guard for this process
    // TODO make this a function?
    CPNTransition* existing_transition = nullptr;
    for(auto &t : net.transitions()) {
        if(t.guard().value().kind() == CPNGuardKind::Isend && t.guard().value().to_string() == guard.to_string()) {
            for(const auto &in_arc : t.in_arcs()) {
                if(in_arc.place() == "P" + std::to_string(rank)) {
                    existing_transition = &t;
                    if(DEBUG) {
                        llvm::errs() << " ==> FOUND EQUAL ISEND TRANSITION\n";
                    }
                    break;
                }
            }
        }
    }

    if(!existing_transition) {
        auto transition = CPNTransition("MPI_Isend", guard, "P" + std::to_string(rank), op.expr, request);
        net.add_transition(transition);

        std::string place_name = "P" + std::to_string(rank);
        CPNExpression expr = CPNExpression(CPNToken(TokenKind::MessageMarker, transition.id(), { src, dest, tag, comm}, request));
        net.add_in_arc(place_name, transition.name(), "s");
        net.add_out_arc(transition.name(), "MessageBuffer", expr);

        auto expr2 = to_marker_expression(ops.front(), transition.id());
        net.add_out_arc(transition.name(), place_name, expr2);
    } else {
        if(DEBUG) {
            llvm::errs() << "Adding to existing MPI_Isend transition\n";
        }

        std::string place_name = "P" + std::to_string(rank);
        for(auto &out_arc : existing_transition->out_arcs()) {
            if(out_arc.place() == place_name) {
                auto expr2 = to_marker_expression(ops.front(), existing_transition->id());
                out_arc.expression().extend_queue(expr2.token().value());
                existing_transition->add_ast_expr("P" + std::to_string(rank), op.expr);
                if(DEBUG) {
                    llvm::errs() << "Extending existing MPI_Isend transition\n";
                }
                break;
            }
        }
    }
}

// TODO comment
void CPNBuilder::mpi_wait(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const
{
    const bool DEBUG = CPNBUILDER_GENERAL_DEBUG;

    auto request = op.args[0].second.get_as_integer().value();

    std::unordered_map<std::string, CPNExpression> guard_expression;
    guard_expression.insert(std::make_pair("r", CPNExpression(CPNToken(TokenKind::RequestMarker, -1, { request }))));
    guard_expression.insert(std::make_pair("s", CPNExpression(CPNToken(TokenKind::WaitMarker, -1, { request }))));
    auto guard = CPNGuard(CPNGuardKind::Wait, guard_expression);

    // Check if there already is a transtition with the same guard for this process
    // TODO make this a function?
    CPNTransition* existing_transition = nullptr;
    for(auto &t : net.transitions()) {
        if(t.guard().value().kind() == CPNGuardKind::Wait && t.guard().value().to_string() == guard.to_string()) {
            for(const auto &in_arc : t.in_arcs()) {
                if(in_arc.place() == "P" + std::to_string(rank)) {
                    existing_transition = &t;
                    if(DEBUG) {
                        llvm::errs() << " ==> FOUND EQUAL WAIT TRANSITION\n";
                    }
                    break;
                }
            }
        }
    }

    if(!existing_transition) {
        auto transition = CPNTransition("MPI_Wait", guard, "P" + std::to_string(rank), op.expr);
        net.add_transition(transition);

        std::string place_name = "P" + std::to_string(rank);
        net.add_in_arc(place_name, transition.name(), "s");
        net.add_in_arc("Signals", transition.name(), "r");

        auto expr = to_marker_expression(ops.front(), transition.id());
        net.add_out_arc(transition.name(), place_name, expr);
    } else {
        if(DEBUG) {
            llvm::errs() << "Adding to existing MPI_Wait transition\n";
        }

        std::string place_name = "P" + std::to_string(rank);
        for(auto &out_arc : existing_transition->out_arcs()) {
            if(out_arc.place() == place_name) {
                auto expr2 = to_marker_expression(ops.front(), existing_transition->id());
                out_arc.expression().extend_queue(expr2.token().value());
                existing_transition->add_ast_expr("P" + std::to_string(rank), op.expr);
                if(DEBUG) {
                    llvm::errs() << "Extending existing MPI_Wait transition\n";
                }
                break;
            }
        }
    }

    // PART II
    // TODO WIP
    // This is a special case for non-blocking collectives
    // A second wait transition is creeated that matches against the type of request tokens created by non-blocking collectives
    // TODO maybe solve this by also having Or expressions in GuardExpressions? might be faster (but more complex)
    std::unordered_map<std::string, CPNExpression> guard_expression2;
    guard_expression2.insert(std::make_pair("r", CPNExpression(CPNToken(TokenKind::RequestMarker, -1, { request, rank }))));
    guard_expression2.insert(std::make_pair("s", CPNExpression(CPNToken(TokenKind::WaitMarker, -1, { request }))));
    auto guard2 = CPNGuard(CPNGuardKind::Wait, guard_expression2);

    // Check if there already is a transtition with the same guard for this process
    // TODO make this a function?
    CPNTransition* existing_transition2 = nullptr;
    for(auto &t : net.transitions()) {
        if(t.guard().value().kind() == CPNGuardKind::Wait && t.guard().value().to_string() == guard2.to_string()) {
            for(const auto &in_arc : t.in_arcs()) {
                if(in_arc.place() == "P" + std::to_string(rank)) {
                    existing_transition2 = &t;
                    if(DEBUG) {
                        llvm::errs() << " ==> FOUND EQUAL WAIT TRANSITION\n";
                    }
                    break;
                }
            }
        }
    }

    if(!existing_transition2) {
        auto transition = CPNTransition("MPI_Wait", guard2, "P" + std::to_string(rank), op.expr);
        net.add_transition(transition);

        std::string place_name = "P" + std::to_string(rank);
        net.add_in_arc(place_name, transition.name(), "s");
        net.add_in_arc("Signals", transition.name(), "r");

        auto expr = to_marker_expression(ops.front(), transition.id());
        net.add_out_arc(transition.name(), place_name, expr);
    } else {
        if(DEBUG) {
            llvm::errs() << "Adding to existing MPI_Wait transition\n";
        }

        std::string place_name = "P" + std::to_string(rank);
        for(auto &out_arc : existing_transition2->out_arcs()) {
            if(out_arc.place() == place_name) {
                auto expr2 = to_marker_expression(ops.front(), existing_transition2->id());
                out_arc.expression().extend_queue(expr2.token().value());
                existing_transition2->add_ast_expr("P" + std::to_string(rank), op.expr);
                if(DEBUG) {
                    llvm::errs() << "Extending existing MPI_Wait transition\n";
                }
                break;
            }
        }
    }

}


void CPNBuilder::mpi_irecv(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const
{
    const bool DEBUG = CPNBUILDER_GENERAL_DEBUG;
    // auto type = op.args[2].second.get_as_integer().value();
    auto src = op.args[3].second.get_as_integer().value();
    auto dest = op.rank.get_as_integer().value();
    auto tag = op.args[4].second.get_as_integer().value();
    auto comm = op.args[5].second.get_as_integer().value();
    auto request = op.args[6].second.get_as_integer().value();

    std::unordered_map<std::string, CPNExpression> guard_expression;
    guard_expression.insert(std::make_pair("r", CPNExpression(CPNToken(TokenKind::IrecvMarker, -1, {src, dest, tag, comm, request}))));
    auto guard = CPNGuard(CPNGuardKind::Irecv, guard_expression);

    // check if there already is a transtition with the same guard for this process
    // TODO make this a function?
    CPNTransition* existing_transition = nullptr;
    for(auto &t : net.transitions()) {
        if(t.guard().value().kind() == CPNGuardKind::Irecv && t.guard().value().to_string() == guard.to_string()) {
            for(const auto &in_arc : t.in_arcs()) {
                if(in_arc.place() == "P" + std::to_string(rank)) {
                    existing_transition = &t;
                    if(DEBUG) {
                        llvm::errs() << " ==> FOUND EQUAL IRECV TRANSITION\n";
                    }
                    break;
                }
            }
        }
    }

    if(!existing_transition) {
        auto transition = CPNTransition("MPI_Irecv", guard, "P" + std::to_string(rank), op.expr, request);
        net.add_transition(transition);

        std::string place_name = "P" + std::to_string(rank);
        net.add_in_arc(place_name, transition.name(), "r");

        auto expr2 = to_marker_expression(ops.front(), transition.id());
        net.add_out_arc(transition.name(), place_name, expr2);

        auto expr4 = CPNExpression(CPNToken(TokenKind::IrecvMarker, transition.id(), { src, dest, tag, comm, request}));
        net.add_out_arc(transition.name(), "Signals", expr4);

        // Add a 'complete' transition that consume the MessageMarker as soon as it shows up and places 
        // the RequestMarker in the signal place to indicate that the non-blocking receive has finished
        std::unordered_map<std::string, CPNExpression> guard_expression2;
        guard_expression2.insert(std::make_pair("s", CPNExpression(CPNToken(TokenKind::MessageMarker, -1, { src, dest, tag, comm}))));
        guard_expression2.insert(std::make_pair("r", CPNExpression(CPNToken(TokenKind::IrecvMarker, -1, { src, dest, tag, comm, request}))));
        auto transition2 = CPNTransition("Complete", CPNGuard(CPNGuardKind::Complete, guard_expression2), "P" + std::to_string(rank), op.expr);
        net.add_transition(transition2);
        net.add_in_arc("MessageBuffer", transition2.name(), "s");
        net.add_in_arc("Signals", transition2.name(), "r");
        auto expr3 = CPNExpression(CPNToken(TokenKind::RequestMarker, -1, { request }));
        net.add_out_arc(transition2.name(), "Signals", expr3);

        // add continuation marker for blocking sends that might have created the message in the buffer
        // make this an Or expression because the continuemarker should only be created if the sendmarker was from a blocking  operation
        // Also add a potential RequestMarker as output in case the message was sent by a non-blocking send
        auto token = CPNToken(TokenKind::ContinueMarker, transition.id(), { src, dest, tag, comm}); 
        auto token2 = CPNToken(TokenKind::EmptyMarker, transition.id(), {});
        auto token3 = CPNToken(TokenKind::RequestMarker, transition.id(), {});
        auto expr = CPNExpression(ExpressionKind::Or, { token, token2, token3 });
        net.add_out_arc(transition2.name(), "Signals", expr);



    } else {
        if(DEBUG) {
            llvm::errs() << "Adding to existing MPI_Irecv transition\n";
        }
        
        std::string place_name = "P" + std::to_string(rank);
        for(auto &out_arc : existing_transition->out_arcs()) {
            if(out_arc.place() == place_name) {
                auto expr2 = to_marker_expression(ops.front(), existing_transition->id());
                out_arc.expression().extend_queue(expr2.token().value());
                existing_transition->add_ast_expr("P" + std::to_string(rank), op.expr);
                if(DEBUG) {
                    llvm::errs() << "Extending existing MPI_Irecv transition\n";
                }
                break;
            }
        }
    }
}


// TODO currently a basic copy of MPI_Barrier function
// TODO currently only checks for the same communicator and not other arguments missmatches
void CPNBuilder::mpi_allgather(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const
{
    const bool DEBUG = CPNBUILDER_GENERAL_DEBUG;

    auto comm = op.args[6].second.get_as_integer().value();

    std::unordered_map<std::string, CPNExpression> guard_expression;
    for(int i = 0; i < net.mpi_size(); i++) {
        guard_expression.insert(std::make_pair("b" + std::to_string(i), CPNExpression(CPNToken(TokenKind::AllgatherMarker, -1, {comm}))));
    }
    auto guard = CPNGuard(CPNGuardKind::Allgather, guard_expression);

    // check if there is already a allgather transition in the net
    // collective operations only get one transition in the whole net
    CPNTransition* existing_transition = nullptr;
    for(auto &t : net.transitions()) {
        if(t.guard().value().kind() == CPNGuardKind::Allgather && t.guard().value().to_string() == guard.to_string()) {
            existing_transition = &t;
        }
    }

    if(!existing_transition) {
        auto transition = CPNTransition("MPI_Allgather", guard, "P" + std::to_string(rank), op.expr);
        net.add_transition(transition);

        std::string place_name = "P" + std::to_string(rank);
        CPNExpression expr = CPNExpression(CPNToken(TokenKind::AllgatherMarker, transition.id(), {comm}));
        net.add_in_arc(place_name, transition.name(), "b" + std::to_string(rank));
        auto expr2 = to_marker_expression(ops.front(), transition.id());
        net.add_out_arc(transition.name(), place_name, expr2);
    } else {
        if(DEBUG) {
            llvm::errs() << rank << " adding to existing allgather\n";
        }
        bool arc_exists = false;
        for(auto &in_arc : existing_transition->in_arcs()) {        
            if(in_arc.place() == "P" + std::to_string(rank)) {
                arc_exists = true;
            }
        }
        
        if(!arc_exists) {
            if(DEBUG) {
                llvm::errs() << "Adding allgather arc from " << "P" + std::to_string(rank) << " to " << existing_transition->name() << "\n";
            }
            std::string place_name = "P" + std::to_string(rank);
            net.add_in_arc(place_name, existing_transition->name(), "b" + std::to_string(rank));
            auto expr2 = to_marker_expression(ops.front(), existing_transition->id());
            net.add_out_arc(existing_transition->name(), place_name, expr2);
            existing_transition->add_ast_expr("P" + std::to_string(rank), op.expr);
        } else {
            if(DEBUG) {
                llvm::errs() << rank << " adding to queue of allgather out arc\n";
            }
            
            std::string place_name = "P" + std::to_string(rank);
            for(auto &out_arc : existing_transition->out_arcs()) {
                if(out_arc.place() == place_name) {
                    auto expr2 = to_marker_expression(ops.front(), existing_transition->id());
                    out_arc.expression().extend_queue(expr2.token().value());
                    existing_transition->add_ast_expr("P" + std::to_string(rank), op.expr);
                    if(DEBUG) {
                        llvm::errs() << "Extending existing MPI_Allgather transition\n";
                    }
                    break;
                }
            }

        }
    }

}

// TODO currently a basic copy of MPI_Barrier function
// TODO currently only checks for the same communicator and not other arguments missmatches
void CPNBuilder::mpi_allreduce(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const
{
    const bool DEBUG = CPNBUILDER_GENERAL_DEBUG;

    auto comm = op.args[5].second.get_as_integer().value();

    std::unordered_map<std::string, CPNExpression> guard_expression;
    for(int i = 0; i < net.mpi_size(); i++) {
        guard_expression.insert(std::make_pair("b" + std::to_string(i), CPNExpression(CPNToken(TokenKind::AllreduceMarker, -1, {comm}))));
    }
    auto guard = CPNGuard(CPNGuardKind::Allreduce, guard_expression);

    // check if there is already a allreduce transition in the net
    // collective operations only get one transition in the whole net
    CPNTransition* existing_transition = nullptr;
    for(auto &t : net.transitions()) {
        if(t.guard().value().kind() == CPNGuardKind::Allreduce && t.guard().value().to_string() == guard.to_string()) {
            existing_transition = &t;
        }
    }

    if(!existing_transition) {
        auto transition = CPNTransition("MPI_Allreduce", guard, "P" + std::to_string(rank), op.expr);
        net.add_transition(transition);

        std::string place_name = "P" + std::to_string(rank);
        CPNExpression expr = CPNExpression(CPNToken(TokenKind::AllreduceMarker, transition.id(), {comm}));
        net.add_in_arc(place_name, transition.name(), "b" + std::to_string(rank));
        auto expr2 = to_marker_expression(ops.front(), transition.id());
        net.add_out_arc(transition.name(), place_name, expr2);
    } else {
        if(DEBUG) {
            llvm::errs() << rank << " adding to existing allreduce\n";
        }
        bool arc_exists = false;
        for(auto &in_arc : existing_transition->in_arcs()) {        
            if(in_arc.place() == "P" + std::to_string(rank)) {
                arc_exists = true;
            }
        }
        
        if(!arc_exists) {
            if(DEBUG) {
                llvm::errs() << "Adding allreduce arc from " << "P" + std::to_string(rank) << " to " << existing_transition->name() << "\n";
            }
            std::string place_name = "P" + std::to_string(rank);
            net.add_in_arc(place_name, existing_transition->name(), "b" + std::to_string(rank));
            auto expr2 = to_marker_expression(ops.front(), existing_transition->id());
            net.add_out_arc(existing_transition->name(), place_name, expr2);
            existing_transition->add_ast_expr("P" + std::to_string(rank), op.expr);
        } else {
            if(DEBUG) {
                llvm::errs() << rank << " adding to queue of allreduce out arc\n";
            }
            
            std::string place_name = "P" + std::to_string(rank);
            for(auto &out_arc : existing_transition->out_arcs()) {
                if(out_arc.place() == place_name) {
                    auto expr2 = to_marker_expression(ops.front(), existing_transition->id());
                    out_arc.expression().extend_queue(expr2.token().value());
                    existing_transition->add_ast_expr("P" + std::to_string(rank), op.expr);
                    if(DEBUG) {
                        llvm::errs() << "Extending existing MPI_Allreduce transition\n";
                    }
                    break;
                }
            }

        }
    }

}

// TODO currently a basic copy of MPI_Barrier function
// TODO currently only checks for the same communicator and not other arguments missmatches
void CPNBuilder::mpi_alltoall(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const
{
    const bool DEBUG = CPNBUILDER_GENERAL_DEBUG;

    auto comm = op.args[6].second.get_as_integer().value();

    std::unordered_map<std::string, CPNExpression> guard_expression;
    for(int i = 0; i < net.mpi_size(); i++) {
        guard_expression.insert(std::make_pair("b" + std::to_string(i), CPNExpression(CPNToken(TokenKind::AlltoallMarker, -1, {comm}))));
    }
    auto guard = CPNGuard(CPNGuardKind::Alltoall, guard_expression);

    // check if there is already a alltoall transition in the net
    // collective operations only get one transition in the whole net
    CPNTransition* existing_transition = nullptr;
    for(auto &t : net.transitions()) {
        if(t.guard().value().kind() == CPNGuardKind::Alltoall && t.guard().value().to_string() == guard.to_string()) {
            existing_transition = &t;
        }
    }

    if(!existing_transition) {
        auto transition = CPNTransition("MPI_Alltoall", guard, "P" + std::to_string(rank), op.expr);
        net.add_transition(transition);

        std::string place_name = "P" + std::to_string(rank);
        CPNExpression expr = CPNExpression(CPNToken(TokenKind::AlltoallMarker, transition.id(), {comm}));
        net.add_in_arc(place_name, transition.name(), "b" + std::to_string(rank));
        auto expr2 = to_marker_expression(ops.front(), transition.id());
        net.add_out_arc(transition.name(), place_name, expr2);
    } else {
        if(DEBUG) {
            llvm::errs() << rank << " adding to existing alltoall\n";
        }
        bool arc_exists = false;
        for(auto &in_arc : existing_transition->in_arcs()) {        
            if(in_arc.place() == "P" + std::to_string(rank)) {
                arc_exists = true;
            }
        }
        
        if(!arc_exists) {
            if(DEBUG) {
                llvm::errs() << "Adding alltoall arc from " << "P" + std::to_string(rank) << " to " << existing_transition->name() << "\n";
            }
            std::string place_name = "P" + std::to_string(rank);
            net.add_in_arc(place_name, existing_transition->name(), "b" + std::to_string(rank));
            auto expr2 = to_marker_expression(ops.front(), existing_transition->id());
            net.add_out_arc(existing_transition->name(), place_name, expr2);
            existing_transition->add_ast_expr("P" + std::to_string(rank), op.expr);
        } else {
            if(DEBUG) {
                llvm::errs() << rank << " adding to queue of alltoall out arc\n";
            }
            
            std::string place_name = "P" + std::to_string(rank);
            for(auto &out_arc : existing_transition->out_arcs()) {
                if(out_arc.place() == place_name) {
                    auto expr2 = to_marker_expression(ops.front(), existing_transition->id());
                    out_arc.expression().extend_queue(expr2.token().value());
                    existing_transition->add_ast_expr("P" + std::to_string(rank), op.expr);
                    if(DEBUG) {
                        llvm::errs() << "Extending existing MPI_Alltoall transition\n";
                    }
                    break;
                }
            }

        }
    }

}

void CPNBuilder::mpi_bcast(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const
{
    const bool DEBUG = CPNBUILDER_GENERAL_DEBUG;

    auto root = op.args[3].second.get_as_integer().value();
    auto comm = op.args[4].second.get_as_integer().value();

    std::unordered_map<std::string, CPNExpression> guard_expression;
    for(int i = 0; i < net.mpi_size(); i++) {
        guard_expression.insert(std::make_pair("b" + std::to_string(i), CPNExpression(CPNToken(TokenKind::BcastMarker, -1, {root, comm}))));
    }
    auto guard = CPNGuard(CPNGuardKind::Bcast, guard_expression);

    // check if there is already a bcast transition in the net
    // collective operations only get one transition in the whole net
    CPNTransition* existing_transition = nullptr;
    for(auto &t : net.transitions()) {
        if(t.guard().value().kind() == CPNGuardKind::Bcast && t.guard().value().to_string() == guard.to_string()) {
            existing_transition = &t;
        }
    }

    if(!existing_transition) {
        auto transition = CPNTransition("MPI_Bcast", guard, "P" + std::to_string(rank), op.expr);
        net.add_transition(transition);

        std::string place_name = "P" + std::to_string(rank);
        CPNExpression expr = CPNExpression(CPNToken(TokenKind::BcastMarker, transition.id(), {comm}));
        net.add_in_arc(place_name, transition.name(), "b" + std::to_string(rank));
        auto expr2 = to_marker_expression(ops.front(), transition.id());
        net.add_out_arc(transition.name(), place_name, expr2);
    } else {
        if(DEBUG) {
            llvm::errs() << rank << " adding to existing bcast\n";
        }
        bool arc_exists = false;
        for(auto &in_arc : existing_transition->in_arcs()) {        
            if(in_arc.place() == "P" + std::to_string(rank)) {
                arc_exists = true;
            }
        }
        
        if(!arc_exists) {
            if(DEBUG) {
                llvm::errs() << "Adding bcast arc from " << "P" + std::to_string(rank) << " to " << existing_transition->name() << "\n";
            }
            std::string place_name = "P" + std::to_string(rank);
            net.add_in_arc(place_name, existing_transition->name(), "b" + std::to_string(rank));
            auto expr2 = to_marker_expression(ops.front(), existing_transition->id());
            net.add_out_arc(existing_transition->name(), place_name, expr2);
            existing_transition->add_ast_expr("P" + std::to_string(rank), op.expr);
        } else {
            if(DEBUG) {
                llvm::errs() << rank << " adding to queue of bcast out arc\n";
            }
            
            std::string place_name = "P" + std::to_string(rank);
            for(auto &out_arc : existing_transition->out_arcs()) {
                if(out_arc.place() == place_name) {
                    auto expr2 = to_marker_expression(ops.front(), existing_transition->id());
                    out_arc.expression().extend_queue(expr2.token().value());
                    existing_transition->add_ast_expr("P" + std::to_string(rank), op.expr);
                    if(DEBUG) {
                        llvm::errs() << "Extending existing MPI_Bcast transition\n";
                    }
                    break;
                }
            }

        }
    }
}

void CPNBuilder::mpi_gather(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const
{
    const bool DEBUG = CPNBUILDER_GENERAL_DEBUG;

    auto root = op.args[6].second.get_as_integer().value();
    auto comm = op.args[7].second.get_as_integer().value();

    std::unordered_map<std::string, CPNExpression> guard_expression;
    for(int i = 0; i < net.mpi_size(); i++) {
        guard_expression.insert(std::make_pair("b" + std::to_string(i), CPNExpression(CPNToken(TokenKind::GatherMarker, -1, {root, comm}))));
    }
    auto guard = CPNGuard(CPNGuardKind::Gather, guard_expression);

    // check if there is already a gather transition in the net
    // collective operations only get one transition in the whole net
    CPNTransition* existing_transition = nullptr;
    for(auto &t : net.transitions()) {
        if(t.guard().value().kind() == CPNGuardKind::Gather && t.guard().value().to_string() == guard.to_string()) {
            existing_transition = &t;
        }
    }

    if(!existing_transition) {
        auto transition = CPNTransition("MPI_Gather", guard, "P" + std::to_string(rank), op.expr);
        net.add_transition(transition);

        std::string place_name = "P" + std::to_string(rank);
        CPNExpression expr = CPNExpression(CPNToken(TokenKind::GatherMarker, transition.id(), {comm}));
        net.add_in_arc(place_name, transition.name(), "b" + std::to_string(rank));
        auto expr2 = to_marker_expression(ops.front(), transition.id());
        net.add_out_arc(transition.name(), place_name, expr2);
    } else {
        if(DEBUG) {
            llvm::errs() << rank << " adding to existing gather\n";
        }
        bool arc_exists = false;
        for(auto &in_arc : existing_transition->in_arcs()) {        
            if(in_arc.place() == "P" + std::to_string(rank)) {
                arc_exists = true;
            }
        }
        
        if(!arc_exists) {
            if(DEBUG) {
                llvm::errs() << "Adding gather arc from " << "P" + std::to_string(rank) << " to " << existing_transition->name() << "\n";
            }
            std::string place_name = "P" + std::to_string(rank);
            net.add_in_arc(place_name, existing_transition->name(), "b" + std::to_string(rank));
            auto expr2 = to_marker_expression(ops.front(), existing_transition->id());
            net.add_out_arc(existing_transition->name(), place_name, expr2);
            existing_transition->add_ast_expr("P" + std::to_string(rank), op.expr);
        } else {
            if(DEBUG) {
                llvm::errs() << rank << " adding to queue of gather out arc\n";
            }
            
            std::string place_name = "P" + std::to_string(rank);
            for(auto &out_arc : existing_transition->out_arcs()) {
                if(out_arc.place() == place_name) {
                    auto expr2 = to_marker_expression(ops.front(), existing_transition->id());
                    out_arc.expression().extend_queue(expr2.token().value());
                    existing_transition->add_ast_expr("P" + std::to_string(rank), op.expr);
                    if(DEBUG) {
                        llvm::errs() << "Extending existing MPI_Gather transition\n";
                    }
                    break;
                }
            }

        }
    }
}

// TODO maybe include OP in matching
void CPNBuilder::mpi_reduce(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const
{
    const bool DEBUG = CPNBUILDER_GENERAL_DEBUG;

    auto root = op.args[5].second.get_as_integer().value();
    auto comm = op.args[6].second.get_as_integer().value();

    std::unordered_map<std::string, CPNExpression> guard_expression;
    for(int i = 0; i < net.mpi_size(); i++) {
        guard_expression.insert(std::make_pair("b" + std::to_string(i), CPNExpression(CPNToken(TokenKind::ReduceMarker, -1, {root, comm}))));
    }
    auto guard = CPNGuard(CPNGuardKind::Reduce, guard_expression);

    // check if there is already a Reduce transition in the net
    // collective operations only get one transition in the whole net
    CPNTransition* existing_transition = nullptr;
    for(auto &t : net.transitions()) {
        if(t.guard().value().kind() == CPNGuardKind::Reduce && t.guard().value().to_string() == guard.to_string()) {
            existing_transition = &t;
        }
    }

    if(!existing_transition) {
        auto transition = CPNTransition("MPI_Reduce", guard, "P" + std::to_string(rank), op.expr);
        net.add_transition(transition);

        std::string place_name = "P" + std::to_string(rank);
        CPNExpression expr = CPNExpression(CPNToken(TokenKind::ReduceMarker, transition.id(), {comm}));
        net.add_in_arc(place_name, transition.name(), "b" + std::to_string(rank));
        auto expr2 = to_marker_expression(ops.front(), transition.id());
        net.add_out_arc(transition.name(), place_name, expr2);
    } else {
        if(DEBUG) {
            llvm::errs() << rank << " adding to existing reduce\n";
        }
        bool arc_exists = false;
        for(auto &in_arc : existing_transition->in_arcs()) {        
            if(in_arc.place() == "P" + std::to_string(rank)) {
                arc_exists = true;
            }
        }
        
        if(!arc_exists) {
            if(DEBUG) {
                llvm::errs() << "Adding reduce arc from " << "P" + std::to_string(rank) << " to " << existing_transition->name() << "\n";
            }
            std::string place_name = "P" + std::to_string(rank);
            net.add_in_arc(place_name, existing_transition->name(), "b" + std::to_string(rank));
            auto expr2 = to_marker_expression(ops.front(), existing_transition->id());
            net.add_out_arc(existing_transition->name(), place_name, expr2);
            existing_transition->add_ast_expr("P" + std::to_string(rank), op.expr);
        } else {
            if(DEBUG) {
                llvm::errs() << rank << " adding to queue of reduce out arc\n";
            }
            
            std::string place_name = "P" + std::to_string(rank);
            for(auto &out_arc : existing_transition->out_arcs()) {
                if(out_arc.place() == place_name) {
                    auto expr2 = to_marker_expression(ops.front(), existing_transition->id());
                    out_arc.expression().extend_queue(expr2.token().value());
                    existing_transition->add_ast_expr("P" + std::to_string(rank), op.expr);
                    if(DEBUG) {
                        llvm::errs() << "Extending existing MPI_Reduce transition\n";
                    }
                    break;
                }
            }

        }
    }
}

// TODO maybe include OP in matching
void CPNBuilder::mpi_scan(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const
{
    const bool DEBUG = CPNBUILDER_GENERAL_DEBUG;

    auto comm = op.args[5].second.get_as_integer().value();

    std::unordered_map<std::string, CPNExpression> guard_expression;
    for(int i = 0; i < net.mpi_size(); i++) {
        guard_expression.insert(std::make_pair("b" + std::to_string(i), CPNExpression(CPNToken(TokenKind::ScanMarker, -1, {comm}))));
    }
    auto guard = CPNGuard(CPNGuardKind::Scan, guard_expression);

    // check if there is already a Scan transition in the net
    // collective operations only get one transition in the whole net
    CPNTransition* existing_transition = nullptr;
    for(auto &t : net.transitions()) {
        if(t.guard().value().kind() == CPNGuardKind::Scan && t.guard().value().to_string() == guard.to_string()) {
            existing_transition = &t;
        }
    }

    if(!existing_transition) {
        auto transition = CPNTransition("MPI_Scan", guard, "P" + std::to_string(rank), op.expr);
        net.add_transition(transition);

        std::string place_name = "P" + std::to_string(rank);
        CPNExpression expr = CPNExpression(CPNToken(TokenKind::ScanMarker, transition.id(), {comm}));
        net.add_in_arc(place_name, transition.name(), "b" + std::to_string(rank));
        auto expr2 = to_marker_expression(ops.front(), transition.id());
        net.add_out_arc(transition.name(), place_name, expr2);
    } else {
        if(DEBUG) {
            llvm::errs() << rank << " adding to existing scan\n";
        }
        bool arc_exists = false;
        for(auto &in_arc : existing_transition->in_arcs()) {        
            if(in_arc.place() == "P" + std::to_string(rank)) {
                arc_exists = true;
            }
        }
        
        if(!arc_exists) {
            if(DEBUG) {
                llvm::errs() << "Adding scan arc from " << "P" + std::to_string(rank) << " to " << existing_transition->name() << "\n";
            }
            std::string place_name = "P" + std::to_string(rank);
            net.add_in_arc(place_name, existing_transition->name(), "b" + std::to_string(rank));
            auto expr2 = to_marker_expression(ops.front(), existing_transition->id());
            net.add_out_arc(existing_transition->name(), place_name, expr2);
            existing_transition->add_ast_expr("P" + std::to_string(rank), op.expr);
        } else {
            if(DEBUG) {
                llvm::errs() << rank << " adding to queue of scan out arc\n";
            }
            
            std::string place_name = "P" + std::to_string(rank);
            for(auto &out_arc : existing_transition->out_arcs()) {
                if(out_arc.place() == place_name) {
                    auto expr2 = to_marker_expression(ops.front(), existing_transition->id());
                    out_arc.expression().extend_queue(expr2.token().value());
                    existing_transition->add_ast_expr("P" + std::to_string(rank), op.expr);
                    if(DEBUG) {
                        llvm::errs() << "Extending existing MPI_Scan transition\n";
                    }
                    break;
                }
            }

        }
    }
}

// TODO maybe include OP in matching
void CPNBuilder::mpi_exscan(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const
{
    const bool DEBUG = CPNBUILDER_GENERAL_DEBUG;

    auto comm = op.args[5].second.get_as_integer().value();

    std::unordered_map<std::string, CPNExpression> guard_expression;
    for(int i = 0; i < net.mpi_size(); i++) {
        guard_expression.insert(std::make_pair("b" + std::to_string(i), CPNExpression(CPNToken(TokenKind::ExscanMarker, -1, {comm}))));
    }
    auto guard = CPNGuard(CPNGuardKind::Exscan, guard_expression);

    // check if there is already a Exscan transition in the net
    // collective operations only get one transition in the whole net
    CPNTransition* existing_transition = nullptr;
    for(auto &t : net.transitions()) {
        if(t.guard().value().kind() == CPNGuardKind::Exscan && t.guard().value().to_string() == guard.to_string()) {
            existing_transition = &t;
        }
    }

    if(!existing_transition) {
        auto transition = CPNTransition("MPI_Exscan", guard, "P" + std::to_string(rank), op.expr);
        net.add_transition(transition);

        std::string place_name = "P" + std::to_string(rank);
        CPNExpression expr = CPNExpression(CPNToken(TokenKind::ExscanMarker, transition.id(), {comm}));
        net.add_in_arc(place_name, transition.name(), "b" + std::to_string(rank));
        auto expr2 = to_marker_expression(ops.front(), transition.id());
        net.add_out_arc(transition.name(), place_name, expr2);
    } else {
        if(DEBUG) {
            llvm::errs() << rank << " adding to existing exscan\n";
        }
        bool arc_exists = false;
        for(auto &in_arc : existing_transition->in_arcs()) {        
            if(in_arc.place() == "P" + std::to_string(rank)) {
                arc_exists = true;
            }
        }
        
        if(!arc_exists) {
            if(DEBUG) {
                llvm::errs() << "Adding exscan arc from " << "P" + std::to_string(rank) << " to " << existing_transition->name() << "\n";
            }
            std::string place_name = "P" + std::to_string(rank);
            net.add_in_arc(place_name, existing_transition->name(), "b" + std::to_string(rank));
            auto expr2 = to_marker_expression(ops.front(), existing_transition->id());
            net.add_out_arc(existing_transition->name(), place_name, expr2);
            existing_transition->add_ast_expr("P" + std::to_string(rank), op.expr);
        } else {
            if(DEBUG) {
                llvm::errs() << rank << " adding to queue of exscan out arc\n";
            }
            
            std::string place_name = "P" + std::to_string(rank);
            for(auto &out_arc : existing_transition->out_arcs()) {
                if(out_arc.place() == place_name) {
                    auto expr2 = to_marker_expression(ops.front(), existing_transition->id());
                    out_arc.expression().extend_queue(expr2.token().value());
                    existing_transition->add_ast_expr("P" + std::to_string(rank), op.expr);
                    if(DEBUG) {
                        llvm::errs() << "Extending existing MPI_Exscan transition\n";
                    }
                    break;
                }
            }

        }
    }
}

void CPNBuilder::mpi_scatter(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const
{
    const bool DEBUG = CPNBUILDER_GENERAL_DEBUG;

    auto root = op.args[6].second.get_as_integer().value();
    auto comm = op.args[7].second.get_as_integer().value();

    std::unordered_map<std::string, CPNExpression> guard_expression;
    for(int i = 0; i < net.mpi_size(); i++) {
        guard_expression.insert(std::make_pair("b" + std::to_string(i), CPNExpression(CPNToken(TokenKind::ScatterMarker, -1, {root, comm}))));
    }
    auto guard = CPNGuard(CPNGuardKind::Scatter, guard_expression);

    // check if there is already a Scatter transition in the net
    // collective operations only get one transition in the whole net
    CPNTransition* existing_transition = nullptr;
    for(auto &t : net.transitions()) {
        if(t.guard().value().kind() == CPNGuardKind::Scatter && t.guard().value().to_string() == guard.to_string()) {
            existing_transition = &t;
        }
    }

    if(!existing_transition) {
        auto transition = CPNTransition("MPI_Scatter", guard, "P" + std::to_string(rank), op.expr);
        net.add_transition(transition);

        std::string place_name = "P" + std::to_string(rank);
        CPNExpression expr = CPNExpression(CPNToken(TokenKind::ScatterMarker, transition.id(), {comm}));
        net.add_in_arc(place_name, transition.name(), "b" + std::to_string(rank));
        auto expr2 = to_marker_expression(ops.front(), transition.id());
        net.add_out_arc(transition.name(), place_name, expr2);
    } else {
        if(DEBUG) {
            llvm::errs() << rank << " adding to existing scatter\n";
        }
        bool arc_exists = false;
        for(auto &in_arc : existing_transition->in_arcs()) {        
            if(in_arc.place() == "P" + std::to_string(rank)) {
                arc_exists = true;
            }
        }
        
        if(!arc_exists) {
            if(DEBUG) {
                llvm::errs() << "Adding scatter arc from " << "P" + std::to_string(rank) << " to " << existing_transition->name() << "\n";
            }
            std::string place_name = "P" + std::to_string(rank);
            net.add_in_arc(place_name, existing_transition->name(), "b" + std::to_string(rank));
            auto expr2 = to_marker_expression(ops.front(), existing_transition->id());
            net.add_out_arc(existing_transition->name(), place_name, expr2);
            existing_transition->add_ast_expr("P" + std::to_string(rank), op.expr);
        } else {
            if(DEBUG) {
                llvm::errs() << rank << " adding to queue of scatter out arc\n";
            }
            
            std::string place_name = "P" + std::to_string(rank);
            for(auto &out_arc : existing_transition->out_arcs()) {
                if(out_arc.place() == place_name) {
                    auto expr2 = to_marker_expression(ops.front(), existing_transition->id());
                    out_arc.expression().extend_queue(expr2.token().value());
                    existing_transition->add_ast_expr("P" + std::to_string(rank), op.expr);
                    if(DEBUG) {
                        llvm::errs() << "Extending existing MPI_Scatter transition\n";
                    }
                    break;
                }
            }

        }
    }
}

void CPNBuilder::mpi_ibarrier(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const
{
    const bool DEBUG = CPNBUILDER_GENERAL_DEBUG;

    auto comm = op.args[0].second.get_as_integer().value();
    auto req = op.args[1].second.get_as_integer().value();

    // TODO make this whole first part its own function?
    // Since the collective part of a non-blocking collective happen as the completion operation we need some transition that in the background checks
    // when all ranks have participated in this operation (this is handled as a shared transition between all ranks similar to the regular collective operations)
    std::unordered_map<std::string, CPNExpression> guard_expression;
    for(int i = 0; i < net.mpi_size(); i++) {
        guard_expression.insert(std::make_pair("b" + std::to_string(i), CPNExpression(CPNToken(TokenKind::IbarrierCompleteMarker, -1, {comm, i}))));
    }
    auto guard = CPNGuard(CPNGuardKind::IbarrierComplete, guard_expression);

    // check if there is already a IbarrierComplete transition in the net
    // collective operations only get one transition in the whole net
    CPNTransition* existing_transition = nullptr;
    for(auto &t : net.transitions()) {
        if(t.guard().value().kind() == CPNGuardKind::IbarrierComplete && t.guard().value().to_string() == guard.to_string()) {
            existing_transition = &t;
        }
    }

    if(!existing_transition) {
        auto transition = CPNTransition("IbarrierComplete", guard, "Signals", op.expr);
        net.add_transition(transition);

        CPNExpression expr = CPNExpression(CPNToken(TokenKind::IbarrierCompleteMarker, transition.id(), {comm, req, rank}));
        net.add_in_arc("Signals", transition.name(), "b" + std::to_string(rank));
        auto expr2 = CPNExpression(CPNToken(TokenKind::RequestMarker, transition.id(), {req, rank}));
        net.add_out_arc(transition.name(), "Signals", expr2);
    // TODO check if I can just get rid of the stuff below
    } else {
        if(DEBUG) {
            llvm::errs() << rank << " adding to existing IbarrierComplete\n";
        }
        bool arc_exists = false;
        for(auto &in_arc : existing_transition->in_arcs()) {        
            if(in_arc.binding() == "b" + std::to_string(rank)) {
                arc_exists = true;
            }
        }

        if(!arc_exists) {
            if(DEBUG) {
                llvm::errs() << "Adding IbarrierComplete arc from " << "Signals" << " to " << existing_transition->name() << "\n";
            }
            net.add_in_arc("Signals", existing_transition->name(), "b" + std::to_string(rank));
            auto expr2 = CPNExpression(CPNToken(TokenKind::RequestMarker, existing_transition->id(), {req, rank}));
            net.add_out_arc(existing_transition->name(), "Signals", expr2);
            existing_transition->add_ast_expr("P" + std::to_string(rank), op.expr);
        } else {
            if(DEBUG) {
                // llvm::errs() << rank << " adding to queue of IbarrierComplete out arc\n";
                warning_print("Hit a special case in IbarrierComplete transition creation. Check if there is something needed here or if this works like this");
            }
            //TODO check if this is needed; should not be necessary because the out_arcs should not need to change for a second identical Ibarrier call

            // for(auto &out_arc : existing_transition->out_arcs()) {
            //     if(out_arc.place() == place_name) {
            //         auto expr2 = to_marker_expression(ops.front(), existing_transition->id());
            //         out_arc.expression().extend_queue(expr2.token().value());
            //         existing_transition->add_ast_expr("P" + std::to_string(rank), op.expr);
            //         if(DEBUG) {
            //             llvm::errs() << "Extending existing MPI_Barrier transition\n";
            //         }
            //         break;
            //     }
            // }

        }
    }

    // PART II
    // This part now creates the actual Ibarrier transition for the rank
    // works the same as an non-blocking operation

    std::unordered_map<std::string, CPNExpression> guard_expression2;
    guard_expression2.insert(std::make_pair("s", CPNExpression(CPNToken(TokenKind::IbarrierMarker, -1, {comm, req}))));
    auto guard2 = CPNGuard(CPNGuardKind::Ibarrier, guard_expression2);

    // Check if there already is a transtition with the same guard for this process
    // TODO make this a function?
    CPNTransition* existing_transition2 = nullptr;
    for(auto &t : net.transitions()) {
        if(t.guard().value().kind() == CPNGuardKind::Ibarrier && t.guard().value().to_string() == guard.to_string()) {
            for(const auto &in_arc : t.in_arcs()) {
                if(in_arc.place() == "P" + std::to_string(rank)) {
                    existing_transition2 = &t;
                    if(DEBUG) {
                        llvm::errs() << " ==> FOUND EQUAL Ibarrier TRANSITION\n";
                    }
                    break;
                }
            }
        }
    }

    if(!existing_transition2) {
        auto transition = CPNTransition("MPI_Ibarrier", guard2, "P" + std::to_string(rank), op.expr, req);
        net.add_transition(transition);

        std::string place_name = "P" + std::to_string(rank);
        CPNExpression expr = CPNExpression(CPNToken(TokenKind::IbarrierCompleteMarker, transition.id(), { comm, rank}));
        net.add_in_arc(place_name, transition.name(), "s");
        net.add_out_arc(transition.name(), "Signals", expr);

        auto expr2 = to_marker_expression(ops.front(), transition.id());
        net.add_out_arc(transition.name(), place_name, expr2);
    } else {
        if(DEBUG) {
            llvm::errs() << "Adding to existing MPI_Ibarrier transition\n";
        }

        std::string place_name = "P" + std::to_string(rank);
        for(auto &out_arc : existing_transition2->out_arcs()) {
            if(out_arc.place() == place_name) {
                auto expr2 = to_marker_expression(ops.front(), existing_transition2->id());
                out_arc.expression().extend_queue(expr2.token().value());
                existing_transition2->add_ast_expr("P" + std::to_string(rank), op.expr);
                if(DEBUG) {
                    llvm::errs() << "Extending existing MPI_Ibarrier transition\n";
                }
                break;
            }
        }
    }

}

// TODO identical to MPI_Ibarrier currently
void CPNBuilder::mpi_iallgather(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const
{
    const bool DEBUG = CPNBUILDER_GENERAL_DEBUG;

    auto comm = op.args[6].second.get_as_integer().value();
    auto req = op.args[7].second.get_as_integer().value();

    // TODO make this whole first part its own function?
    // Since the collective part of a non-blocking collective happen as the completion operation we need some transition that in the background checks
    // when all ranks have participated in this operation (this is handled as a shared transition between all ranks similar to the regular collective operations)
    std::unordered_map<std::string, CPNExpression> guard_expression;
    for(int i = 0; i < net.mpi_size(); i++) {
        guard_expression.insert(std::make_pair("b" + std::to_string(i), CPNExpression(CPNToken(TokenKind::IallgatherCompleteMarker, -1, {comm, i}))));
    }
    auto guard = CPNGuard(CPNGuardKind::IallgatherComplete, guard_expression);

    // check if there is already a IallgatherComplete transition in the net
    // collective operations only get one transition in the whole net
    CPNTransition* existing_transition = nullptr;
    for(auto &t : net.transitions()) {
        if(t.guard().value().kind() == CPNGuardKind::IallgatherComplete && t.guard().value().to_string() == guard.to_string()) {
            existing_transition = &t;
        }
    }

    if(!existing_transition) {
        auto transition = CPNTransition("IallgatherComplete", guard, "Signals", op.expr);
        net.add_transition(transition);

        CPNExpression expr = CPNExpression(CPNToken(TokenKind::IallgatherCompleteMarker, transition.id(), {comm, req, rank}));
        net.add_in_arc("Signals", transition.name(), "b" + std::to_string(rank));
        auto expr2 = CPNExpression(CPNToken(TokenKind::RequestMarker, transition.id(), {req, rank}));
        net.add_out_arc(transition.name(), "Signals", expr2);
    // TODO check if I can just get rid of the stuff below
    } else {
        if(DEBUG) {
            llvm::errs() << rank << " adding to existing IallgatherComplete\n";
        }
        bool arc_exists = false;
        for(auto &in_arc : existing_transition->in_arcs()) {        
            if(in_arc.binding() == "b" + std::to_string(rank)) {
                arc_exists = true;
            }
        }

        if(!arc_exists) {
            if(DEBUG) {
                llvm::errs() << "Adding IallgatherComplete arc from " << "Signals" << " to " << existing_transition->name() << "\n";
            }
            net.add_in_arc("Signals", existing_transition->name(), "b" + std::to_string(rank));
            auto expr2 = CPNExpression(CPNToken(TokenKind::RequestMarker, existing_transition->id(), {req, rank}));
            net.add_out_arc(existing_transition->name(), "Signals", expr2);
            existing_transition->add_ast_expr("P" + std::to_string(rank), op.expr);
        } else {
            if(DEBUG) {
                // llvm::errs() << rank << " adding to queue of IallgatherComplete out arc\n";
                warning_print("Hit a special case in IallgatherComplete transition creation. Check if there is something needed here or if this works like this");
            }
            //TODO check if this is needed; should not be necessary because the out_arcs should not need to change for a second identical Iallgather call

            // for(auto &out_arc : existing_transition->out_arcs()) {
            //     if(out_arc.place() == place_name) {
            //         auto expr2 = to_marker_expression(ops.front(), existing_transition->id());
            //         out_arc.expression().extend_queue(expr2.token().value());
            //         existing_transition->add_ast_expr("P" + std::to_string(rank), op.expr);
            //         if(DEBUG) {
            //             llvm::errs() << "Extending existing MPI_Barrier transition\n";
            //         }
            //         break;
            //     }
            // }

        }
    }

    // PART II
    // This part now creates the actual Iallgather transition for the rank
    // works the same as an non-blocking operation

    std::unordered_map<std::string, CPNExpression> guard_expression2;
    guard_expression2.insert(std::make_pair("s", CPNExpression(CPNToken(TokenKind::IallgatherMarker, -1, {comm, req}))));
    auto guard2 = CPNGuard(CPNGuardKind::Iallgather, guard_expression2);

    // Check if there already is a transtition with the same guard for this process
    // TODO make this a function?
    CPNTransition* existing_transition2 = nullptr;
    for(auto &t : net.transitions()) {
        if(t.guard().value().kind() == CPNGuardKind::Iallgather && t.guard().value().to_string() == guard.to_string()) {
            for(const auto &in_arc : t.in_arcs()) {
                if(in_arc.place() == "P" + std::to_string(rank)) {
                    existing_transition2 = &t;
                    if(DEBUG) {
                        llvm::errs() << " ==> FOUND EQUAL Iallgather TRANSITION\n";
                    }
                    break;
                }
            }
        }
    }

    if(!existing_transition2) {
        auto transition = CPNTransition("MPI_Iallgather", guard2, "P" + std::to_string(rank), op.expr, req);
        net.add_transition(transition);

        std::string place_name = "P" + std::to_string(rank);
        CPNExpression expr = CPNExpression(CPNToken(TokenKind::IallgatherCompleteMarker, transition.id(), { comm, rank}));
        net.add_in_arc(place_name, transition.name(), "s");
        net.add_out_arc(transition.name(), "Signals", expr);

        auto expr2 = to_marker_expression(ops.front(), transition.id());
        net.add_out_arc(transition.name(), place_name, expr2);
    } else {
        if(DEBUG) {
            llvm::errs() << "Adding to existing MPI_Iallgather transition\n";
        }

        std::string place_name = "P" + std::to_string(rank);
        for(auto &out_arc : existing_transition2->out_arcs()) {
            if(out_arc.place() == place_name) {
                auto expr2 = to_marker_expression(ops.front(), existing_transition2->id());
                out_arc.expression().extend_queue(expr2.token().value());
                existing_transition2->add_ast_expr("P" + std::to_string(rank), op.expr);
                if(DEBUG) {
                    llvm::errs() << "Extending existing MPI_Iallgather transition\n";
                }
                break;
            }
        }
    }

}

// TODO identical to MPI_Ibarrier currently
// TODO add MPI_OP to matching
void CPNBuilder::mpi_iallreduce(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const
{
    const bool DEBUG = CPNBUILDER_GENERAL_DEBUG;

    auto comm = op.args[5].second.get_as_integer().value();
    auto req = op.args[6].second.get_as_integer().value();

    // TODO make this whole first part its own function?
    // Since the collective part of a non-blocking collective happen as the completion operation we need some transition that in the background checks
    // when all ranks have participated in this operation (this is handled as a shared transition between all ranks similar to the regular collective operations)
    std::unordered_map<std::string, CPNExpression> guard_expression;
    for(int i = 0; i < net.mpi_size(); i++) {
        guard_expression.insert(std::make_pair("b" + std::to_string(i), CPNExpression(CPNToken(TokenKind::IallreduceCompleteMarker, -1, {comm, i}))));
    }
    auto guard = CPNGuard(CPNGuardKind::IallreduceComplete, guard_expression);

    // check if there is already a IallreduceComplete transition in the net
    // collective operations only get one transition in the whole net
    CPNTransition* existing_transition = nullptr;
    for(auto &t : net.transitions()) {
        if(t.guard().value().kind() == CPNGuardKind::IallreduceComplete && t.guard().value().to_string() == guard.to_string()) {
            existing_transition = &t;
        }
    }

    if(!existing_transition) {
        auto transition = CPNTransition("IallreduceComplete", guard, "Signals", op.expr);
        net.add_transition(transition);

        net.add_in_arc("Signals", transition.name(), "b" + std::to_string(rank));
        auto expr2 = CPNExpression(CPNToken(TokenKind::RequestMarker, transition.id(), {req, rank}));
        net.add_out_arc(transition.name(), "Signals", expr2);
    // TODO check if I can just get rid of the stuff below
    } else {
        if(DEBUG) {
            llvm::errs() << rank << " adding to existing IallreduceComplete\n";
        }
        bool arc_exists = false;
        for(auto &in_arc : existing_transition->in_arcs()) {        
            if(in_arc.binding() == "b" + std::to_string(rank)) {
                arc_exists = true;
            }
        }

        if(!arc_exists) {
            if(DEBUG) {
                llvm::errs() << "Adding IallreduceComplete arc from " << "Signals" << " to " << existing_transition->name() << "\n";
            }
            net.add_in_arc("Signals", existing_transition->name(), "b" + std::to_string(rank));
            auto expr2 = CPNExpression(CPNToken(TokenKind::RequestMarker, existing_transition->id(), {req, rank}));
            net.add_out_arc(existing_transition->name(), "Signals", expr2);
            existing_transition->add_ast_expr("P" + std::to_string(rank), op.expr);
        } else {
            if(DEBUG) {
                // llvm::errs() << rank << " adding to queue of IallreduceComplete out arc\n";
                warning_print("Hit a special case in IallreduceComplete transition creation. Check if there is something needed here or if this works like this");
            }
            //TODO check if this is needed; should not be necessary because the out_arcs should not need to change for a second identical Iallreduce call

            // for(auto &out_arc : existing_transition->out_arcs()) {
            //     if(out_arc.place() == place_name) {
            //         auto expr2 = to_marker_expression(ops.front(), existing_transition->id());
            //         out_arc.expression().extend_queue(expr2.token().value());
            //         existing_transition->add_ast_expr("P" + std::to_string(rank), op.expr);
            //         if(DEBUG) {
            //             llvm::errs() << "Extending existing MPI_Barrier transition\n";
            //         }
            //         break;
            //     }
            // }

        }
    }

    // PART II
    // This part now creates the actual Iallreduce transition for the rank
    // works the same as an non-blocking operation

    std::unordered_map<std::string, CPNExpression> guard_expression2;
    guard_expression2.insert(std::make_pair("s", CPNExpression(CPNToken(TokenKind::IallreduceMarker, -1, {comm, req}))));
    auto guard2 = CPNGuard(CPNGuardKind::Iallreduce, guard_expression2);

    // Check if there already is a transtition with the same guard for this process
    // TODO make this a function?
    CPNTransition* existing_transition2 = nullptr;
    for(auto &t : net.transitions()) {
        if(t.guard().value().kind() == CPNGuardKind::Iallreduce && t.guard().value().to_string() == guard.to_string()) {
            for(const auto &in_arc : t.in_arcs()) {
                if(in_arc.place() == "P" + std::to_string(rank)) {
                    existing_transition2 = &t;
                    if(DEBUG) {
                        llvm::errs() << " ==> FOUND EQUAL Iallreduce TRANSITION\n";
                    }
                    break;
                }
            }
        }
    }

    if(!existing_transition2) {
        auto transition = CPNTransition("MPI_Iallreduce", guard2, "P" + std::to_string(rank), op.expr, req);
        net.add_transition(transition);

        std::string place_name = "P" + std::to_string(rank);
        CPNExpression expr = CPNExpression(CPNToken(TokenKind::IallreduceCompleteMarker, transition.id(), { comm, rank}));
        net.add_in_arc(place_name, transition.name(), "s");
        net.add_out_arc(transition.name(), "Signals", expr);

        auto expr2 = to_marker_expression(ops.front(), transition.id());
        net.add_out_arc(transition.name(), place_name, expr2);
    } else {
        if(DEBUG) {
            llvm::errs() << "Adding to existing MPI_Iallreduce transition\n";
        }

        std::string place_name = "P" + std::to_string(rank);
        for(auto &out_arc : existing_transition2->out_arcs()) {
            if(out_arc.place() == place_name) {
                auto expr2 = to_marker_expression(ops.front(), existing_transition2->id());
                out_arc.expression().extend_queue(expr2.token().value());
                existing_transition2->add_ast_expr("P" + std::to_string(rank), op.expr);
                if(DEBUG) {
                    llvm::errs() << "Extending existing MPI_Iallreduce transition\n";
                }
                break;
            }
        }
    }

}

// TODO identical to MPI_Ibarrier currently
void CPNBuilder::mpi_ialltoall(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const
{
    const bool DEBUG = CPNBUILDER_GENERAL_DEBUG;

    auto comm = op.args[6].second.get_as_integer().value();
    auto req = op.args[7].second.get_as_integer().value();

    // TODO make this whole first part its own function?
    // Since the collective part of a non-blocking collective happen as the completion operation we need some transition that in the background checks
    // when all ranks have participated in this operation (this is handled as a shared transition between all ranks similar to the regular collective operations)
    std::unordered_map<std::string, CPNExpression> guard_expression;
    for(int i = 0; i < net.mpi_size(); i++) {
        guard_expression.insert(std::make_pair("b" + std::to_string(i), CPNExpression(CPNToken(TokenKind::IalltoallCompleteMarker, -1, {comm, i}))));
    }
    auto guard = CPNGuard(CPNGuardKind::IalltoallComplete, guard_expression);

    // check if there is already a IalltoallComplete transition in the net
    // collective operations only get one transition in the whole net
    CPNTransition* existing_transition = nullptr;
    for(auto &t : net.transitions()) {
        if(t.guard().value().kind() == CPNGuardKind::IalltoallComplete && t.guard().value().to_string() == guard.to_string()) {
            existing_transition = &t;
        }
    }

    if(!existing_transition) {
        auto transition = CPNTransition("IalltoallComplete", guard, "Signals", op.expr);
        net.add_transition(transition);

        net.add_in_arc("Signals", transition.name(), "b" + std::to_string(rank));
        auto expr2 = CPNExpression(CPNToken(TokenKind::RequestMarker, transition.id(), {req, rank}));
        net.add_out_arc(transition.name(), "Signals", expr2);
    // TODO check if I can just get rid of the stuff below
    } else {
        if(DEBUG) {
            llvm::errs() << rank << " adding to existing IalltoallComplete\n";
        }
        bool arc_exists = false;
        for(auto &in_arc : existing_transition->in_arcs()) {        
            if(in_arc.binding() == "b" + std::to_string(rank)) {
                arc_exists = true;
            }
        }

        if(!arc_exists) {
            if(DEBUG) {
                llvm::errs() << "Adding IalltoallComplete arc from " << "Signals" << " to " << existing_transition->name() << "\n";
            }
            net.add_in_arc("Signals", existing_transition->name(), "b" + std::to_string(rank));
            auto expr2 = CPNExpression(CPNToken(TokenKind::RequestMarker, existing_transition->id(), {req, rank}));
            net.add_out_arc(existing_transition->name(), "Signals", expr2);
            existing_transition->add_ast_expr("P" + std::to_string(rank), op.expr);
        } else {
            if(DEBUG) {
                // llvm::errs() << rank << " adding to queue of IalltoallComplete out arc\n";
                warning_print("Hit a special case in IalltoallComplete transition creation. Check if there is something needed here or if this works like this");
            }
            //TODO check if this is needed; should not be necessary because the out_arcs should not need to change for a second identical Ialltoall call

            // for(auto &out_arc : existing_transition->out_arcs()) {
            //     if(out_arc.place() == place_name) {
            //         auto expr2 = to_marker_expression(ops.front(), existing_transition->id());
            //         out_arc.expression().extend_queue(expr2.token().value());
            //         existing_transition->add_ast_expr("P" + std::to_string(rank), op.expr);
            //         if(DEBUG) {
            //             llvm::errs() << "Extending existing MPI_Barrier transition\n";
            //         }
            //         break;
            //     }
            // }

        }
    }

    // PART II
    // This part now creates the actual Ialltoall transition for the rank
    // works the same as an non-blocking operation

    std::unordered_map<std::string, CPNExpression> guard_expression2;
    guard_expression2.insert(std::make_pair("s", CPNExpression(CPNToken(TokenKind::IalltoallMarker, -1, {comm, req}))));
    auto guard2 = CPNGuard(CPNGuardKind::Ialltoall, guard_expression2);

    // Check if there already is a transtition with the same guard for this process
    // TODO make this a function?
    CPNTransition* existing_transition2 = nullptr;
    for(auto &t : net.transitions()) {
        if(t.guard().value().kind() == CPNGuardKind::Ialltoall && t.guard().value().to_string() == guard.to_string()) {
            for(const auto &in_arc : t.in_arcs()) {
                if(in_arc.place() == "P" + std::to_string(rank)) {
                    existing_transition2 = &t;
                    if(DEBUG) {
                        llvm::errs() << " ==> FOUND EQUAL Ialltoall TRANSITION\n";
                    }
                    break;
                }
            }
        }
    }

    if(!existing_transition2) {
        auto transition = CPNTransition("MPI_Ialltoall", guard2, "P" + std::to_string(rank), op.expr, req);
        net.add_transition(transition);

        std::string place_name = "P" + std::to_string(rank);
        CPNExpression expr = CPNExpression(CPNToken(TokenKind::IalltoallCompleteMarker, transition.id(), { comm, rank}));
        net.add_in_arc(place_name, transition.name(), "s");
        net.add_out_arc(transition.name(), "Signals", expr);

        auto expr2 = to_marker_expression(ops.front(), transition.id());
        net.add_out_arc(transition.name(), place_name, expr2);
    } else {
        if(DEBUG) {
            llvm::errs() << "Adding to existing MPI_Ialltoall transition\n";
        }

        std::string place_name = "P" + std::to_string(rank);
        for(auto &out_arc : existing_transition2->out_arcs()) {
            if(out_arc.place() == place_name) {
                auto expr2 = to_marker_expression(ops.front(), existing_transition2->id());
                out_arc.expression().extend_queue(expr2.token().value());
                existing_transition2->add_ast_expr("P" + std::to_string(rank), op.expr);
                if(DEBUG) {
                    llvm::errs() << "Extending existing MPI_Ialltoall transition\n";
                }
                break;
            }
        }
    }

}

// TODO identical to MPI_Ibarrier currently
void CPNBuilder::mpi_ibcast(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const
{
    const bool DEBUG = CPNBUILDER_GENERAL_DEBUG;

    auto root = op.args[3].second.get_as_integer().value();
    auto comm = op.args[4].second.get_as_integer().value();
    auto req = op.args[5].second.get_as_integer().value();

    // TODO make this whole first part its own function?
    // Since the collective part of a non-blocking collective happen as the completion operation we need some transition that in the background checks
    // when all ranks have participated in this operation (this is handled as a shared transition between all ranks similar to the regular collective operations)
    std::unordered_map<std::string, CPNExpression> guard_expression;
    for(int i = 0; i < net.mpi_size(); i++) {
        guard_expression.insert(std::make_pair("b" + std::to_string(i), CPNExpression(CPNToken(TokenKind::IbcastCompleteMarker, -1, {root, comm, i}))));
    }
    auto guard = CPNGuard(CPNGuardKind::IbcastComplete, guard_expression);

    // check if there is already a IbcastComplete transition in the net
    // collective operations only get one transition in the whole net
    CPNTransition* existing_transition = nullptr;
    for(auto &t : net.transitions()) {
        if(t.guard().value().kind() == CPNGuardKind::IbcastComplete && t.guard().value().to_string() == guard.to_string()) {
            existing_transition = &t;
        }
    }

    if(!existing_transition) {
        auto transition = CPNTransition("IbcastComplete", guard, "Signals", op.expr);
        net.add_transition(transition);

        net.add_in_arc("Signals", transition.name(), "b" + std::to_string(rank));
        auto expr2 = CPNExpression(CPNToken(TokenKind::RequestMarker, transition.id(), {req, rank}));
        net.add_out_arc(transition.name(), "Signals", expr2);
    // TODO check if I can just get rid of the stuff below
    } else {
        if(DEBUG) {
            llvm::errs() << rank << " adding to existing IbcastComplete\n";
        }
        bool arc_exists = false;
        for(auto &in_arc : existing_transition->in_arcs()) {        
            if(in_arc.binding() == "b" + std::to_string(rank)) {
                arc_exists = true;
            }
        }

        if(!arc_exists) {
            if(DEBUG) {
                llvm::errs() << "Adding IbcastComplete arc from " << "Signals" << " to " << existing_transition->name() << "\n";
            }
            net.add_in_arc("Signals", existing_transition->name(), "b" + std::to_string(rank));
            auto expr2 = CPNExpression(CPNToken(TokenKind::RequestMarker, existing_transition->id(), {req, rank}));
            net.add_out_arc(existing_transition->name(), "Signals", expr2);
            existing_transition->add_ast_expr("P" + std::to_string(rank), op.expr);
        } else {
            if(DEBUG) {
                // llvm::errs() << rank << " adding to queue of IbcastComplete out arc\n";
                warning_print("Hit a special case in IbcastComplete transition creation. Check if there is something needed here or if this works like this");
            }
            //TODO check if this is needed; should not be necessary because the out_arcs should not need to change for a second identical Ibcast call

            // for(auto &out_arc : existing_transition->out_arcs()) {
            //     if(out_arc.place() == place_name) {
            //         auto expr2 = to_marker_expression(ops.front(), existing_transition->id());
            //         out_arc.expression().extend_queue(expr2.token().value());
            //         existing_transition->add_ast_expr("P" + std::to_string(rank), op.expr);
            //         if(DEBUG) {
            //             llvm::errs() << "Extending existing MPI_Barrier transition\n";
            //         }
            //         break;
            //     }
            // }

        }
    }

    // PART II
    // This part now creates the actual Ibcast transition for the rank
    // works the same as an non-blocking operation

    std::unordered_map<std::string, CPNExpression> guard_expression2;
    guard_expression2.insert(std::make_pair("s", CPNExpression(CPNToken(TokenKind::IbcastMarker, -1, {root, comm, req}))));
    auto guard2 = CPNGuard(CPNGuardKind::Ibcast, guard_expression2);

    // Check if there already is a transtition with the same guard for this process
    // TODO make this a function?
    CPNTransition* existing_transition2 = nullptr;
    for(auto &t : net.transitions()) {
        if(t.guard().value().kind() == CPNGuardKind::Ibcast && t.guard().value().to_string() == guard.to_string()) {
            for(const auto &in_arc : t.in_arcs()) {
                if(in_arc.place() == "P" + std::to_string(rank)) {
                    existing_transition2 = &t;
                    if(DEBUG) {
                        llvm::errs() << " ==> FOUND EQUAL Ibcast TRANSITION\n";
                    }
                    break;
                }
            }
        }
    }

    if(!existing_transition2) {
        auto transition = CPNTransition("MPI_Ibcast", guard2, "P" + std::to_string(rank), op.expr, req);
        net.add_transition(transition);

        std::string place_name = "P" + std::to_string(rank);
        CPNExpression expr = CPNExpression(CPNToken(TokenKind::IbcastCompleteMarker, transition.id(), { root, comm, rank}));
        net.add_in_arc(place_name, transition.name(), "s");
        net.add_out_arc(transition.name(), "Signals", expr);

        auto expr2 = to_marker_expression(ops.front(), transition.id());
        net.add_out_arc(transition.name(), place_name, expr2);
    } else {
        if(DEBUG) {
            llvm::errs() << "Adding to existing MPI_Ibcast transition\n";
        }

        std::string place_name = "P" + std::to_string(rank);
        for(auto &out_arc : existing_transition2->out_arcs()) {
            if(out_arc.place() == place_name) {
                auto expr2 = to_marker_expression(ops.front(), existing_transition2->id());
                out_arc.expression().extend_queue(expr2.token().value());
                existing_transition2->add_ast_expr("P" + std::to_string(rank), op.expr);
                if(DEBUG) {
                    llvm::errs() << "Extending existing MPI_Ibcast transition\n";
                }
                break;
            }
        }
    }

}

// TODO identical to MPI_Ibarrier currently
void CPNBuilder::mpi_igather(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const
{
    const bool DEBUG = CPNBUILDER_GENERAL_DEBUG;

    auto root = op.args[6].second.get_as_integer().value();
    auto comm = op.args[7].second.get_as_integer().value();
    auto req = op.args[8].second.get_as_integer().value();

    // TODO make this whole first part its own function?
    // Since the collective part of a non-blocking collective happen as the completion operation we need some transition that in the background checks
    // when all ranks have participated in this operation (this is handled as a shared transition between all ranks similar to the regular collective operations)
    std::unordered_map<std::string, CPNExpression> guard_expression;
    for(int i = 0; i < net.mpi_size(); i++) {
        guard_expression.insert(std::make_pair("b" + std::to_string(i), CPNExpression(CPNToken(TokenKind::IgatherCompleteMarker, -1, {root, comm, i}))));
    }
    auto guard = CPNGuard(CPNGuardKind::IgatherComplete, guard_expression);

    // check if there is already a IgatherComplete transition in the net
    // collective operations only get one transition in the whole net
    CPNTransition* existing_transition = nullptr;
    for(auto &t : net.transitions()) {
        if(t.guard().value().kind() == CPNGuardKind::IgatherComplete && t.guard().value().to_string() == guard.to_string()) {
            existing_transition = &t;
        }
    }

    if(!existing_transition) {
        auto transition = CPNTransition("IgatherComplete", guard, "Signals", op.expr);
        net.add_transition(transition);

        net.add_in_arc("Signals", transition.name(), "b" + std::to_string(rank));
        auto expr2 = CPNExpression(CPNToken(TokenKind::RequestMarker, transition.id(), {req, rank}));
        net.add_out_arc(transition.name(), "Signals", expr2);
    // TODO check if I can just get rid of the stuff below
    } else {
        if(DEBUG) {
            llvm::errs() << rank << " adding to existing IgatherComplete\n";
        }
        bool arc_exists = false;
        for(auto &in_arc : existing_transition->in_arcs()) {        
            if(in_arc.binding() == "b" + std::to_string(rank)) {
                arc_exists = true;
            }
        }

        if(!arc_exists) {
            if(DEBUG) {
                llvm::errs() << "Adding IgatherComplete arc from " << "Signals" << " to " << existing_transition->name() << "\n";
            }
            net.add_in_arc("Signals", existing_transition->name(), "b" + std::to_string(rank));
            auto expr2 = CPNExpression(CPNToken(TokenKind::RequestMarker, existing_transition->id(), {req, rank}));
            net.add_out_arc(existing_transition->name(), "Signals", expr2);
            existing_transition->add_ast_expr("P" + std::to_string(rank), op.expr);
        } else {
            if(DEBUG) {
                // llvm::errs() << rank << " adding to queue of IgatherComplete out arc\n";
                warning_print("Hit a special case in IgatherComplete transition creation. Check if there is something needed here or if this works like this");
            }
            //TODO check if this is needed; should not be necessary because the out_arcs should not need to change for a second identical Igather call

            // for(auto &out_arc : existing_transition->out_arcs()) {
            //     if(out_arc.place() == place_name) {
            //         auto expr2 = to_marker_expression(ops.front(), existing_transition->id());
            //         out_arc.expression().extend_queue(expr2.token().value());
            //         existing_transition->add_ast_expr("P" + std::to_string(rank), op.expr);
            //         if(DEBUG) {
            //             llvm::errs() << "Extending existing MPI_Barrier transition\n";
            //         }
            //         break;
            //     }
            // }

        }
    }

    // PART II
    // This part now creates the actual Igather transition for the rank
    // works the same as an non-blocking operation

    std::unordered_map<std::string, CPNExpression> guard_expression2;
    guard_expression2.insert(std::make_pair("s", CPNExpression(CPNToken(TokenKind::IgatherMarker, -1, {root, comm, req}))));
    auto guard2 = CPNGuard(CPNGuardKind::Igather, guard_expression2);

    // Check if there already is a transtition with the same guard for this process
    // TODO make this a function?
    CPNTransition* existing_transition2 = nullptr;
    for(auto &t : net.transitions()) {
        if(t.guard().value().kind() == CPNGuardKind::Igather && t.guard().value().to_string() == guard.to_string()) {
            for(const auto &in_arc : t.in_arcs()) {
                if(in_arc.place() == "P" + std::to_string(rank)) {
                    existing_transition2 = &t;
                    if(DEBUG) {
                        llvm::errs() << " ==> FOUND EQUAL Igather TRANSITION\n";
                    }
                    break;
                }
            }
        }
    }

    if(!existing_transition2) {
        auto transition = CPNTransition("MPI_Igather", guard2, "P" + std::to_string(rank), op.expr, req);
        net.add_transition(transition);

        std::string place_name = "P" + std::to_string(rank);
        CPNExpression expr = CPNExpression(CPNToken(TokenKind::IgatherCompleteMarker, transition.id(), { root, comm, rank}));
        net.add_in_arc(place_name, transition.name(), "s");
        net.add_out_arc(transition.name(), "Signals", expr);

        auto expr2 = to_marker_expression(ops.front(), transition.id());
        net.add_out_arc(transition.name(), place_name, expr2);
    } else {
        if(DEBUG) {
            llvm::errs() << "Adding to existing MPI_Igather transition\n";
        }

        std::string place_name = "P" + std::to_string(rank);
        for(auto &out_arc : existing_transition2->out_arcs()) {
            if(out_arc.place() == place_name) {
                auto expr2 = to_marker_expression(ops.front(), existing_transition2->id());
                out_arc.expression().extend_queue(expr2.token().value());
                existing_transition2->add_ast_expr("P" + std::to_string(rank), op.expr);
                if(DEBUG) {
                    llvm::errs() << "Extending existing MPI_Igather transition\n";
                }
                break;
            }
        }
    }

}

// TODO identical to MPI_Ibarrier currently
// TODO probably include the MPI_OP here as well
void CPNBuilder::mpi_ireduce(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const
{
    const bool DEBUG = CPNBUILDER_GENERAL_DEBUG;

    auto root = op.args[5].second.get_as_integer().value();
    auto comm = op.args[6].second.get_as_integer().value();
    auto req = op.args[7].second.get_as_integer().value();

    // TODO make this whole first part its own function?
    // Since the collective part of a non-blocking collective happen as the completion operation we need some transition that in the background checks
    // when all ranks have participated in this operation (this is handled as a shared transition between all ranks similar to the regular collective operations)
    std::unordered_map<std::string, CPNExpression> guard_expression;
    for(int i = 0; i < net.mpi_size(); i++) {
        guard_expression.insert(std::make_pair("b" + std::to_string(i), CPNExpression(CPNToken(TokenKind::IreduceCompleteMarker, -1, {root, comm, i}))));
    }
    auto guard = CPNGuard(CPNGuardKind::IreduceComplete, guard_expression);

    // check if there is already a IreduceComplete transition in the net
    // collective operations only get one transition in the whole net
    CPNTransition* existing_transition = nullptr;
    for(auto &t : net.transitions()) {
        if(t.guard().value().kind() == CPNGuardKind::IreduceComplete && t.guard().value().to_string() == guard.to_string()) {
            existing_transition = &t;
        }
    }

    if(!existing_transition) {
        auto transition = CPNTransition("IreduceComplete", guard, "Signals", op.expr);
        net.add_transition(transition);

        net.add_in_arc("Signals", transition.name(), "b" + std::to_string(rank));
        auto expr2 = CPNExpression(CPNToken(TokenKind::RequestMarker, transition.id(), {req, rank}));
        net.add_out_arc(transition.name(), "Signals", expr2);
    // TODO check if I can just get rid of the stuff below
    } else {
        if(DEBUG) {
            llvm::errs() << rank << " adding to existing IreduceComplete\n";
        }
        bool arc_exists = false;
        for(auto &in_arc : existing_transition->in_arcs()) {        
            if(in_arc.binding() == "b" + std::to_string(rank)) {
                arc_exists = true;
            }
        }

        if(!arc_exists) {
            if(DEBUG) {
                llvm::errs() << "Adding IreduceComplete arc from " << "Signals" << " to " << existing_transition->name() << "\n";
            }
            net.add_in_arc("Signals", existing_transition->name(), "b" + std::to_string(rank));
            auto expr2 = CPNExpression(CPNToken(TokenKind::RequestMarker, existing_transition->id(), {req, rank}));
            net.add_out_arc(existing_transition->name(), "Signals", expr2);
            existing_transition->add_ast_expr("P" + std::to_string(rank), op.expr);
        } else {
            if(DEBUG) {
                // llvm::errs() << rank << " adding to queue of IreduceComplete out arc\n";
                warning_print("Hit a special case in IreduceComplete transition creation. Check if there is something needed here or if this works like this");
            }
            //TODO check if this is needed; should not be necessary because the out_arcs should not need to change for a second identical Ireduce call

            // for(auto &out_arc : existing_transition->out_arcs()) {
            //     if(out_arc.place() == place_name) {
            //         auto expr2 = to_marker_expression(ops.front(), existing_transition->id());
            //         out_arc.expression().extend_queue(expr2.token().value());
            //         existing_transition->add_ast_expr("P" + std::to_string(rank), op.expr);
            //         if(DEBUG) {
            //             llvm::errs() << "Extending existing MPI_Barrier transition\n";
            //         }
            //         break;
            //     }
            // }

        }
    }

    // PART II
    // This part now creates the actual Ireduce transition for the rank
    // works the same as an non-blocking operation

    std::unordered_map<std::string, CPNExpression> guard_expression2;
    guard_expression2.insert(std::make_pair("s", CPNExpression(CPNToken(TokenKind::IreduceMarker, -1, {root, comm, req}))));
    auto guard2 = CPNGuard(CPNGuardKind::Ireduce, guard_expression2);

    // Check if there already is a transtition with the same guard for this process
    // TODO make this a function?
    CPNTransition* existing_transition2 = nullptr;
    for(auto &t : net.transitions()) {
        if(t.guard().value().kind() == CPNGuardKind::Ireduce && t.guard().value().to_string() == guard.to_string()) {
            for(const auto &in_arc : t.in_arcs()) {
                if(in_arc.place() == "P" + std::to_string(rank)) {
                    existing_transition2 = &t;
                    if(DEBUG) {
                        llvm::errs() << " ==> FOUND EQUAL Ireduce TRANSITION\n";
                    }
                    break;
                }
            }
        }
    }

    if(!existing_transition2) {
        auto transition = CPNTransition("MPI_Ireduce", guard2, "P" + std::to_string(rank), op.expr, req);
        net.add_transition(transition);

        std::string place_name = "P" + std::to_string(rank);
        CPNExpression expr = CPNExpression(CPNToken(TokenKind::IreduceCompleteMarker, transition.id(), { root, comm, rank}));
        net.add_in_arc(place_name, transition.name(), "s");
        net.add_out_arc(transition.name(), "Signals", expr);

        auto expr2 = to_marker_expression(ops.front(), transition.id());
        net.add_out_arc(transition.name(), place_name, expr2);
    } else {
        if(DEBUG) {
            llvm::errs() << "Adding to existing MPI_Ireduce transition\n";
        }

        std::string place_name = "P" + std::to_string(rank);
        for(auto &out_arc : existing_transition2->out_arcs()) {
            if(out_arc.place() == place_name) {
                auto expr2 = to_marker_expression(ops.front(), existing_transition2->id());
                out_arc.expression().extend_queue(expr2.token().value());
                existing_transition2->add_ast_expr("P" + std::to_string(rank), op.expr);
                if(DEBUG) {
                    llvm::errs() << "Extending existing MPI_Ireduce transition\n";
                }
                break;
            }
        }
    }

}

// TODO identical to MPI_Ibarrier currently
// TODO probably include the MPI_OP here as well
void CPNBuilder::mpi_iscan(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const
{
    const bool DEBUG = CPNBUILDER_GENERAL_DEBUG;

    auto comm = op.args[5].second.get_as_integer().value();
    auto req = op.args[6].second.get_as_integer().value();

    // TODO make this whole first part its own function?
    // Since the collective part of a non-blocking collective happen as the completion operation we need some transition that in the background checks
    // when all ranks have participated in this operation (this is handled as a shared transition between all ranks similar to the regular collective operations)
    std::unordered_map<std::string, CPNExpression> guard_expression;
    for(int i = 0; i < net.mpi_size(); i++) {
        guard_expression.insert(std::make_pair("b" + std::to_string(i), CPNExpression(CPNToken(TokenKind::IscanCompleteMarker, -1, {comm, i}))));
    }
    auto guard = CPNGuard(CPNGuardKind::IscanComplete, guard_expression);

    // check if there is already a IscanComplete transition in the net
    // collective operations only get one transition in the whole net
    CPNTransition* existing_transition = nullptr;
    for(auto &t : net.transitions()) {
        if(t.guard().value().kind() == CPNGuardKind::IscanComplete && t.guard().value().to_string() == guard.to_string()) {
            existing_transition = &t;
        }
    }

    if(!existing_transition) {
        auto transition = CPNTransition("IscanComplete", guard, "Signals", op.expr);
        net.add_transition(transition);

        net.add_in_arc("Signals", transition.name(), "b" + std::to_string(rank));
        auto expr2 = CPNExpression(CPNToken(TokenKind::RequestMarker, transition.id(), {req, rank}));
        net.add_out_arc(transition.name(), "Signals", expr2);
    // TODO check if I can just get rid of the stuff below
    } else {
        if(DEBUG) {
            llvm::errs() << rank << " adding to existing IscanComplete\n";
        }
        bool arc_exists = false;
        for(auto &in_arc : existing_transition->in_arcs()) {        
            if(in_arc.binding() == "b" + std::to_string(rank)) {
                arc_exists = true;
            }
        }

        if(!arc_exists) {
            if(DEBUG) {
                llvm::errs() << "Adding IscanComplete arc from " << "Signals" << " to " << existing_transition->name() << "\n";
            }
            net.add_in_arc("Signals", existing_transition->name(), "b" + std::to_string(rank));
            auto expr2 = CPNExpression(CPNToken(TokenKind::RequestMarker, existing_transition->id(), {req, rank}));
            net.add_out_arc(existing_transition->name(), "Signals", expr2);
            existing_transition->add_ast_expr("P" + std::to_string(rank), op.expr);
        } else {
            if(DEBUG) {
                // llvm::errs() << rank << " adding to queue of IscanComplete out arc\n";
                warning_print("Hit a special case in IscanComplete transition creation. Check if there is something needed here or if this works like this");
            }
            //TODO check if this is needed; should not be necessary because the out_arcs should not need to change for a second identical Iscan call

            // for(auto &out_arc : existing_transition->out_arcs()) {
            //     if(out_arc.place() == place_name) {
            //         auto expr2 = to_marker_expression(ops.front(), existing_transition->id());
            //         out_arc.expression().extend_queue(expr2.token().value());
            //         existing_transition->add_ast_expr("P" + std::to_string(rank), op.expr);
            //         if(DEBUG) {
            //             llvm::errs() << "Extending existing MPI_Barrier transition\n";
            //         }
            //         break;
            //     }
            // }

        }
    }

    // PART II
    // This part now creates the actual Iscan transition for the rank
    // works the same as an non-blocking operation

    std::unordered_map<std::string, CPNExpression> guard_expression2;
    guard_expression2.insert(std::make_pair("s", CPNExpression(CPNToken(TokenKind::IscanMarker, -1, {comm, req}))));
    auto guard2 = CPNGuard(CPNGuardKind::Iscan, guard_expression2);

    // Check if there already is a transtition with the same guard for this process
    // TODO make this a function?
    CPNTransition* existing_transition2 = nullptr;
    for(auto &t : net.transitions()) {
        if(t.guard().value().kind() == CPNGuardKind::Iscan && t.guard().value().to_string() == guard.to_string()) {
            for(const auto &in_arc : t.in_arcs()) {
                if(in_arc.place() == "P" + std::to_string(rank)) {
                    existing_transition2 = &t;
                    if(DEBUG) {
                        llvm::errs() << " ==> FOUND EQUAL Iscan TRANSITION\n";
                    }
                    break;
                }
            }
        }
    }

    if(!existing_transition2) {
        auto transition = CPNTransition("MPI_Iscan", guard2, "P" + std::to_string(rank), op.expr, req);
        net.add_transition(transition);

        std::string place_name = "P" + std::to_string(rank);
        CPNExpression expr = CPNExpression(CPNToken(TokenKind::IscanCompleteMarker, transition.id(), { comm, rank}));
        net.add_in_arc(place_name, transition.name(), "s");
        net.add_out_arc(transition.name(), "Signals", expr);

        auto expr2 = to_marker_expression(ops.front(), transition.id());
        net.add_out_arc(transition.name(), place_name, expr2);
    } else {
        if(DEBUG) {
            llvm::errs() << "Adding to existing MPI_Iscan transition\n";
        }

        std::string place_name = "P" + std::to_string(rank);
        for(auto &out_arc : existing_transition2->out_arcs()) {
            if(out_arc.place() == place_name) {
                auto expr2 = to_marker_expression(ops.front(), existing_transition2->id());
                out_arc.expression().extend_queue(expr2.token().value());
                existing_transition2->add_ast_expr("P" + std::to_string(rank), op.expr);
                if(DEBUG) {
                    llvm::errs() << "Extending existing MPI_Iscan transition\n";
                }
                break;
            }
        }
    }

}

// TODO identical to MPI_Ibarrier currently
// TODO probably include the MPI_OP here as well
void CPNBuilder::mpi_iexscan(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const
{
    const bool DEBUG = CPNBUILDER_GENERAL_DEBUG;

    auto comm = op.args[5].second.get_as_integer().value();
    auto req = op.args[6].second.get_as_integer().value();

    // TODO make this whole first part its own function?
    // Since the collective part of a non-blocking collective happen as the completion operation we need some transition that in the background checks
    // when all ranks have participated in this operation (this is handled as a shared transition between all ranks similar to the regular collective operations)
    std::unordered_map<std::string, CPNExpression> guard_expression;
    for(int i = 0; i < net.mpi_size(); i++) {
        guard_expression.insert(std::make_pair("b" + std::to_string(i), CPNExpression(CPNToken(TokenKind::IexscanCompleteMarker, -1, {comm, i}))));
    }
    auto guard = CPNGuard(CPNGuardKind::IexscanComplete, guard_expression);

    // check if there is already a IexscanComplete transition in the net
    // collective operations only get one transition in the whole net
    CPNTransition* existing_transition = nullptr;
    for(auto &t : net.transitions()) {
        if(t.guard().value().kind() == CPNGuardKind::IexscanComplete && t.guard().value().to_string() == guard.to_string()) {
            existing_transition = &t;
        }
    }

    if(!existing_transition) {
        auto transition = CPNTransition("IexscanComplete", guard, "Signals", op.expr);
        net.add_transition(transition);

        net.add_in_arc("Signals", transition.name(), "b" + std::to_string(rank));
        auto expr2 = CPNExpression(CPNToken(TokenKind::RequestMarker, transition.id(), {req, rank}));
        net.add_out_arc(transition.name(), "Signals", expr2);
    // TODO check if I can just get rid of the stuff below
    } else {
        if(DEBUG) {
            llvm::errs() << rank << " adding to existing IexscanComplete\n";
        }
        bool arc_exists = false;
        for(auto &in_arc : existing_transition->in_arcs()) {        
            if(in_arc.binding() == "b" + std::to_string(rank)) {
                arc_exists = true;
            }
        }

        if(!arc_exists) {
            if(DEBUG) {
                llvm::errs() << "Adding IexscanComplete arc from " << "Signals" << " to " << existing_transition->name() << "\n";
            }
            net.add_in_arc("Signals", existing_transition->name(), "b" + std::to_string(rank));
            auto expr2 = CPNExpression(CPNToken(TokenKind::RequestMarker, existing_transition->id(), {req, rank}));
            net.add_out_arc(existing_transition->name(), "Signals", expr2);
            existing_transition->add_ast_expr("P" + std::to_string(rank), op.expr);
        } else {
            if(DEBUG) {
                // llvm::errs() << rank << " adding to queue of IexscanComplete out arc\n";
                warning_print("Hit a special case in IexscanComplete transition creation. Check if there is something needed here or if this works like this");
            }
            //TODO check if this is needed; should not be necessary because the out_arcs should not need to change for a second identical Iexscan call

            // for(auto &out_arc : existing_transition->out_arcs()) {
            //     if(out_arc.place() == place_name) {
            //         auto expr2 = to_marker_expression(ops.front(), existing_transition->id());
            //         out_arc.expression().extend_queue(expr2.token().value());
            //         existing_transition->add_ast_expr("P" + std::to_string(rank), op.expr);
            //         if(DEBUG) {
            //             llvm::errs() << "Extending existing MPI_Barrier transition\n";
            //         }
            //         break;
            //     }
            // }

        }
    }

    // PART II
    // This part now creates the actual Iexscan transition for the rank
    // works the same as an non-blocking operation

    std::unordered_map<std::string, CPNExpression> guard_expression2;
    guard_expression2.insert(std::make_pair("s", CPNExpression(CPNToken(TokenKind::IexscanMarker, -1, {comm, req}))));
    auto guard2 = CPNGuard(CPNGuardKind::Iexscan, guard_expression2);

    // Check if there already is a transtition with the same guard for this process
    // TODO make this a function?
    CPNTransition* existing_transition2 = nullptr;
    for(auto &t : net.transitions()) {
        if(t.guard().value().kind() == CPNGuardKind::Iexscan && t.guard().value().to_string() == guard.to_string()) {
            for(const auto &in_arc : t.in_arcs()) {
                if(in_arc.place() == "P" + std::to_string(rank)) {
                    existing_transition2 = &t;
                    if(DEBUG) {
                        llvm::errs() << " ==> FOUND EQUAL Iexscan TRANSITION\n";
                    }
                    break;
                }
            }
        }
    }

    if(!existing_transition2) {
        auto transition = CPNTransition("MPI_Iexscan", guard2, "P" + std::to_string(rank), op.expr, req);
        net.add_transition(transition);

        std::string place_name = "P" + std::to_string(rank);
        CPNExpression expr = CPNExpression(CPNToken(TokenKind::IexscanCompleteMarker, transition.id(), { comm, rank}));
        net.add_in_arc(place_name, transition.name(), "s");
        net.add_out_arc(transition.name(), "Signals", expr);

        auto expr2 = to_marker_expression(ops.front(), transition.id());
        net.add_out_arc(transition.name(), place_name, expr2);
    } else {
        if(DEBUG) {
            llvm::errs() << "Adding to existing MPI_Iexscan transition\n";
        }

        std::string place_name = "P" + std::to_string(rank);
        for(auto &out_arc : existing_transition2->out_arcs()) {
            if(out_arc.place() == place_name) {
                auto expr2 = to_marker_expression(ops.front(), existing_transition2->id());
                out_arc.expression().extend_queue(expr2.token().value());
                existing_transition2->add_ast_expr("P" + std::to_string(rank), op.expr);
                if(DEBUG) {
                    llvm::errs() << "Extending existing MPI_Iexscan transition\n";
                }
                break;
            }
        }
    }

}

// TODO identical to MPI_Ibarrier currently
void CPNBuilder::mpi_iscatter(CPNNet &net, const int rank, const SPMDCall &op, const std::vector<SPMDCall> &ops) const
{
    const bool DEBUG = CPNBUILDER_GENERAL_DEBUG;

    auto root = op.args[6].second.get_as_integer().value();
    auto comm = op.args[7].second.get_as_integer().value();
    auto req = op.args[8].second.get_as_integer().value();

    // TODO make this whole first part its own function?
    // Since the collective part of a non-blocking collective happen as the completion operation we need some transition that in the background checks
    // when all ranks have participated in this operation (this is handled as a shared transition between all ranks similar to the regular collective operations)
    std::unordered_map<std::string, CPNExpression> guard_expression;
    for(int i = 0; i < net.mpi_size(); i++) {
        guard_expression.insert(std::make_pair("b" + std::to_string(i), CPNExpression(CPNToken(TokenKind::IscatterCompleteMarker, -1, {root, comm, i}))));
    }
    auto guard = CPNGuard(CPNGuardKind::IscatterComplete, guard_expression);

    // check if there is already a IscatterComplete transition in the net
    // collective operations only get one transition in the whole net
    CPNTransition* existing_transition = nullptr;
    for(auto &t : net.transitions()) {
        if(t.guard().value().kind() == CPNGuardKind::IscatterComplete && t.guard().value().to_string() == guard.to_string()) {
            existing_transition = &t;
        }
    }

    if(!existing_transition) {
        auto transition = CPNTransition("IscatterComplete", guard, "Signals", op.expr);
        net.add_transition(transition);

        net.add_in_arc("Signals", transition.name(), "b" + std::to_string(rank));
        auto expr2 = CPNExpression(CPNToken(TokenKind::RequestMarker, transition.id(), {req, rank}));
        net.add_out_arc(transition.name(), "Signals", expr2);
    // TODO check if I can just get rid of the stuff below
    } else {
        if(DEBUG) {
            llvm::errs() << rank << " adding to existing IscatterComplete\n";
        }
        bool arc_exists = false;
        for(auto &in_arc : existing_transition->in_arcs()) {        
            if(in_arc.binding() == "b" + std::to_string(rank)) {
                arc_exists = true;
            }
        }

        if(!arc_exists) {
            if(DEBUG) {
                llvm::errs() << "Adding IscatterComplete arc from " << "Signals" << " to " << existing_transition->name() << "\n";
            }
            net.add_in_arc("Signals", existing_transition->name(), "b" + std::to_string(rank));
            auto expr2 = CPNExpression(CPNToken(TokenKind::RequestMarker, existing_transition->id(), {req, rank}));
            net.add_out_arc(existing_transition->name(), "Signals", expr2);
            existing_transition->add_ast_expr("P" + std::to_string(rank), op.expr);
        } else {
            if(DEBUG) {
                // llvm::errs() << rank << " adding to queue of IscatterComplete out arc\n";
                warning_print("Hit a special case in IscatterComplete transition creation. Check if there is something needed here or if this works like this");
            }
            //TODO check if this is needed; should not be necessary because the out_arcs should not need to change for a second identical Iscatter call

            // for(auto &out_arc : existing_transition->out_arcs()) {
            //     if(out_arc.place() == place_name) {
            //         auto expr2 = to_marker_expression(ops.front(), existing_transition->id());
            //         out_arc.expression().extend_queue(expr2.token().value());
            //         existing_transition->add_ast_expr("P" + std::to_string(rank), op.expr);
            //         if(DEBUG) {
            //             llvm::errs() << "Extending existing MPI_Barrier transition\n";
            //         }
            //         break;
            //     }
            // }

        }
    }

    // PART II
    // This part now creates the actual Iscatter transition for the rank
    // works the same as an non-blocking operation

    std::unordered_map<std::string, CPNExpression> guard_expression2;
    guard_expression2.insert(std::make_pair("s", CPNExpression(CPNToken(TokenKind::IscatterMarker, -1, {root, comm, req}))));
    auto guard2 = CPNGuard(CPNGuardKind::Iscatter, guard_expression2);

    // Check if there already is a transtition with the same guard for this process
    // TODO make this a function?
    CPNTransition* existing_transition2 = nullptr;
    for(auto &t : net.transitions()) {
        if(t.guard().value().kind() == CPNGuardKind::Iscatter && t.guard().value().to_string() == guard.to_string()) {
            for(const auto &in_arc : t.in_arcs()) {
                if(in_arc.place() == "P" + std::to_string(rank)) {
                    existing_transition2 = &t;
                    if(DEBUG) {
                        llvm::errs() << " ==> FOUND EQUAL Iscatter TRANSITION\n";
                    }
                    break;
                }
            }
        }
    }

    if(!existing_transition2) {
        auto transition = CPNTransition("MPI_Iscatter", guard2, "P" + std::to_string(rank), op.expr, req);
        net.add_transition(transition);

        std::string place_name = "P" + std::to_string(rank);
        CPNExpression expr = CPNExpression(CPNToken(TokenKind::IscatterCompleteMarker, transition.id(), { root, comm, rank}));
        net.add_in_arc(place_name, transition.name(), "s");
        net.add_out_arc(transition.name(), "Signals", expr);

        auto expr2 = to_marker_expression(ops.front(), transition.id());
        net.add_out_arc(transition.name(), place_name, expr2);
    } else {
        if(DEBUG) {
            llvm::errs() << "Adding to existing MPI_Iscatter transition\n";
        }

        std::string place_name = "P" + std::to_string(rank);
        for(auto &out_arc : existing_transition2->out_arcs()) {
            if(out_arc.place() == place_name) {
                auto expr2 = to_marker_expression(ops.front(), existing_transition2->id());
                out_arc.expression().extend_queue(expr2.token().value());
                existing_transition2->add_ast_expr("P" + std::to_string(rank), op.expr);
                if(DEBUG) {
                    llvm::errs() << "Extending existing MPI_Iscatter transition\n";
                }
                break;
            }
        }
    }

}
