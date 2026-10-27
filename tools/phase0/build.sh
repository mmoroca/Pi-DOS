#!/usr/bin/env bash
# Native Linux build only. Cross compilation belongs to the later Buildroot gate.
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
cd "$root"
fetch_args=()
sdl_only=0
for argument in "$@"; do
    case "$argument" in
        --offline) fetch_args+=(--offline);;
        --sdl-only) sdl_only=1;;
        *) echo "Unknown argument: $argument (use --offline or --sdl-only)" >&2; exit 2;;
    esac
done
jobs=${JOBS:-4}
case "$jobs" in ''|*[!0-9]*|0) echo 'JOBS must be a positive integer' >&2; exit 2;; esac
for tool in python3 cmake ninja pkg-config make gcc g++ autoconf automake aclocal autoheader patch; do
    command -v "$tool" >/dev/null || { echo "Missing build tool: $tool" >&2; exit 1; }
done
python3 tools/phase0/fetch.py "${fetch_args[@]}"
revision() { python3 -c 'import json,sys; print(json.load(open("sources.lock.json"))[sys.argv[1]]["directory"])' "$1"; }
sdl="$root/output/src/$(revision sdl2)"
dosbox="$root/output/src/$(revision dosbox-x)"
prefix="$root/output/host-sdl2"
dosbox_build="$root/output/build/dosbox-x-sdl2"
logs="$root/output/logs"
mkdir -p "$logs" "$dosbox_build"
trap 'echo "Build stopped; inspect $logs for the failing step." >&2' ERR

echo 'Configuring native SDL2 (KMS/DRM + ALSA; no desktop backends)...'
cmake -S "$sdl" -B output/build/sdl2 -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$prefix" \
    -DCMAKE_INSTALL_LIBDIR=lib \
    -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST=OFF -DSDL_TESTS=OFF \
    -DSDL_X11=OFF -DSDL_WAYLAND=OFF -DSDL_KMSDRM=ON -DSDL_ALSA=ON \
    -DSDL_PULSEAUDIO=OFF -DSDL_PIPEWIRE=OFF -DSDL_JACK=OFF -DSDL_SNDIO=OFF \
    -DSDL_OPENGLES=ON > "$logs/sdl2-configure.log" 2>&1
cmake --build output/build/sdl2 --parallel "$jobs" > "$logs/sdl2-build.log" 2>&1
cmake --install output/build/sdl2 > "$logs/sdl2-install.log" 2>&1
# CMake may silently disable backends when host development libraries are absent.
sdl_config=output/build/sdl2/include-config-release/SDL2/SDL_config.h
for feature in SDL_VIDEO_DRIVER_KMSDRM SDL_VIDEO_OPENGL_ES2 SDL_AUDIO_DRIVER_ALSA SDL_INPUT_LINUXEV; do
    if ! grep -q "^#define $feature 1$" "$sdl_config"; then
        echo "Required SDL2 feature unavailable: $feature; inspect $logs/sdl2-configure.log" >&2
        exit 1
    fi
done
if [[ "$sdl_only" == 1 ]]; then
    echo "Built SDL2: $prefix/lib/libSDL2.so"
    exit 0
fi

install -m 0644 "$root/src/common/midi_ports.h" "$dosbox/src/gui/pidos_midi_ports.h"
install -m 0644 "$root/src/common/reboot_shortcut.h" "$dosbox/src/gui/pidos_reboot_shortcut.h"
install -m 0644 "$root/src/common/lcd_video.h" "$dosbox/include/pidos_lcd_video.h"
for source_patch in "$root"/patches/dosbox-x/*.patch; do
    [[ -f "$source_patch" ]] || continue
    if patch --batch --forward --fuzz=0 --dry-run -p1 -d "$dosbox" -i "$source_patch" >/dev/null 2>&1; then
        patch --batch --forward --fuzz=0 -p1 -d "$dosbox" -i "$source_patch"
    elif patch --batch --reverse --fuzz=0 --dry-run -p1 -d "$dosbox" -i "$source_patch" >/dev/null 2>&1; then
        echo "Already applied: $(basename -- "$source_patch")"
    else
        echo "Patch does not match the source tree: $source_patch" >&2
        exit 1
    fi
done
printf '#define PI_DOS_VERSION "%s"\n' "$(< "$root/VERSION")" > "$dosbox/include/pi_dos_version.h"
sha256sum patches/dosbox-x/*.patch > "$logs/dosbox-x-patches.sha256"

export PKG_CONFIG_PATH="$prefix/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
export LD_LIBRARY_PATH="$prefix/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export PATH="$prefix/bin:$PATH"
pkg-config --atleast-version=2.32.10 sdl2
echo 'Configuring DOSBox-X with native SDL2...'
if [[ ! -f "$dosbox/configure" ]]; then
    (cd "$dosbox" && ./autogen.sh) > "$logs/dosbox-x-autogen.log" 2>&1
fi
(
    cd "$dosbox_build"
    # Do not pass --disable-sdl3: this upstream revision treats its presence as enable.
    "$dosbox/configure" --enable-sdl2 --disable-sdlnet --disable-libslirp \
        --disable-x11 --disable-avcodec --disable-printer --disable-freetype \
        --disable-libfluidsynth --disable-mt32 \
        LDFLAGS="-Wl,-rpath,$prefix/lib ${LDFLAGS:-}"
) > "$logs/dosbox-x-sdl2-configure.log" 2>&1
grep -q '^#define C_SDL2 1' "$dosbox_build/config.h"
if grep -q '^#define C_SDL3 1' "$dosbox_build/config.h"; then
    echo 'Unexpected SDL3 selection in DOSBox-X configuration' >&2
    exit 1
fi
make -C "$dosbox_build" -j "$jobs" > "$logs/dosbox-x-sdl2-build.log" 2>&1
echo "Built: $dosbox_build/src/dosbox-x"
echo 'Build success does not establish ARM64, DOS execution, or KMS/DRM acceptance.'
