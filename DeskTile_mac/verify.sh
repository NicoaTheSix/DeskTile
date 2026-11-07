#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
app="$PWD/release/DeskTiles.app"
printf 'Input app: %s\nInput ICNS: %s/Contents/Resources/DeskTiles.icns\nOutput verification root: %s/build\n' "$app" "$app" "$PWD"
mkdir -p build
printf '[1/4] Core regression checks\n'
./build/core-tests
printf '[2/4] Verify Finder/Dock icon, signature and architectures\n'
test "$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIconFile' "$app/Contents/Info.plist")" = "DeskTiles.icns"
icon_root=$(mktemp -d "$PWD/build/icon-verification.XXXXXX")
iconutil -c iconset "$app/Contents/Resources/DeskTiles.icns" -o "$icon_root/DeskTiles.iconset"
codesign --verify --deep --strict "$app"
lipo -verify_arch arm64 x86_64 "$app/Contents/MacOS/DeskTiles"
printf '[3/4] AppKit integration and loaded icon\n'
verification_dir=$(mktemp -d "$PWD/build/verification.XXXXXX")
"$app/Contents/MacOS/DeskTiles" --data-dir "$verification_dir" --smoke-test
cat "$verification_dir/smoke-result.txt"
grep -q '^PASS:' "$verification_dir/smoke-result.txt"
printf '[4/4] All checks passed: %s\n' "$verification_dir"
