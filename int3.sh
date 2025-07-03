#!/usr/bin/env bash

if [ -z "$1" ]; then
    echo "Usage: $0 <code cave size>"
    exit 1
fi

a1=$1

echo "#define MAX_CAVE_SIZE ${a1}ul"
echo "static __attribute__((naked)) void cavePadFunc(void)"
echo "{"
for ((i=0; i<a1; i++)); do
    echo "    asm(\"int3\\n\"); // $i"
done
echo "}"
