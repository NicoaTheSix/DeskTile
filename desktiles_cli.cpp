#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <iostream>
#include "win32_cli_transport.hpp"
#include "third_party/nlohmann/json.hpp"

using Json=nlohmann::json;
namespace fs=std::filesystem;
namespace ipc=desktiles_ipc;
struct Reply {HWND server=nullptr;std::string text;};
std::string Utf8(const std::wstring& value) {
    if(value.empty())return {};
    int count=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),nullptr,0,nullptr,nullptr);
    if(!count)throw std::runtime_error("Invalid Unicode argument.");
    std::string result(static_cast<size_t>(count),'\0');
    WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),result.data(),count,nullptr,nullptr);
    return result;
}
void Print(const std::string& text,DWORD stream) {
    auto handle=GetStdHandle(stream);DWORD mode=0,done=0;
    if(GetConsoleMode(handle,&mode)) {
        int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),nullptr,0);
        if(!n)return;
        std::wstring wide(static_cast<size_t>(n),L'\0');
        MultiByteToWideChar(CP_UTF8,0,text.data(),static_cast<int>(text.size()),wide.data(),n);
        for(DWORD offset=0;offset<static_cast<DWORD>(wide.size());offset+=done)
            if(!WriteConsoleW(handle,wide.data()+offset,std::min<DWORD>(8192,static_cast<DWORD>(wide.size())-offset),&done,nullptr) || !done)break;
    } else {
        for(DWORD offset=0;offset<static_cast<DWORD>(text.size());offset+=done)
            if(!WriteFile(handle,text.data()+offset,static_cast<DWORD>(text.size())-offset,&done,nullptr) || !done)break;
    }
}
LRESULT CALLBACK ReplyProc(HWND window,UINT msg,WPARAM w,LPARAM l) {
    if(msg==WM_NCCREATE){
        auto* cs=reinterpret_cast<CREATESTRUCTW*>(l);
        SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
    }
    if(msg==WM_COPYDATA){
        auto* reply=reinterpret_cast<Reply*>(GetWindowLongPtrW(window,GWLP_USERDATA));
        auto* data=reinterpret_cast<const COPYDATASTRUCT*>(l);
        if(!reply || reinterpret_cast<HWND>(w)!=reply->server || !data || data->dwData!=ipc::Response ||
           !data->lpData || data->cbData==0 || data->cbData>ipc::ResponseLimit)return FALSE;
        try{reply->text.assign(static_cast<const char*>(data->lpData),data->cbData);return TRUE;}
        catch(...){return FALSE;}
    }
    return DefWindowProcW(window,msg,w,l);
}
constexpr char Help[]=R"HELP(DeskTiles CLI (Windows)
Usage: desktiles-cli.exe [--data-dir PATH] [--json] COMMAND
  layout list
  layout create NAME [--empty]
  layout switch NAME
  layout rename OLD_NAME NEW_NAME
  layout delete NAME --yes
  tile list [--layout NAME]
  tile get|delete|hide|show --id ID [--layout NAME]
  tile add [--layout NAME] [OPTIONS]
  tile update --id ID [--layout NAME] OPTIONS
  Tile options: --shape hexagon|triangle|rectangle
    --kind text|image|timer|alarm  --text TEXT  --image PATH
    --x X --y Y  --size 80..480  --layer top|desktop
    --timer-mode countdown|countup  --seconds N  --running true|false
    --alarm-time HH:MM  --alarm-repeat once|daily|weekly
    --alarm-date YYYY-MM-DD  --alarm-days MASK  --alarm-enabled true|false
  MASK: Sun=1 Mon=2 Tue=4 Wed=8 Thu=16 Fri=32 Sat=64; add selected days.
Start DeskTiles.exe first. Without --layout, tile commands use the visible layout.
Success prints JSON to stdout. Errors go to stderr. Exit: 0 success, 2 invalid
command or save failed, 3 no running app, 4 no reply (check state before retrying).
)HELP";
int ReportError(const std::string& message,int code,bool json) {
    if(json)Print(Json({{"ok",false},{"error",message},{"exit",code}}).dump(-1,' ',true)+'\n',STD_ERROR_HANDLE);
    else Print("DeskTiles: "+message+'\n',STD_ERROR_HANDLE);
    return code;
}
int wmain(int argc,wchar_t** argv) {
    bool compact=false;
    try{
        wchar_t local[32768]{};
        if(!GetEnvironmentVariableW(L"LOCALAPPDATA",local,32768))throw std::runtime_error("LOCALAPPDATA is unavailable.");
        fs::path directory=fs::path(local)/L"HexDesk";
        int first=1;
        while(first<argc){
            std::wstring flag=argv[first];
            if(flag==L"--json"){compact=true;++first;}
            else if(flag==L"--data-dir"){
                if(++first>=argc || !argv[first][0])throw std::runtime_error("--data-dir needs a path.");
                directory=fs::absolute(argv[first++]);
            }else break;
        }
        if(first==argc || std::wstring(argv[first])==L"--help" || std::wstring(argv[first])==L"help"){Print(Help,STD_OUTPUT_HANDLE);return 0;}
        Json args=Json::array();
        for(int i=first;i<argc;++i)args.push_back(Utf8(argv[i]));
        // Resolve image paths in the invoking shell's directory, never the GUI's.
        if(args.size()>=2 && args[0]=="tile" && (args[1]=="add" || args[1]=="update")){
            for(int i=first+2;i+1<argc;i+=2)if(std::wstring(argv[i])==L"--image" && argv[i+1][0])
                args[static_cast<size_t>(i+1-first)]=Utf8(fs::absolute(argv[i+1]).wstring());
        }
        const auto request=Json({{"version",1},{"args",args}}).dump();
        if(request.size()>ipc::RequestLimit)throw std::runtime_error("The command is too large.");
        Reply reply{ipc::FindController(directory/L"session.lock"),{}};
        if(!reply.server)return ReportError("No running DeskTiles instance for "+Utf8(directory.wstring())+".",3,compact);
        auto instance=GetModuleHandleW(nullptr);
        WNDCLASSW cls{};cls.hInstance=instance;cls.lpszClassName=ipc::ClientClass;cls.lpfnWndProc=ReplyProc;
        if(!RegisterClassW(&cls))throw std::runtime_error("Could not register the CLI reply window.");
        HWND client=CreateWindowExW(0,ipc::ClientClass,L"",0,0,0,0,0,HWND_MESSAGE,nullptr,instance,&reply);
        if(!client)throw std::runtime_error("Could not create the CLI reply window.");
        // The client must service the synchronous reply while waiting for the request.
        bool sent=ipc::CopyMessage(reply.server,client,ipc::Request,request,10000,SMTO_NORMAL);
        DestroyWindow(client);
        if(!sent || reply.text.empty())return ReportError("No reply from DeskTiles. Use a matching app version and check current state before retrying.",4,compact);
        auto result=Json::parse(reply.text);
        if(!result.value("ok",false))return ReportError(result.value("error",std::string("Command failed.")),2,compact);
        DWORD consoleMode=0;bool ascii=!GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE),&consoleMode);
        Print(result.dump(compact?-1:2,' ',ascii)+'\n',STD_OUTPUT_HANDLE);
        return 0;
    }catch(const std::exception& e){return ReportError(e.what(),2,compact);}
}
