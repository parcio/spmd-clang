#!/bin/bash

#  SPMDClang - Clang plugin for correctness checking of MPI programs
#  Copyright (C) 2026-2026  Michael Blesel
# 
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU Lesser General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
# 
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU Lesser General Public License for more details.
# 
#  You should have received a copy of the GNU Lesser General Public License
#  along with this program.  If not, see <https://www.gnu.org/licenses/>.

set -e

# Set this the location of your LLVM installation
LLVM_INSTALL_DIR=/home/michael/Projects/spmd-clang-build
CPUS=$(nproc)

if [ -n "$1" ] && [ $1 = "--rebuild" ]; then
    rm -rf build 
elif [ -n "$1" ]; then
    echo "use --rebuild to first clean build dir"
fi

mkdir -p build 
cd build

cmake -DCMAKE_C_COMPILER=${LLVM_INSTALL_DIR}/bin/clang -DCMAKE_CXX_COMPILER=${LLVM_INSTALL_DIR}/bin/clang++ -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCT_LLVM_INSTALL_DIR=${LLVM_INSTALL_DIR} ..
make -j$CPUS

cp ./compile_commands.json ../
