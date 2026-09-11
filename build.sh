#!/usr/bin/env bash
# Build the Better Transition KWin effect.
set -euo pipefail

cd "$(dirname "$0")"

BUILD_TYPE="${BUILD_TYPE:-Release}"

cmake -S . -B build \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
cmake --build build -j"$(nproc)"

echo
echo "Build finished:"
echo "  build/bin/bettertransition.so"
echo "  build/bin/kwin_bettertransition_config.so"
echo
echo "Install with: sudo cmake --install build   (or ./install.sh)"
