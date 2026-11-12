param([string]$ExecutablePath=(Join-Path $PSScriptRoot 'build\DeskTiles-update.exe'))
$ErrorActionPreference='Stop'
$ExecutablePath=[IO.Path]::GetFullPath($ExecutablePath)
$data=Join-Path $PSScriptRoot ('build\bevel-test-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$report=Join-Path $data 'bevel-results.txt'
Write-Host "輸入執行檔：$ExecutablePath"
Write-Host "輸出測試資料：$data"
Write-Host "輸出預覽：$(Join-Path $data 'tiles-preview-ceramic.png')"
Write-Host "輸出結果：$report"
Write-Host '[1/3] 使用獨立資料目錄啟動測試'
$p=Start-Process -FilePath $ExecutablePath -ArgumentList @('--bevel-test','--data-dir',('"'+$data+'"')) -WindowStyle Hidden -PassThru
if(!$p.WaitForExit(25000)){$p.Kill();throw "Bevel check timed out: $data"}
if($p.ExitCode -ne 0){Get-Content -LiteralPath (Join-Path $data 'session.log') -Encoding UTF8 -Tail 12;throw "Bevel check failed: $data"}
Write-Host '[2/3] 核對結果與預覽'
if((Get-Content -LiteralPath $report -TotalCount 1) -ne 'PASS checks=8'){throw 'Bevel checks did not complete'}
if(!(Test-Path -LiteralPath (Join-Path $data 'tiles-preview-ceramic.png') -PathType Leaf)){throw 'Missing tile preview'}
Write-Host '[3/3] PASS'
Get-Content -LiteralPath $report
