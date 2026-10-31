param([string]$ExecutableName = 'Latest')
$ErrorActionPreference = 'Stop'
if ($ExecutableName -eq 'Latest') {
    $executable = @('DeskTiles.exe','DeskTiles-update.exe') | ForEach-Object { Join-Path $PSScriptRoot ('build\' + $_) } |
        Where-Object { Test-Path -LiteralPath $_ } | ForEach-Object { Get-Item -LiteralPath $_ } |
        Sort-Object LastWriteTime -Descending | Select-Object -First 1 -ExpandProperty FullName
    if (-not $executable) { throw '請先建置桌面拼格。' }
} else { $executable = Join-Path $PSScriptRoot ('build\' + $ExecutableName) }
$releaseRoot = Join-Path $PSScriptRoot 'release'
$staging = Join-Path $PSScriptRoot ('build\package-staging-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$stagingApp = Join-Path $staging 'DeskTiles'
$releaseApp = Join-Path $releaseRoot 'DeskTiles'
$archive = Join-Path $releaseRoot 'DeskTiles-Windows-x64.zip'
Write-Host "輸入執行檔絕對路徑：$executable"
Write-Host "輸入說明文件：$(Join-Path $PSScriptRoot 'docs\使用說明.md')"
Write-Host "輸出使用版目錄：$releaseApp"
Write-Host "輸出 ZIP：$archive"
Write-Host "輸出 SHA256：$(Join-Path $releaseRoot 'SHA256SUMS.txt')"
Write-Host '[打包 1/4] 確認輸入檔案'
if (-not (Test-Path -LiteralPath $executable)) { throw '請先建置最新版本。' }
foreach ($directory in @($stagingApp,$releaseApp)) { New-Item -ItemType Directory -Path $directory -Force | Out-Null }
Write-Host '[打包 2/4] 準備免安裝執行檔、說明與授權'
Copy-Item -LiteralPath $executable -Destination (Join-Path $stagingApp 'DeskTiles.exe')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'docs\使用說明.md') -Destination $stagingApp
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'docs\mixed-shapes.json') -Destination (Join-Path $stagingApp '範例配置.json')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'third_party\nlohmann\LICENSE.MIT') -Destination (Join-Path $stagingApp 'THIRD-PARTY-NOTICES.txt')
@'
桌面拼格 DeskTiles — Windows x64 免安裝版

1. 先完整解壓縮 ZIP，再雙擊 DeskTiles.exe。
2. 適用 Windows 10／11 x64；不需安裝 Python、.NET 或 Visual Studio。
3. 在左側選擇形狀，按「新增圖形」；右側編輯內容後按套用。
4. 右上角「配置 JSON…」可匯出含圖片的備份或匯入範例配置。
5. 關閉控制視窗後，桌面圖形仍會繼續顯示；雙擊圖形可開啟控制視窗。
6. 要結束程式，從圖形右鍵或系統匣選「儲存並結束桌面拼格」。

版面預設儲存於 %LOCALAPPDATA%\HexDesk，沿用舊版本位置。
若已有程式使用相同資料目錄，會帶回既有視窗。要換新版，請先結束舊版。
完整操作說明請看「使用說明.md」。Linux 版尚未開發。
'@ | Set-Content -LiteralPath (Join-Path $stagingApp 'README.txt') -Encoding utf8
Write-Host '[打包 3/4] 壓縮只包含使用檔案的乾淨套件'
Compress-Archive -LiteralPath $stagingApp -DestinationPath $archive -Force
foreach ($file in Get-ChildItem -LiteralPath $stagingApp -File) { Copy-Item -LiteralPath $file.FullName -Destination $releaseApp -Force }
Write-Host '[打包 4/4] 計算檔案雜湊'
$zipHash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash
$exeHash = (Get-FileHash -LiteralPath (Join-Path $releaseApp 'DeskTiles.exe') -Algorithm SHA256).Hash
@("$zipHash  DeskTiles-Windows-x64.zip", "$exeHash  DeskTiles/DeskTiles.exe") | Set-Content -LiteralPath (Join-Path $releaseRoot 'SHA256SUMS.txt') -Encoding ascii
Get-Item -LiteralPath $archive,(Join-Path $releaseApp 'DeskTiles.exe') | Select-Object FullName,Length
$resolvedStaging = (Resolve-Path -LiteralPath $staging).Path
$buildRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot 'build')).TrimEnd('\')
if (-not $resolvedStaging.StartsWith($buildRoot+'\',[StringComparison]::OrdinalIgnoreCase) -or
    (Get-Item -LiteralPath $resolvedStaging).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw '拒絕清理非建置目錄的打包暫存。' }
Remove-Item -LiteralPath $resolvedStaging -Recurse -Force
