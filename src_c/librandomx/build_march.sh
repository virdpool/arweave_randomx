#!/bin/bash
set -e

rm -rf build_$1 2>/dev/null || echo "skip rm build"
mkdir build_$1
cd build_$1
# cmake -DARCH=$1 ..
# cmake -DARCH_ID=$1 ..
cmake -DARCH=$1 $2 ..
make -j$(nproc)
