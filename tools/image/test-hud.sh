#!/usr/bin/env bash
# Host-only SDL dummy tests; does not exercise ARM/KMS or actual hardware.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
sdl_config=${SDL2_CONFIG:-sdl2-config}
command -v "$sdl_config" >/dev/null || { echo 'SDL2_CONFIG or sdl2-config is required' >&2; exit 1; }
tmp=$(mktemp -d "${TMPDIR:-/tmp}/pidos-hud.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
# sdl2-config deliberately returns multiple compiler/linker arguments.
# shellcheck disable=SC2046
"${CXX:-c++}" -std=c++11 -O2 -Wall -Wextra -Werror $($sdl_config --cflags) \
    "$root/tools/image/test-hud.cpp" $($sdl_config --libs) -pthread -o "$tmp/test-hud"
SDL_VIDEODRIVER=dummy "$tmp/test-hud"
