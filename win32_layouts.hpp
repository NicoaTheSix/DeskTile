#pragma once
// Included by win32.cpp after the existing save, image and window helpers.

const wchar_t* LayoutLabel(const wchar_t* zh,const wchar_t* en) {
    return language==Language::English?en:zh;
}
fs::path CatalogPath(){return app.directory/L"layouts.hexdesk.json";}
fs::path CatalogBackup(){return app.directory/L"layouts.backup.json";}
Profile ActiveProfile() {
    Profile result=app.catalog.current();
    result.tiles=app.tiles;result.hidden=app.hidden;result.nextTile=app.nextId;
    NormalizeProfile(result);
    return result;
}
LayoutCatalog CurrentCatalog() {
    auto result=app.catalog;
    result.current()=ActiveProfile();
    return result;
}
void WriteCatalog(const LayoutCatalog& catalog) {
    auto file=CatalogPath();
    auto bytes=CatalogJson(catalog).dump();
    if(bytes.size()>=LayoutFileLimit)throw std::runtime_error("The saved layouts exceed the 128 MiB storage limit.");
    if(fs::exists(file) && !CopyFileW(file.c_str(),CatalogBackup().c_str(),FALSE))
        throw std::runtime_error("Could not back up the layouts. No layout was replaced.");
    AtomicOutput(file,[&](std::ostream& out){out<<bytes<<'\n';});
}
void ActivateStoredProfiles() {
    auto path=CatalogPath();
    if(fs::exists(path)) {
        if(!fs::is_regular_file(path)||fs::file_size(path)>LayoutFileLimit)
            throw std::runtime_error("The saved layouts are not a readable file or exceed 128 MiB.");
        std::ifstream in(path,std::ios::binary);app.catalog=ReadCatalog(in);
        auto now=static_cast<std::int64_t>(std::time(nullptr));
        for(auto& item:app.catalog.profiles) {
            Wide(item.second.name);
            for(auto& t:item.second.tiles){
                Wide(t.text);Wide(t.image);Wide(t.fontFamily);
                if(t.kind==Kind::Alarm)ArmAlarm(t.alarm,now);
            }
        }
        auto& active=app.catalog.current();
        app.tiles=std::move(active.tiles);app.hidden=std::move(active.hidden);app.nextId=active.nextTile;
        app.profileMode=true;app.statePath=path;
    } else {
        Profile initial;
        initial.name=Utf8(LayoutLabel(L"預設",L"Default"));
        app.catalog.profiles.emplace(1,std::move(initial));
    }
}
void ApplyCatalog(LayoutCatalog next,int changedTile=0) {
    const bool switching=next.active!=app.catalog.active;
    const bool changingView=switching||changedTile!=0;
    auto& profile=next.current();
    const auto oldNext=app.nextId;
    std::vector<Tile> oldTiles;
    std::set<int> oldHidden;
    std::map<int,TileView> oldViews;
    Log(std::wstring(LayoutLabel(L"配置輸入：",L"Layout input: "))+app.statePath.wstring()+
        LayoutLabel(L"；輸出：",L"; output: ")+CatalogPath().wstring());
    Status(LayoutLabel(L"[配置 1/3] 準備方塊與顯示",L"[Layouts 1/3] Prepare tiles and views"));
    if(changingView) {
        // Copy before moving the live state, so allocation failure cannot alter it.
        auto preparedTiles=profile.tiles;
        auto preparedHidden=profile.hidden;
        oldTiles=std::move(app.tiles);oldHidden=std::move(app.hidden);
        if(switching){oldViews=std::move(app.views);app.views.clear();}
        else if(auto old=app.views.extract(changedTile);!old.empty())oldViews.insert(std::move(old));
        app.tiles=std::move(preparedTiles);app.hidden=std::move(preparedHidden);
        app.nextId=profile.nextTile;
    }
    try {
        if(changingView) {
            app.stagingViews=true;
            for(auto& tile:app.tiles)if(switching||tile.id==changedTile)CreateTileWindow(tile);
            app.stagingViews=false;
        }
        Status(LayoutLabel(L"[配置 2/3] 原子保存配置",L"[Layouts 2/3] Save layouts atomically"));
        WriteCatalog(next);
    } catch(...) {
        app.stagingViews=false;
        if(changingView) {
            if(switching){
                for(auto& entry:app.views)if(entry.second.window)DestroyWindow(entry.second.window);
                app.views=std::move(oldViews);
            } else {
                auto created=app.views.find(changedTile);
                if(created!=app.views.end()){if(created->second.window)DestroyWindow(created->second.window);app.views.erase(created);}
                app.views.merge(oldViews);
            }
            app.tiles=std::move(oldTiles);app.hidden=std::move(oldHidden);app.nextId=oldNext;
        }
        throw;
    }
    app.profileMode=true;app.statePath=CatalogPath();app.savingFailed=false;
    app.catalog=std::move(next);
    app.catalog.current().tiles.clear();app.catalog.current().hidden.clear();
    for(auto& entry:oldViews)if(entry.second.window)DestroyWindow(entry.second.window);
    if(switching) {
        app.selection.clear();app.primary=0;
        for(auto window:{app.appearance,app.layout,app.backgroundControl,app.alarmControl})if(window)ShowWindow(window,SW_HIDE);
    } else if(changedTile && !Find(changedTile)) {
        app.selection.erase(changedTile);
        if(app.primary==changedTile)app.primary=app.selection.empty()?0:*app.selection.begin();
    }
    if(changingView) {
        for(auto& tile:app.tiles)if(switching||tile.id==changedTile){Position(tile);PresentTile(tile);}
        UpdateAnimation();RefreshControls();
    }
    UpdateClock();
    Status(std::wstring(LayoutLabel(L"[配置 3/3] 已儲存；目前：",L"[Layouts 3/3] Saved; active: "))+Wide(app.catalog.current().name));
}
void PrepareLayoutEdit() {
    EndDrag(false);
    if(app.sharedClock)TickTimers();
}
enum : int {IdProfileSave=3000,IdProfileEmpty,IdProfileRename,IdProfileFirst=3100};
void AddProfileMenu(HMENU menu) {
    HMENU submenu=CreatePopupMenu();
    if(!submenu)return;
    unsigned index=0;
    for(const auto& entry:app.catalog.profiles) {
        std::wstring text;
        for(wchar_t c:Wide(entry.second.name)){if(c==L'&')text+=L'&';text+=c;}
        AppendMenuW(submenu,MF_STRING|(entry.first==app.catalog.active?MF_CHECKED:0),
            static_cast<UINT_PTR>(IdProfileFirst)+index++,text.c_str());
    }
    AppendMenuW(submenu,MF_SEPARATOR,0,nullptr);
    AppendMenuW(submenu,MF_STRING,IdProfileSave,LayoutLabel(L"儲存目前配置的副本…",L"Save a copy of current layout…"));
    AppendMenuW(submenu,MF_STRING,IdProfileEmpty,LayoutLabel(L"新增空白配置…",L"Create empty layout…"));
    AppendMenuW(submenu,MF_STRING,IdProfileRename,LayoutLabel(L"重新命名目前配置…",L"Rename current layout…"));
    if(!AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(submenu),LayoutLabel(L"桌面配置",L"Desktop layouts")))DestroyMenu(submenu);
}
struct LayoutNameDialog {std::wstring name;bool rename;};
INT_PTR CALLBACK LayoutNameProc(HWND dialog,UINT message,WPARAM w,LPARAM l) {
    if(message==WM_INITDIALOG){
        auto* ctx=reinterpret_cast<LayoutNameDialog*>(l);
        SetWindowLongPtrW(dialog,DWLP_USER,reinterpret_cast<LONG_PTR>(ctx));
        SetWindowTextW(dialog,ctx->rename?LayoutLabel(L"重新命名配置",L"Rename layout"):LayoutLabel(L"儲存桌面配置",L"Save desktop layout"));
        RECT area{};GetClientRect(dialog,&area);
        auto create=[&](const wchar_t* cls,const wchar_t* text,DWORD style,int id,int x,int y,int cx,int cy,DWORD ex=0){
            HWND control=CreateWindowExW(ex,cls,text,WS_CHILD|WS_VISIBLE|style,x,y,cx,cy,dialog,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),app.instance,nullptr);
            if(control)SendMessageW(control,WM_SETFONT,reinterpret_cast<WPARAM>(app.font),TRUE);
            return control;
        };
        create(L"STATIC",LayoutLabel(L"配置名稱",L"Layout name"),0,0,12,8,area.right-24,23);
        auto field=create(L"EDIT",ctx->name.c_str(),WS_TABSTOP|ES_AUTOHSCROLL,100,12,34,area.right-24,27,WS_EX_CLIENTEDGE);
        create(L"BUTTON",LayoutLabel(L"儲存",L"Save"),WS_TABSTOP|BS_DEFPUSHBUTTON,IDOK,area.right-179,72,78,29);
        create(L"BUTTON",LayoutLabel(L"取消",L"Cancel"),WS_TABSTOP,IDCANCEL,area.right-90,72,78,29);
        SendMessageW(field,EM_SETLIMITTEXT,128,0);SendMessageW(field,EM_SETSEL,0,-1);SetFocus(field);
        return FALSE;
    }
    if(message==WM_COMMAND && (LOWORD(w)==IDOK || LOWORD(w)==IDCANCEL)){
        auto* ctx=reinterpret_cast<LayoutNameDialog*>(GetWindowLongPtrW(dialog,DWLP_USER));
        if(LOWORD(w)==IDOK && ctx)ctx->name=TextOf(GetDlgItem(dialog,100));
        EndDialog(dialog,LOWORD(w));return TRUE;
    }
    return FALSE;
}
bool ProfileMenuCommand(int id) {
    if(id!=IdProfileSave && id!=IdProfileEmpty && id!=IdProfileRename &&
       (id<IdProfileFirst || id>=IdProfileFirst+LayoutLimit))return false;
    try {
        if(id>=IdProfileFirst) {
            const auto index=static_cast<size_t>(id-IdProfileFirst);
            if(index>=app.catalog.profiles.size())return true;
            auto at=std::next(app.catalog.profiles.begin(),index);
            if(at->first==app.catalog.active)return true;
            PrepareLayoutEdit();auto next=CurrentCatalog();next.active=at->first;ApplyCatalog(std::move(next));
        } else {
            const bool rename=id==IdProfileRename;
            LayoutNameDialog prompt{rename?Wide(app.catalog.current().name):L"",rename};
            struct alignas(DWORD) Template {DLGTEMPLATE header;WORD menu=0,klass=0,title=0;} dialog{};
            dialog.header.style=WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME|DS_CENTER;
            dialog.header.cx=258;dialog.header.cy=76;
            if(DialogBoxIndirectParamW(app.instance,&dialog.header,app.control,LayoutNameProc,reinterpret_cast<LPARAM>(&prompt))!=IDOK)return true;
            PrepareLayoutEdit();auto next=CurrentCatalog();auto name=Utf8(prompt.name);
            if(rename){RequireNewProfileName(next,name,next.active);next.current().name=name;}
            else CreateProfile(next,name,id==IdProfileEmpty);
            ApplyCatalog(std::move(next));
        }
    }catch(const std::exception& e){Error(LocalizeError(Wide(e.what())));}
    return true;
}
