#!/bin/bash

for dr in plugin_*/ ; do
    if [ -d "$dr" ]; then
        make -C "$dr" clean all
    fi
done
