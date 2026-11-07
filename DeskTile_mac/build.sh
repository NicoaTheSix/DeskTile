#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
root="$PWD"
app="$root/release/DeskTiles.app"
printf 'Input sources: %s/Sources/main.mm\nInput icon PNG: %s/assets/DeskTiles-logo.png\nOutput app: %s\nOutput iconset: %s/build/DeskTiles.iconset\n' "$root" "$root" "$app" "$root"
mkdir -p build "$app/Contents/MacOS" "$app/Contents/Resources"
printf '[1/5] Building and testing C++ core…\n'
xcrun clang++ -std=c++17 -O2 -Wno-unknown-pragmas -I Sources tests/tests.cpp -o build/core-tests
./build/core-tests
printf '[2/5] Creating smooth standard and Retina app icons…\n'
iconset="$root/build/DeskTiles.iconset"
mkdir -p "$iconset"
for size in 16 32 128 256 512; do
  sips -z "$size" "$size" assets/DeskTiles-logo.png --out "$iconset/icon_${size}x${size}.png" >/dev/null
  retina=$((size * 2))
  sips -z "$retina" "$retina" assets/DeskTiles-logo.png --out "$iconset/icon_${size}x${size}@2x.png" >/dev/null
done
iconutil -c icns "$iconset" -o "$app/Contents/Resources/DeskTiles.icns"
printf '[3/5] Building Universal 2 native macOS application…\n'
xcrun clang++ -std=c++17 -fobjc-arc -arch arm64 -arch x86_64 -O2 -Wno-deprecated-declarations -Wno-unknown-pragmas -mmacosx-version-min=12.0 Sources/main.mm -framework Cocoa -framework ImageIO -o "$app/Contents/MacOS/DeskTiles"
cp assets/DeskTiles.svg "$app/Contents/Resources/"
cp assets/DeskTiles-logo.png "$app/Contents/Resources/"
cp assets/範例配置.json "$app/Contents/Resources/sample.json"
cp Info.plist "$app/Contents/"
printf '[4/5] Signing the complete app including its icon…\n'
codesign --force --sign - "$app"
codesign --verify --deep --strict "$app"
printf '[5/5] Ready: %s\n' "$app"
