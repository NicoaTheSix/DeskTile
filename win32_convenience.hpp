#pragma once
#include "convenience.hpp"
#include <commctrl.h>
#pragma comment(lib,"comctl32.lib")

namespace desktools {
using namespace desktile_convenience;
enum CommandId {Undo=4100,Redo,Trash,Lock,Through,RecoverInput,HideAll,Options,Templates,Share,ImportShare,Sync,Steam};
struct State {
    History history;
    bool restoring=false,ready=false;
    Json preferences={{"locked",Json::array()},{"through",Json::array()},{"fullscreen",true},{"mute",false},
        {"manualFocus",false},{"hidden",false},{"hotkeys",Json::array({0x064E,0x0654,0x064C,0x0648,0x0655})},
        {"monitors",Json::object()},{"syncFolder",""},{"syncBase",""},{"steamBase",""},{"steamAuto",false},{"shareText",false},{"sharePhotos",false}};
    std::vector<RemovedTile> trash;
    HWND editor=nullptr;
    int editing=0,editingLayout=0;
    WNDPROC editProc=nullptr;
    bool focused=false;
    std::string monitorKey;
    std::vector<RECT> areas;
} state;
const wchar_t* L(const wchar_t* zh,const wchar_t* en){return LayoutLabel(zh,en);}
fs::path PreferencesPath(){return app.directory/L"desktop-options.json";}
void SaveOptions() {
    AtomicOutput(PreferencesPath(),[](std::ostream& out){out<<state.preferences.dump(2);});
}
std::string TileKey(int layout,int id){return std::to_string(layout)+"/"+std::to_string(id);}
bool Flag(const char* key,int id) {
    const auto wanted=TileKey(app.catalog.active,id);
    for(const auto& value:state.preferences.at(key))if(value==wanted)return true;
    return false;
}
void SetFlag(const char* key,int id,bool enable) {
    auto& flags=state.preferences[key];const auto wanted=TileKey(app.catalog.active,id);
    flags.erase(std::remove(flags.begin(),flags.end(),wanted),flags.end());
    if(enable)flags.push_back(wanted);
}
bool Hidden(){return state.ready&&(state.focused||state.preferences.value("hidden",false));}
bool Muted(){return state.ready&&state.focused&&state.preferences.value("mute",false);}
bool Editing(){return state.editor!=nullptr;}
bool Locked(int id){return state.ready&&Flag("locked",id);}
bool ThroughTile(int id){return state.ready&&Flag("through",id);}
void UpdateInput() {
    for(auto& view:app.views){
        const auto style=GetWindowLongPtrW(view.second.window,GWL_EXSTYLE);
        const bool through=ThroughTile(view.first);
        SetWindowLongPtrW(view.second.window,GWL_EXSTYLE,through?(style|WS_EX_TRANSPARENT|WS_EX_LAYERED):(style&~(WS_EX_TRANSPARENT|WS_EX_LAYERED)));
        EnableWindow(view.second.window,TRUE);EnableWindow(view.second.visual,TRUE);
        ShowWindow(view.second.visual,through?SW_HIDE:SW_SHOWNA);
        if(!through)ConfigureTileComposition(view.second.window);
        SetWindowPos(view.second.window,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
    }
}
void RefreshDesktop() {
    UpdateInput();
    for(auto& tile:app.tiles){Position(tile);PresentTile(tile);}
    UpdateAnimation();Repaint();
}
void PersistTrash() {
    Json list=Json::array();
    for(const auto& item:state.trash){
        std::stringstream body;Save(body,{item.tile});
        list.push_back({{"layout",item.layout},{"name",item.name},{"tile",body.str()}});
    }
    AtomicOutput(app.directory/L"recycle.json",[&](std::ostream& out){out<<list;});
}
void Record() {
    if(!state.ready||state.restoring||app.savingFailed)return;
    try {
        auto catalog=CurrentCatalog();
        if(EditKey(catalog)==EditKey(state.history.saved)){state.history.saved=catalog;return;}
        auto removed=Removed(state.history.saved,catalog);
        for(auto& item:removed)state.trash.push_back(std::move(item));
        if(state.trash.size()>100)state.trash.erase(state.trash.begin(),state.trash.end()-100);
        if(!removed.empty())PersistTrash();
        state.history.Record(catalog);
    }catch(const std::exception& e){Log(L"History: "+Wide(e.what()));}
}
void ApplySnapshot(LayoutCatalog next,bool preserveClock) {
    EndDrag(false);
    if(app.sharedClock)TickTimers();
    if(preserveClock)PreserveClocks(next,CurrentCatalog());
    state.restoring=true;
    try {ApplyCatalog(std::move(next),0,true);state.restoring=false;}
    catch(...){state.restoring=false;throw;}
    RefreshDesktop();
}
void Travel(bool undo) {
    auto& from=undo?state.history.undo:state.history.redo;
    auto& to=undo?state.history.redo:state.history.undo;
    if(from.empty())return;
    auto current=CurrentCatalog();
    const auto target=from.back();
    ApplySnapshot(target,true);
    from.pop_back();to.push_back(current);TrimHistory(to);
    state.history.saved=CurrentCatalog();
    Status(L(L"已還原版面；正在運行的計時保留。",L"Layout restored; active timers preserved."));
}
void Commit(LayoutCatalog next) {
    ApplyCatalog(std::move(next),0,true);
    RefreshDesktop();
}

struct Field {
    std::wstring label,value;
    int type=0;
    bool checked=false;
};
struct Form {
    std::wstring title;
    std::vector<Field> fields;
    std::wstring note;
};
INT_PTR CALLBACK FormProc(HWND dialog,UINT message,WPARAM w,LPARAM l) {
    if(message==WM_INITDIALOG){
        auto* form=reinterpret_cast<Form*>(l);SetWindowLongPtrW(dialog,DWLP_USER,l);
        SetWindowTextW(dialog,form->title.c_str());
        auto widget=[&](const wchar_t* type,const wchar_t* text,DWORD style,int id,int x,int y,int width,int height){
            HWND field=CreateWindowExW(wcscmp(type,L"EDIT")==0?WS_EX_CLIENTEDGE:0,type,text,WS_CHILD|WS_VISIBLE|style,
                x,y,width,height,dialog,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),app.instance,nullptr);
            SendMessageW(field,WM_SETFONT,reinterpret_cast<WPARAM>(app.font),TRUE);return field;
        };
        RECT r{};GetClientRect(dialog,&r);
        int y=12;
        for(size_t i=0;i<form->fields.size();++i){
            auto& field=form->fields[i];const int id=100+static_cast<int>(i);
            if(field.type==1){
                auto control=widget(L"BUTTON",field.label.c_str(),WS_TABSTOP|BS_AUTOCHECKBOX,id,14,y,r.right-28,28);
                SendMessageW(control,BM_SETCHECK,field.checked?BST_CHECKED:BST_UNCHECKED,0);
            }else{
                widget(L"STATIC",field.label.c_str(),0,0,14,y,r.right-28,22);
                if(field.type==2){
                    auto control=widget(HOTKEY_CLASSW,L"",WS_TABSTOP,id,14,y+23,r.right-28,28);
                    SendMessageW(control,HKM_SETHOTKEY,std::stoul(field.value),0);
                }else{
                    auto control=widget(L"EDIT",field.value.c_str(),WS_TABSTOP|ES_AUTOHSCROLL,id,14,y+23,r.right-28,28);
                    SendMessageW(control,EM_SETLIMITTEXT,512,0);
                }
            }
            y+=field.type==1?34:62;
        }
        widget(L"STATIC",form->note.c_str(),0,0,14,y,r.right-28,44);
        widget(L"BUTTON",L(L"套用",L"Apply"),WS_TABSTOP|BS_DEFPUSHBUTTON,IDOK,r.right-192,r.bottom-40,84,28);
        widget(L"BUTTON",L(L"取消",L"Cancel"),WS_TABSTOP,IDCANCEL,r.right-98,r.bottom-40,84,28);
        return TRUE;
    }
    if(message==WM_COMMAND&&(LOWORD(w)==IDOK||LOWORD(w)==IDCANCEL)){
        auto* form=reinterpret_cast<Form*>(GetWindowLongPtrW(dialog,DWLP_USER));
        if(LOWORD(w)==IDOK)for(size_t i=0;i<form->fields.size();++i){
            auto& field=form->fields[i];HWND control=GetDlgItem(dialog,100+static_cast<int>(i));
            if(field.type==1)field.checked=SendMessageW(control,BM_GETCHECK,0,0)==BST_CHECKED;
            else if(field.type==2)field.value=std::to_wstring(SendMessageW(control,HKM_GETHOTKEY,0,0));
            else field.value=TextOf(control);
        }
        EndDialog(dialog,LOWORD(w));return TRUE;
    }
    if(message==WM_CLOSE){EndDialog(dialog,IDCANCEL);return TRUE;}
    return FALSE;
}
bool EditForm(Form& form) {
    struct alignas(DWORD) Template {DLGTEMPLATE header;WORD menu=0,klass=0,title=0;} dialog{};
    dialog.header.style=WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME|DS_CENTER;
    dialog.header.cx=290;
    int height=80;
    for(const auto& f:form.fields)height+=f.type==1?22:39;
    dialog.header.cy=static_cast<short>(height);
    return DialogBoxIndirectParamW(app.instance,&dialog.header,app.control,FormProc,reinterpret_cast<LPARAM>(&form))==IDOK;
}
std::optional<fs::path> ChoosePath(bool save,const wchar_t* title) {
    wchar_t path[32768]{};
    OPENFILENAMEW dialog{sizeof(dialog)};dialog.hwndOwner=app.control;dialog.lpstrFile=path;dialog.nMaxFile=32768;
    dialog.lpstrFilter=L"DeskTiles JSON\0*.json\0\0";dialog.lpstrDefExt=L"json";dialog.lpstrTitle=title;
    dialog.Flags=OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|(save?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST);
    if(save?GetSaveFileNameW(&dialog):GetOpenFileNameW(&dialog))return fs::absolute(path);
    return std::nullopt;
}
std::string ManageImage(const fs::path& path) {
    Asset image;image.bytes=ReadBytes(path,ImageLimit);image.mime=ImageMime(image.bytes);ValidateAsset(image);
    fs::create_directories(app.directory/L"assets");
    const auto suffix=image.mime=="image/png"?L".png":image.mime=="image/jpeg"?L".jpg":L".bmp";
    const auto target=app.directory/L"assets"/(Wide(Digest(image.bytes))+suffix);
    if(!fs::exists(target))AtomicOutput(target,[&](std::ostream& out){out.write(image.bytes.data(),image.bytes.size());});
    return target.u8string();
}
void AddContents(const std::vector<std::string>& images,const std::optional<std::string>& text) {
    auto next=CurrentCatalog();auto& p=next.current();
    if(p.tiles.size()+images.size()+(text?1:0)>500)throw std::runtime_error("Tile limit reached.");
    auto add=[&](Tile tile){
        if(p.nextTile>TileIdLimit)throw std::runtime_error("Tile ID limit reached.");
        tile.id=p.nextTile++;PlaceCliTile(p.tiles,tile);p.tiles.push_back(std::move(tile));
    };
    for(const auto& image:images){Tile tile;tile.kind=Kind::Image;tile.image=image;add(tile);}
    if(text){Tile tile;tile.text=*text;add(tile);}
    Commit(std::move(next));
}
void Drop(HDROP drop) {
    struct Release {HDROP value;~Release(){DragFinish(value);}} release{drop};
    const UINT count=DragQueryFileW(drop,0xFFFFFFFF,nullptr,0);
    if(count>100)throw std::runtime_error("Drop up to 100 pictures at once.");
    std::vector<std::string> paths;
    for(UINT i=0;i<count;++i){
        std::wstring name(DragQueryFileW(drop,i,nullptr,0)+1,L'\0');
        DragQueryFileW(drop,i,name.data(),static_cast<UINT>(name.size()));name.resize(name.size()-1);
        paths.push_back(ManageImage(fs::path(name)));
    }
    if(!paths.empty())AddContents(paths,std::nullopt);
}
void Paste() {
    if(!OpenClipboard(app.control))throw std::runtime_error("Clipboard is busy.");
    std::vector<std::string> images;std::optional<std::string> text;
    try {
        if(IsClipboardFormatAvailable(CF_HDROP)){
            auto drop=static_cast<HDROP>(GetClipboardData(CF_HDROP));
            const UINT count=DragQueryFileW(drop,0xFFFFFFFF,nullptr,0);
            if(count>100)throw std::runtime_error("Paste up to 100 pictures at once.");
            for(UINT i=0;i<count;++i){
                std::wstring name(DragQueryFileW(drop,i,nullptr,0)+1,L'\0');
                DragQueryFileW(drop,i,name.data(),static_cast<UINT>(name.size()));name.resize(name.size()-1);
                images.push_back(ManageImage(name));
            }
        }else if(IsClipboardFormatAvailable(CF_DIB)){
            HGLOBAL block=GetClipboardData(CF_DIB);
            const auto size=GlobalSize(block);
            auto* header=static_cast<BITMAPINFOHEADER*>(GlobalLock(block));
            if(!header)throw std::runtime_error("Could not read clipboard image.");
            try {
                if(size<sizeof(BITMAPINFOHEADER))throw std::runtime_error("Incomplete clipboard bitmap.");
                const auto height=std::abs(static_cast<long long>(header->biHeight));
                if(header->biSize<sizeof(BITMAPINFOHEADER)||
                    header->biWidth<=0||height==0||header->biWidth*height>64000000||
                    (header->biBitCount!=24&&header->biBitCount!=32)||header->biCompression!=BI_RGB)
                    throw std::runtime_error("Unsupported clipboard bitmap.");
                const size_t stride=((static_cast<size_t>(header->biWidth)*header->biBitCount+31)/32)*4;
                if(header->biSize>size||stride*height>size-header->biSize)throw std::runtime_error("Incomplete clipboard bitmap.");
                HDC screen=GetDC(nullptr);
                HBITMAP bitmap=CreateDIBitmap(screen,header,CBM_INIT,reinterpret_cast<BYTE*>(header)+header->biSize,
                    reinterpret_cast<BITMAPINFO*>(header),DIB_RGB_COLORS);ReleaseDC(nullptr,screen);
                if(!bitmap)throw std::runtime_error("Could not copy clipboard image.");
                fs::create_directories(app.directory/L"assets");
                const auto temporary=app.directory/L"assets"/L"clipboard-import.png";
                Gdiplus::Status saved=Gdiplus::GenericError;
                {Bitmap image(bitmap,nullptr);CLSID png{0x557cf406,0x1a04,0x11d3,{0x9a,0x73,0x00,0x00,0xf8,0x1e,0xf3,0x2e}};saved=image.Save(temporary.c_str(),&png);}
                DeleteObject(bitmap);
                if(saved!=Ok)throw std::runtime_error("Could not save clipboard image.");
                images.push_back(ManageImage(temporary));
                fs::remove(temporary);
            }catch(...){GlobalUnlock(block);throw;}
            GlobalUnlock(block);
        }else if(IsClipboardFormatAvailable(CF_UNICODETEXT)){
            HGLOBAL block=GetClipboardData(CF_UNICODETEXT);
            const auto* chars=static_cast<const wchar_t*>(GlobalLock(block));
            if(!chars)throw std::runtime_error("Could not read clipboard text.");
            const size_t available=GlobalSize(block)/sizeof(wchar_t);
            size_t n=0;while(n<available&&n<=65536&&chars[n])++n;
            if(n==available||n>65536){GlobalUnlock(block);throw std::runtime_error("Clipboard text is too long.");}
            text=Utf8(std::wstring(chars,n));GlobalUnlock(block);
            if(text->size()>65536)throw std::runtime_error("Clipboard text is too long.");
        }
    }catch(...){CloseClipboard();throw;}
    CloseClipboard();
    if(!images.empty()||text)AddContents(images,text);
}
LRESULT CALLBACK InlineProc(HWND edit,UINT message,WPARAM w,LPARAM l);
void FinishInline(bool commit) {
    if(!state.editor)return;
    HWND editor=state.editor;const int id=state.editing,layout=state.editingLayout;
    const auto text=commit?Utf8(TextOf(editor)):std::string{};
    if(commit&&text.size()>65536){Error(L(L"文字最多 65536 bytes。",L"Text exceeds 65536 bytes."));return;}
    state.editor=nullptr;state.editing=0;
    if(commit&&layout==app.catalog.active)if(auto* tile=Find(id)){
        const auto previous=tile->text;tile->text=text;SaveState();
        if(app.savingFailed){tile->text=previous;state.editor=editor;state.editing=id;return;}
    }
    DestroyWindow(editor);Repaint();RefreshControls();
}
LRESULT CALLBACK InlineProc(HWND edit,UINT message,WPARAM w,LPARAM l) {
    if(message==WM_KEYDOWN){
        if(w==VK_ESCAPE){FinishInline(false);return 0;}
        if(w==VK_RETURN&&(GetKeyState(VK_CONTROL)&0x8000)){FinishInline(true);return 0;}
    }
    if(message==WM_KILLFOCUS){PostMessageW(app.control,WM_APP+62,0,0);}
    return CallWindowProcW(state.editProc,edit,message,w,l);
}
void BeginInline(int id) {
    auto* tile=Find(id);
    if(!tile||tile->kind!=Kind::Text){ShowControl();return;}
    FinishInline(true);if(state.editor)return;EndDrag(false);
    const double inset=tile->shape==Shape::Triangle?.25:.14;
    const int width=static_cast<int>(tile->size*(1-2*inset)),height=static_cast<int>(TileHeight(*tile)*.48);
    state.editing=id;state.editingLayout=app.catalog.active;
    state.editor=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_TOPMOST|WS_EX_CLIENTEDGE,L"EDIT",Wide(tile->text).c_str(),
        WS_POPUP|WS_VISIBLE|WS_VSCROLL|ES_MULTILINE|ES_AUTOVSCROLL|ES_WANTRETURN,
        static_cast<int>(tile->position.x+tile->size*inset),static_cast<int>(tile->position.y+TileHeight(*tile)*.28),
        std::max(100,width),std::max(65,height),app.control,nullptr,app.instance,nullptr);
    if(!state.editor)throw std::runtime_error("Could not open the note editor.");
    SendMessageW(state.editor,WM_SETFONT,reinterpret_cast<WPARAM>(app.font),TRUE);
    SendMessageW(state.editor,EM_SETLIMITTEXT,65536,0);
    state.editProc=reinterpret_cast<WNDPROC>(SetWindowLongPtrW(state.editor,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(InlineProc)));
    SetForegroundWindow(state.editor);SetFocus(state.editor);SendMessageW(state.editor,EM_SETSEL,0,-1);
}

