#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
cp README.md release/使用說明.md
cp Sources/third_party/nlohmann/LICENSE.MIT release/THIRD-PARTY-NOTICES.txt
cp assets/範例配置.json release/範例配置.json
ditto -c -k --sequesterRsrc --keepParent release/DeskTiles.app release/DeskTiles-macOS-universal.zip
(cd release && shasum -a 256 DeskTiles-macOS-universal.zip > SHA256SUMS.txt)
