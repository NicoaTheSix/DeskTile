param([string]$ExecutableName = 'DeskTiles-update.exe',[string]$FirstExecutableName = '')
$ErrorActionPreference = 'Stop'
$executable = Join-Path $PSScriptRoot ('build\' + $ExecutableName)
$firstExecutable = if ($FirstExecutableName) { Join-Path $PSScriptRoot ('build\' + $FirstExecutableName) } else { $executable }
$testDirectory = Join-Path $PSScriptRoot ('build\activation-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
Write-Host "輸入執行檔絕對路徑：$executable"
Write-Host "第一個執行個體輸入：$firstExecutable"
Write-Host "輸入／輸出測試目錄：$testDirectory"
Write-Host '[1/3] 啟動專用測試執行個體'
New-Item -ItemType Directory -Path $testDirectory -Force | Out-Null
Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class DeskTilesActivationFixture {
    private delegate bool Visitor(IntPtr window, IntPtr state);
    [DllImport("user32.dll")] private static extern bool EnumWindows(Visitor callback, IntPtr state);
    [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr window, out uint process);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern int GetClassName(IntPtr window, StringBuilder name, int size);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll")] public static extern bool ShowWindowAsync(IntPtr window, int command);
    public static IntPtr Find(int process) {
        IntPtr found=IntPtr.Zero;
        EnumWindows((window,state)=>{
            uint owner;GetWindowThreadProcessId(window,out owner);
            if(owner!=(uint)process)return true;
            var name=new StringBuilder(128);GetClassName(window,name,128);
            if(name.ToString()!="HexDesk.Control")return true;
            found=window;return false;
        },IntPtr.Zero);
        return found;
    }
}
'@
$first = Start-Process -FilePath $firstExecutable -ArgumentList @('--data-dir', ('"' + $testDirectory + '"')) -PassThru -WindowStyle Hidden
try {
    if (-not $first.WaitForInputIdle(5000)) { throw '第一個測試執行個體未完成初始化。' }
    if ($first.HasExited) { throw '第一個測試執行個體提早退出。' }
    $controller = [DeskTilesActivationFixture]::Find($first.Id)
    for ($attempt=0; $attempt -lt 100 -and ($controller -eq [IntPtr]::Zero -or -not (Test-Path -LiteralPath (Join-Path $testDirectory 'layout.hexdesk'))); $attempt++) {
        Start-Sleep -Milliseconds 50
        $controller = [DeskTilesActivationFixture]::Find($first.Id)
    }
    if ($controller -eq [IntPtr]::Zero) { throw '找不到測試控制視窗。' }
    [DeskTilesActivationFixture]::ShowWindowAsync($controller,0) | Out-Null
    for ($attempt=0; $attempt -lt 20 -and [DeskTilesActivationFixture]::IsWindowVisible($controller); $attempt++) { Start-Sleep -Milliseconds 25 }
    if ([DeskTilesActivationFixture]::IsWindowVisible($controller)) { throw '無法隱藏測試控制視窗。' }
    $beforeHash = (Get-FileHash -LiteralPath (Join-Path $testDirectory 'layout.hexdesk') -Algorithm SHA256).Hash
    Write-Host '[2/3] 再次開啟相同目錄，應帶回既有視窗'
    $second = Start-Process -FilePath $executable -ArgumentList @('--smoke-test','--data-dir', ('"' + $testDirectory + '"')) -PassThru -WindowStyle Hidden
    if (-not $second.WaitForExit(5000)) { $second.Kill(); throw '第二個執行個體未正常交接。' }
    if ($second.ExitCode -ne 0) { throw "重現：第二次開啟被擋住，exit=$($second.ExitCode)；預期正常帶回既有視窗並退出 0。" }
    for ($attempt=0; $attempt -lt 20 -and -not [DeskTilesActivationFixture]::IsWindowVisible($controller); $attempt++) { Start-Sleep -Milliseconds 25 }
    if (-not [DeskTilesActivationFixture]::IsWindowVisible($controller)) { throw '第二次啟動沒有帶回被隱藏的控制視窗。' }
    $first.Refresh()
    if ($first.HasExited) { throw '帶回視窗時意外結束了原執行個體。' }
    if ((Get-FileHash -LiteralPath (Join-Path $testDirectory 'layout.hexdesk') -Algorithm SHA256).Hash -ne $beforeHash) { throw '啟動交接修改了版面檔案。' }
    Write-Host '[3/3] 重複啟動交接通過；原執行個體仍在執行'
    @('PASS: duplicate launch activates hidden controller','PASS: existing process stays alive','PASS: layout SHA256 unchanged') | Set-Content -LiteralPath (Join-Path $testDirectory 'activation-results.txt') -Encoding utf8
} finally {
    # Only this script's disposable fixture process is stopped, never the user's app.
    $first.Refresh()
    if (-not $first.HasExited) { $first.Kill(); $first.WaitForExit() }
    $first.Dispose()
}