void SaveMonitorSnapshot();
void RegisterKeys() {
    for(int i=0;i<5;++i)UnregisterHotKey(app.control,4500+i);
    std::set<unsigned> used;
    const auto& keys=state.preferences.at("hotkeys");
    for(int i=0;i<5;++i){
        const unsigned key=keys.at(i).get<unsigned>();
        if(!key)continue;
        const unsigned flags=(key>>8)&255;
        const UINT mods=MOD_NOREPEAT|((flags&HOTKEYF_CONTROL)?MOD_CONTROL:0)|((flags&HOTKEYF_ALT)?MOD_ALT:0)|((flags&HOTKEYF_SHIFT)?MOD_SHIFT:0);
        if(!used.insert(key).second||!RegisterHotKey(app.control,4500+i,mods,key&255)){
            for(int j=0;j<5;++j)UnregisterHotKey(app.control,4500+j);
            throw std::runtime_error("A shortcut is already in use. Choose a different key combination.");
        }
    }
}
void ShortcutOptions() {
    Form form{L(L"全域快捷鍵",L"Global shortcuts"),{},L(L"按清除鍵可停用。Ctrl+Alt+U 可恢復滑鼠操作。",L"Clear a field to disable it. Restore input remains available from the tray.")};
    const wchar_t* names[]={L(L"新增筆記",L"New note"),L(L"新增五分鐘倒數",L"New five-minute timer"),
        L(L"切換下一個配置",L"Next layout"),L(L"隱藏／顯示全部",L"Hide / show tiles"),L(L"解除穿透並恢復操作",L"Restore mouse input")};
    for(int i=0;i<5;++i)form.fields.push_back({names[i],std::to_wstring(state.preferences["hotkeys"][i].get<unsigned>()),2});
    if(!EditForm(form))return;
    const auto previous=state.preferences;
    try{
        for(int i=0;i<5;++i){
            const unsigned key=static_cast<unsigned>(std::stoul(form.fields[i].value));
            if(key&&((key>>8)&(HOTKEYF_CONTROL|HOTKEYF_ALT))==0)
                throw std::runtime_error("Use Ctrl or Alt in each enabled shortcut.");
            state.preferences["hotkeys"][i]=key;
        }
        RegisterKeys();SaveOptions();
    }catch(...){state.preferences=previous;try{RegisterKeys();}catch(...){}throw;}
}
void Settings() {
    Form form{L(L"桌面偏好",L"Desktop preferences"),{
        {L(L"全螢幕時自動隱藏磁磚並暫停燈效",L"Hide tiles and pause effects during fullscreen"),L"",1,state.preferences.value("fullscreen",true)},
        {L(L"專注模式時靜音到期通知",L"Silence reminders in focus mode"),L"",1,state.preferences.value("mute",false)},
        {L(L"手動啟用專注模式",L"Enable focus mode manually"),L"",1,state.preferences.value("manualFocus",false)},
        {L(L"離線同步資料夾（完整路徑，可留空）",L"Offline sync folder (absolute path or blank)"),Wide(state.preferences.value("syncFolder",std::string{}))}
    },L(L"倒數與鬧鐘持續運作。快捷鍵可在右鍵選單設定。",L"Timers and alarms keep running. Configure shortcuts from the tray menu.")};
    if(!EditForm(form))return;
    const auto previous=state.preferences;
    state.preferences["fullscreen"]=form.fields[0].checked;
    state.preferences["mute"]=form.fields[1].checked;
    state.preferences["manualFocus"]=form.fields[2].checked;
    if(!form.fields[3].value.empty()&&!fs::path(form.fields[3].value).is_absolute()){
        state.preferences=previous;throw std::runtime_error("Use an absolute sync folder path.");
    }
    if(Utf8(form.fields[3].value)!=state.preferences.value("syncFolder",std::string{})){
        state.preferences["syncFolder"]=Utf8(form.fields[3].value);state.preferences["syncBase"]="";
    }
    try{SaveOptions();}catch(...){state.preferences=previous;throw;}
}
bool FullscreenForeground() {
    HWND foreground=GetForegroundWindow();if(!foreground||IsIconic(foreground))return false;
    DWORD process=0;GetWindowThreadProcessId(foreground,&process);
    if(process==GetCurrentProcessId()||foreground==GetDesktopWindow()||foreground==GetShellWindow())return false;
    wchar_t klass[128]{};GetClassNameW(foreground,klass,128);
    if(wcscmp(klass,L"Progman")==0||wcscmp(klass,L"WorkerW")==0||wcscmp(klass,L"Shell_TrayWnd")==0)return false;
    RECT rect{};if(!GetWindowRect(foreground,&rect))return false;
    MONITORINFO monitor{sizeof(monitor)};
    if(!GetMonitorInfoW(MonitorFromWindow(foreground,MONITOR_DEFAULTTONEAREST),&monitor))return false;
    return FullscreenRect({double(rect.left),double(rect.top)},{double(rect.right-rect.left),double(rect.bottom-rect.top)},
        {double(monitor.rcMonitor.left),double(monitor.rcMonitor.top)},
        {double(monitor.rcMonitor.right-monitor.rcMonitor.left),double(monitor.rcMonitor.bottom-monitor.rcMonitor.top)});
}
void FocusTick() {
    const bool focus=state.preferences.value("manualFocus",false)||
        (state.preferences.value("fullscreen",true)&&FullscreenForeground());
    if(focus!=state.focused){FinishInline(true);if(Editing())return;state.focused=focus;RefreshDesktop();}
}
std::string ReadMonitors(std::vector<RECT>& areas) {
    struct MonitorSet {std::vector<std::pair<std::wstring,RECT>> monitors;} set;
    EnumDisplayMonitors(nullptr,nullptr,[](HMONITOR monitor,HDC,LPRECT,LPARAM l)->BOOL {
        auto& output=*reinterpret_cast<MonitorSet*>(l);MONITORINFOEXW info{};info.cbSize=sizeof(info);
        if(GetMonitorInfoW(monitor,&info))output.monitors.push_back({info.szDevice,info.rcWork});return TRUE;
    },reinterpret_cast<LPARAM>(&set));
    std::sort(set.monitors.begin(),set.monitors.end(),[](const auto& a,const auto& b){return a.first<b.first;});
    std::string key;
    for(const auto& monitor:set.monitors) {
        areas.push_back(monitor.second);const auto& r=monitor.second;
        key+=Utf8(monitor.first)+":"+std::to_string(r.left)+","+std::to_string(r.top)+","+
            std::to_string(r.right)+","+std::to_string(r.bottom)+";";
    }
    return key;
}
void SaveMonitorSnapshot() {
    if(state.monitorKey.empty())return;
    Json saved=Json::array();
    const auto catalog=CurrentCatalog();
    for(const auto& p:catalog.profiles)for(const auto& t:p.second.tiles)
        saved.push_back({{"layout",p.first},{"id",t.id},{"x",t.position.x},{"y",t.position.y}});
    auto& snapshots=state.preferences["monitors"];
    snapshots[state.monitorKey]=saved;
    while(snapshots.size()>8)snapshots.erase(snapshots.begin());
}
void MonitorChanged() {
    std::vector<RECT> areas;const auto key=ReadMonitors(areas);
    if(key.empty()||key==state.monitorKey)return;
    FinishInline(true);if(Editing())return;EndDrag(false);
    auto next=CurrentCatalog();
    SaveMonitorSnapshot();
    const auto previousKey=state.monitorKey;const auto previousAreas=state.areas;
    state.monitorKey=key;state.areas=areas;
    const auto& stored=state.preferences["monitors"];
    if(stored.contains(key))for(auto& p:next.profiles)for(auto& tile:p.second.tiles)
        for(const auto& record:stored.at(key))if(record.at("layout")==p.first&&record.at("id")==tile.id)
            tile.position={JsonNumber(record.at("x")),JsonNumber(record.at("y"))};
    const auto visible=[&](const Tile& tile){
        return std::any_of(areas.begin(),areas.end(),[&](const RECT& r){
            return tile.position.x>=r.left&&tile.position.y>=r.top&&
                tile.position.x+tile.size<=r.right&&tile.position.y+TileHeight(tile)<=r.bottom;
        });
    };
    for(auto& p:next.profiles) {
        std::vector<Tile> placed;
        for(const auto& tile:p.second.tiles)if(visible(tile))placed.push_back(tile);
        for(auto& tile:p.second.tiles)if(!visible(tile)){
            Tile moved=tile;
            const RECT& r=areas.front();
            moved.position.x=std::clamp(moved.position.x,double(r.left),std::max(double(r.left),r.right-moved.size));
            moved.position.y=std::clamp(moved.position.y,double(r.top),std::max(double(r.top),r.bottom-TileHeight(moved)));
            if(std::any_of(placed.begin(),placed.end(),[&](const Tile& other){return TilesOverlap(moved,other);}))
                PlaceCliTile(placed,moved);
            tile.position=moved.position;placed.push_back(tile);
        }
    }
    try{Commit(std::move(next));}catch(...){state.monitorKey=previousKey;state.areas=previousAreas;throw;}
    SaveMonitorSnapshot();SaveOptions();
}

