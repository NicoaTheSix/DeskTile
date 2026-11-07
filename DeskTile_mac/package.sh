#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
printf 'Input app: %s/release/DeskTiles.app\nInput README: %s/README.md\nOutput ZIP: %s/release/DeskTiles-macOS-universal.zip\nOutput SHA256: %s/release/SHA256SUMS.txt\n' "$PWD" "$PWD" "$PWD" "$PWD"
printf '[1/3] Verify complete signed app before packaging\n'
codesign --verify --deep --strict release/DeskTiles.app
test -f release/DeskTiles.app/Contents/Resources/DeskTiles.icns
printf '[2/3] Copy instructions and create download\n'
cp README.md release/使用說明.md
cp Sources/third_party/nlohmann/LICENSE.MIT release/THIRD-PARTY-NOTICES.txt
cp assets/範例配置.json release/範例配置.json
ditto -c -k --sequesterRsrc --keepParent release/DeskTiles.app release/DeskTiles-macOS-universal.zip
printf '[3/3] Record SHA256\n'
(cd release && shasum -a 256 DeskTiles-macOS-universal.zip > SHA256SUMS.txt)
