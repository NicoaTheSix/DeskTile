# DeskTiles 黑白像素 Logo

Logo 由六角形、三角形、矩形、圓形排列為 2×2，使用純黑與純白。

輸入來源為此目錄的 `logo-grid.txt`，必須有 32 列，每列 32 個字元；`#` 為黑色、`.` 為白色。程式不在執行時讀取這些檔案，所有圖示會在建置時嵌入 EXE。

執行 `build-logo.ps1` 時，先顯示輸入與全部輸出檔案的絕對路徑，再執行四階段流程：

1. 驗證 32×32 像素來源。
2. 輸出 `DeskTiles.svg` 與 512×512 的 `DeskTiles-logo.png`；方形像素保留硬邊，僅黑白兩色。
3. 以最近鄰取樣產生 `DeskTiles.ico`，包含 16、32、48、64、128、256px 的 Windows 圖示；不使用平滑濾鏡。
4. 顯示完成狀態。

在 Windows PowerShell 執行：

```powershell
# 在本目錄執行
.\build-logo.ps1
```

此腳本使用 Windows 隨附的 System.Drawing，只在開發／建置時使用，不增加 EXE 的執行環境需求。專案根目錄的 `build.ps1` 會自動呼叫此腳本，再以 Resource Compiler 編譯 `DeskTiles.rc` 並連結圖示資源。完整程式操作流程及驗證紀錄見上一層的 `README.md`。
