param([switch]$Integration, [ValidateSet('HexDesk.exe','HexDesk-update.exe','HexDesk-next.exe','DeskTiles.exe','DeskTiles-update.exe')][string]$OutputName = 'DeskTiles.exe')
$ErrorActionPreference = 'Stop'
$sourceRoot = [IO.Path]::GetFullPath($PSScriptRoot)
$outputRoot = Join-Path $sourceRoot 'build'
$executablePath = Join-Path $outputRoot $OutputName
Write-Host "輸入目錄（絕對路徑）：$sourceRoot"
Write-Host "輸入檔案：$(Join-Path $sourceRoot 'win32.cpp')"
Write-Host "核心輸入：$(Join-Path $sourceRoot 'core.hpp')"
Write-Host "配置輸入：$(Join-Path $sourceRoot 'config.hpp')"
Write-Host "JSON 相依檔案：$(Join-Path $sourceRoot 'third_party\nlohmann\json.hpp')"
Write-Host "測試輸入：$(Join-Path $sourceRoot 'tests.cpp')"
Write-Host "Logo 像素輸入：$(Join-Path $sourceRoot 'assets\logo-grid.txt')"
Write-Host "圖示資源輸入：$(Join-Path $sourceRoot 'DeskTiles.rc')"
Write-Host "圖示資源輸出：$(Join-Path $outputRoot 'DeskTiles.res')"
Write-Host "輸出執行檔：$executablePath"
Write-Host "測試輸出：$(Join-Path $outputRoot 'core-test-results.txt')"
Write-Progress -Activity '建置 DeskTiles' -Status '確認編譯器' -PercentComplete 5
Write-Host '[1/4] 尋找 Microsoft C++ 工具鏈'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) { throw '找不到 vswhere，請安裝 Visual Studio C++ 桌面開發工具。' }
$installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installation) { throw '未安裝 C++ x64 編譯工具。' }
$vcvars = Join-Path $installation 'VC\Auxiliary\Build\vcvars64.bat'
# Import the compiler environment only. File operations stay in PowerShell.
New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
$environmentScript = Join-Path $outputRoot 'compiler-environment.cmd'
@('@echo off', ('call "' + $vcvars + '" >nul'), 'if errorlevel 1 exit /b 1', 'set') | Set-Content -LiteralPath $environmentScript -Encoding ascii
$environmentLines = & $env:ComSpec /d /c $environmentScript
if ($LASTEXITCODE -ne 0) { throw '載入編譯器環境失敗。' }
foreach ($line in $environmentLines) {
    if ($line -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process') }
}
Push-Location $outputRoot
try {
    Write-Host '[2/4] 編譯 Windows 應用程式'
    Write-Progress -Activity '建置 DeskTiles' -Status '編譯應用程式' -PercentComplete 25
    & (Join-Path $sourceRoot 'assets\build-logo.ps1')
    & rc.exe /nologo ('/i' + $sourceRoot) /foDeskTiles.res (Join-Path $sourceRoot 'DeskTiles.rc')
    if ($LASTEXITCODE -ne 0) { throw '圖示資源編譯失敗。' }
    & cl.exe /nologo /std:c++17 /utf-8 /EHsc /W4 /external:anglebrackets /external:W0 /O2 /MT (Join-Path $sourceRoot 'win32.cpp') DeskTiles.res ('/Fe:' + $OutputName) /Fo:DeskTiles.obj /link /SUBSYSTEM:WINDOWS /DYNAMICBASE /NXCOMPAT /MANIFEST:EMBED (('/MANIFESTINPUT:') + (Join-Path $sourceRoot 'HexDesk.manifest'))
    if ($LASTEXITCODE -ne 0) { throw '應用程式編譯失敗。' }
    Write-Host '[3/4] 編譯並執行跨平台核心測試'
    Write-Progress -Activity '建置 DeskTiles' -Status '執行核心測試' -PercentComplete 60
    & cl.exe /nologo /std:c++17 /utf-8 /EHsc /W4 /O2 /MT (Join-Path $sourceRoot 'tests.cpp') /Fe:CoreTests.exe /Fo:CoreTests.obj
    if ($LASTEXITCODE -ne 0) { throw '核心測試編譯失敗。' }
    & .\CoreTests.exe (Join-Path $sourceRoot 'core.hpp') | Tee-Object -FilePath (Join-Path $outputRoot 'core-test-results.txt')
    if ($LASTEXITCODE -ne 0) { throw '核心測試未通過。' }
    Write-Host '[4/4] 驗證輸出'
    Write-Progress -Activity '建置 DeskTiles' -Status '驗證輸出' -PercentComplete 85
    if ($Integration) {
        & (Join-Path $sourceRoot 'verify.ps1') -ExecutableName $OutputName
    }
    Get-Item -LiteralPath $executablePath | Select-Object FullName,Length
    Write-Progress -Activity '建置 DeskTiles' -Completed
    Write-Host '完成。以 .\run.ps1 啟動程式。'
} finally { Pop-Location }
