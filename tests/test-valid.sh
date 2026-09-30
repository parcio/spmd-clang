#!/usr/bin/env bash

source ../environment.sh 2> /dev/null 

GREEN="\033[1;32m"  
RED="\033[1;31m"
NC="\033[0m" 
NCBLD="\033[1m"

function test_valid() 
{
    OUTPUT=$(spmd-clang "$1" 2>&1)
    TESTLINE1="The CPN terminates (has 1 terminal states)"
    TESTLINE2="End: FinalizeMarker(),"

    FOUND_LINE1=false
    FOUND_LINE2=false
    
    if echo "$OUTPUT" | grep -q "$TESTLINE1"; then
        FOUND_LINE1=true
    fi
    if echo "$OUTPUT" | grep -q "$TESTLINE2"; then
        FOUND_LINE2=true
    fi
    if [ "$FOUND_LINE1" = true ] && [ "$FOUND_LINE2" = true ]; then
        echo -e "$1 : ${GREEN}PASSED${NC}"
    else
        echo -e "$1 : ${RED}FAILED${NC}"
    fi
}

echo -e "${NCBLD}Running tests for valid cases${NC}"
for file in valid-*.c; do
    test_valid "$file"
done
make clean > /dev/null 