struct Library {int selected=0;bool colorsOnly=false;};
INT_PTR CALLBACK LibraryProc(HWND dialog,UINT message,WPARAM w,LPARAM l) {
    if(message==WM_INITDIALOG){
        SetWindowLongPtrW(dialog,DWLP_USER,l);SetWindowTextW(dialog,L(L"配置樣板庫",L"Layout presets"));
        auto create=[&](const wchar_t* type,const wchar_t* text,DWORD style,int id,int x,int y,int width,int height){
            HWND h=CreateWindowExW(0,type,text,WS_CHILD|WS_VISIBLE|style,x,y,width,height,dialog,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),app.instance,nullptr);
            SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(app.font),TRUE);return h;
        };
        RECT r{};GetClientRect(dialog,&r);
        auto list=create(L"LISTBOX",L"",WS_TABSTOP|LBS_NOTIFY|WS_BORDER,100,12,12,145,r.bottom-60);
        for(auto name:{L(L"黑白磁磚",L"Monochrome"),L(L"鼠尾草綠",L"Sage"),L(L"讀書專注",L"Study"),L(L"工作筆記",L"Work")})
            SendMessageW(list,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(name));
        SendMessageW(list,LB_SETCURSEL,0,0);
        create(L"BUTTON",L(L"只套用配色到選取磁磚",L"Apply colors to selected tiles"),BS_AUTOCHECKBOX|WS_TABSTOP,101,170,r.bottom-95,r.right-184,26);
        create(L"BUTTON",L(L"套用",L"Apply"),WS_TABSTOP|BS_DEFPUSHBUTTON,IDOK,r.right-192,r.bottom-40,84,28);
        create(L"BUTTON",L(L"取消",L"Cancel"),WS_TABSTOP,IDCANCEL,r.right-98,r.bottom-40,84,28);
        return TRUE;
    }
    auto* library=reinterpret_cast<Library*>(GetWindowLongPtrW(dialog,DWLP_USER));
    if(!library)return FALSE;
    if(message==WM_PAINT){
        PAINTSTRUCT ps{};HDC dc=BeginPaint(dialog,&ps);
        RECT r{};GetClientRect(dialog,&r);
        auto tiles=Preset(library->selected);
        Graphics g(dc);g.SetSmoothingMode(SmoothingModeAntiAlias);
        for(size_t i=0;i<tiles.size();++i){
            auto& tile=tiles[i];tile.size=78;tile.rectangleHeight=tile.shape==Shape::Rectangle?50:0;
            const auto polygon=BorderOutline(tile.borderStyle,0,tile.size,tile.shape,tile.rectangleHeight);
            std::vector<PointF> points;
            const float x=180+static_cast<float>(i)*92,y=65;
            for(auto p:polygon)points.emplace_back(x+static_cast<float>(p.x),y+static_cast<float>(p.y));
            SolidBrush fill(BorderColor(tile.backgroundColor));Pen edge(BorderColor(tile.borderColor),3);
            g.FillPolygon(&fill,points.data(),static_cast<int>(points.size()));g.DrawPolygon(&edge,points.data(),static_cast<int>(points.size()));
        }
        EndPaint(dialog,&ps);return TRUE;
    }
    if(message==WM_COMMAND){
        if(LOWORD(w)==100&&HIWORD(w)==LBN_SELCHANGE){
            library->selected=static_cast<int>(SendDlgItemMessageW(dialog,100,LB_GETCURSEL,0,0));InvalidateRect(dialog,nullptr,TRUE);
        }
        if(LOWORD(w)==IDOK||LOWORD(w)==IDCANCEL){
            library->colorsOnly=SendDlgItemMessageW(dialog,101,BM_GETCHECK,0,0)==BST_CHECKED;
            EndDialog(dialog,LOWORD(w));return TRUE;
        }
    }
    if(message==WM_CLOSE){EndDialog(dialog,IDCANCEL);return TRUE;}
    return FALSE;
}
void ApplyPreset(int index,bool colorsOnly) {
    FinishInline(true);auto next=CurrentCatalog();auto colors=Preset(index);
    if(colorsOnly){
        if(app.selection.empty())throw std::runtime_error("Select tiles before applying colors.");
        for(auto& t:next.current().tiles)if(app.selection.count(t.id)){
            const auto& style=colors.front();t.borderStyle=style.borderStyle;t.borderColor=style.borderColor;
            t.borderWidth=style.borderWidth;t.backgroundColor=style.backgroundColor;t.textColor=style.textColor;
        }
    }else if(index==1){
        std::string name="Sage";int suffix=2;
        while(std::any_of(next.profiles.begin(),next.profiles.end(),[&](const auto& p){return p.second.name==name;}))name="Sage "+std::to_string(suffix++);
        next.active=CreateSageLayout(next,name);
    }else NewProfile(next,index==2?"Study":index==3?"Work":"Monochrome",std::move(colors));
    Commit(std::move(next));
}
void ShowTemplates() {
    Library library;
    struct alignas(DWORD) Template {DLGTEMPLATE header;WORD menu=0,klass=0,title=0;} dialog{};
    dialog.header.style=WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME|DS_CENTER;dialog.header.cx=320;dialog.header.cy=180;
    if(DialogBoxIndirectParamW(app.instance,&dialog.header,app.control,LibraryProc,reinterpret_cast<LPARAM>(&library))==IDOK)
        ApplyPreset(library.selected,library.colorsOnly);
}
INT_PTR CALLBACK TrashProc(HWND dialog,UINT message,WPARAM w,LPARAM l) {
    if(message==WM_INITDIALOG){
        SetWindowLongPtrW(dialog,DWLP_USER,l);SetWindowTextW(dialog,L(L"回收桶",L"Recycle bin"));
        RECT r{};GetClientRect(dialog,&r);
        auto list=CreateWindowExW(WS_EX_CLIENTEDGE,L"LISTBOX",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_VSCROLL|LBS_NOTIFY,
            12,12,r.right-24,r.bottom-66,dialog,reinterpret_cast<HMENU>(100),app.instance,nullptr);
        SendMessageW(list,WM_SETFONT,reinterpret_cast<WPARAM>(app.font),TRUE);
        for(const auto& removed:state.trash){
            auto text=Wide(removed.name)+L" · #"+std::to_wstring(removed.tile.id)+L" · "+Wide(removed.tile.text.substr(0,80));
            std::replace(text.begin(),text.end(),L'\n',L' ');std::replace(text.begin(),text.end(),L'\r',L' ');
            SendMessageW(list,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(text.c_str()));
        }
        SendMessageW(list,LB_SETCURSEL,*reinterpret_cast<int*>(l),0);
        for(const auto& item:{std::pair<int,const wchar_t*>{IDOK,L(L"還原",L"Restore")},{IDCANCEL,L(L"取消",L"Cancel")}}){
            auto button=CreateWindowExW(0,L"BUTTON",item.second,WS_CHILD|WS_VISIBLE|WS_TABSTOP,
                r.right-(item.first==IDOK?192:98),r.bottom-40,84,28,dialog,reinterpret_cast<HMENU>(static_cast<INT_PTR>(item.first)),app.instance,nullptr);
            SendMessageW(button,WM_SETFONT,reinterpret_cast<WPARAM>(app.font),TRUE);
        }
        return TRUE;
    }
    if(message==WM_COMMAND){
        const bool restore=LOWORD(w)==IDOK||(LOWORD(w)==100&&HIWORD(w)==LBN_DBLCLK);
        if(restore||LOWORD(w)==IDCANCEL){
            if(restore)*reinterpret_cast<int*>(GetWindowLongPtrW(dialog,DWLP_USER))=
                static_cast<int>(SendDlgItemMessageW(dialog,100,LB_GETCURSEL,0,0));
            EndDialog(dialog,restore?IDOK:IDCANCEL);return TRUE;
        }
    }
    if(message==WM_CLOSE){EndDialog(dialog,IDCANCEL);return TRUE;}
    return FALSE;
}

