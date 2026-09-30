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

#ifndef UTILSSS_H
#define UTILSSS_H

#include <clang/Analysis/CFG.h>
#include <llvm/Support/Errc.h>
#include <llvm/Support/raw_os_ostream.h>
#include <llvm/Support/WithColor.h>

#include <string>
#include <sstream>

// TODO simplify?
/// Extracts the contents of a CFGElement into a string
inline std::string cfgelement_to_str(const clang::CFGElement *elem)
{
    std::string result;
    std::ostringstream oss;
    llvm::raw_os_ostream ros(oss);

    elem->dumpToStream(ros);
    ros.flush();

    result = oss.str();
    result.pop_back();

    return result;
}

/// Helper function for graph visualization
/// Escapes the required special symbols for HTML
inline std::string escape_to_html(std::string input)
{
    std::string output;
    for(auto c : input) {
        switch(c) {
            case '&':
                output.append("&amp;");
                break;
            case '<':
                output.append("&lt;");
                break;
            case '>':
                output.append("&gt;");
                break;
            case '"':
                output.append("\\\"");
                break;
            default:
                output.push_back(c);
                break;
        }
    }
    return output;
}

/// Colored error printing function
inline void error_print(std::string message)
{
    llvm::WithColor::error();
    llvm::errs() << message << "\n";
}

/// Colored warning printing function
inline void warning_print(std::string message)
{
    llvm::WithColor::warning();
    llvm::errs() << message << "\n";
}

#endif
