# DeskTiles 圓角黑白 Logo

採用使用者選定的 A 圓角幾何提案；保留六角形、三角形、矩形、圓形的 2×2 排列，白色圓角底板有一圈 28/1024 的深灰（#404040）細邊；原外輪廓與內部四圖形維持原樣，底板之外仍為透明。

## 來源與輸出

DeskTiles.svg 是唯一向量母稿，路徑使用 M/L/C/Z 指令。build-logo.ps1 直接渲染母稿，不再讀取舊的 logo-grid.txt；舊像素來源保留供歷史參考。

執行 Windows PowerShell 5.1：
powershell -NoProfile -ExecutionPolicy Bypass -File .\build-logo.ps1

執行前顯示輸入 SVG、輸出 PNG／ICO 與 Mac 素材目錄的絕對路徑，依四階段顯示進度：
1. 讀取母稿，檢查可用繪圖組件。
2. 以四倍取樣及抗鋸齒產生 1024×1024 PNG，保留外角 alpha。
3. 每個 ICO 尺寸都直接從向量重新渲染，再封裝 16／20／24／32／40／48／64／128／256px PNG frame，供檔案、工作列、視窗與系統匣使用。
4. 同步 DeskTile_mac/assets 的 SVG 與 PNG，並由同一母稿產生 16–1024px 的 ICNS 素材（含 Retina）。Mac build.sh 仍以 sips／iconutil 製作正式 App bundle 的 ICNS，於簽章前加入。

原圖示建置時嵌入 Windows EXE；Mac 的 Info.plist 設定 CFBundleIconFile，控制面板與 Dock 載入相同 ICNS。更新後須退出舊 App 再開啟新包；舊捷徑可能暫時保留系統圖示快取。

設計提案與採用來源：原工作區 D:\DeskTile\assets\icon-concepts\A-rounded-geometric-r01.png；概念快照 933eabed33af563a9f9615f46367db1c7a1d2161。本次以原生向量重繪正式製作圖，去除提案展示畫布；不是直接縮小生成圖片。
