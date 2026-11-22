param([string]$ExecutablePath=(Join-Path $PSScriptRoot 'build\DeskTiles-update.exe'))
$ErrorActionPreference='Stop'
$ExecutablePath=[IO.Path]::GetFullPath($ExecutablePath)
$data=Join-Path $PSScriptRoot ('build\convenience-test-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$report=Join-Path $data 'convenience-results.txt'
Write-Host "輸入程式：$ExecutablePath"
Write-Host "輸出測試資料：$data"
Write-Host "輸出驗證報告：$report"
Write-Host '[1/3] 啟動獨立資料目錄驗證'
$p=Start-Process -FilePath $ExecutablePath -ArgumentList @('--convenience-test','--data-dir',('"'+$data+'"')) -WindowStyle Hidden -PassThru
if(!$p.WaitForExit(50000)){$p.Kill();throw "Convenience test timed out: $data"}
if($p.ExitCode -ne 0){
 Get-Content -LiteralPath (Join-Path $data 'session.log') -Encoding UTF8 -Tail 20
 throw "Convenience test failed: $data"
}
Write-Host '[2/3] 核對驗證與持久資料'
$summary=Get-Content -LiteralPath $report -TotalCount 1
if($summary -notmatch '^PASS checks=(\d+)$' -or [int]$matches[1] -lt 25){throw 'Incomplete convenience checks'}
foreach($file in @('layouts.hexdesk.json','recycle.json','share-preview.png','offline-cloud\desktiles-sync.json')){
 if(!(Test-Path -LiteralPath (Join-Path $data $file))){throw "Missing artifact: $file"}
}
Write-Host '[3/3] PASS'
Get-Content -LiteralPath $report
