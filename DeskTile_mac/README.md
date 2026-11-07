# 桌面拼格 DeskTiles — macOS 原生版

雙擊 `release/DeskTiles.app` 即可啟動。支援 macOS 12 以上；執行檔同時包含 Apple silicon 與 Intel 架構，不需要另外安裝執行環境。

## 使用

- 左側選六角形、三角形或 3:2 矩形，按「＋ 新增圖形」。
- 點擊選取，Shift＋點擊多選；拖曳任一選取格可移動整組。沿用 Windows 原版的 22 點吸附、混合形狀碰撞與重疊回復規則。
- 右側選文字、圖片或計時器，修改後按「套用至所有選取格」。圖片支援 PNG、JPEG、BMP，依形狀等比例裁切。
- 「邊框與風格…」設定實色、像素、RGB 或呼吸燈；「大小與文字…」設定尺寸與 Mac 已安裝字體；「背景與透明度…」只改背景。
- 右鍵圖形或使用選單列的 ▦ 可開啟控制面板、新增、選取全部、設定圖層、操作計時器、找回圖形與結束程式。
- 關閉控制面板不會結束程式。雙擊圖形或使用 ▦ 可帶回面板；使用「儲存並結束桌面拼格」或 Command＋Q 結束。
- 「配置 JSON…」可匯出包含圖片的單一配置，或覆蓋／合併匯入 Windows 和 Mac 的配置。匯入的計時器預設暫停。
- Windows 字體不一定安裝在 Mac；找不到的字體使用系統字體顯示。可在「大小與文字…」填入 `PingFang TC` 等已安裝字體名稱。

## 資料

預設存於 `~/Library/Application Support/DeskTiles/`。`layout.hexdesk` 為版面，`layout.backup` 為上次保存備份；匯入前另存 `import-before-*.hexdesk`。內嵌圖片解出到 `assets/`，以 SHA256 命名。直接瀏覽的圖片仍引用來源路徑，搬移時請匯出 JSON。

同一資料目錄採用系統檔案鎖，重複啟動會帶回既有面板；請勿以刪除 `session.lock` 的方式處理。載入損壞版面時會保留原檔並結束，不會以空白配置覆寫。

指定獨立資料目錄：

```sh
./release/DeskTiles.app/Contents/MacOS/DeskTiles --data-dir /tmp/my-desktile-data
```

## 與 Windows 版的對應及差異

C++ 核心與 JSON 格式直接沿用原版，介面以 Objective-C++／AppKit 重寫；沒有使用 Wine、網頁容器或模擬器。控制面板保留原版配置、中文標籤、配色與操作，但標題列、按鈕、選單和檔案對話框使用 macOS 原生樣式，並非逐像素相同。

- 尺寸以 macOS 邏輯點顯示，Retina 螢幕實際像素數較多。
- 「固定於桌面」採用桌面圖示下方的原生視窗層級；不使用 Windows Explorer 宿主機制。圖形會顯示於各桌面空間，macOS 的 Mission Control／Stage Manager／顯示桌面行為仍需實機驗收。
- 圖形外的透明角落以游標位置更新點擊穿透，更新頻率約 12 Hz；完全透明的圖形內部仍可拖曳。
- 計時結束以系統提示音提示；沒有額外申請通知權限。
- 預設用 PingFang TC；字體輸入及色碼欄位取代 Windows 的字體清單與選色對話框。
- 原始程式的階段式進度視窗、日誌系統及 Windows 驗證腳本未移植；Mac 使用狀態列和原生錯誤提示。
- 已執行本機核心及 AppKit 整合測試。尚未對 Intel 實機、多螢幕、不同 macOS 版本、500 格長時間壓力與所有滑鼠操作逐項驗收。

此本機版本採 ad-hoc 簽章，尚未申請 Developer ID 簽章與 Apple 公證，適合在本機開發與試用。

## 開發與驗證

需要 Xcode Command Line Tools 或 Xcode。

```sh
./build.sh
./verify.sh
```

`build.sh` 執行原版 170 項核心檢查，再編譯 Universal 2 App 並簽章。
`verify.sh` 使用 `build/verification.*` 的獨立資料目錄，在圖形環境檢查 AppKit 視窗、三種形狀、外觀套用、計時、透明繪製、Windows 範例圖片解碼、JSON 往返／合併、桌面層和版面保存，並驗證簽章與兩種架構。測試產生的 view 快取 PNG 不等同螢幕截圖；原生 layer 的部份控制項不一定包含在快取中。

來源：<https://github.com/NicoaTheSix/DeskTile>

參考提交：`4a7fc0aa41e05e48f01e389e3ee948a628022064`。

`Sources/core.hpp`、`Sources/config.hpp`、`tests/tests.cpp`、Logo 和範例配置取自該提交，保留原始內容；Windows clone 位於相鄰的 `DeskTile_windows/`，未修改。JSON 相依檔案為 nlohmann/json，MIT 授權見 `Sources/third_party/nlohmann/LICENSE.MIT`。
