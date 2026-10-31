param([ValidateSet('Latest','HexDesk.exe','HexDesk-update.exe','HexDesk-next.exe','DeskTiles.exe','DeskTiles-update.exe')][string]$ExecutableName = 'Latest')
$ErrorActionPreference = 'Stop'
if ($ExecutableName -eq 'Latest') {
    $candidates = @('HexDesk.exe','HexDesk-update.exe','HexDesk-next.exe','DeskTiles.exe','DeskTiles-update.exe') | ForEach-Object { Join-Path $PSScriptRoot ('build\' + $_) } | Where-Object { Test-Path -LiteralPath $_ }
    $executable = $candidates | ForEach-Object { Get-Item -LiteralPath $_ } | Sort-Object LastWriteTime -Descending | Select-Object -First 1 -ExpandProperty FullName
    if (-not $executable) { $executable = Join-Path $PSScriptRoot 'build\DeskTiles.exe' }
} else { $executable = Join-Path $PSScriptRoot ('build\' + $ExecutableName) }
$testDirectory = Join-Path $PSScriptRoot ('build\verify-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
Write-Host "輸入執行檔（絕對路徑）：$executable"
Write-Host "輸入／輸出測試目錄（絕對路徑）：$testDirectory"
Write-Host "輸出驗證摘要：$(Join-Path $testDirectory 'verification.txt')"
if (-not (Test-Path -LiteralPath $executable)) { throw '請先建置桌面拼格 DeskTiles。' }
New-Item -ItemType Directory -Path $testDirectory -Force | Out-Null
function Invoke-TestProcess([string]$Directory) {
    $process = Start-Process -FilePath $executable -ArgumentList @('--smoke-test', '--data-dir', ('"' + $Directory + '"')) -PassThru -WindowStyle Hidden
    if (-not $process.WaitForExit(30000)) { $process.Kill(); throw "整合測試超時：$Directory\session.log" }
    return $process.ExitCode
}
Write-Host '[1/3] 實際 Windows 視窗整合與資源量測'
$uiDirectory = Join-Path $testDirectory 'Unicode 路徑測試'
if ((Invoke-TestProcess $uiDirectory) -ne 0) { throw "視窗整合未通過：$uiDirectory\session.log。桌面宿主測試需在一般使用者桌面執行，受限沙箱可能拒絕 SetParent。" }
Get-Content -LiteralPath (Join-Path $uiDirectory 'integration-results.txt')
Write-Host '[2/3] 損壞版面保護'
$corruptDirectory = Join-Path $testDirectory 'corrupt'
New-Item -ItemType Directory -Path $corruptDirectory | Out-Null
$corruptFile = Join-Path $corruptDirectory 'layout.hexdesk'
[IO.File]::WriteAllText($corruptFile, 'this is intentionally invalid')
$beforeHash = (Get-FileHash -LiteralPath $corruptFile -Algorithm SHA256).Hash
if ((Invoke-TestProcess $corruptDirectory) -ne 1) { throw '損壞資料沒有以錯誤狀態退出。' }
if ((Get-FileHash -LiteralPath $corruptFile -Algorithm SHA256).Hash -ne $beforeHash) { throw '損壞資料遭到覆寫。' }
Write-Host '[3/3] 重複執行保護'
$lockedDirectory = Join-Path $testDirectory 'locked'
New-Item -ItemType Directory -Path $lockedDirectory | Out-Null
$handle = [IO.File]::Open((Join-Path $lockedDirectory 'session.lock'), [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
try {
    if ((Invoke-TestProcess $lockedDirectory) -ne 2) { throw '未阻擋同一資料目錄重複執行。' }
    if (Test-Path -LiteralPath (Join-Path $lockedDirectory 'layout.hexdesk')) { throw '重複執行建立了不應產生的版面。' }
} finally { $handle.Dispose() }
@("PASS: Windows UI and resource measurement", "PASS: Unicode and space in data paths", "PASS: corrupt layout preserved (SHA256)", "PASS: exclusive session lock", "UI results: $uiDirectory") | Set-Content -LiteralPath (Join-Path $testDirectory 'verification.txt') -Encoding utf8
Get-Content -LiteralPath (Join-Path $testDirectory 'verification.txt')
