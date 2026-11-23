# DeskTiles Windows v1.07

2026-11-23。使用者驗收快照：c4717b7374c55df4d8eb8e91d0fedebd52103a18。這次升版將已核准的 Windows 桌面工具打包為正式下載版。

## 操作變更

雙擊文字磁磚改為直接編輯；從系統匣或右鍵「開啟控制視窗」返回面板。新增拖放／貼上、復原／重做、回收桶、位置鎖定、滑鼠穿透與恢復輸入、自訂全域快捷鍵、螢幕排列記憶、專注模式、樣板庫、分享包及離線資料夾同步。

升級前請先儲存並結束舊程式。原有資料保留在 %LOCALAPPDATA%\HexDesk；匯入、同步帶入的計時器及鬧鐘先停用。macOS 下載維持 v1.02。

## 範圍與驗證

本次正式 EXE 重驗 235 項核心、24 項便利功能核心、31 項原生便利功能、192 項 ZIP 解壓後 Windows 整合，並實際驗證 30＋34 項置頂／桌面層、遮擋、透明點選、拖曳及圖片顯示。

已驗收快照另通過 38 項配置／CLI（含重啟後背景鬧鐘）、8 項黑白磁磚、14 項鼠尾草綠樣板測試。正式升版沒有更動這些功能的原始碼。

Steam App ID／SDK 尚未取得。SDK 編譯及 Steam Cloud、Workshop 真實連線留待後續；目前提供離線資料夾同步與分享包。實體螢幕插拔、獨占全螢幕遊戲尚待實機確認。

## 建置與發布流程

輸入：D:\DeskTile\build\convenience-v1.07 的已驗收原始碼、VERSION、DeskTiles.rc 與雙語指南。

在 Windows PowerShell 5.1 執行 build.ps1 -OutputName DeskTiles-update.exe；確認編譯及核心測試通過後，以 package.ps1 -ExecutableName DeskTiles-update.exe 產生 release\DeskTiles-Windows-x64.zip、release\DeskTiles\DeskTiles.exe 與 SHA256SUMS.txt。

verify-convenience.ps1、verify-package.ps1 -Composition 及 verify-icons.ps1 分階段列出輸入輸出絕對路徑並驗證。最終文件重新打包後，再執行 verify-package.ps1；核對 ZIP 內雙語說明與原檔一致。

將發布提交快轉合併至 D:\DeskTile 的 main，建立 v1.07 標籤，再以 atomic push 推送 main 與標籤。正式檔案位於 D:\DeskTile\release。Portfolio 交接見 portfolio-v1.07-handoff.md。每個檔案的大小、雜湊及來源另列於 windows-v1.07-artifacts.json。
