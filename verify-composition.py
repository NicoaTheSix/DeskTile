"""Verify normal packaged startup using actual desktop pixels, never PrintWindow."""
import ctypes as C
from ctypes import wintypes as W
from pathlib import Path
import argparse, subprocess, time, json, threading
from PIL import Image, ImageGrab
u=C.WinDLL('user32',use_last_error=True)
u.SetProcessDPIAware()
CB=C.WINFUNCTYPE(W.BOOL,W.HWND,W.LPARAM)
for name,args,result in [
 ('GetWindowLongPtrW',[W.HWND,C.c_int],C.c_ssize_t),('GetParent',[W.HWND],W.HWND),('UpdateWindow',[W.HWND],W.BOOL),
 ('GetWindow',[W.HWND,W.UINT],W.HWND),('GetDlgItem',[W.HWND,C.c_int],W.HWND),
 ('SendMessageW',[W.HWND,W.UINT,W.WPARAM,W.LPARAM],W.LPARAM),
 ('SetWindowPos',[W.HWND,W.HWND,C.c_int,C.c_int,C.c_int,C.c_int,W.UINT],W.BOOL),
 ('CreateWindowExW',[W.DWORD,W.LPCWSTR,W.LPCWSTR,W.DWORD,C.c_int,C.c_int,C.c_int,C.c_int,W.HWND,W.HMENU,W.HINSTANCE,C.c_void_p],W.HWND),
 ('DestroyWindow',[W.HWND],W.BOOL),('ShowWindow',[W.HWND,C.c_int],W.BOOL),('IsIconic',[W.HWND],W.BOOL),
 ('SetForegroundWindow',[W.HWND],W.BOOL),('EnumWindows',[CB,W.LPARAM],W.BOOL),('EnumChildWindows',[W.HWND,CB,W.LPARAM],W.BOOL),
 ('GetWindowRect',[W.HWND,C.POINTER(W.RECT)],W.BOOL),('WindowFromPoint',[W.POINT],W.HWND),('GetAncestor',[W.HWND,W.UINT],W.HWND),('SetWindowLongPtrW',[W.HWND,C.c_int,C.c_ssize_t],C.c_ssize_t),('GetWindowThreadProcessId',[W.HWND,C.POINTER(W.DWORD)],W.DWORD),
 ('GetClassNameW',[W.HWND,W.LPWSTR,C.c_int],C.c_int),('GetMessageW',[C.POINTER(W.MSG),W.HWND,W.UINT,W.UINT],W.BOOL),('DispatchMessageW',[C.POINTER(W.MSG)],W.LPARAM),('PostMessageW',[W.HWND,W.UINT,W.WPARAM,W.LPARAM],W.BOOL)]:
 f=getattr(u,name);f.argtypes=args;f.restype=result

def allwindows(pid):
 found={};seen=set()
 @CB
 def visit(h,_):
  if h in seen:return True
  seen.add(h);p=W.DWORD();u.GetWindowThreadProcessId(h,C.byref(p))
  if p.value==pid:
   name=C.create_unicode_buffer(128);u.GetClassNameW(h,name,128)
   if name.value=='HexDesk.Tile':found[int(u.GetWindowLongPtrW(h,-21))]=h
   elif name.value=='HexDesk.Control':found['control']=h
  return True
 @CB
 def top(h,_):visit(h,0);u.EnumChildWindows(h,visit,0);return True
 u.EnumWindows(top,0);return found

def rect(h):
 r=W.RECT();u.GetWindowRect(h,C.byref(r));return r.left,r.top,r.right,r.bottom

def meta(h):return dict(hwnd=h,parent=u.GetParent(h),owner=u.GetWindow(h,4),style=hex(u.GetWindowLongPtrW(h,-16)&0xffffffff),exstyle=hex(u.GetWindowLongPtrW(h,-20)&0xffffffff),rect=rect(h))

