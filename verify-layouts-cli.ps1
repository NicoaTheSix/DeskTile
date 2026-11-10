param(
    [string]$ExecutablePath=(Join-Path $PSScriptRoot 'build\DeskTiles-update.exe'),
    [string]$CliPath=(Join-Path $PSScriptRoot 'build\desktiles-cli.exe')
)
$ErrorActionPreference='Stop'
$ExecutablePath=[IO.Path]::GetFullPath($ExecutablePath)
$CliPath=[IO.Path]::GetFullPath($CliPath)
$testRoot=Join-Path $PSScriptRoot ('build\layouts-cli-test-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$data=Join-Path $testRoot '獨立 測試資料'
$report=Join-Path $testRoot 'results.txt'
Write-Host "輸入 App：$ExecutablePath"
Write-Host "輸入 CLI：$CliPath"
Write-Host "輸出測試資料：$data"
Write-Host "輸出測試結果：$report"
New-Item -ItemType Directory -Path $data -Force|Out-Null
$script:checkCount=0
$script:results=[Collections.Generic.List[string]]::new()
$script:process=$null
function Check([bool]$Condition,[string]$Label) {
    if(!$Condition){throw "FAIL: $Label"}
    $script:checkCount++
    $script:results.Add("PASS: $Label")
}
function Quote-Arg([string]$Value) {
    '"' + ($Value -replace '(\\*)"','$1$1\"' -replace '(\\+)$','$1$1') + '"'
}
function Read-Process([string]$File,[string]$Arguments) {
    $start=[Diagnostics.ProcessStartInfo]::new($File,$Arguments)
    $start.UseShellExecute=$false
    $start.CreateNoWindow=$true
    $start.RedirectStandardOutput=$true
    $start.RedirectStandardError=$true
    $child=[Diagnostics.Process]::Start($start)
    try {
        $out=$child.StandardOutput.ReadToEndAsync()
        $err=$child.StandardError.ReadToEndAsync()
        if(!$child.WaitForExit(16000)){$child.Kill();throw "Timed out: $File"}
        [pscustomobject]@{Code=$child.ExitCode;Output=$out.Result;Error=$err.Result}
    } finally {$child.Dispose()}
}
function Invoke-DeskTilesCli([string[]]$Tokens,[int]$Code=0,[string]$Directory=$data) {
    $argsLine=(@('--data-dir',$Directory,'--json')+$Tokens|ForEach-Object {Quote-Arg $_}) -join ' '
    $r=Read-Process $CliPath $argsLine
    if($r.Code -ne $Code){throw "Unexpected exit $($r.Code) for $Tokens : $($r.Error) $($r.Output)"}
    if($Code -eq 0){$r.Output|ConvertFrom-Json}else{$r.Error|ConvertFrom-Json}
}
Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class DeskTilesLayoutProbe {
 delegate bool Visitor(IntPtr w, IntPtr d);
 [DllImport("user32.dll")] static extern bool EnumWindows(Visitor v,IntPtr d);
 [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr w,out uint id);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetClassName(IntPtr w,StringBuilder t,int n);
 [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr w);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern bool PostMessage(IntPtr w,uint msg,IntPtr a,IntPtr b);
 static bool Match(IntPtr w,int pid,string cls) {
   uint owner; GetWindowThreadProcessId(w,out owner);
   if(owner!=(uint)pid)return false;
   var buffer=new StringBuilder(128);GetClassName(w,buffer,128);
   return buffer.ToString()==cls;
 }
 public static int Tiles(int pid,bool visible) {
   int n=0;
   EnumWindows((w,d)=>{if(Match(w,pid,"HexDesk.Tile")&&(!visible||IsWindowVisible(w)))n++;return true;},IntPtr.Zero);
   return n;
 }
 public static bool Exit(int pid) {
   bool sent=false;
   EnumWindows((w,d)=>{if(!Match(w,pid,"HexDesk.Control"))return true;
     sent=PostMessage(w,0x111,new IntPtr(116),IntPtr.Zero);return false;},IntPtr.Zero);
   return sent;
 }
}
'@
function Start-TestApp {
    $script:process=Start-Process -FilePath $ExecutablePath -ArgumentList @('--startup','--data-dir',(Quote-Arg $data)) -PassThru -WindowStyle Hidden
    for($i=0;$i -lt 24;$i++){
        if($process.HasExited){throw "DeskTiles quit during startup: $data\session.log"}
        $r=Read-Process $CliPath ('--data-dir '+(Quote-Arg $data)+' --json layout list')
        if($r.Code -eq 0){return ($r.Output|ConvertFrom-Json)}
        Start-Sleep -Milliseconds 150
    }
    throw 'DeskTiles did not accept a CLI request.'
}
function Stop-TestApp {
    if($null -eq $process -or $process.HasExited){return}
    if(![DeskTilesLayoutProbe]::Exit($process.Id) -or !$process.WaitForExit(8000)){throw 'Test app did not close gracefully.'}
    Check ($process.ExitCode -eq 0) 'app closed with saved state'
}
try {
    Write-Host '[1/6] 啟動舊配置；實際執行 CMD 與 PowerShell CLI'
    $missing=Invoke-DeskTilesCli @('layout','list') 3
    Check (!$missing.ok) 'closed app returns exit 3'
    $legacy=Join-Path $data 'layout.hexdesk'
    [IO.File]::WriteAllText($legacy,('HEXDESK 1'+[char]10+'1'+[char]10+'41 80 100 0 1 1 300 0 "舊版配置" ""'+[char]10),[Text.UTF8Encoding]::new($false))
    $initial=Start-TestApp
    $original=$initial.active
    $legacyHash=(Get-FileHash -LiteralPath $legacy).Hash
    Check ((Invoke-DeskTilesCli @('tile','get','--id','41')).tile.text -eq '舊版配置') 'legacy data loaded without losing tile or Unicode'
    $psReply=& $CliPath --data-dir $data --json layout list
    Check ($LASTEXITCODE -eq 0 -and ($psReply|ConvertFrom-Json).active -eq $original) 'PowerShell native invocation'
    $cmdLine='/d /s /c ""'+$CliPath+'" --data-dir "'+$data+'" --json layout list"'
    $cmdReply=Read-Process $env:ComSpec $cmdLine
    Check ($cmdReply.Code -eq 0 -and ($cmdReply.Output|ConvertFrom-Json).active -eq $original) 'CMD native invocation'
    $noOther=Invoke-DeskTilesCli @('layout','list') 3 (Join-Path $testRoot 'other')
    Check (!$noOther.ok) 'CLI does not fall back to a different data directory'
    Write-Host '[2/6] 保存、切換、編輯、隱藏及刪除方塊'
    $catalog=Join-Path $data 'layouts.hexdesk.json'
    $null=Invoke-DeskTilesCli @('layout','create','工作 & 計劃','--empty')
    Check ((Get-FileHash -LiteralPath $legacy).Hash -eq $legacyHash) 'migration leaves original layout file intact'
    Check ((Test-Path -LiteralPath $catalog)) 'layout catalog created'
    $null=Invoke-DeskTilesCli @('layout','create','工作 & 計劃','--empty') 2
    $null=Invoke-DeskTilesCli @('layout','switch','工作 & 計劃')
    Check ([DeskTilesLayoutProbe]::Tiles($process.Id,$false) -eq 0) 'switching to an empty layout destroys previous tile windows'
    $add=Invoke-DeskTilesCli @('tile','add','--shape','triangle','--text','新工作：測試','--x','100','--y','100')
    $id=[string]$add.tile.id
    Check ($add.tile.shape -eq 'triangle' -and $add.tile.text -eq '新工作：測試') 'add triangle with Unicode content'
    Check ([DeskTilesLayoutProbe]::Tiles($process.Id,$true) -eq 1) 'added tile actually visible'
    $null=Invoke-DeskTilesCli @('tile','add','--x','100','--y','100') 2
    $null=Invoke-DeskTilesCli @('tile','update','--id',$id,'--text','完成 ✔')
    Check ((Invoke-DeskTilesCli @('tile','get','--id',$id)).tile.text -eq '完成 ✔') 'update and retrieve by ID'
    $null=Invoke-DeskTilesCli @('tile','hide','--id',$id)
    Check ([DeskTilesLayoutProbe]::Tiles($process.Id,$true) -eq 0 -and (Invoke-DeskTilesCli @('tile','get','--id',$id)).tile.hidden) 'hide removes the real window from the desktop'
    $null=Invoke-DeskTilesCli @('layout','switch',$original)
    Check ([DeskTilesLayoutProbe]::Tiles($process.Id,$false) -eq 1) 'only active layout has tile windows'
    $null=Invoke-DeskTilesCli @('tile','update','--layout','工作 & 計劃','--id',$id,'--text','未顯示時更新')
    Check ([DeskTilesLayoutProbe]::Tiles($process.Id,$false) -eq 1) 'editing inactive layout does not create a window'
    $null=Invoke-DeskTilesCli @('layout','switch','工作 & 計劃')
    Check ([DeskTilesLayoutProbe]::Tiles($process.Id,$true) -eq 0) 'hide survives switch away and back'
    $null=Invoke-DeskTilesCli @('tile','show','--id',$id)
    Check ([DeskTilesLayoutProbe]::Tiles($process.Id,$true) -eq 1 -and (Invoke-DeskTilesCli @('tile','get','--id',$id)).tile.text -eq '未顯示時更新') 'show restores edited tile'
    $null=Invoke-DeskTilesCli @('tile','delete','--id',$id)
    Check ([DeskTilesLayoutProbe]::Tiles($process.Id,$false) -eq 0 -and !(Invoke-DeskTilesCli @('tile','get','--id',$id) 2).ok) 'delete removes data and HWND'
    $add=Invoke-DeskTilesCli @('tile','add','--text','保留工作格','--x','100','--y','100')
    $workId=[string]$add.tile.id
    Check ([int]$workId -gt [int]$id) 'deleted ID is not reused'
    $null=Invoke-DeskTilesCli @('layout','rename','工作 & 計劃','工作')
    $null=Invoke-DeskTilesCli @('layout','delete','工作','--yes') 2
    Check ((Invoke-DeskTilesCli @('layout','list')).active -eq '工作') 'rename persists; active layout cannot be deleted'
    Write-Host '[3/6] 背景計時、不重複啟動副本與儲存失敗回復'
    $null=Invoke-DeskTilesCli @('layout','switch',$original)
    $up=Invoke-DeskTilesCli @('tile','add','--kind','timer','--timer-mode','countup','--running','true','--x','300','--y','100')
    $down=Invoke-DeskTilesCli @('tile','add','--kind','timer','--seconds','4','--running','true','--x','500','--y','100')
    $upId=[string]$up.tile.id
    $downId=[string]$down.tile.id
    $null=Invoke-DeskTilesCli @('layout','create','副本')
    Check (!(Invoke-DeskTilesCli @('tile','get','--layout','副本','--id',$upId)).tile.timer.running) 'saved copy does not duplicate a running timer'
    $null=Invoke-DeskTilesCli @('layout','delete','副本') 2
    $null=Invoke-DeskTilesCli @('layout','delete','副本','--yes')
    $null=Invoke-DeskTilesCli @('layout','switch','工作')
    Check ([DeskTilesLayoutProbe]::Tiles($process.Id,$false) -eq 1) 'background timers have no windows'
    for($i=0;$i -lt 18;$i++) {
        $countdown=(Invoke-DeskTilesCli @('tile','get','--layout',$original,'--id',$downId)).tile.timer
        if(!$countdown.running){break}
        Start-Sleep -Milliseconds 300
    }
    $countup=(Invoke-DeskTilesCli @('tile','get','--layout',$original,'--id',$upId)).tile.timer
    Check ($countup.running -and $countup.elapsed -gt 1) 'countup continues in an inactive layout'
    Check (!$countdown.running -and $countdown.elapsed -eq 4) 'inactive countdown completes'
    $log=Join-Path $data 'session.log'
    Check ((Get-Content -LiteralPath $log -Encoding UTF8 -Raw).Contains("$original · 圖形 #$($downId)：倒數結束。")) 'inactive countdown reminder records the correct layout and ID'
    $null=Invoke-DeskTilesCli @('layout','switch',$original)
    Check ((Invoke-DeskTilesCli @('tile','get','--id',$upId)).tile.timer.elapsed -ge $countup.elapsed) 'switch back retains elapsed time'
    $null=Invoke-DeskTilesCli @('layout','switch','工作')
    $hash=(Get-FileHash -LiteralPath $catalog).Hash
    $lock=[IO.File]::Open($catalog,'Open','Read','Read')
    try{$null=Invoke-DeskTilesCli @('tile','update','--id',$workId,'--text','不能保存') 2}finally{$lock.Dispose()}
    Check ((Get-FileHash -LiteralPath $catalog).Hash -eq $hash -and (Invoke-DeskTilesCli @('tile','get','--id',$workId)).tile.text -eq '保留工作格') 'locked save rolls back model and preserves original disk contents'
    Check ([DeskTilesLayoutProbe]::Tiles($process.Id,$true) -eq 1) 'failed save restores the old window'
    Write-Host '[4/6] 建立背景單次鬧鐘、關閉並重新開啟'
    $now=Get-Date
    $due=$now.Date.AddMinutes([math]::Floor($now.TimeOfDay.TotalMinutes)+1)
    if(($due-$now).TotalSeconds -lt 15){$due=$due.AddMinutes(1)}
    $alarm=Invoke-DeskTilesCli @('tile','add','--layout',$original,'--kind','alarm','--text','背景鬧鐘測試','--alarm-time',$due.ToString('HH:mm'),'--alarm-date',$due.ToString('yyyy-MM-dd'),'--alarm-enabled','true','--x','700','--y','100')
    $alarmId=[string]$alarm.tile.id
    Check ($alarm.tile.alarm.enabled -and !$alarm.tile.visible) 'inactive alarm armed through CLI'
    Stop-TestApp
    $restarted=Start-TestApp
    Check ($restarted.active -eq '工作' -and $restarted.layouts.Count -eq 2) 'catalog and active layout survive restart'
    Check ((Invoke-DeskTilesCli @('tile','get','--id',$workId)).tile.text -eq '保留工作格') 'tile content and stable ID survive restart'
    Check ((Invoke-DeskTilesCli @('tile','get','--layout',$original,'--id',$alarmId)).tile.alarm.nextDue -gt 0) 'inactive alarm schedule rearmed after restart'
    Write-Host "[5/6] 等待實際背景鬧鐘 $($due.ToString('HH:mm:ss'))"
    $deadline=$due.AddSeconds(10)
    $nextProgress=Get-Date
    do {
        $alarmState=(Invoke-DeskTilesCli @('tile','get','--layout',$original,'--id',$alarmId)).tile.alarm
        if($alarmState.lastFired -gt 0){break}
        if((Get-Date) -ge $nextProgress){
            Write-Host ('背景鬧鐘等待秒數：'+[Math]::Max(0,[Math]::Ceiling(($due-(Get-Date)).TotalSeconds)))
            $nextProgress=(Get-Date).AddSeconds(10)
        }
        Start-Sleep -Milliseconds 500
    } while((Get-Date) -lt $deadline)
    Check ($alarmState.lastFired -gt 0 -and !$alarmState.enabled) 'inactive alarm fires without any active timer or layout switch after restart'
    Check ((Get-Content -LiteralPath $log -Encoding UTF8 -Raw).Contains("$original · 圖形 #$($alarmId)：鬧鐘時間到。")) 'background alarm reminder names the inactive layout'
    Check ((Invoke-DeskTilesCli @('layout','list')).active -eq '工作' -and [DeskTilesLayoutProbe]::Tiles($process.Id,$false) -eq 1) 'alarm does not switch layout or create background tile windows'
    Stop-TestApp
    Write-Host '[6/6] 實際建立系統匣選單及核對損毀檔保護'
    $tray=Join-Path $testRoot 'tray-fixture'
    $p=Start-Process -FilePath $ExecutablePath -ArgumentList @('--layouts-menu-test','--data-dir',(Quote-Arg $tray)) -PassThru -WindowStyle Hidden
    if(!$p.WaitForExit(18000)){$p.Kill();throw 'Tray smoke did not finish'}
    Check ($p.ExitCode -eq 0 -and (Get-Content -LiteralPath (Join-Path $tray 'tray-layout-results.txt') -TotalCount 1) -eq 'PASS checks=4') 'native tray menu checks: tick mark, switch, remove windows, restore'
    $broken=Join-Path $testRoot 'corrupt-catalog'
    New-Item -ItemType Directory -Path $broken|Out-Null
    $bad=Join-Path $broken 'layouts.hexdesk.json'
    [IO.File]::WriteAllText($bad,'{broken')
    $badHash=(Get-FileHash -LiteralPath $bad).Hash
    $p=Start-Process -FilePath $ExecutablePath -ArgumentList @('--layouts-menu-test','--data-dir',(Quote-Arg $broken)) -PassThru -WindowStyle Hidden
    if(!$p.WaitForExit(12000)){$p.Kill();throw 'Corrupt catalog was not rejected'}
    Check ($p.ExitCode -eq 1 -and (Get-FileHash -LiteralPath $bad).Hash -eq $badHash) 'corrupt catalog is preserved, not overwritten'
    @("PASS checks=$checkCount")+$results|Set-Content -LiteralPath $report -Encoding utf8
    Write-Host "PASS：$checkCount 項，結果 $report"
} catch {
    @("FAIL: "+$_.Exception.Message)+$results|Set-Content -LiteralPath $report -Encoding utf8
    throw
} finally {
    if($process -and !$process.HasExited){
        $null=[DeskTilesLayoutProbe]::Exit($process.Id)
        if(!$process.WaitForExit(3000)){$process.Kill()}
    }
}
