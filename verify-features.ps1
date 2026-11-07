param([string]$ExecutableName='DeskTiles-update.exe',[string]$ExecutablePath='')
$ErrorActionPreference='Stop'
$executable=if($ExecutablePath){[IO.Path]::GetFullPath($ExecutablePath)}else{Join-Path $PSScriptRoot ('build\'+$ExecutableName)}
$testRoot=Join-Path $PSScriptRoot ('build\features-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
Write-Host "輸入執行檔絕對路徑：$executable"
Write-Host "輸出測試配置與結果絕對路徑：$testRoot"
Write-Host "輸出驗證摘要絕對路徑：$(Join-Path $testRoot 'feature-results.txt')"
Write-Host '[新功能驗證 1/3] 記錄真實登入設定，測試期間不允許啟用'
$runKey='HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
function Read-LoginValue { if(Test-Path -LiteralPath $runKey){ $value=(Get-ItemProperty -LiteralPath $runKey).PSObject.Properties['DeskTiles']; if($value){return $value.Value} }; return $null }
$before=Read-LoginValue
New-Item -ItemType Directory -Path $testRoot -Force | Out-Null
Write-Host '[新功能驗證 2/3] 鬧鐘、文字、非阻塞提醒及隔離登錄測試'
$process=Start-Process -FilePath $executable -ArgumentList @('--feature-test','--data-dir',('"'+$testRoot+'"')) -PassThru -WindowStyle Hidden
if(-not $process.WaitForExit(20000)){$process.Kill();throw '新功能驗證逾時。'}
$after=Read-LoginValue
if($before -cne $after){throw '真實登入啟動設定遭到更動。'}
if($process.ExitCode -ne 0){Get-Content -LiteralPath (Join-Path $testRoot 'session.log') -Tail 15;throw '新功能驗證失敗。'}
Write-Host '[新功能驗證 3/3] 確認真實啟動設定未變更並保存結果'
Get-Content -LiteralPath (Join-Path $testRoot 'feature-results.txt')
'PASS: actual HKCU Run DeskTiles value unchanged' | Add-Content -LiteralPath (Join-Path $testRoot 'feature-results.txt') -Encoding utf8
