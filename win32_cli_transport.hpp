#pragma once
// Included by the GUI and the console client, after windows.h.
#include <restartmanager.h>
#include <filesystem>
#include <set>
#include <string>
#include <vector>
#pragma comment(lib,"Rstrtmgr.lib")
#pragma comment(lib,"advapi32.lib")
#pragma comment(lib,"user32.lib")

namespace desktiles_ipc {
constexpr wchar_t ControllerClass[]=L"HexDesk.Control";
constexpr wchar_t ClientClass[]=L"DeskTiles.CliReply.v1";
constexpr ULONG_PTR Request=0x44544331, Response=0x44545231;
constexpr DWORD RequestLimit=512*1024,ResponseLimit=1024*1024;
inline std::vector<BYTE> UserSid(DWORD pid) {
    HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
    if(!process)return {};
    HANDLE token=nullptr;
    if(!OpenProcessToken(process,TOKEN_QUERY,&token)){CloseHandle(process);return {};}
    CloseHandle(process);
    DWORD size=0;GetTokenInformation(token,TokenUser,nullptr,0,&size);
    std::vector<BYTE> buffer(size),sid;
    if(size && GetTokenInformation(token,TokenUser,buffer.data(),size,&size)){
        auto* value=reinterpret_cast<TOKEN_USER*>(buffer.data());
        sid.resize(GetLengthSid(value->User.Sid));
        if(!CopySid(static_cast<DWORD>(sid.size()),sid.data(),value->User.Sid))sid.clear();
    }
    CloseHandle(token);return sid;
}
inline bool SameUserWindow(HWND window,const wchar_t* expectedClass) {
    wchar_t klass[128]{};
    if(!window || !GetClassNameW(window,klass,128) || wcscmp(klass,expectedClass)!=0)return false;
    DWORD owner=0;GetWindowThreadProcessId(window,&owner);
    if(!owner || owner==GetCurrentProcessId())return false;
    static const auto self=UserSid(GetCurrentProcessId());
    const auto other=UserSid(owner);return !self.empty() && self==other;
}
inline HWND FindController(const std::filesystem::path& lockPath) {
    DWORD session=0;wchar_t key[CCH_RM_SESSION_KEY+1]{};
    if(RmStartSession(&session,0,key)!=ERROR_SUCCESS)return nullptr;
    struct Guard {DWORD handle;~Guard(){RmEndSession(handle);}} guard{session};
    auto name=lockPath.wstring();LPCWSTR file=name.c_str();
    // Query owners only. Never shut down or restart the app.
    if(RmRegisterResources(session,1,&file,0,nullptr,0,nullptr)!=ERROR_SUCCESS)return nullptr;
    UINT needed=0,count=0;DWORD reasons=0;std::vector<RM_PROCESS_INFO> processes;
    DWORD result=RmGetList(session,&needed,&count,nullptr,&reasons);
    for(int attempt=0;result==ERROR_MORE_DATA && attempt<3;++attempt){
        if(needed>4096)return nullptr;
        processes.resize(needed);count=needed;
        result=RmGetList(session,&needed,&count,processes.data(),&reasons);
    }
    if(result!=ERROR_SUCCESS || !count)return nullptr;
    struct Search {std::set<DWORD> owners;HWND window=nullptr;} search;
    for(UINT i=0;i<count;++i)search.owners.insert(processes[i].Process.dwProcessId);
    EnumWindows([](HWND window,LPARAM context)->BOOL {
        auto& found=*reinterpret_cast<Search*>(context);DWORD owner=0;GetWindowThreadProcessId(window,&owner);
        if(!found.owners.count(owner) || !SameUserWindow(window,ControllerClass))return TRUE;
        found.window=window;return FALSE;
    },reinterpret_cast<LPARAM>(&search));
    return search.window;
}
inline bool CopyMessage(HWND target,HWND source,ULONG_PTR kind,const std::string& data,UINT timeout,UINT flags) {
    COPYDATASTRUCT transfer{kind,static_cast<DWORD>(data.size()),const_cast<char*>(data.data())};
    DWORD_PTR result=0;
    return SendMessageTimeoutW(target,WM_COPYDATA,reinterpret_cast<WPARAM>(source),
        reinterpret_cast<LPARAM>(&transfer),flags|SMTO_ABORTIFHUNG|SMTO_ERRORONEXIT,timeout,&result) && result;
}
} // namespace desktiles_ipc
