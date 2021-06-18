#!/bin/bash
set -e

rm -rf build 2>/dev/null || echo "skip rm build"
mkdir build
cd build
cmake $* ..
make -j$(nproc)
