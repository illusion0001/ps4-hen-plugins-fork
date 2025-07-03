#!/usr/bin/env python3

import sys

def main():
    a1 = int(sys.argv[1])
    print(f"")
    print(f"#define MAX_CAVE_SIZE {a1}ul")
    print("static void cavePadFunc(void)\n{")
    for i in range(a1):
        print("    asm(\"int3\\n\");")
    print("}")

if __name__ == "__main__":
    main()
