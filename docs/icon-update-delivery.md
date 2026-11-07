# DeskTiles v1.01 圓角圖示交付

使用者選定 A 圓角幾何並授權部署、更新 Windows／Mac 及交給 Portfolio 網站專案。來源概念：D:\DeskTile\assets\icon-concepts\A-rounded-geometric-r01.png，概念快照 933eabed33af563a9f9615f46367db1c7a1d2161。

正式素材以平滑向量重繪，保留四圖形 2×2 排列，白色圓角底板之外透明。Windows 內嵌九種圖示尺寸；Mac 具 Finder CFBundleIconFile 入口，控制面板與 Dock 共用重新簽章前打包的 ICNS。

## 下載與素材

- Windows：D:\DeskTile\release\DeskTiles-Windows-x64.zip；504041 bytes；SHA256 507C4B6DC30CA8D50418D4EE83A60847E0CBC3CDBB04B31CB328D4A9FC0FD79E。
- Windows EXE：D:\DeskTile\release\DeskTiles\DeskTiles.exe；1068032 bytes；SHA256 DC69FBB53EA3454954B0140983D48227F0F21A7AA7D857AFE4002FF093A90606。
- Mac Universal 2：D:\DeskTile\DeskTile_mac\release\DeskTiles-macOS-universal.zip；425519 bytes；SHA256 9A4182F469863228F3B63F5899C2BA3FB269541632D4849C3447E7672EFA7032。
- 共用素材：D:\DeskTile\assets\DeskTiles.svg、DeskTiles-logo.png、DeskTiles.ico。
- Mac 圖示：D:\DeskTile\DeskTile_mac\assets\DeskTiles.icns。
- 文件：D:\DeskTile\README.md、docs\使用說明.md、docs\UserGuide.en.md、DeskTile_mac\README.md。
- 含新圖示的中英文控制面板：D:\DeskTile\docs\control.png、control-en.png。

## 檢查結果

Windows：222 核心；192 視窗整合；18 語言與重啟保護；英文完整整合 192；ZIP 解壓後執行 192。verify-icons.ps1 確認九個 ICO frame 解碼、透明外角、平滑邊緣、Windows LoadImage 及 EXE 內嵌位元組逐一一致，檔案版本為 1.01。

Mac：GitHub macOS 原生工作 42359672604 成功，170 核心、標準／Retina ICNS 解碼、Finder 入口、arm64／x86_64、codesign 完整驗證及 AppKit 圖示載入／既有整合。已再次核對下載 ZIP CRC、bundle 版本、ICNS／PNG／SVG 與來源位元組、Mach-O 雙架構、簽章清單及 Unix 執行權限。

工作紀錄：https://github.com/NicoaTheSix/DeskTile/actions/runs/42359672604

## 範圍與限制

為避免更新圖示時退回 GitHub 舊 Windows 功能，本批一起納入本機先前已交付的語言、鬧鐘與啟動同意來源和說明，沒有額外啟用開機啟動。Mac 沿用 GitHub afe5098 原生實作；沒有宣稱 Mac 新增 Windows 的鬧鐘或中英文功能。

Mac 仍為 ad-hoc 簽章，未申請 Apple 公證；Intel 實機、多螢幕、其他 macOS 版本驗收限制沿用。Windows 已知持久化鬧鐘重啟後時脈啟用問題尚待獨立處理，本次未修復或宣稱通過到期提醒回歸。

兩平台更新前先退出舊程式，完整解壓縮新包。舊捷徑可能暫存舊圖示；此更新不修改個人版面資料。

## Portfolio 交接要求

使用者已授權交給「GitHub網站管理員」更新並發布。請更新專案卡、DeskTiles 中英文頁面／文件、Logo、兩平台下載與 SHA256，沿用原網站發布流程，保留目前其他已部署功能。Windows 與 macOS 功能範圍分開如實說明；Mac 下載不得誤標成 Windows EXE 或已 Apple 公證。

資料與素材全部取自以上正式來源；不要使用舊交接段落中的歷史雜湊。原工作區未提交內容會另存可復原的 Git stash，這不是其他歷史任務驗收。

平台配置限制（原始碼核對）：Windows匯出JSON v3並讀取v1–v3；Mac僅匯出/讀取v1–v2，尚不能直接匯入Windows新版v3，即使未使用鬧鐘亦然。Mac v2可由Windows匯入；本批不新增Mac v3支援。
