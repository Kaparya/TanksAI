#!/bin/bash
set -e

cd "$(dirname "$0")/.."

echo "=== Building TANKS server ==="
mkdir -p build
cd build
CMAKE=$(command -v cmake || echo /opt/homebrew/opt/cmake/bin/cmake)
$CMAKE ..
make -j$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)
cd ..

echo ""
echo "=== Starting server ==="
./build/tanks_server "$@"
