// Included by win32.cpp after its widget helpers; Win32-only feature UI.
constexpr wchar_t AlarmClass[]=L"HexDesk.Alarm";
constexpr wchar_t ReminderClass[]=L"HexDesk.Reminder";
constexpr wchar_t StartupKey[]=L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t StartupValue[]=L"DeskTiles";
std::wstring AlarmCaption(const Alarm& alarm) {
    std::wstring rule;
    if(alarm.repeat==AlarmRepeat::Once)rule=alarm.date.empty()?Tr(L"單次"):Wide(alarm.date);
    else if(alarm.repeat==AlarmRepeat::Daily)rule=Tr(L"每天");
    else {
        const wchar_t* days[]={L"週日",L"週一",L"週二",L"週三",L"週四",L"週五",L"週六"};
        for(int day=0;day<7;++day)if(alarm.weekdays&(1<<day)){if(!rule.empty())rule+=L" ";rule+=Tr(days[day]);}
    }
    return rule+L" · "+(alarm.enabled?Tr(L"已啟用"):Tr(L"已停用"));
}
std::wstring AlarmSummary(const Alarm& alarm) {return AlarmCaption(alarm)+L" · "+Wide(AlarmTimeText(alarm.minute));}
std::wstring StartupCommand() {
    std::vector<wchar_t> executable(32768);DWORD length=GetModuleFileNameW(nullptr,executable.data(),static_cast<DWORD>(executable.size()));
    if(!length||length>=executable.size())return {};
    return desktile_startup::Command(fs::absolute(fs::path(executable.data())).wstring(),app.directory.wstring());
}
std::wstring ReadStartupValue(const wchar_t* key=StartupKey) {
    DWORD type=0,bytes=0;
    if(RegGetValueW(HKEY_CURRENT_USER,key,StartupValue,RRF_RT_REG_SZ,&type,nullptr,&bytes)!=ERROR_SUCCESS||bytes>65536)return {};
    std::vector<wchar_t> value(bytes/sizeof(wchar_t)+1);
    if(RegGetValueW(HKEY_CURRENT_USER,key,StartupValue,RRF_RT_REG_SZ,&type,value.data(),&bytes)!=ERROR_SUCCESS)return {};
    return value.data();
}
bool WriteStartupValue(bool enabled,const std::wstring& command,const wchar_t* key=StartupKey,bool explicitConsent=false) {
    if(enabled&&!explicitConsent)return false;
    if(enabled && (command.empty()||command.size()>260))return false;
    HKEY registry=nullptr;LONG error=RegCreateKeyExW(HKEY_CURRENT_USER,key,0,nullptr,0,KEY_SET_VALUE,nullptr,&registry,nullptr);
    if(error!=ERROR_SUCCESS)return false;
    if(enabled)error=RegSetValueExW(registry,StartupValue,0,REG_SZ,reinterpret_cast<const BYTE*>(command.c_str()),static_cast<DWORD>((command.size()+1)*sizeof(wchar_t)));
    else {error=RegDeleteValueW(registry,StartupValue);if(error==ERROR_FILE_NOT_FOUND)error=ERROR_SUCCESS;}
    RegCloseKey(registry);return error==ERROR_SUCCESS;
}
void RefreshStartup() {
    if(!app.widgets.count(IdStartup))return;
    // Test runs must never reflect or alter the real login registration.
    SendMessageW(app.widgets[IdStartup],BM_SETCHECK,!app.smoke&&!ReadStartupValue().empty()?BST_CHECKED:BST_UNCHECKED,0);
}
void ToggleStartup() {
    bool requested=SendMessageW(app.widgets[IdStartup],BM_GETCHECK,0,0)==BST_CHECKED;
    auto command=StartupCommand();bool consent=false;
    if(requested&&!app.smoke) {
        auto message=std::wstring(Tr(L"允許在 Windows 登入後自動啟動 DeskTiles？\n\n程式與資料路徑：\n"))+command+
            Tr(L"\n\n預設為關閉；選擇「是」才會加入目前使用者的啟動設定。可隨時取消勾選停用。");
        consent=MessageBoxW(app.control,message.c_str(),Tr(L"同意開機自動啟動"),MB_YESNO|MB_DEFBUTTON2|MB_ICONQUESTION)==IDYES;
    }
    if(app.smoke|| (requested&&!consent)){RefreshStartup();return;}
    Log(Tr(L"開機啟動輸入：")+command+Tr(L"；輸出：HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run\\DeskTiles"));
    Status(Tr(L"[開機啟動 1/2] 更新使用者設定"));
    bool saved=desktile_startup::Change(requested,consent,[&](bool enabled){return WriteStartupValue(enabled,command,StartupKey,consent);});
    RefreshStartup();
    if(!saved)Error(Tr(L"無法更新開機啟動設定，已保留原設定。"));
    else Status(requested?Tr(L"[開機啟動 2/2] 已經使用者同意啟用。"):Tr(L"[開機啟動 2/2] 已停用。"));
}
void UpdateAlarmControls() {
    if(!app.alarmControl)return;auto& f=app.alarmWidgets;
    auto repeat=SendMessageW(f[IdAlarmRepeat],CB_GETCURSEL,0,0);
    EnableWindow(f[IdAlarmDate],repeat==0);
    for(int day=0;day<7;++day)EnableWindow(f[IdAlarmDay0+day],repeat==2);
    EnableWindow(f[IdAlarmApply],Find(app.primary)&&!app.selection.empty());
}
void RefreshAlarm() {
    if(app.widgets.count(245)) {
        auto* tile=Find(app.primary);
        SetWindowTextW(app.widgets[245],tile?AlarmSummary(tile->alarm).c_str():L"");
    }
    if(!app.alarmControl)return;
    auto* tile=Find(app.primary);if(!tile){UpdateAlarmControls();return;}auto alarm=tile->alarm;
    bool fresh=tile->kind!=Kind::Alarm&&alarm.date.empty();
    auto now=static_cast<std::int64_t>(std::time(nullptr));auto nextMinute=LocalTime(now+60);
    if(alarm.date.empty())alarm.date=DateText(nextMinute);
    if(fresh)alarm.minute=nextMinute.tm_hour*60+nextMinute.tm_min;
    auto& f=app.alarmWidgets;
    SetWindowTextW(f[IdAlarmDate],Wide(alarm.date).c_str());SetWindowTextW(f[IdAlarmTime],Wide(AlarmTimeText(alarm.minute)).c_str());
    SendMessageW(f[IdAlarmRepeat],CB_SETCURSEL,static_cast<int>(alarm.repeat),0);
    SendMessageW(f[IdAlarmEnabled],BM_SETCHECK,fresh||alarm.enabled?BST_CHECKED:BST_UNCHECKED,0);
    for(int day=0;day<7;++day)SendMessageW(f[IdAlarmDay0+day],BM_SETCHECK,alarm.weekdays&(1<<day)?BST_CHECKED:BST_UNCHECKED,0);
    UpdateAlarmControls();
}
bool ApplyAlarmSettings() {
    auto* primary=Find(app.primary);if(!primary||app.selection.empty())return false;
    Log(Tr(L"鬧鐘輸入：控制視窗；輸出：")+app.statePath.wstring());Status(Tr(L"[鬧鐘 1/3] 驗證本地時間與重複規則"));
    Alarm alarm;auto& f=app.alarmWidgets;
    int repeat=static_cast<int>(SendMessageW(f[IdAlarmRepeat],CB_GETCURSEL,0,0));alarm.repeat=static_cast<AlarmRepeat>(repeat);
    alarm.date=Utf8(TextOf(f[IdAlarmDate]));alarm.enabled=SendMessageW(f[IdAlarmEnabled],BM_GETCHECK,0,0)==BST_CHECKED;
    alarm.weekdays=0;for(int day=0;day<7;++day)if(SendMessageW(f[IdAlarmDay0+day],BM_GETCHECK,0,0)==BST_CHECKED)alarm.weekdays|=1<<day;
    if(!ParseAlarmTime(Utf8(TextOf(f[IdAlarmTime])),alarm.minute)||!ValidAlarm(alarm)) {
        Error(Tr(L"請輸入有效日期（1970–2099，YYYY-MM-DD）、時間（HH:MM），指定星期至少選一天。"));return false;
    }
    auto now=static_cast<std::int64_t>(std::time(nullptr));
    if(alarm.enabled&&!NextAlarm(alarm,now)){Error(Tr(L"啟用的單次鬧鐘必須是未來的有效本地時間。"));return false;}
    ArmAlarm(alarm,now);auto text=Utf8(TextOf(app.widgets[IdText]));
    Status(Tr(L"[鬧鐘 2/3] 套用至選取格"));
    for(auto& tile:app.tiles)if(app.selection.count(tile.id)) {
        tile.kind=Kind::Alarm;tile.alarm=alarm;tile.text=text;tile.running=false;
        app.views[tile.id].image.reset();app.views[tile.id].imagePath.clear();
    }
    SaveState();UpdateClock();RefreshControls();Repaint();
    Status(app.savingFailed?Tr(L"[鬧鐘 3/3] 已更新顯示，但儲存失敗。"):Tr(L"[鬧鐘 3/3] 已儲存。"));return !app.savingFailed;
}
LRESULT CALLBACK AlarmProc(HWND window,UINT message,WPARAM w,LPARAM l) {
    switch(message) {
    case WM_ERASEBKGND:{RECT r;GetClientRect(window,&r);FillRect(reinterpret_cast<HDC>(w),&r,app.background);return 1;}
    case WM_CTLCOLORSTATIC:SetBkMode(reinterpret_cast<HDC>(w),TRANSPARENT);SetTextColor(reinterpret_cast<HDC>(w),RGB(42,62,69));return reinterpret_cast<LRESULT>(app.background);
    case WM_COMMAND:
        if(LOWORD(w)==IdAlarmRepeat&&HIWORD(w)==CBN_SELCHANGE)UpdateAlarmControls();
        else if(LOWORD(w)==IdAlarmApply)ApplyAlarmSettings();
        else if(LOWORD(w)==IdAlarmClose)ShowWindow(window,SW_HIDE);
        return 0;
    case WM_CLOSE:ShowWindow(window,SW_HIDE);return 0;
    }
    return DefWindowProcW(window,message,w,l);
}
void ShowAlarm() {
    if(!Find(app.primary)||app.selection.empty())return;
    // Show the text editor without refreshing away the user's pending note.
    ShowWindow(app.control,SW_SHOWNORMAL);
    SendMessageW(app.widgets[IdType],CB_SETCURSEL,3,0);UpdateContentVisibility();
    if(!app.alarmControl) {
        WNDCLASSW klass{};klass.hInstance=app.instance;klass.lpszClassName=AlarmClass;klass.lpfnWndProc=AlarmProc;
        klass.hCursor=LoadCursor(nullptr,IDC_ARROW);klass.hIcon=app.icon;RegisterClassW(&klass);
        RECT area{0,0,UiWidth(520),420};AdjustWindowRect(&area,WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,FALSE);
        RECT parent;GetWindowRect(app.control,&parent);
        app.alarmControl=CreateWindowExW(WS_EX_DLGMODALFRAME,AlarmClass,Tr(L"DeskTiles · 鬧鐘設定"),WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,
            parent.left+25,parent.top+100,area.right-area.left,area.bottom-area.top,app.control,nullptr,app.instance,nullptr);
        auto field=[](int id,const wchar_t* type,const wchar_t* text,DWORD style,int x,int y,int width,int height){return WidgetFor(app.alarmControl,app.alarmWidgets,id,type,text,style,x,y,width,height);};
        field(250,L"STATIC",Tr(L"日期 YYYY-MM-DD"),0,22,24,155,26);field(IdAlarmDate,L"EDIT",L"",ES_AUTOHSCROLL|WS_TABSTOP,190,22,300,32);
        field(251,L"STATIC",Tr(L"時間 HH:MM（24 小時）"),0,22,72,165,26);field(IdAlarmTime,L"EDIT",L"09:00",ES_AUTOHSCROLL|WS_TABSTOP,190,70,300,32);
        field(252,L"STATIC",Tr(L"重複規則"),0,22,120,155,26);field(IdAlarmRepeat,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,190,118,300,150);
        for(auto text:{Tr(L"單次"),Tr(L"每天"),Tr(L"指定星期")})SendMessageW(app.alarmWidgets[IdAlarmRepeat],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(text));
        const wchar_t* days[]={L"週日",L"週一",L"週二",L"週三",L"週四",L"週五",L"週六"};
        for(int day=0;day<7;++day)field(IdAlarmDay0+day,L"BUTTON",Tr(days[day]),BS_AUTOCHECKBOX|WS_TABSTOP,22+day*69,174,68,30);
        field(IdAlarmEnabled,L"BUTTON",Tr(L"啟用鬧鐘"),BS_AUTOCHECKBOX|WS_TABSTOP,22,226,468,32);
        field(253,L"STATIC",Tr(L"使用 Windows 本地時間。程式關閉時不會響鈴。\n睡眠喚醒後補提醒一次；重新開啟時不補響過期鬧鐘。"),0,22,275,475,54);
        field(IdAlarmApply,L"BUTTON",Tr(L"套用鬧鐘與文字至選取格"),WS_TABSTOP|BS_DEFPUSHBUTTON,22,357,325,40);
        field(IdAlarmClose,L"BUTTON",Tr(L"關閉"),WS_TABSTOP,365,357,125,40);
        SendMessageW(app.alarmWidgets[IdAlarmDate],EM_SETLIMITTEXT,10,0);SendMessageW(app.alarmWidgets[IdAlarmTime],EM_SETLIMITTEXT,5,0);
    }
    RefreshAlarm();ShowWindow(app.alarmControl,SW_SHOWNORMAL);SetForegroundWindow(app.alarmControl);
}
LRESULT CALLBACK ReminderProc(HWND window,UINT message,WPARAM w,LPARAM l) {
    if((message==WM_COMMAND&&LOWORD(w)==IdDismiss) || message==WM_CLOSE) {
        ShowWindow(window,SW_HIDE);app.reminderMessages.clear();return 0;
    }
    if(message==WM_ERASEBKGND){RECT r;GetClientRect(window,&r);FillRect(reinterpret_cast<HDC>(w),&r,app.background);return 1;}
    return DefWindowProcW(window,message,w,l);
}
void RefreshReminder() {
    if(!app.reminder)return;
    SetWindowTextW(app.reminder,Tr(L"DeskTiles · 提醒"));
    std::wstring text;for(const auto& item:app.reminderMessages)
        text+=std::wstring(item.alarm?Tr(L"鬧鐘時間到"):Tr(L"倒數結束"))+L" · #"+std::to_wstring(item.id)+L"\r\n"+item.text+L"\r\n\r\n";
    SetWindowTextW(GetDlgItem(app.reminder,IdReminderText),text.c_str());
    SetWindowTextW(GetDlgItem(app.reminder,IdDismiss),Tr(L"知道了"));
}
void NotifyTile(const Tile& tile,bool alarm,bool preview) {
    if(app.smoke&&!preview)return;
    if(!app.reminder) {
        WNDCLASSW klass{};klass.hInstance=app.instance;klass.lpszClassName=ReminderClass;klass.lpfnWndProc=ReminderProc;
        klass.hCursor=LoadCursor(nullptr,IDC_ARROW);klass.hIcon=app.icon;RegisterClassW(&klass);
        RECT area{0,0,UiWidth(470),280};AdjustWindowRect(&area,WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,FALSE);
        app.reminder=CreateWindowExW(WS_EX_TOPMOST|WS_EX_TOOLWINDOW,ReminderClass,Tr(L"DeskTiles · 提醒"),WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,
            160,160,area.right-area.left,area.bottom-area.top,app.control,nullptr,app.instance,nullptr);
        HWND text=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_VSCROLL|ES_MULTILINE|ES_READONLY,
            20,20,UiWidth(438),195,app.reminder,reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdReminderText)),app.instance,nullptr);
        HWND close=CreateWindowExW(0,L"BUTTON",Tr(L"知道了"),WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_DEFPUSHBUTTON,
            20,230,UiWidth(438),35,app.reminder,reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdDismiss)),app.instance,nullptr);
        for(auto h:{text,close})SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(app.font),TRUE);
    }
    std::wstring title=std::wstring(alarm?Tr(L"鬧鐘時間到"):Tr(L"倒數結束"))+L" · #"+std::to_wstring(tile.id);
    app.reminderMessages.push_back({tile.id,alarm,Wide(tile.text).substr(0,4000)});
    if(app.reminderMessages.size()>10)app.reminderMessages.erase(app.reminderMessages.begin());
    RefreshReminder();ShowWindow(app.reminder,SW_SHOWNORMAL);
    if(app.smoke)return;
    MessageBeep(MB_ICONINFORMATION);
    auto notification=app.tray;notification.uFlags=NIF_INFO;wcscpy_s(notification.szInfoTitle,title.substr(0,63).c_str());
    wcscpy_s(notification.szInfo,Wide(tile.text).substr(0,255).c_str());notification.dwInfoFlags=NIIF_INFO;Shell_NotifyIconW(NIM_MODIFY,&notification);
}