class WindowClass(C.Structure):
 _fields_=[('style',W.UINT),('proc',C.c_void_p),('class_extra',C.c_int),('window_extra',C.c_int),('instance',W.HINSTANCE),('icon',W.HICON),('cursor',W.HANDLE),('background',W.HBRUSH),('menu',W.LPCWSTR),('name',W.LPCWSTR)]
g=C.WinDLL('gdi32');g.GetStockObject.argtypes=[C.c_int];g.GetStockObject.restype=W.HANDLE
u.RegisterClassW.argtypes=[C.POINTER(WindowClass)];u.RegisterClassW.restype=W.WORD
u.SetClassLongPtrW.argtypes=[W.HWND,C.c_int,C.c_ssize_t];u.SetClassLongPtrW.restype=C.c_size_t
u.InvalidateRect.argtypes=[W.HWND,C.POINTER(W.RECT),W.BOOL]
k=C.WinDLL('kernel32');k.GetModuleHandleW.argtypes=[W.LPCWSTR];k.GetModuleHandleW.restype=W.HINSTANCE

def make_cover(x,y):
 ready=threading.Event();result=[]
 def worker():
  name='DeskTiles.Composition.Underlay.'+str(threading.get_ident())
  klass=WindowClass();klass.proc=C.cast(u.DefWindowProcW,C.c_void_p);klass.instance=k.GetModuleHandleW(None)
  klass.background=g.GetStockObject(0);klass.name=name
  if not u.RegisterClassW(C.byref(klass)):raise C.WinError(C.get_last_error())
  h=u.CreateWindowExW(0,name,'DeskTiles regression underlay',0x80000000,x,y,560,240,None,None,klass.instance,None)
  u.ShowWindow(h,5);u.ShowWindow(h,5);u.UpdateWindow(h);result.append(h);ready.set()
  msg=W.MSG()
  while u.GetMessageW(C.byref(msg),None,0,0)>0:
   if msg.message==0x8001:u.DestroyWindow(h);break
   u.TranslateMessage(C.byref(msg));u.DispatchMessageW(C.byref(msg))
 thread=threading.Thread(target=worker,daemon=True);thread.start();ready.wait(5)
 if not result:raise RuntimeError('Underlay creation failed')
 return result[0],thread

def foreground(h):
 u.GetForegroundWindow.restype=W.HWND
 k=C.WinDLL('kernel32');k.GetCurrentThreadId.restype=W.DWORD
 active=u.GetForegroundWindow();thread=u.GetWindowThreadProcessId(active,None);current=k.GetCurrentThreadId()
 u.AttachThreadInput.argtypes=[W.DWORD,W.DWORD,W.BOOL]
 linked=thread!=current and u.AttachThreadInput(current,thread,True)
 try:u.SetForegroundWindow(h)
 finally:
  if linked:u.AttachThreadInput(current,thread,False)

def underlay(h,black=False):
 # The class brush is persistent across WM_PAINT; a one-off DC fill can be erased
 # by a delayed paint or visibility transition and invalidate the pixel oracle.
 u.SetClassLongPtrW(h,-10,int(g.GetStockObject(4 if black else 0)))
 u.InvalidateRect(h,None,True);u.UpdateWindow(h);C.WinDLL('dwmapi').DwmFlush()

def white(h):underlay(h)

ORIGINALLY_MINIMIZED=set()
@CB
def remember_minimized(h,_):
 if u.IsIconic(h):ORIGINALLY_MINIMIZED.add(h)
 return True
u.EnumWindows(remember_minimized,0)

