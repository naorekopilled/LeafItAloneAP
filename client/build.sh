#!/bin/bash
# Usage: client/build.sh [test]  Output: client/build/version.dll
set -e
R=$(cd "$(dirname "$0")/.." && pwd)
CXX=$(ls "$R"/tools/llvm-mingw/*/bin/x86_64-w64-mingw32-clang++); CC=${CXX%++}
V=$R/tools/vendor; M=$V/minhook; I=$V/imgui
B=$R/client/build; mkdir -p "$B"; cd "$B"
[ -f hde64.o ] || "$CC" -O2 -c -I"$M/include" -I"$M/src" "$M"/src/buffer.c "$M"/src/hook.c "$M"/src/trampoline.c "$M"/src/hde/hde64.c
[ -f imgui.o ] || "$CXX" -O2 -std=c++20 -c -I"$I" -I"$I/backends" "$I"/imgui.cpp "$I"/imgui_draw.cpp "$I"/imgui_tables.cpp "$I"/imgui_widgets.cpp "$I"/backends/imgui_impl_dx11.cpp "$I"/backends/imgui_impl_win32.cpp
D=""; [ "$1" = test ] && D="-DLIA_TEST"
"$CXX" -O2 -std=c++20 -shared -static $D -I"$M/include" -I"$I" -I"$I/backends" -I"$V/json/single_include" -I"$R/client/src" \
  "$R"/client/src/{proxy,main,ap,game,overlay,test_cmd}.cpp buffer.o hook.o trampoline.o hde64.o imgui.o imgui_draw.o imgui_tables.o imgui_widgets.o imgui_impl_dx11.o imgui_impl_win32.o \
  "$R/client/src/version.def" -o version.dll -Wl,--kill-at -ld3d11 -ldxgi -ld3dcompiler -lws2_32 -lsecur32 -ldwmapi -lgdi32
ls -la version.dll
