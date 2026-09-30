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

#ifndef CGNODEINSTANCE_H
#define CGNODEINSTANCE_H

#include "CGContext.h"
#include "CGNode.h"
#include "CGStatementVisitor.h"
#include <clang/Analysis/CFG.h>

class CGNodeInstance : public CGNode
{
public:
    CGNodeInstance(const CGContext &initial, const CGNode &node, clang::ASTContext *ctx);
    CGNodeInstance(CGNodeInstance &&) = delete;
    CGNodeInstance(const CGNodeInstance &) = delete;
    CGNodeInstance &operator=(CGNodeInstance &&) = delete;
    CGNodeInstance &operator=(const CGNodeInstance &) = delete;

    const CGContext &context() const { return _context; }
    // TODO remove this again after LOOP STARTS testing is done
    CGContext &context() { return _context; }
    CGStatementVisitor &visitor() { return _visitor; }
    const CGNode *original() const { return _original; }

    void dump() const override;
    void dump2() const;
    std::string get_evaluated_cfg_as_string(const clang::CFGElement &element) const;

private:
    void append_element(const clang::CFGElement &element) override;
    std::string cgv_to_str(std::optional<CGValue> value) const;

    const CGNode *_original;
    CGContext _context;
    CGStatementVisitor _visitor;
    clang::ASTContext *_ctx;
};

#endif
