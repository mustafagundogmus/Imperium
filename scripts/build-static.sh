#!/usr/bin/env bash
#
# Builds Arbitrium as a genuine single-file executable — the same way
# .github/workflows/release.yml builds the one that ships on the releases
# page — and NOT the way pacman's mingw-w64-x86_64-qt6-static package does.
#
# That package configures Qt -system-zlib, -system-freetype, -system-harfbuzz,
# -system-openssl and so on, and its CMake config files hardcode absolute
# import-library paths a consumer cannot override. The result is a "static"
# Qt whose executable still needs a dozen-plus MSYS2 DLLs beside it —
# libcrypto-3-x64.dll, libbrotlidec.dll, libb2-1.dll, libfreetype-6.dll among
# them. That is the exact set of "kod yürütülmesi ... bulunamadı" errors this
# script exists to avoid.
#
# Instead this builds qtbase + qtsvg from Qt's own source tarballs, with
# -DFEATURE_system_*=OFF so Qt compiles its own bundled zlib/libpng/jpeg/
# pcre2/harfbuzz/freetype/md4c, and with OpenSSL, brotli and zstd disabled
# entirely (TLS is Schannel-only). Nothing is left to resolve at runtime.
# Every release since 0.9.5 has been built this way, which is why it imports
# nothing but Windows itself.
#
# Built once and cached under Qt-static/<version>/ next to this repo —
# rebuilding Qt from source takes most of an hour, and only needs to happen
# again if QT_VERSION changes.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
QT_VERSION="${QT_VERSION:-6.11.1}"
QT_PREFIX="$ROOT_DIR/Qt-static/$QT_VERSION"
QT_SRC="$ROOT_DIR/Qt-static/src-$QT_VERSION"

if [ -f "$QT_PREFIX/lib/cmake/Qt6/Qt6Config.cmake" ]; then
    echo "==> Static Qt $QT_VERSION already built at $QT_PREFIX — skipping (delete that folder to force a rebuild)"
else
    echo "==> Building static Qt $QT_VERSION from source (qtbase + qtsvg)."
    echo "    First time only — this is the slow step, expect up to an hour."
    rm -rf "$QT_SRC"
    mkdir -p "$QT_SRC"
    cd "$QT_SRC"

    base="https://download.qt.io/archive/qt/${QT_VERSION%.*}/$QT_VERSION/submodules"
    for m in qtbase qtsvg; do
        echo "    fetching $m..."
        curl -fsSL "$base/$m-everywhere-src-$QT_VERSION.tar.xz" | tar -xJ
        mv "$m-everywhere-src-$QT_VERSION" "$m"
    done

    cmake -S qtbase -B build-qtbase -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$QT_PREFIX" \
        -DBUILD_SHARED_LIBS=OFF \
        -DQT_BUILD_EXAMPLES=OFF \
        -DQT_BUILD_TESTS=OFF \
        -DQT_BUILD_BENCHMARKS=OFF \
        -DFEATURE_system_zlib=OFF \
        -DFEATURE_system_libpng=OFF \
        -DFEATURE_system_jpeg=OFF \
        -DFEATURE_system_pcre2=OFF \
        -DFEATURE_system_harfbuzz=OFF \
        -DFEATURE_system_freetype=OFF \
        -DFEATURE_system_md4c=OFF \
        -DFEATURE_icu=OFF \
        -DFEATURE_dbus=OFF \
        -DFEATURE_openssl=OFF \
        -DFEATURE_schannel=ON \
        -DFEATURE_sql=OFF \
        -DFEATURE_system_libb2=OFF \
        -DFEATURE_brotli=OFF \
        -DFEATURE_zstd=OFF
    cmake --build build-qtbase --parallel
    cmake --install build-qtbase

    # Svg is a separate module; the app's icons are SVG paths and link it directly.
    cmake -S qtsvg -B build-qtsvg -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_PREFIX_PATH="$QT_PREFIX" \
        -DCMAKE_INSTALL_PREFIX="$QT_PREFIX"
    cmake --build build-qtsvg --parallel
    cmake --install build-qtsvg

    cd "$ROOT_DIR"
    rm -rf "$QT_SRC"
    echo "==> Static Qt $QT_VERSION built and cached at $QT_PREFIX"
fi

echo "==> Configuring Arbitrium against the static Qt"
cd "$ROOT_DIR"
cmake -S . -B build-static -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$QT_PREFIX" \
    -DCMAKE_RC_COMPILER=windres \
    -DCMAKE_RUNTIME_OUTPUT_DIRECTORY="$ROOT_DIR" \
    -DCMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE="$ROOT_DIR"

echo "==> Building Arbitrium"
cmake --build build-static

echo "==> Verifying the result is actually a single file (same check the release build uses)"
exe="$ROOT_DIR/Arbitrium.exe"
[ -f "$exe" ] || { echo "!! build produced no Arbitrium.exe" >&2; exit 1; }

system="$(cygpath -u "$(cmd //c 'echo %SystemRoot%' | tr -d '\r')")/System32"
outside=$(objdump -p "$exe" | sed -n 's/^\s*DLL Name: //p' | sort -fu \
          | while read -r dll; do
              case "$dll" in
                api-ms-win-*|ext-ms-*) continue ;;
              esac
              if [ ! -e "$system/$dll" ]; then echo "$dll"; fi
            done)

if [ -n "$outside" ]; then
    echo "!! Still not single-file — also needs: $(echo $outside | tr '\n' ' ')" >&2
    exit 1
fi

echo ""
echo "==> Arbitrium.exe imports nothing but Windows itself. No DLL needs to travel with it."
