param([string]$ExecutablePath=(Join-Path $PSScriptRoot 'build\DeskTiles-update.exe'))
$ErrorActionPreference='Stop'
$ExecutablePath=[IO.Path]::GetFullPath($ExecutablePath)
$data=Join-Path $PSScriptRoot ('build\sage-test-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$report=Join-Path $data 'sage-results.txt'
$preview=Join-Path $data 'tiles-preview-sage.png'
Write-Host "輸入執行檔：$ExecutablePath"
Write-Host "輸出資料：$data"
Write-Host "輸出報告：$report"
Write-Host "輸出預覽：$preview"
Write-Host '[1/3] 在獨立資料目錄執行樣板測試'
$p=Start-Process -FilePath $ExecutablePath -ArgumentList @('--sage-test','--data-dir',('"'+$data+'"')) -WindowStyle Hidden -PassThru
if(!$p.WaitForExit(40000)){$p.Kill();throw "Sage test timed out: $data"}
if($p.ExitCode -ne 0){Get-Content -LiteralPath (Join-Path $data 'session.log') -Encoding UTF8 -Tail 15;throw "Sage test failed: $data"}
Write-Host '[2/3] 核對樣板操作與預覽'
if((Get-Content -LiteralPath $report -TotalCount 1) -ne 'PASS checks=14'){throw 'Sage checks did not complete'}
if(!(Test-Path -LiteralPath $preview -PathType Leaf)){throw 'Missing Sage preview'}
Write-Host '[3/3] PASS'
Get-Content -LiteralPath $report
