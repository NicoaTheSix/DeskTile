# 桌面拼格 DeskTiles

專案倉庫：[NicoaTheSix/DeskTile](https://github.com/NicoaTheSix/DeskTile)。本機開發目錄為 `D:\DeskTile`；免安裝執行檔位於 `release/DeskTiles/DeskTiles.exe`。搬移不影響原本的使用者版面資料。

可在 Windows 桌面拼放六角形、三角形與矩形，顯示文字、圖片或計時器。支援多選拖曳、吸附、置頂／桌面層、背景透明、像素／RGB／呼吸邊框及含圖片的 JSON 配置。

## 一般使用者從這裡開始

1. 取得 [DeskTiles-Windows-x64.zip](release/DeskTiles-Windows-x64.zip)，完整解壓縮。
2. 進入解壓後的 `DeskTiles` 資料夾，雙擊 `DeskTiles.exe`。
3. 左側選形狀並新增；右側編輯內容後按套用。

**免安裝 ZIP 解壓後即可執行。** 適用 Windows 10／11 x64，不需要安裝 Visual Studio、Python、.NET 或其他額外執行套件，也不需要管理員權限。EXE 使用靜態 C++ 執行階段，Logo 已內嵌。

**原始碼專案需要建置。** 若只下載程式原始碼而沒有使用版 EXE，請依下方開發指令建置。Linux 版尚未開發，列於後續規劃。

本機已解壓的使用版位於 `release/DeskTiles/DeskTiles.exe`。完整步驟見 [使用說明](docs/使用說明.md)，套件提供 `README.txt`、使用說明、內嵌圖片的範例配置與第三方授權。

Portfolio 網站管理員的上架資料與素材來源見 [網站上架交接](docs/網站上架交接.md)。

## 常用操作

| 要做的事 | 操作 |
| --- | --- |
| 新增 | 左側選六角形（預設）、朝上正三角形或 3:2 矩形，再按「新增圖形」 |
| 多選／移動 | Shift＋單擊多選，拖曳任一選取格移動整組；靠近其他格吸附 |
| 文字、圖片、計時器 | 選格後在右側選內容類型，設定後按套用 |
| 大小與字體 | 「大小與文字…」；寬度 80～480 px，字級 8～96 px |
| 外觀 | 「邊框與風格…」「背景與透明度…」，可多選套用 |
| 顯示層 | 右鍵或控制視窗選置頂／固定桌面 |
| 備份與搬移 | 「配置 JSON…」匯出含圖片的配置；匯入可覆蓋或合併 |
| 找回圖形 | 系統匣選「找回全部圖形」 |
| 結束 | 先套用編輯，再從右鍵／系統匣選「儲存並結束桌面拼格」 |

關閉控制視窗只會隱藏面板，圖形仍在背景運作。再次雙擊圖形或重新執行程式可帶回控制視窗。換新版之前請先儲存並結束舊版。

![控制視窗](docs/control.png)

## 功能與範圍

- 六角形、正三角形、3:2 矩形，獨立無邊框視窗；圖形外可操作原桌面。
- 文字、PNG／JPEG／BMP 圖片、正向／倒數計時器；多選可批次套用。
- 多選保持相對位置、混合形狀碰撞檢查與 22 px 範圍吸附；重疊放置回復原位。
- 自訂尺寸、字體、字級、文字顏色；縮放造成碰撞時整批保留原設定。
- 邊框實色、像素、RGB 彩虹與指定色呼吸燈；厚度 1～12 px，動畫週期 2～60 秒。
- 背景顏色及 0～100% 透明度獨立套用。只影響背景；全透明區仍可選取和拖曳。
- 控制介面 18 px、標題 32 px，黑白像素 2×2 Logo；內容字級另行設定。
- JSON 含全版面與 Base64 圖片，支援覆蓋及合併；匯入格子的計時器預設暫停。
- 最多 500 格；JSON 128 MiB，單張圖片 16 MiB，圖片總量 64 MiB，解析度上限 6400 萬像素。

## 資料位置與啟動問題

預設資料存於 `%LOCALAPPDATA%\HexDesk`，保留舊名稱以沿用既有版面。可執行：

```powershell
.\DeskTiles.exe --data-dir 'D:\DeskTilesData'
```

啟動時會先顯示版面與日誌的絕對路徑及進度。版面為 `layout.hexdesk`，上次保存備份為 `layout.backup`；JSON 匯入另存 `import-before-*.hexdesk`。匯入的圖片存於 `assets`，使用 SHA256 命名。選自其他資料夾的圖片仍引用來源路徑，請用 JSON 一起備份。

同一資料目錄只允許一份程式寫入。重複啟動時會找到資料鎖持有者，帶回該執行個體的控制視窗；不會重設未套用的欄位或終止舊版。若確實無法寫入，會顯示資料位置及 Windows 錯誤碼。`session.lock` 留在磁碟上屬正常情況，不應靠刪除它處理啟動問題。

## 執行流程與進度

- **啟動**：解析資料位置 → 取得資料鎖或帶回既有視窗 → 顯示絕對輸入／輸出路徑 → 五階段初始化 → 載入版面與圖示 → 建立圖形、控制視窗及系統匣 → 等待操作。
- **新增**：列出輸出版面路徑 → 三階段找相鄰空間、建立圖形、保存。
- **拖曳**：記錄整組原位置 → 產生接觸候選 → 檢查整組碰撞 → 吸附 → 提交或回復 → 保存。
- **外觀／大小／背景**：顯示輸入及輸出位置 → 驗證 → 套用勾選欄位並更新快取 → 保存；各視窗顯示三階段進度。
- **保存**：寫入暫存檔 → 備份上一版 → 原子替換。損壞檔案不直接覆寫；重開後計時器暫停。
- **JSON 匯出**：顯示路徑 → 整理配置與圖片 → 驗證 → 暫存檔完成後原子替換輸出。
- **JSON 匯入**：顯示路徑 → 驗證格式、數值、重疊與圖片 → 備份 → 保存圖片及候選版面 → 重建視窗。合併保留既有 ID 與計時狀態，匯入格分配新 ID，必要時整組平移。
- **動畫與計時**：靜態內容快取重用，啟用時共用約 12 FPS 動畫或每秒計時事件；不需要時移除事件。

## 開發、驗證與打包

開發需要 Windows x64、PowerShell、Visual Studio C++ 桌面開發工具與 Windows SDK。使用版不需要這些工具。

```powershell
# 建置與 170 項核心檢查；另輸出更新版，避免原 EXE 使用中
.\build.ps1 -OutputName DeskTiles-update.exe

# Windows UI、損壞檔保護與獨占鎖測試
.\verify.ps1 -ExecutableName DeskTiles-update.exe

# 重複啟動應帶回隱藏視窗，原程序與版面維持不變
.\verify-activation.ps1 -ExecutableName DeskTiles-update.exe

# 複製執行檔與說明、建立 ZIP 和 SHA256
.\package.ps1

# 從 ZIP 解壓並直接執行，驗證完整 Windows 功能
.\verify-package.ps1
```

每個腳本都先列出絕對輸入／輸出路徑，再顯示階段進度。`run.ps1` 可啟動最新建置，也可指定 `-ExecutableName`、`-DataDir`。Windows 桌面層測試需在可存取一般桌面與 Explorer 的環境執行；應用程式本身不需管理員權限。

最新結果集中於 [驗證摘要](docs/驗證摘要.txt)。測試使用專用工作目錄，不使用或打包個人版面。

## 專案檔案

| 位置 | 用途 |
| --- | --- |
| `release/` | 可直接使用的資料夾、ZIP 與 SHA256 |
| `core.hpp` | 跨平台模型、形狀、選取、吸附、碰撞、計時及內部保存 |
| `config.hpp` | JSON、Base64 圖片、驗證與合併 |
| `win32.cpp` | Windows 圖形／控制視窗、桌面層、圖示、系統匣及整合檢查 |
| `tests.cpp` | 核心檢查 |
| `DeskTiles.rc`、`HexDesk.manifest` | 內嵌圖示、一般使用者權限及 DPI 宣告 |
| `assets/` | 黑白像素來源、Logo 輸出與產生腳本 |
| `third_party/nlohmann/` | JSON 相依檔及 MIT 授權；不需建置時下載 |
| `docs/` | 使用說明、最新截圖、範例配置與驗證摘要 |
| `build/` | 最新開發執行檔及可重新產生的編譯／測試產物 |

內部格式為 `HEXDESK 5`，可讀取 v1～v4；JSON 為 `format: HexDesk`、version 2，可讀取 version 1。形狀值為 0 六角形／1 三角形／2 矩形。舊格式缺少形狀時還原為六角形。格子選取、控制項代碼等暫時狀態不寫入 JSON；字體以名稱保存，不打包字型檔案。

## 技術依據與後續工作

採 C++17、Win32、GDI+ 與 Windows 內建函式庫。透明顯示與六角形／三角形／矩形的互動區域分開，避免 alpha 0 區域點擊穿透。桌面層依賴 Explorer 宿主，屬相容性敏感部分。

- 重複啟動只用 [Restart Manager](https://learn.microsoft.com/en-us/windows/win32/api/restartmanager/nf-restartmanager-rmgetlist) 查詢鎖持有者，再帶回控制視窗；不呼叫關閉或重啟其他程式的 API。
- 視窗透明依據 [UpdateLayeredWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-updatelayeredwindow) 與 [extended window styles](https://learn.microsoft.com/en-us/windows/win32/winmsg/extended-window-styles)。
- JSON 使用 [nlohmann/json v3.12.0](https://github.com/nlohmann/json/releases/tag/v3.12.0)，授權隨使用版提供。
- 後續開發 Linux 顯示後端與打包；Windows EXE 不能直接作為 Linux 原生程式執行。
- 待驗收：不同 Windows 版本、DPI／多螢幕、Explorer 重啟、大量格子及長時間資源壓力。既有數字是本機短時間測試，並非所有環境的效能保證。

## Git 上傳流程

在專案根目錄檢查變更與遠端 → 確認不含個人版面、日誌與暫存檔 → 驗證腳本及套件 SHA256 → 提交原始碼、文件與使用版 → 推送至 origin/main → 比對本機與遠端提交。build/、Data/ 及 *.user 由 .gitignore 排除；使用者資料仍保存在專案外的 LocalAppData。
