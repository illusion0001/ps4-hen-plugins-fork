#!/bin/bash

# Build int3 codecave
bash int3.sh 4096 > common/cave.inc.c

for dr in plugin_*/ ; do
    if [ -d "$dr" ]; then
        echo "::group::Build $dr"
        make -C "$dr" clean all
        echo "::endgroup::"
    fi
done
