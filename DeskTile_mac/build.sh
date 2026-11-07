#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p build release/DeskTiles.app/Contents/MacOS release/DeskTiles.app/Contents/Resources
printf 'Building and testing C++ core…\n'
xcrun clang++ -std=c++17 -O2 -Wno-unknown-pragmas -I Sources tests/tests.cpp -o build/core-tests
./build/core-tests
printf 'Building native macOS application…\n'
xcrun clang++ -std=c++17 -fobjc-arc -arch arm64 -arch x86_64 -O2 -Wno-deprecated-declarations -Wno-unknown-pragmas -mmacosx-version-min=12.0 Sources/main.mm -framework Cocoa -framework ImageIO -o release/DeskTiles.app/Contents/MacOS/DeskTiles
cp assets/DeskTiles.svg release/DeskTiles.app/Contents/Resources/
cp assets/範例配置.json release/DeskTiles.app/Contents/Resources/sample.json
cp Info.plist release/DeskTiles.app/Contents/
codesign --force --sign - release/DeskTiles.app
printf 'Ready: %s/release/DeskTiles.app\n' "$PWD"
