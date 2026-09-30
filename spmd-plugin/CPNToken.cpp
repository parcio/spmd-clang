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

#include "CPNToken.h"

CPNToken::CPNToken(TokenKind kind, std::int64_t id, std::vector<std::int64_t> args, std::int64_t request_id) : _kind(kind), _transition_id(id), _request_id(request_id), _args(args)
{
    std::string token_kind = kind_to_string();
    std::string arguments = "(";
    for(int i = 0; i < _args.size(); i++) {
        arguments += std::to_string(_args[i]);
        if(i < _args.size() - 1) {
            arguments += ", ";
        }
    }
    arguments += ")";
    _token_string = token_kind + arguments;
    _argument_string = arguments;
}

// TODO find an automated way to do this
std::string CPNToken::kind_to_string() const
{
    std::string result;
    switch(_kind) {
        case TokenKind::EmptyMarker: 
            result = "EmptyMarker";
            break;
        case TokenKind::InitMarker: 
            result = "InitMarker";
            break;
        case TokenKind::FinalizeMarker: 
            result = "FinalizeMarker";
            break;
        case TokenKind::MessageMarker: 
            result = "MessageMarker";
            break;
        case TokenKind::SsendMarker: 
            result = "SsendMarker";
            break;
        case TokenKind::SendMarker: 
            result = "SendMarker";
            break;
        case TokenKind::RsendMarker: 
            result = "RsendMarker";
            break;
        case TokenKind::BsendMarker: 
            result = "BsendMarker";
            break;
        case TokenKind::RecvMarker: 
            result = "RecvMarker";
            break;
        case TokenKind::IsendMarker: 
            result = "IsendMarker";
            break;
        case TokenKind::WaitMarker: 
            result = "WaitMarker";
            break;
        case TokenKind::ContinueMarker:
            result = "ContinueMarker";
            break;
        case TokenKind::RequestMarker:
            result = "RequestMarker";
            break;
        case TokenKind::IrecvMarker:
            result = "IrecvMarker";
            break;
        case TokenKind::CompleteMarker:
            result = "CompleteMarker";
            break;
        case TokenKind::BarrierMarker:
            result = "BarrierMarker";
            break;
        case TokenKind::AllgatherMarker:
            result = "AllgatherMarker";
            break;
        case TokenKind::AllreduceMarker:
            result = "AllreduceMarker";
            break;
        case TokenKind::AlltoallMarker:
            result = "AlltoallMarker";
            break;
        case TokenKind::BcastMarker:
            result = "BcastMarker";
            break;
        case TokenKind::GatherMarker:
            result = "GatherMarker";
            break;
        case TokenKind::ReduceMarker:
            result = "ReduceMarker";
            break;
        case TokenKind::ScanMarker:
            result = "ScanMarker";
            break;
        case TokenKind::ExscanMarker:
            result = "ExscanMarker";
            break;
        case TokenKind::ScatterMarker:
            result = "ScatterMarker";
            break;
        case TokenKind::IbarrierMarker:
            result = "IbarrierMarker";
            break;
        case TokenKind::IbarrierCompleteMarker:
            result = "IbarrierCompleteMarker";
            break;
        case TokenKind::IallgatherMarker:
            result = "IallgatherMarker";
            break;
        case TokenKind::IallgatherCompleteMarker:
            result = "IallgatherCompleteMarker";
            break;
        case TokenKind::IallreduceMarker:
            result = "IallreduceMarker";
            break;
        case TokenKind::IallreduceCompleteMarker:
            result = "IallreduceCompleteMarker";
            break;
        case TokenKind::IalltoallMarker:
            result = "IalltoallMarker";
            break;
        case TokenKind::IalltoallCompleteMarker:
            result = "IalltoallCompleteMarker";
            break;
        case TokenKind::IbcastMarker:
            result = "IbcastMarker";
            break;
        case TokenKind::IbcastCompleteMarker:
            result = "IbcastCompleteMarker";
            break;
        case TokenKind::IgatherMarker:
            result = "IgatherMarker";
            break;
        case TokenKind::IgatherCompleteMarker:
            result = "IgatherCompleteMarker";
            break;
        case TokenKind::IreduceMarker:
            result = "IreduceMarker";
            break;
        case TokenKind::IreduceCompleteMarker:
            result = "IreduceCompleteMarker";
            break;
        case TokenKind::IscanMarker:
            result = "IscanMarker";
            break;
        case TokenKind::IscanCompleteMarker:
            result = "IscanCompleteMarker";
            break;
        case TokenKind::IexscanMarker:
            result = "IexscanMarker";
            break;
        case TokenKind::IexscanCompleteMarker:
            result = "IexscanCompleteMarker";
            break;
        case TokenKind::IscatterMarker:
            result = "IscatterMarker";
            break;
        case TokenKind::IscatterCompleteMarker:
            result = "IscatterCompleteMarker";
            break;
        default:
            result = "UnkownToken";
    }
    return result;
}

bool operator==(const CPNToken &lhs, const CPNToken &rhs)
{
    return (lhs.to_string() == rhs.to_string());
}

bool operator<(const CPNToken &lhs, const CPNToken &rhs)
{
    // TODO double check if this makes a problem. The operator is only implemented for std::set to work
    return lhs.to_string() < rhs.to_string();
}
