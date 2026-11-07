param([string]$ExecutableName='DeskTiles-update.exe',[switch]$FullEnglish,[string]$ExecutablePath='')
$ErrorActionPreference='Stop'
$executable=if($ExecutablePath){[IO.Path]::GetFullPath($ExecutablePath)}else{Join-Path $PSScriptRoot ('build\'+$ExecutableName)}
$testRoot=Join-Path $PSScriptRoot ('build\language-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
Write-Host "輸入執行檔絕對路徑：$executable"
Write-Host "輸出專用測試資料／結果絕對路徑：$testRoot"
New-Item -ItemType Directory -Path $testRoot -Force | Out-Null
Write-Host '[語言驗證 1/4] 實際介面、草稿保留、錯誤與設定保存保護'
$process=Start-Process -FilePath $executable -ArgumentList @('--language-test','--data-dir',('"'+$testRoot+'"')) -PassThru -WindowStyle Hidden
if(-not $process.WaitForExit(15000)){$process.Kill();throw '語言回歸驗證逾時'}
if($process.ExitCode -ne 0){Get-Content (Join-Path $testRoot 'session.log') -Tail 12;throw '語言回歸驗證失敗'}
Get-Content -LiteralPath (Join-Path $testRoot 'language-results.txt')
Add-Type -TypeDefinition @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class DeskTilesLanguageFixture {
    private delegate bool Visitor(IntPtr window,IntPtr state);
    [DllImport("user32.dll")] private static extern bool EnumWindows(Visitor callback,IntPtr state);
    [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr window,out uint process);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] private static extern int GetClassName(IntPtr window,StringBuilder name,int size);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] private static extern int GetWindowText(IntPtr window,StringBuilder text,int size);
    [DllImport("user32.dll")] private static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr window,int id);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr window,uint message,IntPtr w,IntPtr l);
    public static IntPtr Find(int process) {
        IntPtr found=IntPtr.Zero;
        EnumWindows((window,state)=>{uint owner;GetWindowThreadProcessId(window,out owner);
            if(owner!=(uint)process)return true;
            var name=new StringBuilder(128);GetClassName(window,name,128);
            if(name.ToString()!="HexDesk.Control" || GetDlgItem(window,154)==IntPtr.Zero)return true;
            found=window;return false;},IntPtr.Zero);
        return found;
    }
    public static string Text(IntPtr window){var text=new StringBuilder(1024);GetWindowText(window,text,1024);return text.ToString();}
    public static void SelectLanguage(IntPtr window,int index){var combo=GetDlgItem(window,154);SendMessage(combo,0x14e,new IntPtr(index),IntPtr.Zero);SendMessage(window,0x111,new IntPtr(154|(1<<16)),combo);}
    public static void Exit(IntPtr window){SendMessage(window,0x111,new IntPtr(116),IntPtr.Zero);}
}
"@
function Start-Fixture([string]$Directory) {
    $taskProcess=Start-Process -FilePath $executable -ArgumentList @('--data-dir',('"'+$Directory+'"')) -PassThru -WindowStyle Hidden
    [void]$taskProcess.WaitForInputIdle(5000)
    for($attempt=0;$attempt -lt 100;$attempt++) {
        if($taskProcess.HasExited){throw '測試程序提早退出'}
        $controller=[DeskTilesLanguageFixture]::Find($taskProcess.Id)
        if($controller -ne [IntPtr]::Zero){return @{Process=$taskProcess;Window=$controller}}
        Start-Sleep -Milliseconds 50
    }
    $taskProcess.Kill();throw '找不到語言測試視窗'
}
function Stop-Fixture($Fixture) {
    [DeskTilesLanguageFixture]::Exit($Fixture.Window)
    if(-not $Fixture.Process.WaitForExit(5000)){$Fixture.Process.Kill();throw '測試程序未正常結束'}
    if($Fixture.Process.ExitCode -ne 0){throw '測試程序錯誤結束'}
}
Write-Host '[語言驗證 2/4] 重新啟動沿用英文，再由實際選單切回繁體中文'
$beforeHash=(Get-FileHash -LiteralPath (Join-Path $testRoot 'layout.hexdesk')).Hash
$fixture=Start-Fixture $testRoot
try {
    if([DeskTilesLanguageFixture]::Text($fixture.Window) -ne 'DeskTiles'){throw '重新啟動未沿用英文'}
    $add=[DeskTilesLanguageFixture]::GetDlgItem($fixture.Window,109)
    if([DeskTilesLanguageFixture]::Text($add) -ne '+ Add tile'){throw '重新啟動英文按鈕有誤'}
    [DeskTilesLanguageFixture]::SelectLanguage($fixture.Window,0)
    if([DeskTilesLanguageFixture]::Text($add) -ne '＋ 新增圖形'){throw '繁體中文選單切換未生效'}
} finally {Stop-Fixture $fixture}
$fixture=Start-Fixture $testRoot
try {
    if([DeskTilesLanguageFixture]::Text($fixture.Window) -ne '桌面拼格 · DeskTiles'){throw '重新啟動未沿用繁體中文'}
} finally {Stop-Fixture $fixture}
if((Get-FileHash -LiteralPath (Join-Path $testRoot 'layout.hexdesk')).Hash -ne $beforeHash){throw '切換或重啟改動了原版面'}
Write-Host '[語言驗證 3/4] 損壞偏好檔回復預設語言，保留原設定檔'
$invalidRoot=Join-Path $testRoot 'invalid-preferences'
New-Item -ItemType Directory -Path $invalidRoot | Out-Null
$invalidFile=Join-Path $invalidRoot 'ui-settings.json'
[IO.File]::WriteAllText($invalidFile,'{invalid preferences')
$invalidHash=(Get-FileHash -LiteralPath $invalidFile).Hash
$fixture=Start-Fixture $invalidRoot
try {if([DeskTilesLanguageFixture]::Text($fixture.Window) -ne '桌面拼格 · DeskTiles'){throw '損壞偏好未安全回復預設語言'}}finally{Stop-Fixture $fixture}
if((Get-FileHash -LiteralPath $invalidFile).Hash -ne $invalidHash){throw '啟動時覆寫損壞的偏好檔'}
Write-Host '[語言驗證 4/4] 英文環境完整功能驗證'
if($FullEnglish) {
    $englishRoot=Join-Path $testRoot 'english-full'
    New-Item -ItemType Directory -Path $englishRoot | Out-Null
    [IO.File]::WriteAllText((Join-Path $englishRoot 'ui-settings.json'),'{"language":"en","version":1}')
    $process=Start-Process -FilePath $executable -ArgumentList @('--smoke-test','--data-dir',('"'+$englishRoot+'"')) -PassThru -WindowStyle Hidden
    if(-not $process.WaitForExit(30000)){$process.Kill();throw '英文完整驗證逾時'}
    if($process.ExitCode -ne 0){Get-Content (Join-Path $englishRoot 'session.log') -Tail 15;throw '英文完整驗證失敗'}
    Get-Content -LiteralPath (Join-Path $englishRoot 'integration-results.txt')
} else {Write-Host '快速模式；加 -FullEnglish 可執行全部 Windows 整合檢查。'}
@('PASS: language switch preserves pending edits, focus, selection and timers','PASS: English preference survives actual process restart','PASS: Traditional Chinese preference survives actual process restart','PASS: layout SHA256 unchanged across switching/restarts','PASS: corrupt preferences safely default without overwriting source',"Full English integration: $FullEnglish") | Set-Content -LiteralPath (Join-Path $testRoot 'language-verification.txt') -Encoding utf8
Write-Host "PASS：$testRoot\language-verification.txt"
