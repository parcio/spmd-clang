LLVM_PATH="/llvm-install"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PLUGIN_PATH="${SCRIPT_DIR}/build/spmd-plugin/lib/libSPMDPlugin.so"

if [ -d "$LLVM_PATH" ]; then
    printf 'Setting spmdclang Path and aliases\n' >&2
    PATH="${LLVM_PATH}/bin:${PATH}"
    LD_LIBRARY_PATH="${LLVM_PATH}/lib:${LD_LIBRARY_PATH}"
    export PATH
    export LD_LIBRARY_PATH
else 
    printf 'Build directory does not exist' >&2
fi

export SPMD_PLUGIN="${PLUGIN_PATH}"

spmd-clang() {
    local c_file="$1"
    if [[ ! -f "$c_file" || ! "$c_file" =~ \.c$ ]]; then
        echo "Error: File must exist and have a .c extension"
        return 1
    fi
    local output_file="${c_file%.c}"
    MPICH_CC=clang mpicc -g -fplugin=$SPMD_PLUGIN -o "$output_file" "$c_file"
}

spmd-clang-ast() {
    local c_file="$1"
    if [[ ! -f "$c_file" || ! "$c_file" =~ \.c$ ]]; then
        echo "Error: File must exist and have a .c extension"
        return 1
    fi
    local output_file="${c_file%.c}"
    MPICH_CC=clang mpicc -g -fplugin=$SPMD_PLUGIN -Xclang -ast-dump -fno-color-diagnostics -o "$output_file" "$c_file" > "$output_file.ast"
}

spmd-clang-filter-errors() {
    local c_file="$1"
    if [[ ! -f "$c_file" || ! "$c_file" =~ \.c$ ]]; then
        echo "Error: File must exist and have a .c extension"
        return 1
    fi
    local output_file="${c_file%.c}"
    MPICH_CC=clang mpicc -g -fplugin=$SPMD_PLUGIN -o "$output_file" "$c_file" 2>&1 | grep -e warning -e error
}

spmd-clang-tracing() {
    local c_file="$1"
    if [[ ! -f "$c_file" || ! "$c_file" =~ \.c$ ]]; then
        echo "Error: File must exist and have a .c extension"
        return 1
    fi
    local output_file="${c_file%.c}"
    MPICH_CC=clang mpicc -g -ftime-trace="$output_file"-trace.json -fplugin=$SPMD_PLUGIN -o "$output_file" "$c_file"
}

spmd-clang-perf() {
    local c_file="$1"
    if [[ ! -f "$c_file" || ! "$c_file" =~ \.c$ ]]; then
        echo "Error: File must exist and have a .c extension"
        return 1
    fi
    local output_file="${c_file%.c}"
    MPICH_CC=clang perf record -F max clang -g -fplugin=$SPMD_PLUGIN -o "$output_file" "$c_file"
}

# Function to convert GraphViz files to PDF
dotviz() {
    local gv_file="$1"
    if [[ ! -f "$gv_file" || ! "$gv_file" =~ \.gv$ ]]; then
        echo "Error: File must exist and have a .gv extension"
        return 1
    fi
    local pdf_file="${gv_file%.gv}.pdf"
    dot -Tpdf "$gv_file" -o "$pdf_file" && $READER "$pdf_file" &
}
