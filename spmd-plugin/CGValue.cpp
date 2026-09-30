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

#include "CGValue.h"
#include <llvm/Support/raw_ostream.h>
#include <optional>


CGValue::~CGValue() 
{
}


std::optional<std::int64_t> CGValue::get_as_integer() const
{
    switch(_kind) {
        case Kind::Integer:
            return _int_value;
        case Kind::Boolean:
            return _bool_value ? 1 : 0;
        default:
            return std::nullopt;
    }
}
   

std::optional<bool> CGValue::get_as_boolean() const
{
    switch(_kind) {
        case Kind::Integer:
            return (_int_value > 0) ? true : false;
        case Kind::Boolean:
            return _bool_value;
        default:
            return std::nullopt;
    }
}

void CGValue::dump() const
{
    llvm::errs() << get_as_string();
}

std::string CGValue::get_as_string() const
{
    switch (_kind) {
        case Kind::Bottom:
            return "Bottom";
            break;
        case Kind::Top:
            return "Top";
            break;
        case Kind::Integer:
            return std::to_string(_int_value);
            break;
        case Kind::Boolean:
            if (_bool_value) {
                return "True";
            } else {
                return "False";
            }
            break;
        default:
            break;
    }
}

CGValue operator<(const CGValue &lhs, const CGValue &rhs)
{
    const auto lval = lhs.get_as_integer();
    const auto rval = rhs.get_as_integer();

    if(!lval || !rval) {
        return CGValue::top();
    }
    return CGValue::integer(lval.value() < rval.value());
}

CGValue operator>(const CGValue &lhs, const CGValue &rhs)
{
    const auto lval = lhs.get_as_integer();
    const auto rval = rhs.get_as_integer();

    if(!lval || !rval) {
        return CGValue::top();
    }
    return CGValue::integer(lval.value() > rval.value());
}

CGValue operator<=(const CGValue &lhs, const CGValue &rhs)
{
    const auto lval = lhs.get_as_integer();
    const auto rval = rhs.get_as_integer();

    if(!lval || !rval) {
        return CGValue::top();
    }
    return CGValue::integer(lval.value() <= rval.value());
}

CGValue operator>=(const CGValue &lhs, const CGValue &rhs)
{
    const auto lval = lhs.get_as_integer();
    const auto rval = rhs.get_as_integer();

    if(!lval || !rval) {
        return CGValue::top();
    }
    return CGValue::integer(lval.value() >= rval.value());
}

CGValue operator==(const CGValue &lhs, const CGValue &rhs)
{
    const auto lval = lhs.get_as_integer();
    const auto rval = rhs.get_as_integer();

    if(!lval || !rval) {
        return CGValue::top();
    }
    return CGValue::boolean(lval.value() == rval.value());
}

CGValue operator!=(const CGValue &lhs, const CGValue &rhs)
{
    const auto lval = lhs.get_as_integer();
    const auto rval = rhs.get_as_integer();

    if(!lval || !rval) {
        return CGValue::top();
    }
    return CGValue::boolean(lval.value() != rval.value());
}

CGValue operator+(const CGValue &lhs, const CGValue &rhs)
{
    const auto lval = lhs.get_as_integer();
    const auto rval = rhs.get_as_integer();

    if(!lval || !rval) {
        return CGValue::top();
    }
    return CGValue::integer(lval.value() + rval.value());
}


CGValue operator-(const CGValue &lhs, const CGValue &rhs)
{
    const auto lval = lhs.get_as_integer();
    const auto rval = rhs.get_as_integer();

    if(!lval || !rval) {
        return CGValue::top();
    }
    return CGValue::integer(lval.value() - rval.value());
}

CGValue operator*(const CGValue &lhs, const CGValue &rhs)
{
    const auto lval = lhs.get_as_integer();
    const auto rval = rhs.get_as_integer();

    if(!lval || !rval) {
        return CGValue::top();
    }
    return CGValue::integer(lval.value() * rval.value());
}

CGValue operator/(const CGValue &lhs, const CGValue &rhs)
{
    const auto lval = lhs.get_as_integer();
    const auto rval = rhs.get_as_integer();

    if(!lval || !rval) {
        return CGValue::top();
    }
    return CGValue::integer(lval.value() / rval.value());
}

CGValue operator%(const CGValue &lhs, const CGValue &rhs)
{
    const auto lval = lhs.get_as_integer();
    const auto rval = rhs.get_as_integer();

    if(!lval || !rval) {
        return CGValue::top();
    }
    return CGValue::integer(lval.value() % rval.value());
}

CGValue operator << (const CGValue &lhs, const CGValue &rhs)
{
    const auto lval = lhs.get_as_integer();
    const auto rval = rhs.get_as_integer();

    if(!lval || !rval) {
        return CGValue::top();
    }
    return CGValue::integer(lval.value() << rval.value());
}

CGValue operator>>(const CGValue &lhs, const CGValue &rhs)
{
    const auto lval = lhs.get_as_integer();
    const auto rval = rhs.get_as_integer();

    if(!lval || !rval) {
        return CGValue::top();
    }
    return CGValue::integer(lval.value() >> rval.value());
}

CGValue operator&(const CGValue &lhs, const CGValue &rhs)
{
    const auto lval = lhs.get_as_integer();
    const auto rval = rhs.get_as_integer();

    if(!lval || !rval) {
        return CGValue::top();
    }
    return CGValue::integer(lval.value() & rval.value());
}

CGValue operator^(const CGValue &lhs, const CGValue &rhs)
{
    const auto lval = lhs.get_as_integer();
    const auto rval = rhs.get_as_integer();

    if(!lval || !rval) {
        return CGValue::top();
    }
    return CGValue::integer(lval.value() ^ rval.value());
}

CGValue operator|(const CGValue &lhs, const CGValue &rhs)
{
    const auto lval = lhs.get_as_integer();
    const auto rval = rhs.get_as_integer();

    if(!lval || !rval) {
        return CGValue::top();
    }
    return CGValue::integer(lval.value() | rval.value());
}

CGValue operator&&(const CGValue &lhs, const CGValue &rhs)
{
    const auto lval = lhs.get_as_integer();
    const auto rval = rhs.get_as_integer();

    if(!lval || !rval) {
        return CGValue::top();
    }
    return CGValue::integer(lval.value() && rval.value());
}

CGValue operator||(const CGValue &lhs, const CGValue &rhs)
{
    const auto lval = lhs.get_as_integer();
    const auto rval = rhs.get_as_integer();

    if(!lval || !rval) {
        return CGValue::top();
    }
    return CGValue::integer(lval.value() || rval.value());
}

CGValue operator+(const CGValue &rhs)
{
    const auto rval = rhs.get_as_integer();
    
    if(!rval) {
        return CGValue::top();
    }
    return CGValue::integer(+rval.value());
}

CGValue operator-(const CGValue &rhs)
{
    const auto rval = rhs.get_as_integer();
    
    if(!rval) {
        return CGValue::top();
    }
    return CGValue::integer(-rval.value());
}

CGValue operator~(const CGValue &rhs)
{
    const auto rval = rhs.get_as_integer();
    
    if(!rval) {
        return CGValue::top();
    }
    return CGValue::integer(~rval.value());
}

CGValue operator!(const CGValue &rhs)
{
    const auto rval = rhs.get_as_integer();
    
    if(!rval) {
        return CGValue::top();
    }
    return CGValue::integer(!rval.value());
}
