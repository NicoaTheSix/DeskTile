param([string]$ExecutableName='DeskTiles-update.exe')
$ErrorActionPreference='Stop'
$executable=Join-Path $PSScriptRoot ('build\'+$ExecutableName)
$testRoot=Join-Path $PSScriptRoot ('build\interaction-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
Write-Host "輸入執行檔絕對路徑：$executable"
Write-Host "輸出專用版面／結果絕對路徑：$testRoot"
Write-Host '[互動驗證 1/2] 執行拖曳與字體回歸，不使用個人版面'
$process=Start-Process -FilePath $executable -ArgumentList @('--interaction-test','--data-dir',('"'+$testRoot+'"')) -PassThru -WindowStyle Hidden
if(-not $process.WaitForExit(15000)){$process.Kill();throw '互動驗證逾時。'}
Write-Host '[互動驗證 2/2] 讀取實際視窗事件及內容像素檢查結果'
Get-Content -LiteralPath (Join-Path $testRoot 'interaction-results.txt')
if($process.ExitCode -ne 0){throw "互動驗證失敗：$testRoot\session.log"}
