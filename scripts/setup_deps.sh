#!/bin/bash
# Downloads the Windows cross toolchain and libraries into tools/. Needs git, curl and tar.
set -e
R=$(cd "$(dirname "$0")/.." && pwd)
mkdir -p "$R/tools/vendor" "$R/tools/llvm-mingw"
cd "$R/tools/vendor"
[ -d minhook ] || git clone -q --depth 1 --branch v1.3.4 https://github.com/TsudaKageyu/minhook.git
[ -d imgui ] || git clone -q --depth 1 --branch v1.92.1 https://github.com/ocornut/imgui.git
[ -d json ] || git clone -q --depth 1 --branch v3.12.0 https://github.com/nlohmann/json.git
cd "$R/tools/llvm-mingw"
T=llvm-mingw-20261020-ucrt-ubuntu-22.04-x86_64.tar.xz
[ -d "${T%.tar.xz}" ] || { curl -sSL -o "$T" "https://github.com/mstorsjo/llvm-mingw/releases/download/20261020/$T"; tar xf "$T"; rm "$T"; }
