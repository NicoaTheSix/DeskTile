param([string]$DataDir = (Join-Path $env:LOCALAPPDATA 'HexDesk'),
    [ValidateSet('Latest','HexDesk.exe','HexDesk-update.exe','HexDesk-next.exe','DeskTiles.exe','DeskTiles-update.exe')][string]$ExecutableName = 'Latest')
$ErrorActionPreference = 'Stop'
if ($ExecutableName -eq 'Latest') {
    $candidates = @('HexDesk.exe','HexDesk-update.exe','HexDesk-next.exe','DeskTiles.exe','DeskTiles-update.exe') | ForEach-Object { Join-Path $PSScriptRoot ('build\' + $_) } | Where-Object { Test-Path -LiteralPath $_ }
    $executable = $candidates | ForEach-Object { Get-Item -LiteralPath $_ } | Sort-Object LastWriteTime -Descending | Select-Object -First 1 -ExpandProperty FullName
    if (-not $executable) { $executable = Join-Path $PSScriptRoot 'build\DeskTiles.exe' }
} else { $executable = Join-Path $PSScriptRoot ('build\' + $ExecutableName) }
$dataDirectory = [IO.Path]::GetFullPath($DataDir)
Write-Host "輸入執行檔（絕對路徑）：$executable"
Write-Host "輸入版面（首次啟動時可不存在）：$(Join-Path $dataDirectory 'layout.hexdesk')"
Write-Host "輸出版面：$(Join-Path $dataDirectory 'layout.hexdesk')"
Write-Host "輸出日誌：$(Join-Path $dataDirectory 'session.log')"
Write-Host '[1/2] 確認執行檔'
if (-not (Test-Path -LiteralPath $executable)) { throw '尚未建置，請先執行 .\build.ps1。' }
Write-Host '[2/2] 啟動 HexDesk；初始化階段會另顯示進度'
Start-Process -FilePath $executable -ArgumentList @('--data-dir', ('"' + $dataDirectory + '"')) -WindowStyle Hidden
