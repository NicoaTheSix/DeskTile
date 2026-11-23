# DeskTiles Windows v1.07：Portfolio 更新交接

日期：2026-11-23。使用者已驗收快照 c4717b7374c55df4d8eb8e91d0fedebd52103a18，並授權部署 Windows、推上 GitHub、交由 Portfolio 網站專案更新。正式版為 v1.07；macOS 維持 v1.02。

## 更新項目

更新 DeskTiles 的繁中／英文頁面、Windows 下載 ZIP、EXE、SHA256、中英文操作說明與 README。以此批檔案取代舊 Windows 版，沿用網站現有樣式、導覽和追蹤設定。

新增文字格直接編輯、拖放圖片與剪貼簿貼上、復原／重做與回收桶、位置鎖定及滑鼠穿透、自訂全域快捷鍵、多螢幕位置復原、專注模式、四款配置樣板、分享包及離線同步。功能從磁磚或系統匣右鍵的「桌面工具」進入。

Steam 實際連線待取得 App ID／SDK 後處理。這次只能宣稱離線同步、分享包和衝突備份已驗證；SDK 區段編譯、Steam Cloud 上傳／下載、工作坊發布／訂閱尚未驗證。實體螢幕插拔與獨占全螢幕遊戲也尚待實機確認。

## 最終來源

| 檔案 | 本機路徑 |
| --- | --- |
| Windows ZIP | D:\DeskTile\release\DeskTiles-Windows-x64.zip |
| Windows EXE | D:\DeskTile\release\DeskTiles\DeskTiles.exe |
| 命令列工具 | D:\DeskTile\release\DeskTiles\desktiles-cli.exe |
| SHA256 | D:\DeskTile\release\SHA256SUMS.txt |
| 繁中使用說明 | D:\DeskTile\docs\使用說明.md |
| English user guide | D:\DeskTile\docs\UserGuide.en.md |
| English README | D:\DeskTile\README.md |
| 繁中 README | D:\DeskTile\README.zh-TW.md |
| 實際樣板庫預覽 | D:\DeskTile\docs\desktop-tools-presets.png |
| 部署說明 | D:\DeskTile\docs\windows-v1.07-deployment.md |

ZIP：1004683 bytes；SHA256：BD515EB5E036A2F9EA9F9C5E1AEDE00C391297488BBE9CABB02795F8E860733E。

EXE：1587712 bytes；SHA256：89FA0598F6681438CC3E259F5A1E72FDD26787CD00DD7FCE13594EC7E4EC49A4。檔案版本與產品版本均為 1.07。

GitHub 版本連結：
- https://github.com/NicoaTheSix/DeskTile/tree/v1.07
- https://raw.githubusercontent.com/NicoaTheSix/DeskTile/v1.07/release/DeskTiles-Windows-x64.zip
- https://raw.githubusercontent.com/NicoaTheSix/DeskTile/v1.07/release/DeskTiles/DeskTiles.exe

## 發布核對

核對中英文頁面皆顯示 Windows v1.07，下載 ZIP 與 EXE 的 SHA256 與上列一致；站上兩份操作說明須包含本批桌面工具及 Steam 離線範圍。macOS 下載維持 v1.02。請回報網站提交、正式網址、下載雜湊與說明檢查結果。

升級前先從系統匣儲存並結束舊版；解壓後執行 DeskTiles.exe。桌面工具不會替使用者開啟登入自動啟動。
