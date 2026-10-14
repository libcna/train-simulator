#!/usr/bin/env bash
set -euo pipefail

project_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
emsdk_root=${EMSDK_ROOT:-"$HOME/emsdk"}

if [[ ! -f "$emsdk_root/emsdk_env.sh" ]]; then
    echo "Emscripten SDK not found at $emsdk_root (set EMSDK_ROOT to override)." >&2
    exit 1
fi
if ! command -v ccache >/dev/null 2>&1; then
    echo "ccache is required." >&2
    exit 1
fi

source "$emsdk_root/emsdk_env.sh" >/dev/null
export CCACHE_DIR=/rv/cnaccache
export CCACHE_BASEDIR=/rv

emcmake cmake -S "$project_root" -B "$project_root/build-probe" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache \
    -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
    -DTRAINSIM_BUILD_GAME=ON \
    -DTRAINSIM_BUILD_TESTS=OFF
cmake --build "$project_root/build-probe" --target train-simulator --parallel 4

echo
echo "Web build: $project_root/build-probe/train-simulator.html"
echo "Serve it with: python3 -m http.server 8000 --directory $project_root/build-probe"