def main(force_desktop=False):
 p=argparse.ArgumentParser();p.add_argument('--executable',default=str(Path(__file__).resolve().parent/'release/DeskTiles/DeskTiles.exe'));p.add_argument('--inspect-pid',type=int);p.add_argument('--desktop-start',action='store_true');p.add_argument('--both',action='store_true');a=p.parse_args();a.desktop_start|=force_desktop
 if a.inspect_pid:
  print(json.dumps({str(k):meta(v) for k,v in allwindows(a.inspect_pid).items()},indent=2));return
 exe=Path(a.executable).resolve();work=Path(__file__).resolve().parent/'build'/('composition-'+time.strftime('%Y%m%d-%H%M%S')+f'-{time.time_ns()%1000000:06d}');work.mkdir(parents=True)
 print(f'Input executable: {exe}\nInput layout: {work / "layout.hexdesk"}\nOutput screenshots/results: {work}',flush=True)
 # Launch an ordinary process with either saved layer; retain the same handles during switches.
 # Choose an exposed desktop area so desktop-mode pixels can be verified without
 # minimizing or closing any user's window. Only class/owner metadata is inspected.
 for h in ORIGINALLY_MINIMIZED:
  if not u.IsIconic(h):u.ShowWindow(h,7)
 time.sleep(.8)
 origin=None
 for y in range(u.GetSystemMetrics(1)-320,120,-240):
  for x in range(60,u.GetSystemMetrics(0)-620,300):
   roots=[]
   for dx,dy in [(0,0),(560,0),(0,240),(560,240),(280,120)]:
    root=u.GetAncestor(u.WindowFromPoint(W.POINT(x+dx,y+dy)),2)
    name=C.create_unicode_buffer(128);u.GetClassNameW(root,name,128);roots.append(name.value)
   if all(n in ('Progman','WorkerW') for n in roots):origin=(x,y);break
  if origin:break
 if not origin:raise RuntimeError('No exposed desktop area for compositor test; expose a 600x300 area and retry.')
 initially_minimized=ORIGINALLY_MINIMIZED
 ox,oy=origin
 lines=['HEXDESK 5','2']
 for i,shape in [(1,0),(2,2)]:
  lines.append(f'{i} {ox+20+(i-1)*260} {oy+20} 0 {0 if a.desktop_start else 1} 1 300 0 "DESKTILES ABC" "" 0 16711680 6 6 0 220 "Microsoft JhengHei UI" 30 16777215 2289504 0 {shape}')
 (work/'layout.hexdesk').write_text('\n'.join(lines)+'\n',encoding='utf-8')
 (work/'ui-settings.json').write_text('{"version":1,"language":"en"}',encoding='utf-8')
 print('[1/4] Launch without test flags',flush=True)
 app=subprocess.Popen([str(exe),'--data-dir',str(work)]);cover=None;checks=[]
 def check(ok,name,detail=''):
  checks.append((bool(ok),name,detail))
 def raise_cover():
  u.SetWindowPos(cover,W.HWND(-1),0,0,0,0,0x13)
  u.SetWindowPos(cover,W.HWND(-2),0,0,0,0,0x13)
  foreground(cover);white(cover);time.sleep(.3)
 def sample(label):
  if 'desktop' in label and 'to-top' not in label:
   # Live tool output can reactivate a window that the wrapper just minimized.
   # Re-minimize only those same handles; the wrapper restores their original state.
   for attempt in range(3):
    interferers=set()
    for i in (1,2):
     r=rect(views[i]);h=u.GetAncestor(u.WindowFromPoint(W.POINT((r[0]+r[2])//2,(r[1]+r[3])//2)),2)
     if h in initially_minimized:interferers.add(h)
    for h in interferers:u.ShowWindow(h,7)
    if not interferers:break
    time.sleep(.2)
  if cover and not u.GetParent(views[1]):raise_cover()
  time.sleep(.8)
  for i in (1,2):
   h=views[i];r=rect(h)
   # Check occlusion at capture time, after activation/minimize animations settle.
   # A different application is an environmental blocker, not a rendering verdict.
   for attempt in range(8):
    point=W.POINT((r[0]+r[2])//2,(r[1]+r[3])//2)
    hit_root=u.GetAncestor(u.WindowFromPoint(point),2)
    if hit_root not in initially_minimized:break
    u.ShowWindow(hit_root,7);time.sleep(.6)
   im=ImageGrab.grab(bbox=r,all_screens=True);im.save(work/f'{label}-{i}.png')
   # Lower interior excludes text; red border count proves real content, not a black polygon.
   color=im.getpixel((im.width//2,int(im.height*.88)))
   red=sum(1 for rr,gg,bb in im.get_flattened_data() if rr>200 and gg<50 and bb<50)
   check(sum(abs(x-y) for x,y in zip(color,(34,239,96)))<15,label+f' visible background {i}',color)
   if red<=100:
    point=W.POINT(r[0]+im.width//2,r[1]+int(im.height*.88));hit=u.WindowFromPoint(point)
    print(label+' diagnostic visible root: '+str(meta(u.GetAncestor(hit,2))))
   check(red>100,label+f' visible red border {i}',red)
 try:
  end=time.monotonic()+10
  while time.monotonic()<end:
   views=allwindows(app.pid)
   if all(k in views for k in (1,2,'control')):break
   if app.poll() is not None:raise RuntimeError('App exited')
   time.sleep(.1)
  else:raise RuntimeError('Windows missing')
  u.ShowWindow(views['control'],0)
  # Actual pixels and ordinary foreground occluder.
  if a.desktop_start:
   sample('normal-start-desktop')
   u.SendMessageW(views['control'],0x111,118,0);u.SendMessageW(views['control'],0x111,113,0)
  cover,cover_thread=make_cover(ox,oy)
  u.ShowWindow(cover,5);u.UpdateWindow(cover);u.SetWindowPos(cover,0,0,0,0,0,0x13);u.SetForegroundWindow(cover)
  sample('normal-start-top')
  ImageGrab.grab(bbox=rect(cover),all_screens=True).save(work/'cover-start.png')
  for i in (1,2):check(bool(u.GetWindowLongPtrW(views[i],-20)&8),'topmost style '+str(i),meta(views[i]))
  # Desktop then return to on top against the same occluder.
  u.SendMessageW(views['control'],0x111,118,0)
  u.SendMessageW(views['control'],0x111,114,0);time.sleep(.6)
  for i in (1,2):check(bool(u.GetWindow(views[i],4)) and not(u.GetWindowLongPtrW(views[i],-16)&0x40000000) and not(u.GetWindowLongPtrW(views[i],-20)&8),'desktop-owned composited popup '+str(i),meta(views[i]))
  raise_cover();u.UpdateWindow(cover);time.sleep(.8)
  ImageGrab.grab(bbox=rect(cover),all_screens=True).save(work/'cover-desktop.png')
  # Occluder must cover desktop-layer shapes.
  raise_cover();white(cover)
  im=ImageGrab.grab(bbox=rect(views[1]),all_screens=True)
  check(im.getpixel((110,150))==(255,255,255),'desktop stays below ordinary window',im.getpixel((110,150)))
  check(u.GetAncestor(u.WindowFromPoint(W.POINT(ox+130,oy+170)),2)==cover,'ordinary foreground window receives desktop-layer input')
  u.ShowWindow(cover,0);u.SetForegroundWindow(u.GetWindow(views[1],4));time.sleep(.6)
  sample('switched-desktop')
  u.ShowWindow(cover,5)
  u.SendMessageW(views['control'],0x111,113,0)
  u.SetWindowPos(cover,0,0,0,0,0,0x13);u.SetForegroundWindow(cover)
  sample('desktop-to-top')
  for i in (1,2):check(not u.GetParent(views[i]) and bool(u.GetWindowLongPtrW(views[i],-20)&8),'returned to real topmost '+str(i),meta(views[i]))
  # Fully transparent background preserves content and input.
  u.SendMessageW(views['control'],0x111,138,0)
  panels=[]
  @CB
  def find_panel(h,_):
   name=C.create_unicode_buffer(128);u.GetClassNameW(h,name,128)
   if name.value=='HexDesk.Background':panels.append(h)
   return True
  u.EnumWindows(find_panel,0)
  panel=panels[0];alpha=u.GetDlgItem(panel,140);value=C.create_unicode_buffer('100')
  u.SendMessageW(alpha,0xC,0,C.addressof(value));u.SendMessageW(panel,0x111,144,0);u.ShowWindow(panel,0)
  # Isolate the alpha oracle from unrelated applications. Both fixture windows
  # stay above this topmost white plate; ordinary-window z order was tested above.
  u.SetWindowPos(cover,W.HWND(-1),0,0,0,0,0x13);white(cover)
  for i in (1,2):u.SetWindowPos(views[i],W.HWND(-1),0,0,0,0,0x13)
  C.WinDLL('dwmapi').DwmFlush();time.sleep(.8)
  for i in (1,2):
   im=ImageGrab.grab(bbox=rect(views[i]),all_screens=True);im.save(work/f'transparent-{i}.png')
   color=im.getpixel((im.width//2,int(im.height*.88)))
   check(color==(255,255,255),'fully transparent actual background '+str(i),color)
   r=rect(views[i]);point=W.POINT(r[0]+im.width//2,r[1]+int(im.height*.88))
   check(u.WindowFromPoint(point)==views[i],'transparent area accepts real mouse input '+str(i))
   red=sum(1 for rr,gg,bb in im.get_flattened_data() if rr>200 and gg<50 and bb<50)
   check(red>100,'transparent background retains border '+str(i),red)
  underlay(cover,black=True);time.sleep(.3)
  for i in (1,2):
   im=ImageGrab.grab(bbox=rect(views[i]),all_screens=True);im.save(work/f'transparent-black-{i}.png')
   center=im.crop((im.width//4,im.height//3,im.width*3//4,im.height*3//4))
   white_text=sum(1 for rr,gg,bb in center.get_flattened_data() if rr>220 and gg>220 and bb>220)
   check(white_text>40,'transparent background retains visible text '+str(i),white_text)

  # The same fully transparent point must also support an actual window-message drag.
  tile=views[1];before=rect(tile);px=110;py=int((before[3]-before[1])*.88)
  def mouse(msg,x,y,keys=0):u.SendMessageW(tile,msg,keys,(x&65535)|((y&65535)<<16))
  mouse(0x201,px,py,1);mouse(0x200,px+15,py-10,1);mouse(0x202,px,py)
  after=rect(tile)
  check(after[0]==before[0]+15 and after[1]==before[1]-10,'transparent point selects and drags tile',after)
  # Verify a real image on the real layered window, including background alpha 0.
  image_path=work/'sample.png';Image.new('RGB',(64,64),(52,120,220)).save(image_path)
  content=u.GetDlgItem(views['control'],102);u.SendMessageW(content,0x14E,1,0)
  path=C.create_unicode_buffer(str(image_path));u.SendMessageW(u.GetDlgItem(views['control'],104),0xC,0,C.addressof(path))
  u.SendMessageW(views['control'],0x111,108,0);time.sleep(.6)
  im=ImageGrab.grab(bbox=rect(tile),all_screens=True);im.save(work/'transparent-image.png')
  check(im.getpixel((im.width//2,im.height//2))==(52,120,220),'image keeps full color with transparent background')

 finally:
  if cover:u.PostMessageW(cover,0x8001,0,0);cover_thread.join(2)
  if app.poll() is None:
   if 'views' in locals() and 'control' in views:u.SendMessageW(views['control'],0x111,116,0)
   try:app.wait(timeout=3)
   except subprocess.TimeoutExpired:app.terminate();app.wait()
  (work/'results.json').write_text(json.dumps(checks,indent=2),encoding='utf-8')
 for ok,name,detail in checks:print(('PASS ' if ok else 'FAIL ')+name+' '+str(detail))
 print(f'Result: {sum(c[0] for c in checks)}/{len(checks)} checks passed; {work / "results.json"}')
 if not checks or any(not c[0] for c in checks):raise SystemExit(1)
 if a.both and not force_desktop:main(True)

if __name__=='__main__':main()