void ShowTrash() {
    if(state.trash.empty()){Status(L(L"回收桶目前是空的。",L"The recycle bin is empty."));return;}
    int selected=static_cast<int>(state.trash.size())-1;
    struct alignas(DWORD) Template {DLGTEMPLATE header;WORD menu=0,klass=0,title=0;} dialog{};
    dialog.header.style=WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME|DS_CENTER;
    dialog.header.cx=320;dialog.header.cy=200;
    if(DialogBoxIndirectParamW(app.instance,&dialog.header,app.control,TrashProc,reinterpret_cast<LPARAM>(&selected))!=IDOK)return;
    if(selected<0||static_cast<size_t>(selected)>=state.trash.size())return;
    auto next=CurrentCatalog();auto& p=next.current();auto tile=state.trash[selected].tile;
    if(p.tiles.size()>=500||p.nextTile>TileIdLimit)throw std::runtime_error("Tile limit reached.");
    tile.id=p.nextTile++;tile.running=false;tile.alarm.enabled=false;tile.alarm.nextDue=0;
    PlaceCliTile(p.tiles,tile);p.tiles.push_back(tile);Commit(std::move(next));
    state.trash.erase(state.trash.begin()+selected);PersistTrash();
}

desktile_steam::Service steam;
std::vector<Tile> Materialize(Bundle bundle) {
    for(const auto& asset:bundle.images){
        ValidateAsset(asset.second);
        const auto suffix=asset.second.mime=="image/png"?L".png":asset.second.mime=="image/jpeg"?L".jpg":L".bmp";
        fs::create_directories(app.directory/L"assets");
        const auto path=app.directory/L"assets"/(Wide(Digest(asset.second.bytes))+suffix);
        if(!fs::exists(path))AtomicOutput(path,[&](std::ostream& out){out.write(asset.second.bytes.data(),asset.second.bytes.size());});
        for(auto& tile:bundle.tiles)if(bundle.tileImages.count(tile.id)&&bundle.tileImages.at(tile.id)==asset.first)tile.image=path.u8string();
    }
    PauseSchedules(bundle.tiles);return bundle.tiles;
}
void ImportPackage(const fs::path& path) {
    const auto bytes=ReadBytes(path,JsonLimit);std::stringstream in(bytes);
    auto bundle=ParseBundle(in);
    auto tiles=Materialize(std::move(bundle));
    auto next=CurrentCatalog();NewProfile(next,path.stem().u8string().substr(0,100),std::move(tiles));
    Commit(std::move(next));
}
void SharePreview(const Bundle& bundle,const fs::path& path) {
    auto tiles=Materialize(bundle);
    Bitmap canvas(640,360,PixelFormat32bppARGB);Graphics graphics(&canvas);graphics.Clear(Color(255,32,34,33));
    size_t index=0;
    for(auto tile:tiles){
        if(index==3)break;
        tile.id=-100-static_cast<int>(index);tile.size=160;
        if(tile.shape==Shape::Rectangle&&tile.rectangleHeight>0)tile.rectangleHeight=80;
        LoadImage(tile);
        auto& frame=RenderTile(tile);
        Bitmap bitmap(frame.width,frame.height,frame.width*4,PixelFormat32bppPARGB,static_cast<BYTE*>(frame.bits));
        graphics.DrawImage(&bitmap,35+static_cast<int>(index)*200,95);
        app.views.erase(tile.id);++index;
    }
    CLSID png{0x557cf406,0x1a04,0x11d3,{0x9a,0x73,0x00,0x00,0xf8,0x1e,0xf3,0x2e}};
    if(canvas.Save(path.c_str(),&png)!=Ok)throw std::runtime_error("Could not write the share preview.");
}
fs::path CreateShare(const fs::path& path,bool text,bool photos) {
    auto bundle=ShareBundle(app.tiles,text,photos,[](const std::string& source){
        Asset image;image.bytes=ReadBytes(fs::u8path(source),ImageLimit);image.mime=ImageMime(image.bytes);return image;
    });
    auto json=BundleJson(bundle);ReadBundle(json);
    AtomicOutput(path,[&](std::ostream& out){out<<json.dump(2);});
    return path;
}
void ShareLayout(bool publish) {
    Form form{L(L"分享配置預覽",L"Share layout preview"),{
        {L(L"分享標題",L"Share title"),L"My DeskTiles layout"},
        {L(L"包含我的文字內容",L"Include my text"),L"",1,false},
        {L(L"包含我的圖片",L"Include my pictures"),L"",1,false}
    },L(L"預設以範例文字取代私人筆記，圖片留空；所有排程先停用。",L"By default, notes use sample text and pictures are empty. Schedules are disabled.")};
    if(!EditForm(form))return;
    CheckProfileName(Utf8(form.fields[0].value));
    const bool includeText=form.fields[1].checked,includePhotos=form.fields[2].checked;
    std::wstring details;
    for(const auto& tile:app.tiles)details+=L"#"+std::to_wstring(tile.id)+L" "+
        (includeText?Wide(tile.text.substr(0,100)):L(L"範例文字",L"Sample text"))+
        (includePhotos&&!tile.image.empty()?L" ["+fs::u8path(tile.image).filename().wstring()+L"]":L"")+L"\r\n";
    if(!app.smoke&&MessageBoxW(app.control,details.c_str(),L(L"確認分享內容",L"Confirm shared content"),MB_OKCANCEL)!=IDOK)return;
    if(!publish){
        const auto path=ChoosePath(true,L(L"匯出分享包",L"Export share package"));
        if(path)CreateShare(*path,includeText,includePhotos);
        return;
    }
    if(!steam.Available())throw std::runtime_error(steam.status);
    const auto directory=app.directory/L"workshop-staging"/std::to_wstring(GetTickCount64());
    fs::create_directories(directory);
    const auto path=CreateShare(directory/L"layout.json",includeText,includePhotos);
    std::ifstream in(path,std::ios::binary);const auto bundle=ParseBundle(in);
    SharePreview(bundle,directory/L"preview.png");
    steam.Publish(directory,directory/L"preview.png",Utf8(form.fields[0].value));
}
Json CloudPackage() {
    auto catalog=CurrentCatalog();
    Json profiles=Json::array();
    for(auto& entry:catalog.profiles) {
        auto tiles=entry.second.tiles;PauseSchedules(tiles);
        for(auto& t:tiles){t.elapsed=0;t.notified=false;t.alarm.lastFired=0;}
        const auto bundle=Pack(tiles,[](const std::string& path){
            Asset a;a.bytes=ReadBytes(fs::u8path(path),ImageLimit);a.mime=ImageMime(a.bytes);return a;
        });
        const auto body=BundleJson(bundle);ReadBundle(body);
        profiles.push_back({{"name",entry.second.name},{"bundle",body}});
    }
    Json result={{"format","DeskTilesSync"},{"version",1},{"active",catalog.current().name},{"profiles",profiles}};
    if(result.dump().size()>64*1024*1024)throw std::runtime_error("Sync packages are limited to 64 MiB.");
    return result;
}
LayoutCatalog ReadCloudPackage(const std::string& bytes) {
    if(bytes.size()>64*1024*1024)throw std::runtime_error("Sync package is too large.");
    const auto json=Json::parse(bytes,[](int depth,Json::parse_event_t,Json&){
        if(depth>20)throw std::runtime_error("Sync package nesting is too deep.");
        return true;
    });
    if(json.at("format")!="DeskTilesSync"||json.at("version")!=1||!json.at("profiles").is_array()||
        json.at("profiles").empty()||json.at("profiles").size()>LayoutLimit)throw std::runtime_error("Invalid sync package.");
    std::vector<std::pair<std::string,Bundle>> bundles;
    std::set<std::string> names;
    for(const auto& p:json.at("profiles")){
        auto name=JsonString(p.at("name"),128);CheckProfileName(name);
        if(!names.insert(name).second)throw std::runtime_error("Duplicate sync layout name.");
        bundles.push_back({name,ReadBundle(p.at("bundle"))});
    }
    const auto active=JsonString(json.at("active"),128);
    if(!names.count(active))throw std::runtime_error("Invalid active layout.");
    LayoutCatalog next;next.profiles.clear();next.nextLayout=1;
    for(auto& bundle:bundles){
        Profile p;p.id=next.nextLayout++;p.name=bundle.first;p.tiles=Materialize(std::move(bundle.second));NormalizeProfile(p);
        if(p.name==active)next.active=p.id;
        next.profiles.emplace(p.id,std::move(p));
    }
    return next;
}
void Synchronize(bool interactive=true) {
    const bool online=steam.Available();
    const auto folder=state.preferences.value("syncFolder",std::string{});
    if(!online&&folder.empty()){
        if(!interactive)return;
        Form form{L(L"離線同步資料夾",L"Offline sync folder"),{{L(L"選擇兩次測試共用的完整資料夾路徑",L"Absolute folder path shared by test instances"),L""}},
            L(L"尚未連線 Steam。此資料夾用來驗證上傳、下載與衝突。",L"Steam is not connected. This folder tests upload, download and conflict handling.")};
        if(!EditForm(form)||form.fields[0].value.empty())return;
        const fs::path path=form.fields[0].value;
        if(!path.is_absolute())throw std::runtime_error("Use an absolute sync folder path.");
        state.preferences["syncFolder"]=path.u8string();SaveOptions();
        Synchronize(interactive);return;
    }
    HANDLE folderLock=INVALID_HANDLE_VALUE;
    struct Unlock {HANDLE& handle;~Unlock(){if(handle!=INVALID_HANDLE_VALUE)CloseHandle(handle);}} guard{folderLock};
    fs::path remotePath;
    if(!online){
        remotePath=fs::u8path(folder)/L"desktiles-sync.json";
        fs::create_directories(remotePath.parent_path());
        folderLock=CreateFileW((remotePath.parent_path()/L"sync.lock").c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(folderLock==INVALID_HANDLE_VALUE)throw std::runtime_error("The sync folder is busy. Try again.");
    }
    const auto read=[&](){return online?steam.ReadCloud():fs::exists(remotePath)?ReadBytes(remotePath,64*1024*1024):std::string{};};
    const auto remote=read();
    if(!remote.empty())ReadCloudPackage(remote); // Fully validate before any overwrite.
    const auto local=CloudPackage().dump();
    const auto localHash=Digest(local),remoteHash=remote.empty()?std::string{}:Digest(Json::parse(remote).dump());
    const auto base=state.preferences.value(online?"steamBase":"syncBase",std::string{});
    auto decision=DecideSync(localHash,remoteHash,base);
    if(decision==SyncDecision::Conflict){
        if(!interactive){Status(L(L"同步有衝突，請從選單決定保留版本。",L"Sync conflict. Open Sync now to choose a version."));return;}
        const int choice=MessageBoxW(app.control,L(L"本機和遠端都有變更。是：上傳本機；否：下載遠端。兩者都會先備份。",L"Both versions changed. Yes: upload local. No: download remote. Both are backed up first."),
            L(L"同步衝突",L"Sync conflict"),MB_YESNOCANCEL|MB_ICONQUESTION);
        if(choice==IDCANCEL)return;
        decision=choice==IDYES?SyncDecision::Upload:SyncDecision::Download;
    }
    if(decision!=SyncDecision::NoChange){
        const auto backup=app.directory/L"sync-backups"/std::to_wstring(GetTickCount64());
        fs::create_directories(backup);
        AtomicOutput(backup/L"local.json",[&](std::ostream& out){out<<local;});
        if(!remote.empty())AtomicOutput(backup/L"remote.json",[&](std::ostream& out){out<<remote;});
    }
    Log(L"Sync input: "+app.statePath.wstring()+L"; output: "+(online?L"Steam Cloud":remotePath.wstring()));
    Status(L(L"[同步 1/3] 核對資料與變更",L"[Sync 1/3] Validate data and revisions"));
    if(decision==SyncDecision::Download){
        auto next=ReadCloudPackage(remote);Commit(std::move(next));
        state.preferences["locked"]=Json::array();state.preferences["through"]=Json::array();RefreshDesktop();
    }else if(decision==SyncDecision::Upload){
        if(read()!=remote)throw std::runtime_error("Remote data changed. Sync again.");
        Status(L(L"[同步 2/3] 上傳配置",L"[Sync 2/3] Upload layouts"));
        if(online)steam.WriteCloud(local);
        else AtomicOutput(remotePath,[&](std::ostream& out){out<<local;});
    }
    state.preferences[online?"steamBase":"syncBase"]=Digest(CloudPackage().dump());
    SaveOptions();Status(L(L"[同步 3/3] 完成",L"[Sync 3/3] Complete"));
}
void SteamOptions() {
    const auto installed=steam.InstalledItems();
    Form form{L(L"Steam 與工作坊",L"Steam and Workshop"),{
        {L(L"從已訂閱項目匯入（輸入編號）",L"Import an installed subscription (number)"),L""},
        {L(L"啟動／結束與每五分鐘自動同步",L"Sync at startup, exit and every five minutes"),L"",1,state.preferences.value("steamAuto",false)}
    },Wide(steam.status)};
    std::wstring items;
    for(size_t i=0;i<installed.size();++i)items+=std::to_wstring(i+1)+L" "+installed[i].parent_path().filename().wstring()+L"\r\n";
    if(!items.empty())MessageBoxW(app.control,items.c_str(),L(L"已訂閱樣板",L"Subscribed presets"),MB_OK);
    if(!EditForm(form))return;
    state.preferences["steamAuto"]=form.fields[1].checked;SaveOptions();
    if(!form.fields[0].value.empty()){
        size_t parsed=0;const auto item=std::stoul(form.fields[0].value,&parsed);
        if(parsed!=form.fields[0].value.size()||item<1||item>installed.size())throw std::runtime_error("Choose an installed subscription number.");
        ImportPackage(installed[item-1]);
    }
}
void NewTimer() {
    auto next=CurrentCatalog();auto& p=next.current();Tile tile;tile.id=p.nextTile++;
    if(p.tiles.size()>=500||tile.id>TileIdLimit)throw std::runtime_error("Tile limit reached.");
    tile.kind=Kind::Timer;tile.duration=300;tile.text.clear();tile.shape=Shape::Rectangle;
    tile.fontSize=24;PlaceCliTile(p.tiles,tile);p.tiles.push_back(tile);Commit(std::move(next));
}
void Hotkey(int index) {
    if(index<3){FinishInline(true);if(Editing())return;}
    switch(index){
    case 0:AddContents({},Utf8(L(L"新筆記",L"New note")));if(!app.tiles.empty())BeginInline(app.tiles.back().id);break;
    case 1:NewTimer();break;
    case 2:{FinishInline(true);PrepareLayoutEdit();auto next=CurrentCatalog();auto at=next.profiles.upper_bound(next.active);
        if(at==next.profiles.end())at=next.profiles.begin();next.active=at->first;Commit(std::move(next));break;}
    case 3:state.preferences["hidden"]=!state.preferences.value("hidden",false);SaveOptions();RefreshDesktop();break;
    case 4:state.preferences["through"]=Json::array();state.preferences["locked"]=Json::array();
        state.preferences["hidden"]=false;state.preferences["manualFocus"]=false;SaveOptions();RefreshDesktop();ShowControl();break;
    }
}
void AddMenu(HMENU menu) {
    HMENU tools=CreatePopupMenu();
    auto item=[&](UINT id,const wchar_t* text,bool enabled=true){AppendMenuW(tools,MF_STRING|(enabled?0:MF_GRAYED),id,text);};
    item(Undo,L(L"復原 Ctrl+Z",L"Undo Ctrl+Z"),!state.history.undo.empty());
    item(Redo,L(L"重做 Ctrl+Y",L"Redo Ctrl+Y"),!state.history.redo.empty());
    item(Trash,L(L"回收桶…",L"Recycle bin…"));
    item(4114,L(L"從剪貼簿新增 Ctrl+V",L"Paste as new tile Ctrl+V"));
    item(Lock,L(L"切換選取磁磚的位置鎖定",L"Toggle position lock for selection"),!app.selection.empty());
    item(Through,L(L"切換選取磁磚的滑鼠穿透",L"Toggle click-through for selection"),!app.selection.empty());
    item(RecoverInput,L(L"解除穿透並恢復操作",L"Restore mouse input"));
    item(HideAll,L(L"隱藏／顯示全部磁磚",L"Hide / show all tiles"));
    item(Options,L(L"桌面偏好…",L"Desktop preferences…"));
    item(4115,L(L"全域快捷鍵…",L"Global shortcuts…"));
    item(Templates,L(L"配置樣板庫…",L"Layout presets…"));
    item(Share,L(L"匯出分享包（先預覽）…",L"Export share package (preview)…"));
    item(ImportShare,L(L"匯入分享包至新配置…",L"Import package as new layout…"));
    item(Sync,L(L"立即同步…",L"Sync now…"));
    item(Steam,L(L"Steam 連線與訂閱樣板…",L"Steam connection and subscriptions…"));
    item(4116,L(L"發布至工作坊（先預覽）…",L"Publish to Workshop (preview)…"),steam.Available());
    item(4117,L(L"瀏覽 Steam 工作坊",L"Browse Steam Workshop"),steam.Available());
    AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(tools),L(L"桌面工具",L"Desktop tools"));
}
bool HandleCommand(int id) {
    if(id<Undo||id>4117)return false;
    try{
        switch(id){
        case Undo:Travel(true);break;case Redo:Travel(false);break;case Trash:ShowTrash();break;
        case Lock:case Through:{
            const char* key=id==Lock?"locked":"through";
            const bool enable=!std::all_of(app.selection.begin(),app.selection.end(),[&](int tile){return Flag(key,tile);});
            const auto previous=state.preferences;
            for(int tile:app.selection)SetFlag(key,tile,enable);
            try{SaveOptions();}catch(...){state.preferences=previous;throw;}
            RefreshDesktop();break;
        }
        case RecoverInput:Hotkey(4);break;case HideAll:Hotkey(3);break;case Options:Settings();FocusTick();break;
        case Templates:ShowTemplates();break;case Share:ShareLayout(false);break;
        case ImportShare:{const auto path=ChoosePath(false,L(L"匯入分享包",L"Import share package"));if(path)ImportPackage(*path);break;}
        case Sync:Synchronize();break;case Steam:SteamOptions();break;case 4115:ShortcutOptions();break;
        case 4114:Paste();break;
        case 4116:ShareLayout(true);break;
        case 4117:{const auto url=L"https://steamcommunity.com/workshop/browse/?appid="+std::to_wstring(steam.AppId());
            ShellExecuteW(app.control,L"open",url.c_str(),nullptr,nullptr,SW_SHOWNORMAL);break;}
        }
    }catch(const std::exception& e){Error(Wide(e.what()));}
    return true;
}
void Start() {
    InitCommonControls();
    const auto path=PreferencesPath();
    try{
        if(fs::exists(path)){
            const auto saved=Json::parse(ReadBytes(path,1024*1024));
            for(auto it=saved.begin();it!=saved.end();++it)if(state.preferences.contains(it.key()))state.preferences[it.key()]=it.value();
        }
        for(auto key:{"locked","through"})if(!state.preferences[key].is_array())throw std::runtime_error("Invalid input flags.");
        for(auto key:{"fullscreen","mute","manualFocus","hidden","shareText","sharePhotos","steamAuto"})if(!state.preferences[key].is_boolean())throw std::runtime_error("Invalid desktop preferences.");
        if(!state.preferences["hotkeys"].is_array()||state.preferences["hotkeys"].size()!=5||!state.preferences["monitors"].is_object())
            throw std::runtime_error("Invalid desktop preferences.");
        for(const auto& key:state.preferences["hotkeys"])if(!key.is_number_unsigned()||key.get<unsigned>()>65535)
            throw std::runtime_error("Invalid shortcut.");
        if(!state.preferences["syncFolder"].is_string()||!state.preferences["syncBase"].is_string()||!state.preferences["steamBase"].is_string())throw std::runtime_error("Invalid sync preference.");
    }catch(const std::exception& e){
        Log(L"Desktop preferences preserved: "+Wide(e.what()));
        State defaults;state.preferences=std::move(defaults.preferences);
    }
    try{
        const auto file=app.directory/L"recycle.json";
        if(fs::exists(file)){
            const auto items=Json::parse(ReadBytes(file,16*1024*1024));
            if(!items.is_array()||items.size()>100)throw std::runtime_error("Invalid recycle bin.");
            for(const auto& item:items){
                std::stringstream body(JsonString(item.at("tile"),128*1024));auto tiles=Load(body);
                if(tiles.size()!=1)throw std::runtime_error("Invalid recycle tile.");
                state.trash.push_back({JsonInt(item.at("layout"),1,LayoutIdLimit),JsonString(item.at("name"),128),tiles.front()});
            }
        }
    }catch(const std::exception& e){state.trash.clear();Log(L"Recycle bin preserved: "+Wide(e.what()));}
    state.history.Reset(CurrentCatalog());state.ready=true;
    state.monitorKey=ReadMonitors(state.areas);
    if(!app.smoke){
        bool offscreen=false;
        for(const auto& p:CurrentCatalog().profiles)for(const auto& tile:p.second.tiles){
            RECT r{LONG(tile.position.x),LONG(tile.position.y),LONG(tile.position.x+tile.size),LONG(tile.position.y+TileHeight(tile))};
            offscreen|=MonitorFromRect(&r,MONITOR_DEFAULTTONULL)==nullptr;
        }
        if(offscreen){state.monitorKey.clear();MonitorChanged();}
    }
    if(!app.smoke){
        try{RegisterKeys();}catch(const std::exception& e){Error(Wide(e.what()));}
        SetTimer(app.control,47,1500,nullptr);
    }
    DragAcceptFiles(app.control,TRUE);for(auto& view:app.views)DragAcceptFiles(view.second.window,TRUE);
    UpdateInput();steam.Start();
    if(steam.Available()){SetTimer(app.control,48,100,nullptr);SetTimer(app.control,49,300000,nullptr);}
    if(steam.Available()&&state.preferences.value("steamAuto",false))try{Synchronize(false);}catch(const std::exception& e){Status(Wide(e.what()));}
}
void Stop() {
    if(!state.ready)return;
    FinishInline(true);
    try{SaveMonitorSnapshot();SaveOptions();}catch(const std::exception& e){Log(Wide(e.what()));}
    if(steam.Available()&&state.preferences.value("steamAuto",false))try{Synchronize(false);}catch(const std::exception& e){Log(Wide(e.what()));}
    for(int i=0;i<5;++i)UnregisterHotKey(app.control,4500+i);
    for(int id:{47,48,49,50})KillTimer(app.control,id);
    steam.Stop();state.ready=false;
}
bool Message(HWND window,UINT message,WPARAM w,LPARAM l) {
    if(!state.ready)return false;
    try{
        if(message==WM_APP+62){if(state.editor&&GetFocus()!=state.editor)FinishInline(true);return true;}
        if(message==WM_DROPFILES){Drop(reinterpret_cast<HDROP>(w));return true;}
        if(message==WM_HOTKEY&&w>=4500&&w<4505){Hotkey(static_cast<int>(w)-4500);return true;}
        if(message==WM_DISPLAYCHANGE||message==WM_DPICHANGED||message==WM_SETTINGCHANGE){SetTimer(app.control,50,500,nullptr);return false;}
        if(message==WM_TIMER){
            if(w==47){FocusTick();return true;}
            if(w==48){const auto previous=steam.status;steam.Poll();if(previous!=steam.status)Status(Wide(steam.status));return true;}
            if(w==49){if(state.preferences.value("steamAuto",false))Synchronize(false);return true;}
            if(w==50){KillTimer(app.control,50);MonitorChanged();return true;}
        }
    }catch(const std::exception& e){Error(Wide(e.what()));return true;}
    (void)window;(void)l;return false;
}
bool Accelerator(const MSG& message) {
    if(message.message!=WM_KEYDOWN||!(GetKeyState(VK_CONTROL)&0x8000)||message.hwnd==state.editor)return false;
    wchar_t klass[32]{};GetClassNameW(message.hwnd,klass,32);
    if(_wcsicmp(klass,L"EDIT")==0||_wcsicmp(klass,L"COMBOBOX")==0)return false;
    try{
        if(message.wParam=='Z'){Travel(true);return true;}
        if(message.wParam=='Y'){Travel(false);return true;}
        if(message.wParam=='V'){Paste();return true;}
    }catch(const std::exception& e){Error(Wide(e.what()));return true;}
    return false;
}
} // namespace desktools
