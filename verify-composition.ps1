param([string]$ExecutableName='DeskTiles.exe',[string]$ExecutablePath='', [string]$PythonPath='')
$ErrorActionPreference='Stop'
$executable=if($ExecutablePath){[IO.Path]::GetFullPath($ExecutablePath)}else{Join-Path $PSScriptRoot ('build\'+$ExecutableName)}
if(-not $PythonPath){
    $bundled=Join-Path $env:USERPROFILE '.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
    $PythonPath=if(Test-Path -LiteralPath $bundled){$bundled}else{(Get-Command python -ErrorAction Stop).Source}
}
$PythonPath=[IO.Path]::GetFullPath($PythonPath)
$fixture=Join-Path $PSScriptRoot 'verify-composition.py'
Write-Host "輸入執行檔絕對路徑：$executable"
Write-Host "輸入測試腳本絕對路徑：$fixture"
Write-Host "輸入 Python 絕對路徑：$PythonPath"
Write-Host "輸出目錄絕對路徑：$(Join-Path $PSScriptRoot 'build')\composition-時間戳記（各輪會列出實際路徑）"
foreach($path in @($executable,$fixture,$PythonPath)){if(-not(Test-Path -LiteralPath $path)){throw "找不到輸入檔案：$path"}}
# This is a developer-only interactive desktop test. Restore the shell's minimized
# windows even when the ordinary-process pixel regression fails.
$shell=New-Object -ComObject Shell.Application
try {
    Write-Host '[螢幕驗證 1/3] 暫時顯示桌面；測試只截取專用圖形，完成後還原視窗'
    $shell.MinimizeAll();Start-Sleep -Milliseconds 600
    Write-Host '[螢幕驗證 2/3] 一般啟動：置頂／桌面切換、遮擋、透明點選與拖曳、圖片'
    & $PythonPath -X utf8 $fixture --executable $executable --both
    if($LASTEXITCODE -ne 0){throw '一般啟動（置頂及桌面模式）的實際螢幕驗證失敗。'}
    Write-Host '[螢幕驗證 3/3] 兩種初始配置均完成；結果與截圖已保存，還原原視窗'
} finally {$shell.UndoMinimizeALL()}
