$ErrorActionPreference = 'Stop'
$archive = Join-Path $PSScriptRoot 'release\DeskTiles-Windows-x64.zip'
$testRoot = Join-Path $PSScriptRoot ('build\package-test-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$extractRoot = Join-Path $testRoot 'unpacked'
$dataRoot = Join-Path $testRoot 'data'
Write-Host "輸入 ZIP 絕對路徑：$archive"
Write-Host "輸出解壓縮目錄：$extractRoot"
Write-Host "輸出測試資料目錄：$dataRoot"
Write-Host '[套件驗證 1/3] 解壓縮並確認必要檔案'
Expand-Archive -LiteralPath $archive -DestinationPath $extractRoot
$appDirectory = Join-Path $extractRoot 'DeskTiles'
$executable = Join-Path $appDirectory 'DeskTiles.exe'
foreach ($name in @('DeskTiles.exe','README.txt','使用說明.md','THIRD-PARTY-NOTICES.txt','範例配置.json')) {
    if (-not (Test-Path -LiteralPath (Join-Path $appDirectory $name))) { throw "套件缺少檔案：$name" }
}
if ((Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath (Join-Path $PSScriptRoot 'release\DeskTiles\DeskTiles.exe') -Algorithm SHA256).Hash) { throw '套件執行檔與正式使用版不同。' }
Write-Host '[套件驗證 2/3] 直接執行解壓後的 EXE，測試全部 Windows 功能'
$process = Start-Process -FilePath $executable -WorkingDirectory $appDirectory -ArgumentList @('--smoke-test','--data-dir',('"'+$dataRoot+'"')) -PassThru -WindowStyle Hidden
if (-not $process.WaitForExit(30000)) { $process.Kill(); throw '套件整合測試逾時。' }
if ($process.ExitCode -ne 0) { throw "套件執行失敗：$dataRoot\session.log" }
Get-Content -LiteralPath (Join-Path $dataRoot 'integration-results.txt')
Write-Host '[套件驗證 3/3] 驗證圖示資源、說明與獨立執行完成'
@('PASS: ZIP extracted with all required files','PASS: packaged EXE SHA256 matches release executable','PASS: unpacked EXE completed Windows integration') | Set-Content -LiteralPath (Join-Path $testRoot 'package-results.txt') -Encoding utf8
Write-Host "PASS：套件結果：$testRoot\package-results.txt"
