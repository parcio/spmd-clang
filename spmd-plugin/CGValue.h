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

#ifndef CGVALUE_H
#define CGVALUE_H

#include <cstdint>
#include <optional>
#include <string>

class CGValue 
{
public:
    CGValue(CGValue &&) = default;
    CGValue(const CGValue &) = default;
    CGValue &operator=(CGValue &&) = default;
    CGValue &operator=(const CGValue &) = default;
    ~CGValue();

    static CGValue top() { return CGValue(Kind::Top); }
    static CGValue bottom() { return CGValue(Kind::Bottom); }
    static CGValue integer(std::int64_t value)
    {
      return CGValue(Kind::Integer, value);
    }
    static CGValue boolean(bool value)
    {
      return CGValue(Kind::Boolean, value);
    }

    bool is_top() { return _kind == Kind::Top; }
    bool is_bottom() { return _kind == Kind::Bottom; }
    bool is_integer() { return _kind == Kind::Integer; }
    bool is_boolean() { return _kind == Kind::Boolean; }

    std::optional<std::int64_t> get_as_integer() const;
    std::optional<bool> get_as_boolean() const;

    void dump() const;
    std::string get_as_string() const;

private:
    enum Kind
    {
      Top,
      Integer,
      Boolean,
      Bottom,
    };

    CGValue(Kind kind): _kind(kind) {}
    CGValue(Kind kind, std::int64_t value): _kind(kind), _int_value(value) {}
    CGValue(Kind kind, bool value): _kind(kind), _bool_value(value) {}

    Kind _kind;
    std::int64_t _int_value;
    bool _bool_value;
    
};

CGValue operator<(const CGValue &lhs, const CGValue &rhs);
CGValue operator>(const CGValue &lhs, const CGValue &rhs);
CGValue operator<=(const CGValue &lhs, const CGValue &rhs);
CGValue operator>=(const CGValue &lhs, const CGValue &rhs);
CGValue operator==(const CGValue &lhs, const CGValue &rhs);
CGValue operator!=(const CGValue &lhs, const CGValue &rhs);
CGValue operator+(const CGValue &lhs, const CGValue &rhs);
CGValue operator-(const CGValue &lhs, const CGValue &rhs);
CGValue operator*(const CGValue &lhs, const CGValue &rhs);
CGValue operator/(const CGValue &lhs, const CGValue &rhs);
CGValue operator%(const CGValue &lhs, const CGValue &rhs);
CGValue operator<<(const CGValue &lhs, const CGValue &rhs);
CGValue operator>>(const CGValue &lhs, const CGValue &rhs);
CGValue operator&(const CGValue &lhs, const CGValue &rhs);
CGValue operator^(const CGValue &lhs, const CGValue &rhs);
CGValue operator|(const CGValue &lhs, const CGValue &rhs);
CGValue operator&&(const CGValue &lhs, const CGValue &rhs);
CGValue operator||(const CGValue &lhs, const CGValue &rhs);

CGValue operator+(const CGValue &rhs);
CGValue operator-(const CGValue &rhs);
CGValue operator~(const CGValue &rhs);
CGValue operator!(const CGValue &rhs);


#endif 
