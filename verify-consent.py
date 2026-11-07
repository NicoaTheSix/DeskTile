"""Exercise real UI consent rejection and alarm restart; never grants autostart consent."""
import argparse, ctypes as C, datetime, json, runpy, shlex, subprocess, threading, time, winreg
from pathlib import Path

root=Path(__file__).resolve().parent
native=runpy.run_path(str(root/'record-demo.py'))
u=native['u'];W=native['W'];CB=native['CALLBACK'];capture=native['capture'];windows=native['windows'];command=native['command'];set_text=native['set_text']
u.GetWindowThreadProcessId.argtypes=[W.HWND,C.POINTER(W.DWORD)];u.GetWindowThreadProcessId.restype=W.DWORD
u.EnumChildWindows.argtypes=[W.HWND,CB,W.LPARAM]
u.GetWindowTextLengthW.argtypes=[W.HWND];u.GetWindowTextLengthW.restype=C.c_int
u.GetWindowTextW.argtypes=[W.HWND,W.LPWSTR,C.c_int];u.ShowWindow.argtypes=[W.HWND,C.c_int]

def text(h):
    value=C.create_unicode_buffer(u.GetWindowTextLengthW(h)+1);u.GetWindowTextW(h,value,len(value));return value.value

def find_class(pid,klass):
    found=[]
    @CB
    def visit(h,_):
        p=W.DWORD();u.GetWindowThreadProcessId(h,C.byref(p));name=C.create_unicode_buffer(128);u.GetClassNameW(h,name,128)
        if p.value==pid and name.value==klass:found.append(h)
        return True
    u.EnumWindows(visit,0);return found[0] if found else None

def wait_for(getter,timeout=10):
    deadline=time.monotonic()+timeout
    while time.monotonic()<deadline:
        value=getter()
        if value:return value
        time.sleep(.05)
    raise RuntimeError('Timed out waiting for fixture UI')

def login_value():
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER,r'Software\Microsoft\Windows\CurrentVersion\Run') as key:
            return winreg.QueryValueEx(key,'DeskTiles')
    except FileNotFoundError:return None

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--executable',default=str(root/'build/DeskTiles-update.exe'));args=parser.parse_args()
    exe=Path(args.executable).resolve();work=root/'build'/('consent-'+time.strftime('%Y%m%d-%H%M%S'));
    print(f'Input EXE: {exe}\nInput/output fictional layout: {work / "layout.hexdesk"}\nOutput results: {work / "results.json"}\nOutput screenshot directory: {work}',flush=True)
    work.mkdir()
    before=login_value();checks=[];app=None;control=None
    def check(ok,label):
        checks.append((bool(ok),label))
        if not ok:raise AssertionError(label)
    def launch(startup=False):
        p=subprocess.Popen([str(exe)]+(['--startup'] if startup else [])+['--data-dir',str(work)])
        ctl=wait_for(lambda:windows(p.pid).get('control'));time.sleep(.25)
        return p,ctl
    def close():
        nonlocal app,control
        if app and app.poll() is None:
            command(control,116)
            try:app.wait(3)
            except subprocess.TimeoutExpired:app.terminate();app.wait()
    try:
        print('[1/4] Ordinary startup; timer text and alarm controls',flush=True)
        app,control=launch();u.ShowWindow(control,5);u.ShowWindow(control,5)
        checkbox=u.GetDlgItem(control,159)
        # If a user has already registered startup, leave it untouched; no toggle is sent.
        if before is not None:raise RuntimeError('An existing user startup registration is present; consent UI test must not toggle it.')
        check(u.SendMessageW(checkbox,0xF0,0,0)==0,'ordinary startup leaves login registration off')
        set_text(u.GetDlgItem(control,103),'專注工作 / Focus')
        u.SendMessageW(u.GetDlgItem(control,102),0x14E,2,0);u.SendMessageW(control,0x111,(1<<16)|102,0);command(control,108)
        capture(control).save(work/'timer-text.png')
        set_text(u.GetDlgItem(control,103),'讀書 / Study')
        u.SendMessageW(u.GetDlgItem(control,102),0x14E,3,0);u.SendMessageW(control,0x111,(1<<16)|102,0);command(control,158)
        alarm=wait_for(lambda:find_class(app.pid,'HexDesk.Alarm'));u.ShowWindow(alarm,5)
        check(u.SendMessageW(u.GetDlgItem(alarm,162),0x147,0,0)==0,'new alarm repeat defaults to Once')
        future=datetime.datetime.now()+datetime.timedelta(days=2)
        set_text(u.GetDlgItem(alarm,160),future.strftime('%Y-%m-%d'));set_text(u.GetDlgItem(alarm,161),'07:30')
        u.SendMessageW(u.GetDlgItem(alarm,163),0xF1,1,0);command(alarm,164)
        tokens=shlex.split((work/'layout.hexdesk').read_text(encoding='utf-8'))
        check(tokens[1]=='6' and tokens[6]=='3' and tokens[11]=='讀書 / Study' and tokens[25]=='0' and tokens[26]=='450' and tokens[29]=='1','ordinary UI saves a one-time alarm and its text in v6')
        capture(alarm).save(work/'alarm-once.png');u.ShowWindow(alarm,0)
        print('[2/4] Real consent dialog: verify paths/default No, then explicitly decline',flush=True)
        worker=threading.Thread(target=lambda:u.SendMessageW(checkbox,0xF5,0,0),daemon=True);worker.start()
        dialog=wait_for(lambda:find_class(app.pid,'#32770'))
        try:
            values=[]
            @CB
            def child(h,_):values.append(text(h));return True
            u.EnumChildWindows(dialog,child,0);body='\n'.join(values)
            check(str(exe) in body and str(work) in body,'consent dialog displays absolute program and data paths')
            no=u.GetDlgItem(dialog,7)
            check(no and (u.GetWindowLongPtrW(no,-16)&15)==1,'No is the default consent button')
            capture(dialog).save(work/'startup-consent.png')
        finally:
            # The test never presses Yes or registers real automatic execution.
            u.SendMessageW(dialog,0x111,7,0);worker.join(3)
        check(u.SendMessageW(checkbox,0xF0,0,0)==0 and login_value()==before,'declining consent restores unchecked state and leaves Run untouched')
        print('[3/4] English alarm UI and actual restart with --startup',flush=True)
        command(control,155);command(control,158);u.ShowWindow(alarm,5)
        check('Alarm settings' in text(alarm),'alarm settings title switches to English')
        capture(alarm).save(work/'alarm-en.png');u.ShowWindow(alarm,0)
        before_layout=(work/'layout.hexdesk').read_bytes();close()
        app,control=launch(True)
        check(not u.IsWindowVisible(control),'login launch hides controller and retains tray/tiles')
        check((work/'layout.hexdesk').read_bytes()==before_layout,'actual restart preserves future alarm and custom text')
        check(login_value()==before,'--startup argument does not register automatic execution')
        print('[4/4] Save evidence and stop only the fictional fixture process',flush=True)
    finally:
        close()
        if login_value()!=before:raise AssertionError('real startup registration unexpectedly changed')
        (work/'results.json').write_text(json.dumps(checks,indent=2),encoding='utf-8')
    print(f'PASS {len(checks)} consent/UI/restart checks; real login registration unchanged; {work}',flush=True)

if __name__=='__main__':main()
