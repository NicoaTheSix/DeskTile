#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p build
./build/core-tests
verification_dir=$(mktemp -d "$PWD/build/verification.XXXXXX")
printf 'Running AppKit verification in %s\n' "$verification_dir"
./release/DeskTiles.app/Contents/MacOS/DeskTiles --data-dir "$verification_dir" --smoke-test
cat "$verification_dir/smoke-result.txt"
grep -q '^PASS:' "$verification_dir/smoke-result.txt"
codesign --verify --deep --strict release/DeskTiles.app
lipo -info release/DeskTiles.app/Contents/MacOS/DeskTiles
