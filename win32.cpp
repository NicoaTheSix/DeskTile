#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objidl.h>
#include <windowsx.h>
#include <shellapi.h>
#include <commdlg.h>
#include <psapi.h>
#include <bcrypt.h>
#include <shlwapi.h>
#include <dwmapi.h>
#include <restartmanager.h>
#include <gdiplus.h>
#undef min
#undef max
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <chrono>
#include <functional>
#include "core.hpp"
#include "config.hpp"
#pragma comment(lib,"user32.lib")
#pragma comment(lib,"gdi32.lib")
#pragma comment(lib,"gdiplus.lib")
#pragma comment(lib,"shell32.lib")
#pragma comment(lib,"comdlg32.lib")
#pragma comment(lib,"advapi32.lib")
#pragma comment(lib,"bcrypt.lib")
#pragma comment(lib,"shlwapi.lib")
#pragma comment(lib,"msimg32.lib")
#pragma comment(lib,"dwmapi.lib")
#pragma comment(lib,"Rstrtmgr.lib")

using namespace hexdesk;
using Gdiplus::Bitmap;
using Gdiplus::Graphics;
using Gdiplus::Color;
using Gdiplus::PointF;
using Gdiplus::GraphicsPath;
using Gdiplus::SolidBrush;
using Gdiplus::Pen;
using Gdiplus::FontFamily;
using Gdiplus::StringFormat;
using Gdiplus::RectF;
using Gdiplus::GdiplusStartupInput;
using Gdiplus::GdiplusStartup;
using Gdiplus::GdiplusShutdown;
using Gdiplus::Ok;
using Gdiplus::InterpolationModeHighQualityBicubic;
using Gdiplus::SmoothingModeAntiAlias;
using Gdiplus::TextRenderingHintClearTypeGridFit;
using Gdiplus::FontStyleRegular;
using Gdiplus::FontStyleBold;
using Gdiplus::UnitPoint;
using Gdiplus::UnitPixel;
using Gdiplus::StringAlignmentCenter;
using Gdiplus::StringTrimmingEllipsisCharacter;
namespace fs=std::filesystem;
constexpr wchar_t TileClass[]=L"HexDesk.Tile";
constexpr wchar_t ControlClass[]=L"HexDesk.Control";
constexpr wchar_t AppearanceClass[]=L"HexDesk.Appearance";
constexpr wchar_t LayoutClass[]=L"HexDesk.Layout";
constexpr wchar_t VisualClass[]=L"HexDesk.Visual";
constexpr wchar_t BackgroundClass[]=L"HexDesk.Background";
constexpr UINT TrayMessage=WM_APP+1;
constexpr int TileW=160,TileH=139;
constexpr int IdList=101,IdType=102,IdText=103,IdImage=104,IdBrowse=105,
    IdMode=106,IdSeconds=107,IdApply=108,IdAdd=109,IdDelete=110,
    IdStart=111,IdReset=112,IdTop=113,IdDesktop=114,IdShow=115,IdExit=116,
    IdEdit=117,IdSelectAll=118,IdAppearance=119,IdBorderStyle=120,IdBorderColor=121,
    IdChooseColor=122,IdBorderWidth=123,IdRgbSpeed=124,IdAnimate=125,IdApplyBorder=126,IdCloseBorder=127,
    IdLayout=128,IdResizeCheck=129,IdTileSize=130,IdTextCheck=131,IdFontFamily=132,
    IdFontSize=133,IdTextColor=134,IdChooseTextColor=135,IdApplyLayout=136,IdCloseLayout=137,
    IdBackground=138,IdBackgroundColor=139,IdBackgroundAlpha=140,IdChooseBackgroundColor=141,
    IdBackgroundColorCheck=142,IdBackgroundAlphaCheck=143,IdApplyBackground=144,IdCloseBackground=145,
    IdExport=146,IdImportReplace=147,IdImportMerge=148,IdConfig=149,
    IdAddHex=150,IdAddTriangle=151,IdAddRectangle=152,IdNewShape=153;
int PixelWidth(const Tile& tile){return static_cast<int>(std::ceil(tile.size));}
int UiWidth(int value){return static_cast<int>(std::lround(value*1.25));}
int PixelHeight(const Tile& tile){return static_cast<int>(std::ceil(TileHeight(tile.size,tile.shape)));}
std::wstring ShapeName(Shape shape){return shape==Shape::Triangle?L"三角形":shape==Shape::Rectangle?L"矩形":L"六角形";}
std::wstring Wide(const std::string& s) {
    if (s.empty()) return {};
    int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),nullptr,0);
    if (!n) throw std::runtime_error("無效的 UTF-8 文字");
    std::wstring r(n,L'\0'); MultiByteToWideChar(CP_UTF8,0,s.data(),static_cast<int>(s.size()),r.data(),n); return r;
}
std::string Utf8(const std::wstring& s) {
    if (s.empty()) return {};
    int n=WideCharToMultiByte(CP_UTF8,0,s.data(),static_cast<int>(s.size()),nullptr,0,nullptr,nullptr);
    std::string r(n,'\0'); WideCharToMultiByte(CP_UTF8,0,s.data(),static_cast<int>(s.size()),r.data(),n,nullptr,nullptr); return r;
}
std::wstring TextOf(HWND h) {
    std::wstring s(GetWindowTextLengthW(h)+1,L'\0'); GetWindowTextW(h,s.data(),static_cast<int>(s.size()));
    s.resize(wcslen(s.c_str())); return s;
}
struct Dib {
    HDC dc=nullptr;HBITMAP bitmap=nullptr;HGDIOBJ old=nullptr;void* bits=nullptr;int width=0,height=0;
    Dib(int w,int h):width(w),height(h) {
        BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=w;
        info.bmiHeader.biHeight=-h;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
        dc=CreateCompatibleDC(nullptr);bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);
        if(!dc || !bitmap){if(bitmap)DeleteObject(bitmap);if(dc)DeleteDC(dc);throw std::runtime_error("無法建立透明畫面緩衝區");}
        old=SelectObject(dc,bitmap);
    }
    ~Dib(){SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);}
};
struct TileView {
    HWND window=nullptr,visual=nullptr;
    std::unique_ptr<Bitmap> image,surface;
    std::unique_ptr<Dib> frame;
    std::string imagePath;
    bool imageFailed=false,surfaceDirty=true,presentationFailed=false;
};
struct App {
    HINSTANCE instance=nullptr;
    HWND control=nullptr,progress=nullptr,desktop=nullptr;
    HFONT font=nullptr,titleFont=nullptr;
    HBRUSH background=nullptr;
    std::map<int,HWND> widgets;
    HWND appearance=nullptr;
    std::map<int,HWND> appearanceWidgets;
    HWND layout=nullptr;
    std::map<int,HWND> layoutWidgets;
    HWND backgroundControl=nullptr;
    std::map<int,HWND> backgroundWidgets;
    std::vector<Tile> tiles;
    std::map<int,TileView> views;
    std::set<int> selection;
    int primary=0,nextId=1;
    fs::path directory,statePath,logPath;
    std::ofstream log;
    bool loadingControls=false,dragging=false,moved=false,savingFailed=false,exiting=false;
    bool smoke=false,sharedClock=false;
    Point dragStart,dragDelta;
    std::vector<Tile> dragOriginal;
    std::set<int> dragSelection;
    std::chrono::steady_clock::time_point lastTick;
    NOTIFYICONDATAW tray{};
    UINT taskbarCreated=0;
    int smokeStep=0,smokeChecks=0;
    bool smokeDone=false;
    double smokeCpu=0,smokeWall=0;
    unsigned long long idleCpuStart=0,idleWallStart=0;
    int processExit=0;
    unsigned long long paintCount=0,idlePaintStart=0;
    HICON icon=nullptr,smallIcon=nullptr,brandIcon=nullptr;
    bool animationTimer=false;
    unsigned long long animationFrames=0;
    double animationSecondsOverride=-1;
    unsigned long long bodyBuilds=0,rgbBodyStart=0,rgbFrameStart=0,rgbPaintStart=0;
    double rgbSampleCpu=0,rgbSampleWall=0;
    unsigned long long rgbSampleFrames=0,rgbSamplePaints=0;
    int layeredErrors=0;
} app;
LRESULT CALLBACK TileProc(HWND,UINT,WPARAM,LPARAM);
LRESULT CALLBACK ControlProc(HWND,UINT,WPARAM,LPARAM);
LRESULT CALLBACK AppearanceProc(HWND,UINT,WPARAM,LPARAM);
LRESULT CALLBACK LayoutProc(HWND,UINT,WPARAM,LPARAM);
LRESULT CALLBACK BackgroundProc(HWND,UINT,WPARAM,LPARAM);
void RefreshControls();
void Repaint();
void UpdateClock();
void SaveState();
void AddTile(Shape shape=Shape::Hexagon);
void Command(int id);
void RefreshAppearance();
void ShowAppearance();
void UpdateAnimation();
void RefreshLayout();
void ShowLayout();
void PresentTile(Tile& tile);
void ShowBackground();
void RefreshBackground();
void ChooseExport();
void ChooseImport(bool merge);
Tile* Find(int id) { for(auto& t:app.tiles) if(t.id==id) return &t; return nullptr; }
void Log(const std::wstring& message) {
    SYSTEMTIME now; GetLocalTime(&now);
    app.log << std::setfill('0') << std::setw(2) << now.wHour << ':' << std::setw(2)
            << now.wMinute << ':' << std::setw(2) << now.wSecond << ' ' << Utf8(message) << '\n';
    app.log.flush(); OutputDebugStringW((message+L"\n").c_str());
}
void Status(const std::wstring& message) {
    if(app.widgets.count(200)) SetWindowTextW(app.widgets[200],message.c_str());
    Log(message);
}
void Error(const std::wstring& message) {
    Log(L"ERROR: "+message);
    if(app.widgets.count(200))SetWindowTextW(app.widgets[200],message.c_str());
    if(!app.smoke) MessageBoxW(app.control,message.c_str(),L"桌面拼格",MB_OK|MB_ICONWARNING);
}
void Progress(int n,const std::wstring& message) {
    std::wstring text=L"["+std::to_wstring(n)+L"/5] "+message;
    Log(text);
    SetWindowTextW(app.progress,(L"DeskTiles 啟動進度\r\n\r\n"+text+L"\r\n\r\n輸入："+
        app.statePath.wstring()+L"\r\n輸出："+app.statePath.wstring()+L"\r\n日誌："+app.logPath.wstring()).c_str());
    UpdateWindow(app.progress);
}
Point DisplayPosition(const Tile& t) {
    if(app.dragging && app.dragSelection.count(t.id)) {
        for(const auto& old:app.dragOriginal) if(old.id==t.id) return old.position+app.dragDelta;
    }
    return t.position;
}
void Position(Tile& t) {
    auto it=app.views.find(t.id); if(it==app.views.end()) return;
    HWND window=it->second.window; auto p=DisplayPosition(t);
    POINT point{static_cast<LONG>(std::lround(p.x)),static_cast<LONG>(std::lround(p.y))};
    HWND parent=GetParent(window);
    if(parent) ScreenToClient(parent,&point);
    SetWindowPos(window,t.topmost?HWND_TOPMOST:HWND_TOP,point.x,point.y,PixelWidth(t),PixelHeight(t),
                 SWP_NOACTIVATE|SWP_SHOWWINDOW);
}
HWND DesktopHost() {
    HWND host=nullptr;
    EnumWindows([](HWND window,LPARAM target)->BOOL {
        if(FindWindowExW(window,nullptr,L"SHELLDLL_DefView",nullptr)) {
            *reinterpret_cast<HWND*>(target)=window; return FALSE;
        }
        return TRUE;
    },reinterpret_cast<LPARAM>(&host));
    return host;
}
bool SetLayer(Tile& t,bool topmost) {
    HWND window=app.views[t.id].window;
    HWND host=topmost?nullptr:DesktopHost();
    if(!topmost && !host) {
        Log(L"桌面宿主搜尋失敗：未找到 SHELLDLL_DefView。");
        EnumWindows([](HWND w,LPARAM)->BOOL {
            wchar_t klass[256]{};GetClassNameW(w,klass,256);
            if(wcsstr(klass,L"Worker") || wcsstr(klass,L"Progman")) Log(std::wstring(L"桌面候選：")+klass);
            return TRUE;
        },0);
        return false;
    }
    LONG_PTR originalStyle=GetWindowLongPtrW(window,GWL_STYLE);
    if(host) SetWindowLongPtrW(window,GWL_STYLE,(originalStyle&~WS_POPUP)|WS_CHILD);
    SetLastError(0);
    SetParent(window,host);
    DWORD parentError=GetLastError();
    if(!host) SetWindowLongPtrW(window,GWL_STYLE,(originalStyle&~WS_CHILD)|WS_POPUP);
    if(GetParent(window)!=host) {
        Log(L"SetParent 失敗，Windows 錯誤碼："+std::to_wstring(parentError));
        SetWindowLongPtrW(window,GWL_STYLE,originalStyle); return false;
    }
    t.topmost=topmost;
    SetWindowPos(window,topmost?HWND_TOPMOST:HWND_NOTOPMOST,0,0,0,0,
                 SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_FRAMECHANGED);
    Position(t); return true;
}
void LoadImage(Tile& tile) {
    auto& view=app.views[tile.id];
    if(tile.kind!=Kind::Image) {view.image.reset();view.imagePath.clear();return;}
    if(view.imagePath==tile.image && (!view.image ||
        (view.image->GetWidth()==static_cast<UINT>(PixelWidth(tile)) && view.image->GetHeight()==static_cast<UINT>(PixelHeight(tile)))))return;
    view.image.reset(); view.imagePath=tile.image; view.imageFailed=false;
    if(tile.image.empty() || tile.kind!=Kind::Image) return;
    int tileWidth=PixelWidth(tile),tileHeight=PixelHeight(tile);
    Log(L"[圖片 1/2] 輸入："+Wide(tile.image)+L"；輸出：記憶體縮圖（"+std::to_wstring(tileWidth)+L" × "+std::to_wstring(tileHeight)+L"）");
    try {
        std::unique_ptr<Bitmap> source(Bitmap::FromFile(Wide(tile.image).c_str()));
        if(!source || source->GetLastStatus()!=Ok || !source->GetWidth() || !source->GetHeight()) {
            view.imageFailed=true; return;
        }
        // Keep only a thumbnail. The source decoder and its file handle are released here.
        auto thumbnail=std::make_unique<Bitmap>(tileWidth,tileHeight,PixelFormat32bppPARGB);
        Graphics g(thumbnail.get()); g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        double scale=std::max(double(tileWidth)/source->GetWidth(),double(tileHeight)/source->GetHeight());
        int w=static_cast<int>(std::ceil(source->GetWidth()*scale));
        int h=static_cast<int>(std::ceil(source->GetHeight()*scale));
        g.DrawImage(source.get(),(tileWidth-w)/2,(tileHeight-h)/2,w,h);
        view.image=std::move(thumbnail);
        Log(L"[圖片 2/2] 縮圖完成，已釋放原始解碼器。");
    } catch(...) { view.imageFailed=true; }
}
GraphicsPath* OutlinePath(BorderStyle style,double inset,double size=Width,Shape shape=Shape::Hexagon) {
    auto outline=BorderOutline(style,inset,size,shape);std::vector<PointF> points;
    for(auto p:outline)points.push_back({static_cast<float>(p.x),static_cast<float>(p.y)});
    auto* path=new GraphicsPath();path->AddPolygon(points.data(),static_cast<int>(points.size()));return path;
}
Color BorderColor(std::uint32_t value,BYTE alpha=255) {
    return Color(alpha,BYTE((value>>16)&255),BYTE((value>>8)&255),BYTE(value&255));
}
void DrawBorder(Graphics& g,const Tile& tile) {
    const bool pixel=tile.borderStyle==BorderStyle::Pixel;
    g.SetSmoothingMode(pixel?Gdiplus::SmoothingModeNone:SmoothingModeAntiAlias);
    float width=static_cast<float>(tile.borderWidth);
    std::unique_ptr<GraphicsPath> path(OutlinePath(tile.borderStyle,width/2+2,tile.size,tile.shape));
    if(tile.borderStyle==BorderStyle::Rgb) {
        Pen glow(Color(55,130,185,255),width+4);g.DrawPath(&glow,path.get());
        auto corners=BorderOutline(BorderStyle::Solid,width/2+2,tile.size,tile.shape);
        double seconds=app.animationSecondsOverride>=0?app.animationSecondsOverride:
            std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
        double phase=ColorHue(tile.borderColor)+(tile.rgbAnimated?seconds/tile.rgbCycleSeconds:0);
        constexpr int PerEdge=8;int segments=static_cast<int>(corners.size())*PerEdge;
        for(size_t edge=0;edge<corners.size();++edge) for(int segment=0;segment<PerEdge;++segment) {
            auto a=corners[edge],b=corners[(edge+1)%corners.size()];
            double t=double(segment)/PerEdge,u=double(segment+1)/PerEdge;
            Pen led(BorderColor(Rainbow(phase+double(edge*PerEdge+segment)/segments)),width);
            led.SetStartCap(Gdiplus::LineCapRound);led.SetEndCap(Gdiplus::LineCapRound);
            g.DrawLine(&led,static_cast<float>(a.x+(b.x-a.x)*t),static_cast<float>(a.y+(b.y-a.y)*t),
                static_cast<float>(a.x+(b.x-a.x)*u),static_cast<float>(a.y+(b.y-a.y)*u));
        }
    } else {
        auto color=tile.borderColor;
        if(tile.borderStyle==BorderStyle::Breathing) {
            double seconds=app.animationSecondsOverride>=0?app.animationSecondsOverride:
                std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
            double level=BreathLevel(seconds,tile.rgbCycleSeconds,tile.rgbAnimated);
            color=(static_cast<unsigned>(std::round(((color>>16)&255)*level))<<16)|
                (static_cast<unsigned>(std::round(((color>>8)&255)*level))<<8)|static_cast<unsigned>(std::round((color&255)*level));
        }
        Pen pen(BorderColor(color),width);g.DrawPath(&pen,path.get());
    }
    if(app.selection.count(tile.id)) {
        // Selection markers stay separate so the chosen color and thickness remain visible.
        SolidBrush marker(Color(255,236,255,248));
        for(auto p:BorderOutline(BorderStyle::Solid,width+7,tile.size,tile.shape))
            g.FillRectangle(&marker,static_cast<float>(p.x-1.5),static_cast<float>(p.y-1.5),3.0f,3.0f);
    }
}
void DrawTileBody(Graphics& g,Tile& tile) {
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
    std::unique_ptr<GraphicsPath> polygon(OutlinePath(tile.borderStyle,0,tile.size,tile.shape));
    g.Clear(Color(0,0,0,0));
    SolidBrush background(BorderColor(tile.backgroundColor,static_cast<BYTE>(std::lround(255*(1-tile.backgroundTransparency/100)))));
    g.FillPath(&background,polygon.get());
    if(tile.kind==Kind::Image) {
        auto& view=app.views[tile.id];
        if(view.image) { g.SetClip(polygon.get()); g.DrawImage(view.image.get(),0,0); g.ResetClip(); }
    }
    FontFamily requested(Wide(tile.fontFamily).c_str());
    const FontFamily* family=requested.GetLastStatus()==Ok && requested.IsStyleAvailable(FontStyleRegular)?&requested:FontFamily::GenericSansSerif();
    Gdiplus::Font captionFont(family,static_cast<float>(std::clamp(tile.fontSize*.65,8.0,14.0)),FontStyleRegular,UnitPixel),
        normal(family,static_cast<float>(tile.fontSize),FontStyleRegular,UnitPixel),
        clock(family,static_cast<float>(tile.fontSize),family->IsStyleAvailable(FontStyleBold)?FontStyleBold:FontStyleRegular,UnitPixel);
    StringFormat center; center.SetAlignment(StringAlignmentCenter); center.SetLineAlignment(StringAlignmentCenter);
    center.SetTrimming(StringTrimmingEllipsisCharacter);
    SolidBrush muted(BorderColor(tile.textColor,175)),content(BorderColor(tile.textColor));
    float w=static_cast<float>(tile.size),h=static_cast<float>(TileHeight(tile.size,tile.shape));
    bool triangle=tile.shape==Shape::Triangle;
    g.SetClip(polygon.get());
    std::wstring label=L"#"+std::to_wstring(tile.id)+L"  "+(tile.topmost?L"置頂":L"桌面");
    if(tile.kind!=Kind::Image || !app.views[tile.id].image)
        g.DrawString(label.c_str(),-1,&captionFont,triangle?RectF(w*.35f,h*.32f,w*.3f,h*.14f):RectF(w*.21f,h*.1f,w*.58f,h*.17f),&center,&muted);
    if(tile.kind==Kind::Text) {
        auto text=Wide(tile.text);g.DrawString(text.c_str(),-1,&normal,triangle?RectF(w*.26f,h*.5f,w*.48f,h*.35f):RectF(w*.14f,h*.28f,w*.72f,h*.5f),&center,&content);
    } else if(tile.kind==Kind::Timer) {
        auto text=Wide(Clock(tile));g.DrawString(text.c_str(),-1,&clock,triangle?RectF(w*.23f,h*.53f,w*.54f,h*.25f):RectF(w*.07f,h*.32f,w*.86f,h*.32f),&center,&content);
        std::wstring state=tile.running?L"計時中":((tile.countdown && tile.elapsed>=tile.duration)?L"時間到":L"已暫停");
        g.DrawString(state.c_str(),-1,&captionFont,triangle?RectF(w*.18f,h*.8f,w*.64f,h*.13f):RectF(w*.18f,h*.67f,w*.64f,h*.17f),&center,&muted);
    } else if(!app.views[tile.id].image) {
        const wchar_t* hint=app.views[tile.id].imageFailed?L"圖片無法讀取\n雙擊重新選擇":L"雙擊選擇圖片";
        g.DrawString(hint,-1,&normal,triangle?RectF(w*.26f,h*.5f,w*.48f,h*.35f):RectF(w*.15f,h*.30f,w*.70f,h*.45f),&center,&content);
    }
}
Dib& RenderTile(Tile& tile) {
    auto& view=app.views[tile.id];
    if(view.surfaceDirty || !view.surface) {
        ++app.bodyBuilds;
        view.surface=std::make_unique<Bitmap>(PixelWidth(tile),PixelHeight(tile),PixelFormat32bppPARGB);
        Graphics cache(view.surface.get());DrawTileBody(cache,tile);view.surfaceDirty=false;
    }
    int w=PixelWidth(tile),h=PixelHeight(tile);
    if(!view.frame || view.frame->width!=w || view.frame->height!=h)view.frame=std::make_unique<Dib>(w,h);
    Bitmap frame(w,h,w*4,PixelFormat32bppPARGB,static_cast<BYTE*>(view.frame->bits));
    Graphics g(&frame);g.Clear(Color(0,0,0,0));g.DrawImage(view.surface.get(),0,0);DrawBorder(g,tile);
    return *view.frame;
}
void DrawTile(HDC dc,Tile& tile) {
    auto& frame=RenderTile(tile);Bitmap bitmap(frame.width,frame.height,frame.width*4,PixelFormat32bppPARGB,static_cast<BYTE*>(frame.bits));
    Graphics target(dc);target.DrawImage(&bitmap,0,0);
}
void PresentTile(Tile& tile) {
    auto& view=app.views[tile.id];if(!view.visual)return;
    auto& frame=RenderTile(tile);SIZE size{frame.width,frame.height};POINT origin{};
    SetWindowPos(view.visual,nullptr,0,0,size.cx,size.cy,SWP_NOZORDER|SWP_NOACTIVATE|SWP_SHOWWINDOW);
    BLENDFUNCTION blend{AC_SRC_OVER,0,255,AC_SRC_ALPHA};
    if(!UpdateLayeredWindow(view.visual,nullptr,nullptr,&size,frame.dc,&origin,0,&blend,ULW_ALPHA)) {
        ++app.layeredErrors;
        if(!view.presentationFailed)Error(L"透明畫面更新失敗，Windows 錯誤："+std::to_wstring(GetLastError()));
        view.presentationFailed=true;
    }else view.presentationFailed=false;
}
void Repaint() {
    for(auto& item:app.views) {item.second.surfaceDirty=true;InvalidateRect(item.second.window,nullptr,FALSE);}
}
void UpdateTileRegion(Tile& tile) {
    auto shape=BorderOutline(tile.borderStyle,0,tile.size,tile.shape);std::vector<POINT> points;
    for(auto p:shape)points.push_back({LONG(std::lround(p.x)),LONG(std::lround(p.y))});
    SetWindowRgn(app.views[tile.id].window,CreatePolygonRgn(points.data(),static_cast<int>(points.size()),WINDING),TRUE);
}
void CreateTileWindow(Tile& t) {
    app.views[t.id].surfaceDirty=true;
    auto title=L"DeskTiles · "+ShapeName(t.shape);
    HWND window=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOREDIRECTIONBITMAP,TileClass,title.c_str(),WS_POPUP,
        0,0,PixelWidth(t),PixelHeight(t),nullptr,nullptr,app.instance,reinterpret_cast<void*>(static_cast<INT_PTR>(t.id)));
    if(!window) throw std::runtime_error("建立圖形視窗失敗");
    app.views[t.id].window=window;
    app.views[t.id].visual=CreateWindowExW(WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE,VisualClass,L"",WS_CHILD|WS_VISIBLE,
        0,0,PixelWidth(t),PixelHeight(t),window,nullptr,app.instance,nullptr);
    if(!app.views[t.id].visual)throw std::runtime_error("無法建立透明顯示層");
    UpdateTileRegion(t);
    LoadImage(t);
    if(!SetLayer(t,t.topmost)) {
        t.topmost=true; SetLayer(t,true);
        Error(L"桌面宿主無法使用，此格暫以置頂顯示。可從右鍵重試固定於桌面層。");
    }
    PresentTile(t);
}
void SaveState() {
    if(app.exiting) return;
    try {
        auto temporary=app.directory/L"layout.tmp";
        { std::ofstream out(temporary,std::ios::binary|std::ios::trunc); Save(out,app.tiles); out.close();
          if(!out) throw std::runtime_error("無法完成版面寫入"); }
        if(fs::exists(app.statePath)) {
            if(!CopyFileW(app.statePath.c_str(),(app.directory/L"layout.backup").c_str(),FALSE))
                throw std::runtime_error("無法備份版面，未覆寫原始檔案");
        }
        if(!MoveFileExW(temporary.c_str(),app.statePath.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("無法替換版面，已保留原始檔案");
        app.savingFailed=false;
    } catch(const std::exception& e) {
        if(!app.savingFailed) Error(Wide(e.what()));
        app.savingFailed=true;
    }
}
void ShowControl() {
    RefreshControls(); ShowWindow(app.control,SW_SHOWNORMAL); SetForegroundWindow(app.control);
}
void UpdateClock() {
    bool running=false;
    for(auto& t:app.tiles) if(t.kind==Kind::Timer && t.running) running=true;
    if(running && !app.sharedClock) {
        app.lastTick=std::chrono::steady_clock::now(); SetTimer(app.control,1,1000,nullptr); app.sharedClock=true;
    } else if(!running && app.sharedClock) { KillTimer(app.control,1); app.sharedClock=false; }
}
void UpdateAnimation() {
    bool needed=std::any_of(app.tiles.begin(),app.tiles.end(),[](const Tile& t){return AnimatedBorder(t);});
    if(needed && !app.animationTimer) {
        app.animationTimer=SetTimer(app.control,3,83,nullptr)!=0;
    } else if(!needed && app.animationTimer) {KillTimer(app.control,3);app.animationTimer=false;}
}
void AnimateBorders() {
    if(!app.animationTimer)return;
    ++app.animationFrames;
    for(auto& tile:app.tiles) if(AnimatedBorder(tile)) {
        auto& view=app.views[tile.id];
        if(IsWindowVisible(view.window))InvalidateRect(view.window,nullptr,FALSE);
    }
}
void TickTimers() {
    auto now=std::chrono::steady_clock::now();
    double seconds=std::chrono::duration<double>(now-app.lastTick).count(); app.lastTick=now;
    bool done=false;
    for(auto& t:app.tiles) if(t.kind==Kind::Timer && t.running) {
        if(Advance(t,seconds)) {
            done=true;
            Status(L"圖形 #"+std::to_wstring(t.id)+L"：倒數結束。");
            if(!app.smoke) MessageBeep(MB_ICONINFORMATION);
        }
        app.views[t.id].surfaceDirty=true;InvalidateRect(app.views[t.id].window,nullptr,FALSE);
    }
    UpdateClock(); if(done) SaveState();
}
void EndDrag(bool commit) {
    if(!app.dragging) return;
    Point delta=app.dragDelta;
    app.dragging=false;
    if(commit && app.moved && ValidDelta(app.dragOriginal,app.dragSelection,delta)) {
        Move(app.tiles,app.dragSelection,delta); SaveState();
        Status(L"已移動 "+std::to_wstring(app.dragSelection.size())+L" 個圖形；相對位置保持不變。");
    } else if(commit && app.moved) Status(L"位置與其他圖形重疊，已回到拖曳前位置。");
    for(auto& t:app.tiles) Position(t);
    app.dragOriginal.clear(); app.dragSelection.clear();
    if(GetCapture()) ReleaseCapture();
}
void BeginDrag(int id,Point cursor,bool shift) {
    if(shift) Select(app.selection,id,true);
    else if(!app.selection.count(id)) Select(app.selection,id,false);
    app.primary=id;
    RefreshControls(); Repaint();
    if(shift || !app.selection.count(id)) return;
    app.dragStart=cursor; app.dragDelta={}; app.dragOriginal=app.tiles;
    app.dragSelection=app.selection; app.dragging=true; app.moved=false;
    SetCapture(app.views[id].window);
}
void DragTo(Point cursor) {
    if(!app.dragging) return;
    auto desired=cursor-app.dragStart;
    if(!app.moved && std::abs(desired.x)<GetSystemMetrics(SM_CXDRAG) &&
                      std::abs(desired.y)<GetSystemMetrics(SM_CYDRAG)) return;
    app.moved=true;
    app.dragDelta=Snap(app.dragOriginal,app.dragSelection,desired);
    for(auto& t:app.tiles) if(app.dragSelection.count(t.id)) Position(t);
}
std::wstring KindName(Kind k) { return k==Kind::Text?L"文字":k==Kind::Image?L"圖片":L"計時器"; }
void UpdateContentVisibility() {
    int type=static_cast<int>(SendMessageW(app.widgets[IdType],CB_GETCURSEL,0,0));
    for(int id:{IdText,201}) ShowWindow(app.widgets[id],type==0?SW_SHOW:SW_HIDE);
    for(int id:{IdImage,IdBrowse,202}) ShowWindow(app.widgets[id],type==1?SW_SHOW:SW_HIDE);
    for(int id:{IdMode,IdSeconds,IdStart,IdReset,203,204}) ShowWindow(app.widgets[id],type==2?SW_SHOW:SW_HIDE);
}
void RefreshControls() {
    if(!app.control || app.widgets.empty()) return;
    app.loadingControls=true;
    HWND list=app.widgets[IdList]; SendMessageW(list,LB_RESETCONTENT,0,0);
    int index=0,selectedIndex=-1;
    for(auto& t:app.tiles) {
        auto label=(app.selection.count(t.id)?L"● ":L"   ")+std::wstring(L"#")+std::to_wstring(t.id)+L" "+ShapeName(t.shape)+L" · "+KindName(t.kind);
        int at=static_cast<int>(SendMessageW(list,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str())));
        SendMessageW(list,LB_SETITEMDATA,at,t.id);
        if(t.id==app.primary) selectedIndex=index;
        ++index;
    }
    SendMessageW(list,LB_SETCURSEL,selectedIndex,0);
    Tile* tile=Find(app.primary);
    if(!tile || !app.selection.count(tile->id)) tile=nullptr;
    std::wstring summary=tile ? L"編輯 #"+std::to_wstring(tile->id)+L" · 已選取 "+std::to_wstring(app.selection.size())+L" 格" : L"請先選取一個圖形";
    SetWindowTextW(app.widgets[205],summary.c_str());
    for(int id:{IdType,IdText,IdImage,IdBrowse,IdMode,IdSeconds,IdApply,IdDelete,IdStart,IdReset,IdTop,IdDesktop,IdAppearance,IdLayout,IdBackground})
        EnableWindow(app.widgets[id],tile!=nullptr);
    if(tile) {
        SendMessageW(app.widgets[IdType],CB_SETCURSEL,static_cast<int>(tile->kind),0);
        SetWindowTextW(app.widgets[IdText],Wide(tile->text).c_str());
        SetWindowTextW(app.widgets[IdImage],Wide(tile->image).c_str());
        SendMessageW(app.widgets[IdMode],CB_SETCURSEL,tile->countdown?0:1,0);
        SetWindowTextW(app.widgets[IdSeconds],std::to_wstring(static_cast<int>(tile->duration)).c_str());
        SetWindowTextW(app.widgets[IdStart],tile->running?L"暫停計時":L"開始計時");
    }
    UpdateContentVisibility();
    app.loadingControls=false;
    RefreshAppearance();
    RefreshLayout();
    RefreshBackground();
}
void AddTile(Shape shape) {
    if(app.tiles.size()>=500) { Error(L"第一版最多支援 500 格。"); return; }
    Log(L"新增圖形輸入：控制視窗／右鍵選單；輸出："+app.statePath.wstring());
    Status(L"[新增 1/3] 尋找 "+ShapeName(shape)+L" 的可用相鄰位置");
    Tile tile;tile.id=app.nextId++;tile.shape=shape;
    RECT area; SystemParametersInfoW(SPI_GETWORKAREA,0,&area,0);
    tile.position={double(area.left+80),double(area.top+100)};
    bool found=false;
    std::vector<Tile> search=app.tiles;
    if(auto* current=Find(app.primary)) {
        tile.size=current->size;
        auto at=std::find_if(search.begin(),search.end(),[&](const Tile& t){return t.id==current->id;});
        if(at!=search.end()) std::rotate(search.begin(),at,search.end());
    }
    if(app.tiles.empty()) found=true;
    for(const auto& reference:search) {
        for(auto direction:Neighbors(reference.size,tile.size,reference.shape,tile.shape)) {
            auto p=reference.position+direction;
            bool collision=false;
            for(const auto& other:app.tiles) if(Overlaps(p,other.position,tile.size,other.size,tile.shape,other.shape)) collision=true;
            RECT r{LONG(p.x),LONG(p.y),LONG(p.x+tile.size),LONG(p.y+TileHeight(tile.size,tile.shape))};
            if(!collision && MonitorFromRect(&r,MONITOR_DEFAULTTONULL)) {tile.position=p;found=true;break;}
        }
        if(found) break;
    }
    if(!found) { Error(L"附近沒有可用的位置，請先移動部分圖形。"); return; }
    Status(L"[新增 2/3] 建立圖形與互動區域");app.tiles.push_back(tile);CreateTileWindow(app.tiles.back());
    app.selection={tile.id}; app.primary=tile.id; Repaint(); RefreshControls(); SaveState();
    Status(L"[新增 3/3] 已新增 "+ShapeName(shape)+L" #"+std::to_wstring(tile.id)+(app.savingFailed?L"，儲存失敗。":L" 並儲存。"));
}
void ApplyContent() {
    auto* primary=Find(app.primary); if(!primary || app.selection.empty()) return;
    int kind=static_cast<int>(SendMessageW(app.widgets[IdType],CB_GETCURSEL,0,0));
    if(kind<0 || kind>2) return;
    auto text=Utf8(TextOf(app.widgets[IdText])); auto image=Utf8(TextOf(app.widgets[IdImage]));
    double duration=300; bool countdown=SendMessageW(app.widgets[IdMode],CB_GETCURSEL,0,0)==0;
    if(kind==2) {
        auto value=TextOf(app.widgets[IdSeconds]); size_t end=0;
        try { duration=std::stod(value,&end); } catch(...) {duration=0;}
        if(end!=value.size() || !std::isfinite(duration) || duration<1 || duration>8640000) {
            Error(L"請輸入 1 到 8640000 之間的秒數。"); return;
        }
    }
    if(kind==1) {
        if(image.empty() || !fs::is_regular_file(fs::u8path(image))) { Error(L"請先選擇存在的圖片檔案。"); return; }
        image=fs::absolute(fs::u8path(image)).u8string();
        std::unique_ptr<Bitmap> probe(Bitmap::FromFile(Wide(image).c_str()));
        if(!probe || probe->GetLastStatus()!=Ok) { Error(L"圖片無法讀取，請選擇 PNG、JPEG 或 BMP。"); return; }
    }
    if(app.sharedClock) TickTimers();
    for(auto& t:app.tiles) if(app.selection.count(t.id)) {
        bool reset= t.kind!=Kind::Timer || t.duration!=duration || t.countdown!=countdown;
        t.kind=static_cast<Kind>(kind); t.text=text;
        if(kind==1) {t.image=image;app.views[t.id].imagePath.clear();LoadImage(t);}
        else {app.views[t.id].image.reset(); app.views[t.id].imagePath.clear();}
        if(kind==2) {t.duration=duration;t.countdown=countdown;if(reset){t.elapsed=0;t.running=false;t.notified=false;}}
        else t.running=false;
    }
    UpdateClock(); SaveState(); RefreshControls(); Repaint();
    Status(L"已套用至 "+std::to_wstring(app.selection.size())+L" 個圖形。");
}
void ToggleTimer() {
    if(app.sharedClock) TickTimers();
    bool anyPaused=false;
    for(auto& t:app.tiles) if(app.selection.count(t.id) && t.kind==Kind::Timer && !t.running) anyPaused=true;
    for(auto& t:app.tiles) if(app.selection.count(t.id) && t.kind==Kind::Timer) {
        if(anyPaused && t.countdown && t.elapsed>=t.duration) {t.elapsed=0;t.notified=false;}
        t.running=anyPaused;
    }
    UpdateClock(); SaveState(); RefreshControls(); Repaint();
}
void DeleteSelected() {
    if(app.selection.empty()) return;
    auto message=L"確定刪除選取的 "+std::to_wstring(app.selection.size())+L" 個圖形？";
    if(!app.smoke && MessageBoxW(app.control,message.c_str(),L"刪除圖形",MB_YESNO|MB_ICONQUESTION)!=IDYES) return;
    for(auto id:app.selection) {DestroyWindow(app.views[id].window);app.views.erase(id);}
    app.tiles.erase(std::remove_if(app.tiles.begin(),app.tiles.end(),[](const Tile& t){return app.selection.count(t.id)!=0;}),app.tiles.end());
    app.selection.clear(); app.primary=0; UpdateClock();UpdateAnimation(); RefreshControls(); SaveState();
}
void ChangeSelectedLayer(bool top) {
    int failed=0;
    for(auto& t:app.tiles) if(app.selection.count(t.id) && !SetLayer(t,top)) ++failed;
    Repaint(); SaveState();
    if(failed) {
        Status(L"層級切換："+std::to_wstring(app.selection.size()-failed)+L" 格完成，"+std::to_wstring(failed)+L" 格保留原設定。");
        Error(L"部分格子無法切換層級，已保留原本設定。請確認 Windows Explorer 正在執行。");
    } else Status(top?L"選取圖形已設為永遠置頂。":L"選取圖形已固定於桌面層。");
}
void AddTray() {
    app.tray={sizeof(NOTIFYICONDATAW)};app.tray.hWnd=app.control;app.tray.uID=1;
    app.tray.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP; app.tray.uCallbackMessage=TrayMessage;
    app.tray.hIcon=app.smallIcon; wcscpy_s(app.tray.szTip,L"桌面拼格 · DeskTiles");
    Shell_NotifyIconW(NIM_ADD,&app.tray);
}
void Popup(HWND window,int id,POINT point,bool trayMenu=false) {
    EndDrag(false);
    if(!trayMenu && id) {
        if(!app.selection.count(id)) app.selection={id};
        app.primary=id; RefreshControls(); Repaint();
    }
    HMENU menu=CreatePopupMenu();
    AppendMenuW(menu,MF_STRING,IdEdit,L"開啟控制視窗");
    HMENU shapes=CreatePopupMenu();
    AppendMenuW(shapes,MF_STRING,IdAddHex,L"六角形（預設）");AppendMenuW(shapes,MF_STRING,IdAddTriangle,L"三角形");
    AppendMenuW(shapes,MF_STRING,IdAddRectangle,L"矩形（3:2）");AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(shapes),L"新增相鄰圖形");
    AppendMenuW(menu,MF_STRING,IdSelectAll,L"選取全部");
    AppendMenuW(menu,MF_STRING,IdExport,L"匯出整體配置（含圖片）…");
    AppendMenuW(menu,MF_STRING,IdImportReplace,L"匯入 JSON：覆蓋配置…");
    AppendMenuW(menu,MF_STRING,IdImportMerge,L"匯入 JSON：合併配置…");
    if(!trayMenu && !app.selection.empty()) {
        bool allTop=true,allDesktop=true,hasTimer=false;
        for(const auto& t:app.tiles) if(app.selection.count(t.id)) {
            allTop&=t.topmost; allDesktop&=!t.topmost; hasTimer|=t.kind==Kind::Timer;
        }
        AppendMenuW(menu,MF_SEPARATOR,0,nullptr);
        AppendMenuW(menu,MF_STRING,IdAppearance,L"邊框顏色、厚度與風格…");
        AppendMenuW(menu,MF_STRING,IdLayout,L"圖形大小與文字樣式…");
        AppendMenuW(menu,MF_STRING,IdBackground,L"背景顏色與透明度…");
        if(!allTop && !allDesktop) AppendMenuW(menu,MF_STRING|MF_DISABLED,0,L"顯示層級：混合狀態");
        AppendMenuW(menu,MF_STRING|(allTop?MF_CHECKED:0),IdTop,L"顯示於最上層");
        AppendMenuW(menu,MF_STRING|(allDesktop?MF_CHECKED:0),IdDesktop,L"固定於桌面層");
        if(hasTimer) {
            AppendMenuW(menu,MF_SEPARATOR,0,nullptr);
            AppendMenuW(menu,MF_STRING,IdStart,L"開始／暫停選取的計時器");
            AppendMenuW(menu,MF_STRING,IdReset,L"重設選取的計時器");
        }
        AppendMenuW(menu,MF_SEPARATOR,0,nullptr);
        AppendMenuW(menu,MF_STRING,IdDelete,L"刪除選取圖形…");
    }
    AppendMenuW(menu,MF_SEPARATOR,0,nullptr);
    AppendMenuW(menu,MF_STRING,IdShow,L"找回全部圖形（移至主螢幕並置頂）");
    AppendMenuW(menu,MF_STRING,IdExit,L"儲存並結束桌面拼格");
    // Shell child windows cannot reliably own a foreground context menu; use the app controller.
    SetForegroundWindow(app.control);
    int result=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,point.x,point.y,0,app.control,nullptr);
    DestroyMenu(menu); PostMessageW(window,WM_NULL,0,0);
    if(result) Command(result);
}
void Command(int id) {
    switch(id) {
    case IdEdit:ShowControl();break;
    case IdAppearance:ShowAppearance();break;
    case IdLayout:ShowLayout();break;
    case IdBackground:ShowBackground();break;
    case IdExport:ChooseExport();break;
    case IdImportReplace:ChooseImport(false);break;
    case IdImportMerge:ChooseImport(true);break;
    case IdConfig: {
        RECT r;GetWindowRect(app.widgets[IdConfig],&r);HMENU menu=CreatePopupMenu();
        AppendMenuW(menu,MF_STRING,IdExport,L"匯出整體配置（含圖片）…");
        AppendMenuW(menu,MF_STRING,IdImportReplace,L"匯入 JSON：覆蓋配置…");
        AppendMenuW(menu,MF_STRING,IdImportMerge,L"匯入 JSON：合併配置…");
        int chosen=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,r.left,r.bottom,0,app.control,nullptr);
        DestroyMenu(menu);if(chosen)Command(chosen);break;
    }
    case IdAdd: {
        auto shape=SendMessageW(app.widgets[IdNewShape],CB_GETCURSEL,0,0);
        AddTile(shape>=0&&shape<=2?static_cast<Shape>(shape):Shape::Hexagon);break;
    }
    case IdAddHex:AddTile(Shape::Hexagon);break;
    case IdAddTriangle:AddTile(Shape::Triangle);break;
    case IdAddRectangle:AddTile(Shape::Rectangle);break;
    case IdApply:ApplyContent();break;
    case IdDelete:DeleteSelected();break;
    case IdTop:ChangeSelectedLayer(true);break;
    case IdDesktop:ChangeSelectedLayer(false);break;
    case IdStart:ToggleTimer();break;
    case IdReset:
        if(app.sharedClock) TickTimers();
        for(auto& t:app.tiles) if(app.selection.count(t.id) && t.kind==Kind::Timer) {t.elapsed=0;t.running=false;t.notified=false;}
        UpdateClock();SaveState();RefreshControls();Repaint();break;
    case IdSelectAll:
        app.selection.clear();for(auto& t:app.tiles) app.selection.insert(t.id);
        if(!Find(app.primary) && !app.tiles.empty()) app.primary=app.tiles.front().id;
        RefreshControls();Repaint();break;
    case IdBrowse: {
        wchar_t path[32768]{}; OPENFILENAMEW ofn{sizeof(ofn)};
        ofn.hwndOwner=app.control;ofn.lpstrFile=path;ofn.nMaxFile=32768;
        ofn.lpstrFilter=L"圖片檔案\0*.png;*.jpg;*.jpeg;*.bmp\0所有檔案\0*.*\0";
        ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
        if(GetOpenFileNameW(&ofn)) SetWindowTextW(app.widgets[IdImage],fs::absolute(path).c_str());
        break;
    }
    case IdShow: {
        RECT area; SystemParametersInfoW(SPI_GETWORKAREA,0,&area,0);
        // Preserve arrangement whenever it fits; a large layout is packed into a visible grid.
        if(!app.tiles.empty()) {
            double minX=app.tiles.front().position.x,minY=app.tiles.front().position.y,maxX=minX,maxY=minY;
            int cellW=0,cellH=0;
            for(auto& t:app.tiles) {minX=std::min(minX,t.position.x);minY=std::min(minY,t.position.y);maxX=std::max(maxX,t.position.x+t.size);maxY=std::max(maxY,t.position.y+TileHeight(t.size,t.shape));cellW=std::max(cellW,PixelWidth(t)+6);cellH=std::max(cellH,PixelHeight(t)+6);}
            bool fits=maxX-minX<area.right-area.left-40 && maxY-minY<area.bottom-area.top-40;
            int i=0,columns=std::max(1,int((area.right-area.left-40)/cellW));
            for(auto& t:app.tiles) {
                t.position=fits?t.position+Point{area.left+20-minX,area.top+20-minY}:
                    Point{double(area.left+20+(i%columns)*cellW),double(area.top+20+(i/columns)*cellH)};
                SetLayer(t,true);++i;
            }
        }
        SaveState();Repaint();Status(L"已將圖形移至主螢幕並置頂。"); break;
    }
    case IdExit:
        if(app.sharedClock) TickTimers();SaveState();
        if(app.savingFailed && !app.smoke) {
            if(MessageBoxW(app.control,L"版面尚未成功儲存，仍要結束嗎？",L"儲存失敗",MB_YESNO|MB_ICONWARNING)!=IDYES) break;
        }
        app.exiting=true; PostQuitMessage(app.processExit);break;
    }
}
HWND WidgetFor(HWND parent,std::map<int,HWND>& widgets,int id,const wchar_t* klass,const wchar_t* text,DWORD style,int x,int y,int w,int h) {
    if(wcscmp(klass,L"STATIC")==0 && (style&SS_TYPEMASK)!=SS_ICON)
        h=std::max(h,wcsstr(text,L"\n")?52:26);
    HWND window=CreateWindowExW(wcscmp(klass,L"EDIT")==0?WS_EX_CLIENTEDGE:0,klass,text,
        WS_CHILD|WS_VISIBLE|style,UiWidth(x),y,UiWidth(w),h,parent,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),app.instance,nullptr);
    SendMessageW(window,WM_SETFONT,reinterpret_cast<WPARAM>(app.font),TRUE);widgets[id]=window;return window;
}
HWND Widget(int id,const wchar_t* klass,const wchar_t* text,DWORD style,int x,int y,int w,int h) {
    return WidgetFor(app.control,app.widgets,id,klass,text,style,x,y,w,h);
}
void UpdateAppearanceControls() {
    auto& fields=app.appearanceWidgets;if(fields.empty())return;
    auto style=SendMessageW(fields[IdBorderStyle],CB_GETCURSEL,0,0);bool rgb=style==2 || style==3;
    bool selected=Find(app.primary) && app.selection.count(app.primary);
    EnableWindow(fields[IdRgbSpeed],rgb && selected);EnableWindow(fields[IdAnimate],rgb && selected);
}
void RefreshAppearance() {
    if(!app.appearance || app.appearanceWidgets.empty())return;
    auto& fields=app.appearanceWidgets;Tile* tile=Find(app.primary);
    if(tile && !app.selection.count(tile->id))tile=nullptr;
    for(int id:{IdBorderStyle,IdBorderColor,IdChooseColor,IdBorderWidth,IdRgbSpeed,IdAnimate,IdApplyBorder})
        EnableWindow(fields[id],tile!=nullptr);
    bool mixed=false;
    if(tile)for(const auto& t:app.tiles)if(app.selection.count(t.id) && !SameAppearance(t,*tile))mixed=true;
    auto summary=tile?L"#"+std::to_wstring(tile->id)+L" · 選取 "+std::to_wstring(app.selection.size())+L" 格"+
        (mixed?L" · 混合設定，以此格為範本":L""):L"請先選取圖形";
    SetWindowTextW(fields[220],summary.c_str());
    if(tile) {
        SendMessageW(fields[IdBorderStyle],CB_SETCURSEL,static_cast<int>(tile->borderStyle),0);
        SetWindowTextW(fields[IdBorderColor],Wide(ColorText(tile->borderColor)).c_str());
        std::wostringstream width;width<<tile->borderWidth;SetWindowTextW(fields[IdBorderWidth],width.str().c_str());
        // Numeric seconds preserve any valid cycle length loaded from a layout.
        std::wostringstream cycle;cycle<<tile->rgbCycleSeconds;SetWindowTextW(fields[IdRgbSpeed],cycle.str().c_str());
        SendMessageW(fields[IdAnimate],BM_SETCHECK,tile->rgbAnimated?BST_CHECKED:BST_UNCHECKED,0);
    }
    UpdateAppearanceControls();
}
bool ReadNumber(HWND field,double lo,double hi,double& value) {
    auto text=TextOf(field);size_t end=0;
    try {value=std::stod(text,&end);}catch(...){return false;}
    return end==text.size() && std::isfinite(value) && value>=lo && value<=hi;
}
bool ApplyAppearance() {
    if(app.selection.empty() || !Find(app.primary))return false;
    auto& f=app.appearanceWidgets;
    Log(L"邊框設定輸入：目前選取圖形；輸出："+app.statePath.wstring());
    Status(L"[邊框 1/3] 檢查顏色、厚度與風格");
    int style=static_cast<int>(SendMessageW(f[IdBorderStyle],CB_GETCURSEL,0,0));
    std::uint32_t color=0;double width=0,cycle=0;
    if(style<0 || style>3 || !ParseColor(Utf8(TextOf(f[IdBorderColor])),color)) {
        Error(L"顏色請輸入六位十六進位，例如 #66ECC1。");return false;
    }
    if(!ReadNumber(f[IdBorderWidth],1,12,width)) {Error(L"邊框厚度請輸入 1～12 像素。");return false;}
    if(!ReadNumber(f[IdRgbSpeed],2,60,cycle)) {Error(L"燈效循環時間請輸入 2～60 秒。");return false;}
    bool animated=SendMessageW(f[IdAnimate],BM_GETCHECK,0,0)==BST_CHECKED;
    Status(L"[邊框 2/3] 套用至 "+std::to_wstring(app.selection.size())+L" 格");
    for(auto& tile:app.tiles)if(app.selection.count(tile.id)) {
        tile.borderStyle=static_cast<BorderStyle>(style);tile.borderColor=color;tile.borderWidth=width;
        tile.rgbCycleSeconds=cycle;tile.rgbAnimated=animated;UpdateTileRegion(tile);
    }
    UpdateAnimation();Repaint();SaveState();RefreshAppearance();
    auto status=app.savingFailed?L"[邊框 3/3] 已更新顯示，但儲存失敗。":L"[邊框 3/3] 外觀已更新並儲存。";
    Status(status);SetWindowTextW(f[221],status);return !app.savingFailed;
}
void ChooseFieldColor(HWND owner,HWND field) {
    static COLORREF custom[16]{};std::uint32_t color=0x66ECC1;
    ParseColor(Utf8(TextOf(field)),color);
    CHOOSECOLORW picker{sizeof(picker)};picker.hwndOwner=owner;picker.lpCustColors=custom;
    picker.rgbResult=RGB((color>>16)&255,(color>>8)&255,color&255);picker.Flags=CC_FULLOPEN|CC_RGBINIT;
    if(ChooseColorW(&picker)) {
        color=(GetRValue(picker.rgbResult)<<16)|(GetGValue(picker.rgbResult)<<8)|GetBValue(picker.rgbResult);
        SetWindowTextW(field,Wide(ColorText(color)).c_str());
    }
}
void ShowAppearance() {
    if(app.selection.empty())return;
    ShowControl();
    if(!app.appearance) {
        RECT bounds{0,0,UiWidth(446),425};DWORD style=WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU;
        AdjustWindowRect(&bounds,style,FALSE);RECT parent;GetWindowRect(app.control,&parent);
        int x=parent.left+70,y=parent.top+80;
        app.appearance=CreateWindowExW(WS_EX_DLGMODALFRAME,AppearanceClass,L"DeskTiles · 邊框與風格",style,
            x,y,bounds.right-bounds.left,bounds.bottom-bounds.top,app.control,nullptr,app.instance,nullptr);
        auto field=[](int id,const wchar_t* type,const wchar_t* text,DWORD style,int x,int y,int w,int h) {
            return WidgetFor(app.appearance,app.appearanceWidgets,id,type,text,style,x,y,w,h);
        };
        field(220,L"STATIC",L"",0,22,20,406,25);
        field(222,L"STATIC",L"邊框風格",0,22,65,110,24);
        field(IdBorderStyle,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,142,60,280,150);
        for(auto name:{L"一般實色",L"像素風",L"RGB 動態燈條",L"單色呼吸燈"})
            SendMessageW(app.appearanceWidgets[IdBorderStyle],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(name));
        field(223,L"STATIC",L"邊框顏色",0,22,112,110,24);
        field(IdBorderColor,L"EDIT",L"#485F69",WS_TABSTOP|ES_AUTOHSCROLL,142,106,157,30);
        SendMessageW(app.appearanceWidgets[IdBorderColor],EM_SETLIMITTEXT,7,0);
        field(IdChooseColor,L"BUTTON",L"選擇顏色…",WS_TABSTOP,310,104,112,34);
        field(224,L"STATIC",L"邊框厚度",0,22,159,110,24);
        field(IdBorderWidth,L"EDIT",L"2",WS_TABSTOP|ES_AUTOHSCROLL,142,153,80,30);
        field(225,L"STATIC",L"像素（1～12）",0,235,159,185,24);
        field(226,L"STATIC",L"燈效循環",0,22,206,110,24);
        field(IdRgbSpeed,L"EDIT",L"6",WS_TABSTOP|ES_AUTOHSCROLL,142,200,80,30);
        field(227,L"STATIC",L"秒／圈（2～60）",0,235,206,185,24);
        field(IdAnimate,L"BUTTON",L"播放燈效（取消後顯示靜態邊框）",WS_TABSTOP|BS_AUTOCHECKBOX,22,249,400,28);
        field(228,L"STATIC",L"RGB 沿邊框流動；呼吸燈以設定色明暗循環。\r\n僅作用於邊框，背景與內容保持原設定。",0,22,290,404,42);
        field(IdApplyBorder,L"BUTTON",L"套用至所有選取格",WS_TABSTOP|BS_DEFPUSHBUTTON,22,348,270,38);
        field(IdCloseBorder,L"BUTTON",L"關閉",WS_TABSTOP,309,348,113,38);
        field(221,L"STATIC",L"修改後按套用；可單選或多選。",SS_PATHELLIPSIS,22,397,406,23);
    }
    SendMessageW(app.appearance,WM_SETICON,ICON_SMALL,reinterpret_cast<LPARAM>(app.smallIcon));
    RefreshAppearance();ShowWindow(app.appearance,SW_SHOWNORMAL);SetForegroundWindow(app.appearance);
}
LRESULT CALLBACK AppearanceProc(HWND window,UINT message,WPARAM w,LPARAM l) {
    switch(message) {
    case WM_ERASEBKGND:{RECT r;GetClientRect(window,&r);FillRect(reinterpret_cast<HDC>(w),&r,app.background);return 1;}
    case WM_CTLCOLORSTATIC:SetBkMode(reinterpret_cast<HDC>(w),TRANSPARENT);SetTextColor(reinterpret_cast<HDC>(w),RGB(42,62,69));return reinterpret_cast<LRESULT>(app.background);
    case WM_COMMAND:
        if(LOWORD(w)==IdBorderStyle && HIWORD(w)==CBN_SELCHANGE)UpdateAppearanceControls();
        else if(HIWORD(w)==BN_CLICKED) {
            switch(LOWORD(w)) {
            case IdApplyBorder:ApplyAppearance();break;
            case IdChooseColor:ChooseFieldColor(app.appearance,app.appearanceWidgets[IdBorderColor]);break;
            case IdCloseBorder:ShowWindow(window,SW_HIDE);break;
            }
        }
        return 0;
    case WM_CLOSE:ShowWindow(window,SW_HIDE);return 0;
    }
    return DefWindowProcW(window,message,w,l);
}
void UpdateLayoutControls() {
    auto& f=app.layoutWidgets;if(f.empty())return;
    bool selected=Find(app.primary) && app.selection.count(app.primary);
    for(int id:{IdResizeCheck,IdTextCheck,IdApplyLayout})EnableWindow(f[id],selected);
    bool size=selected && SendMessageW(f[IdResizeCheck],BM_GETCHECK,0,0)==BST_CHECKED;
    bool text=selected && SendMessageW(f[IdTextCheck],BM_GETCHECK,0,0)==BST_CHECKED;
    EnableWindow(f[IdTileSize],size);
    for(int id:{IdFontFamily,IdFontSize,IdTextColor,IdChooseTextColor})EnableWindow(f[id],text);
}
void RefreshLayout() {
    if(!app.layout || app.layoutWidgets.empty())return;
    auto& f=app.layoutWidgets;auto* tile=Find(app.primary);
    if(tile && !app.selection.count(tile->id))tile=nullptr;
    bool mixed=false;
    if(tile)for(const auto& t:app.tiles)if(app.selection.count(t.id) &&
        (t.size!=tile->size || !SameTypography(t,*tile)))mixed=true;
    auto label=tile?L"#"+std::to_wstring(tile->id)+L" · 選取 "+std::to_wstring(app.selection.size())+
        L" 格"+(mixed?L" · 混合設定，以此格為範本":L""):L"請先選取圖形";
    SetWindowTextW(f[230],label.c_str());
    if(tile) {
        std::wostringstream size,font;size<<tile->size;font<<tile->fontSize;
        SetWindowTextW(f[IdTileSize],size.str().c_str());SetWindowTextW(f[IdFontSize],font.str().c_str());
        SetWindowTextW(f[IdFontFamily],Wide(tile->fontFamily).c_str());
        SetWindowTextW(f[IdTextColor],Wide(ColorText(tile->textColor)).c_str());
    }
    UpdateLayoutControls();
}
bool ApplyLayout() {
    if(app.selection.empty() || !Find(app.primary))return false;
    auto& f=app.layoutWidgets;
    bool resize=SendMessageW(f[IdResizeCheck],BM_GETCHECK,0,0)==BST_CHECKED;
    bool typography=SendMessageW(f[IdTextCheck],BM_GETCHECK,0,0)==BST_CHECKED;
    Log(L"大小與文字輸入：目前選取格；輸出："+app.statePath.wstring());
    Status(L"[大小／文字 1/3] 驗證輸入與可用空間");
    if(!resize && !typography){Error(L"請先勾選要套用的大小或文字樣式。");return false;}
    double size=0,fontSize=0;std::uint32_t color=0;std::wstring family;
    if(resize && !ReadNumber(f[IdTileSize],80,480,size)){Error(L"圖形寬度請輸入 80～480 像素。");return false;}
    if(typography) {
        family=TextOf(f[IdFontFamily]);FontFamily available(family.c_str());
        if(family.empty() || Utf8(family).size()>256 || available.GetLastStatus()!=Ok || !available.IsStyleAvailable(FontStyleRegular)) {
            Error(L"請選擇本機可用、支援一般字型的字體。");return false;
        }
        if(!ReadNumber(f[IdFontSize],8,96,fontSize)){Error(L"字級請輸入 8～96 像素。");return false;}
        if(!ParseColor(Utf8(TextOf(f[IdTextColor])),color)){Error(L"文字顏色請輸入六位色碼，例如 #FFFFFF。");return false;}
    }
    EndDrag(false);if(app.sharedClock)TickTimers();
    auto candidate=app.tiles;
    if(resize && !Resize(candidate,app.selection,app.primary,size)) {
        Error(L"調整後會與其他格子重疊。請選取整組一起調整，或先移開附近格子；設定已保留原狀。");return false;
    }
    std::set<int> resized;
    for(auto& t:candidate)if(app.selection.count(t.id)) {
        if(t.size!=Find(t.id)->size)resized.insert(t.id);
        if(typography){t.fontFamily=Utf8(family);t.fontSize=fontSize;t.textColor=color;}
    }
    Status(L"[大小／文字 2/3] 更新 "+std::to_wstring(app.selection.size())+L" 格");
    app.tiles=std::move(candidate);
    if(resize)for(auto& t:app.tiles)if(app.selection.count(t.id)) {
        Position(t);UpdateTileRegion(t);
        if(resized.count(t.id) && t.kind==Kind::Image){app.views[t.id].imagePath.clear();LoadImage(t);}
    }
    Repaint();SaveState();RefreshLayout();
    auto status=app.savingFailed?L"[大小／文字 3/3] 顯示已更新，但儲存失敗。":L"[大小／文字 3/3] 已套用並儲存。";
    Status(status);SetWindowTextW(f[231],status);return !app.savingFailed;
}
int CALLBACK CollectFonts(const LOGFONTW* font,const TEXTMETRICW*,DWORD,LPARAM data) {
    if(font->lfFaceName[0] && font->lfFaceName[0]!=L'@')
        reinterpret_cast<std::set<std::wstring>*>(data)->insert(font->lfFaceName);
    return 1;
}
void ShowLayout() {
    if(app.selection.empty())return;
    ShowControl();
    if(!app.layout) {
        RECT bounds{0,0,UiWidth(492),445};DWORD style=WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU;
        AdjustWindowRect(&bounds,style,FALSE);RECT parent;GetWindowRect(app.control,&parent);
        app.layout=CreateWindowExW(WS_EX_DLGMODALFRAME,LayoutClass,L"DeskTiles · 大小與文字",style,
            parent.left+40,parent.top+65,bounds.right-bounds.left,bounds.bottom-bounds.top,app.control,nullptr,app.instance,nullptr);
        auto field=[](int id,const wchar_t* type,const wchar_t* text,DWORD style,int x,int y,int w,int h) {
            return WidgetFor(app.layout,app.layoutWidgets,id,type,text,style,x,y,w,h);
        };
        field(230,L"STATIC",L"",0,22,20,454,26);
        field(IdResizeCheck,L"BUTTON",L"套用大小",WS_TABSTOP|BS_AUTOCHECKBOX,22,60,115,30);
        field(IdTileSize,L"EDIT",L"160",WS_TABSTOP|ES_AUTOHSCROLL,150,60,80,30);
        field(232,L"STATIC",L"寬度 px（80～480）",0,242,65,226,24);
        field(IdTextCheck,L"BUTTON",L"套用文字樣式（含計時數字）",WS_TABSTOP|BS_AUTOCHECKBOX,22,108,442,30);
        SendMessageW(app.layoutWidgets[IdTextCheck],BM_SETCHECK,BST_CHECKED,0);
        field(233,L"STATIC",L"字體",0,22,160,110,24);
        field(IdFontFamily,L"COMBOBOX",L"",WS_TABSTOP|WS_VSCROLL|CBS_DROPDOWN|CBS_AUTOHSCROLL|CBS_SORT,150,155,318,250);
        SendMessageW(app.layoutWidgets[IdFontFamily],CB_LIMITTEXT,31,0);
        field(234,L"STATIC",L"字級",0,22,207,110,24);
        field(IdFontSize,L"EDIT",L"14",WS_TABSTOP|ES_AUTOHSCROLL,150,201,80,30);
        field(235,L"STATIC",L"像素（8～96）",0,242,207,226,24);
        field(236,L"STATIC",L"文字顏色",0,22,254,110,24);
        field(IdTextColor,L"EDIT",L"#EBF4F5",WS_TABSTOP|ES_AUTOHSCROLL,150,248,170,30);
        SendMessageW(app.layoutWidgets[IdTextColor],EM_SETLIMITTEXT,7,0);
        field(IdChooseTextColor,L"BUTTON",L"選擇顏色…",WS_TABSTOP,334,246,134,34);
        field(237,L"STATIC",L"只套用勾選項目。主選取格位置保持不變。\r\n多選縮放會等比例調整位置；空間不足時保留原狀。",0,22,303,448,45);
        field(IdApplyLayout,L"BUTTON",L"套用至所有選取格",WS_TABSTOP|BS_DEFPUSHBUTTON,22,365,290,38);
        field(IdCloseLayout,L"BUTTON",L"關閉",WS_TABSTOP,330,365,138,38);
        field(231,L"STATIC",L"修改後按套用；內容文字不會被覆寫。",SS_PATHELLIPSIS,22,417,448,22);
        Status(L"[字體清單 1/2] 讀取本機已安裝字體");
        std::set<std::wstring> names;LOGFONTW query{};query.lfCharSet=DEFAULT_CHARSET;
        HDC dc=GetDC(app.layout);EnumFontFamiliesExW(dc,&query,CollectFonts,reinterpret_cast<LPARAM>(&names),0);ReleaseDC(app.layout,dc);
        for(const auto& name:names)SendMessageW(app.layoutWidgets[IdFontFamily],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(name.c_str()));
        Status(L"[字體清單 2/2] 已讀取 "+std::to_wstring(names.size())+L" 個字體。");
    }
    SendMessageW(app.layout,WM_SETICON,ICON_SMALL,reinterpret_cast<LPARAM>(app.smallIcon));
    RefreshLayout();ShowWindow(app.layout,SW_SHOWNORMAL);SetForegroundWindow(app.layout);
}
LRESULT CALLBACK LayoutProc(HWND window,UINT message,WPARAM w,LPARAM l) {
    switch(message) {
    case WM_ERASEBKGND:{RECT r;GetClientRect(window,&r);FillRect(reinterpret_cast<HDC>(w),&r,app.background);return 1;}
    case WM_CTLCOLORSTATIC:SetBkMode(reinterpret_cast<HDC>(w),TRANSPARENT);SetTextColor(reinterpret_cast<HDC>(w),RGB(42,62,69));return reinterpret_cast<LRESULT>(app.background);
    case WM_COMMAND:
        if(HIWORD(w)==BN_CLICKED)switch(LOWORD(w)) {
        case IdResizeCheck:case IdTextCheck:UpdateLayoutControls();break;
        case IdApplyLayout:ApplyLayout();break;
        case IdChooseTextColor:ChooseFieldColor(window,app.layoutWidgets[IdTextColor]);break;
        case IdCloseLayout:ShowWindow(window,SW_HIDE);break;
        }
        return 0;
    case WM_CLOSE:ShowWindow(window,SW_HIDE);return 0;
    }
    return DefWindowProcW(window,message,w,l);
}
void UpdateBackgroundControls() {
    auto& f=app.backgroundWidgets;if(f.empty())return;
    bool selected=Find(app.primary) && app.selection.count(app.primary);
    for(int id:{IdBackgroundColorCheck,IdBackgroundAlphaCheck,IdApplyBackground})EnableWindow(f[id],selected);
    bool color=selected && SendMessageW(f[IdBackgroundColorCheck],BM_GETCHECK,0,0)==BST_CHECKED;
    bool alpha=selected && SendMessageW(f[IdBackgroundAlphaCheck],BM_GETCHECK,0,0)==BST_CHECKED;
    EnableWindow(f[IdBackgroundColor],color);EnableWindow(f[IdChooseBackgroundColor],color);EnableWindow(f[IdBackgroundAlpha],alpha);
}
void RefreshBackground() {
    if(!app.backgroundControl)return;
    auto& f=app.backgroundWidgets;if(f.empty())return;auto* tile=Find(app.primary);
    if(tile && !app.selection.count(tile->id))tile=nullptr;bool mixed=false;
    if(tile)for(const auto& t:app.tiles)if(app.selection.count(t.id) &&
        (t.backgroundColor!=tile->backgroundColor || t.backgroundTransparency!=tile->backgroundTransparency))mixed=true;
    auto summary=tile?L"#"+std::to_wstring(tile->id)+L" · 選取 "+std::to_wstring(app.selection.size())+L" 格"+
        (mixed?L" · 混合設定":L""):L"請選取圖形";
    SetWindowTextW(f[240],summary.c_str());
    if(tile) {
        SetWindowTextW(f[IdBackgroundColor],Wide(ColorText(tile->backgroundColor)).c_str());
        std::wostringstream value;value<<tile->backgroundTransparency;SetWindowTextW(f[IdBackgroundAlpha],value.str().c_str());
    }
    UpdateBackgroundControls();
}
bool ApplyBackground() {
    if(app.selection.empty() || !Find(app.primary))return false;
    auto& f=app.backgroundWidgets;bool color=SendMessageW(f[IdBackgroundColorCheck],BM_GETCHECK,0,0)==BST_CHECKED;
    bool alpha=SendMessageW(f[IdBackgroundAlphaCheck],BM_GETCHECK,0,0)==BST_CHECKED;
    Log(L"背景輸入：選取圖形；輸出："+app.statePath.wstring());Status(L"[背景 1/3] 驗證設定");
    std::uint32_t value=0;double transparency=0;
    if(!color && !alpha){Error(L"請勾選要套用的項目。");return false;}
    if(color && !ParseColor(Utf8(TextOf(f[IdBackgroundColor])),value)){Error(L"背景顏色請輸入六位色碼。");return false;}
    if(alpha && !ReadNumber(f[IdBackgroundAlpha],0,100,transparency)){Error(L"背景透明度請輸入 0～100%。");return false;}
    Status(L"[背景 2/3] 套用至 "+std::to_wstring(app.selection.size())+L" 格");
    for(auto& tile:app.tiles)if(app.selection.count(tile.id)) {
        if(color)tile.backgroundColor=value;if(alpha)tile.backgroundTransparency=transparency;
    }
    Repaint();SaveState();RefreshBackground();
    auto status=app.savingFailed?L"[背景 3/3] 已更新顯示，但儲存失敗。":L"[背景 3/3] 已更新並儲存。";
    Status(status);SetWindowTextW(f[241],status);return !app.savingFailed;
}
void ShowBackground() {
    if(app.selection.empty())return;ShowControl();
    if(!app.backgroundControl) {
        RECT bounds{0,0,UiWidth(478),325};DWORD style=WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU;AdjustWindowRect(&bounds,style,FALSE);
        RECT parent;GetWindowRect(app.control,&parent);
        app.backgroundControl=CreateWindowExW(WS_EX_DLGMODALFRAME,BackgroundClass,L"DeskTiles · 背景與透明度",style,
            parent.left+40,parent.top+90,bounds.right-bounds.left,bounds.bottom-bounds.top,app.control,nullptr,app.instance,nullptr);
        auto field=[](int id,const wchar_t* type,const wchar_t* text,DWORD style,int x,int y,int w,int h) {
            return WidgetFor(app.backgroundControl,app.backgroundWidgets,id,type,text,style,x,y,w,h);
        };
        field(240,L"STATIC",L"",0,22,20,434,25);
        field(IdBackgroundColorCheck,L"BUTTON",L"套用背景色",WS_TABSTOP|BS_AUTOCHECKBOX,22,65,125,30);
        field(IdBackgroundColor,L"EDIT",L"#172730",WS_TABSTOP|ES_AUTOHSCROLL,160,65,148,30);
        field(IdChooseBackgroundColor,L"BUTTON",L"選擇顏色…",WS_TABSTOP,322,63,134,34);
        SendMessageW(app.backgroundWidgets[IdBackgroundColor],EM_SETLIMITTEXT,7,0);
        field(IdBackgroundAlphaCheck,L"BUTTON",L"套用透明度",WS_TABSTOP|BS_AUTOCHECKBOX,22,117,125,30);
        field(IdBackgroundAlpha,L"EDIT",L"0",WS_TABSTOP|ES_AUTOHSCROLL,160,117,80,30);
        field(242,L"STATIC",L"%（0 不透明，100 全透明）",0,251,122,218,25);
        field(243,L"STATIC",L"只影響背景；文字、圖片與邊框保持原樣。\r\n完全透明處仍可選取、拖曳和使用右鍵選單。",0,22,175,434,45);
        field(IdApplyBackground,L"BUTTON",L"套用至所有選取格",WS_TABSTOP|BS_DEFPUSHBUTTON,22,240,285,38);
        field(IdCloseBackground,L"BUTTON",L"關閉",WS_TABSTOP,323,240,133,38);
        field(241,L"STATIC",L"修改後按套用。",SS_PATHELLIPSIS,22,296,434,23);
        SendMessageW(app.backgroundWidgets[IdBackgroundColorCheck],BM_SETCHECK,BST_CHECKED,0);
        SendMessageW(app.backgroundWidgets[IdBackgroundAlphaCheck],BM_SETCHECK,BST_CHECKED,0);
    }
    SendMessageW(app.backgroundControl,WM_SETICON,ICON_SMALL,reinterpret_cast<LPARAM>(app.smallIcon));
    RefreshBackground();ShowWindow(app.backgroundControl,SW_SHOWNORMAL);SetForegroundWindow(app.backgroundControl);
}
LRESULT CALLBACK BackgroundProc(HWND window,UINT message,WPARAM w,LPARAM l) {
    switch(message) {
    case WM_ERASEBKGND:{RECT r;GetClientRect(window,&r);FillRect(reinterpret_cast<HDC>(w),&r,app.background);return 1;}
    case WM_CTLCOLORSTATIC:SetBkMode(reinterpret_cast<HDC>(w),TRANSPARENT);SetTextColor(reinterpret_cast<HDC>(w),RGB(42,62,69));return reinterpret_cast<LRESULT>(app.background);
    case WM_COMMAND:
        if(HIWORD(w)==BN_CLICKED)switch(LOWORD(w)) {
        case IdBackgroundColorCheck:case IdBackgroundAlphaCheck:UpdateBackgroundControls();break;
        case IdChooseBackgroundColor:ChooseFieldColor(window,app.backgroundWidgets[IdBackgroundColor]);break;
        case IdApplyBackground:ApplyBackground();break;
        case IdCloseBackground:ShowWindow(window,SW_HIDE);break;
        }return 0;
    case WM_CLOSE:ShowWindow(window,SW_HIDE);return 0;
    }
    return DefWindowProcW(window,message,w,l);
}
std::string ReadBytes(const fs::path& path,size_t limit) {
    if(!fs::is_regular_file(path) || fs::file_size(path)>limit)throw std::runtime_error("檔案不存在或超過大小上限");
    std::ifstream in(path,std::ios::binary);std::string bytes((std::istreambuf_iterator<char>(in)),{});
    if(!in || bytes.size()>limit)throw std::runtime_error("檔案讀取失敗或超過大小上限");return bytes;
}
void AtomicOutput(const fs::path& path,const std::function<void(std::ostream&)>& write) {
    auto temporary=path;temporary+=L".hexdesk-"+std::to_wstring(GetCurrentProcessId())+L".tmp";
    try {
        {std::ofstream out(temporary,std::ios::binary|std::ios::trunc);write(out);out.close();if(!out)throw std::runtime_error("無法完成檔案寫入");}
        if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("無法替換輸出檔案");
    }catch(...){std::error_code ignored;fs::remove(temporary,ignored);throw;}
}
std::string Digest(const std::string& bytes) {
    BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)throw std::runtime_error("無法建立圖片雜湊");
    BYTE result[32]{};NTSTATUS code=BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0);
    if(code>=0)code=BCryptHashData(hash,reinterpret_cast<PUCHAR>(const_cast<char*>(bytes.data())),static_cast<ULONG>(bytes.size()),0);
    if(code>=0)code=BCryptFinishHash(hash,result,32,0);
    if(hash)BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(algorithm,0);
    if(code<0)throw std::runtime_error("圖片雜湊失敗");
    std::ostringstream out;for(auto b:result)out<<std::hex<<std::setfill('0')<<std::setw(2)<<static_cast<int>(b);return out.str();
}
void ValidateAsset(const Asset& asset) {
    IStream* stream=SHCreateMemStream(reinterpret_cast<const BYTE*>(asset.bytes.data()),static_cast<UINT>(asset.bytes.size()));
    if(!stream)throw std::runtime_error("無法讀取內嵌圖片");
    bool valid=false;
    {std::unique_ptr<Bitmap> picture(Bitmap::FromStream(stream));valid=picture && picture->GetLastStatus()==Ok &&
        picture->GetWidth()>0 && picture->GetHeight()>0 && static_cast<unsigned long long>(picture->GetWidth())*picture->GetHeight()<=64000000;}
    stream->Release();if(!valid)throw std::runtime_error("內嵌圖片損壞或解析度超過 6400 萬像素");
}
bool ExportConfig(const fs::path& requested) {
    try {
        auto path=fs::absolute(requested);auto extension=path.extension().wstring();
        std::transform(extension.begin(),extension.end(),extension.begin(),[](wchar_t c){return static_cast<wchar_t>(towlower(c));});
        if(extension!=L".json")throw std::runtime_error("匯出檔案必須使用 .json 副檔名");
        Log(L"JSON 匯出輸入："+app.statePath.wstring()+L"；輸出："+path.wstring());
        Status(L"[JSON 匯出 1/3] 整理配置與圖片");if(app.sharedClock)TickTimers();
        auto bundle=Pack(app.tiles,[](const std::string& name){
            Log(L"圖片匯出輸入："+Wide(name));Asset asset;asset.bytes=ReadBytes(fs::u8path(name),ImageLimit);ValidateAsset(asset);return asset;
        });
        Status(L"[JSON 匯出 2/3] 驗證內嵌圖片與配置");auto json=BundleJson(bundle);ReadBundle(json);
        Status(L"[JSON 匯出 3/3] 寫入 "+path.wstring());
        AtomicOutput(path,[&](std::ostream& out){out<<std::setw(2)<<json<<'\n';});
        Status(L"[JSON 匯出 3/3] 已完成："+path.wstring());return true;
    }catch(const std::exception& e){Error(L"JSON 匯出失敗："+Wide(e.what()));return false;}
}
bool ImportConfig(const fs::path& requested,bool merge) {
    try {
        auto path=fs::absolute(requested);Log(L"JSON 匯入輸入："+path.wstring()+L"；輸出："+app.statePath.wstring()+L"；圖片目錄："+(app.directory/L"assets").wstring());
        Status(L"[JSON 匯入 1/4] 讀取檔案");
        if(!fs::is_regular_file(path) || fs::file_size(path)>JsonLimit)throw std::runtime_error("JSON 不存在或超過 128 MiB");
        std::ifstream in(path,std::ios::binary);auto bundle=ParseBundle(in);in.close();
        Status(L"[JSON 匯入 2/4] 驗證配置與圖片");
        std::map<std::string,fs::path> imagePaths;int missingFonts=0;
        for(const auto& item:bundle.images) {
            ValidateAsset(item.second);auto extension=item.second.mime=="image/png"?L".png":item.second.mime=="image/jpeg"?L".jpg":L".bmp";
            imagePaths[item.first]=app.directory/L"assets"/(Wide(Digest(item.second.bytes))+extension);
        }
        for(auto& t:bundle.tiles) {
            Wide(t.text);auto family=Wide(t.fontFamily);FontFamily font(family.c_str());
            if(font.GetLastStatus()!=Ok || !font.IsStyleAvailable(FontStyleRegular))++missingFonts;
            auto at=bundle.tileImages.find(t.id);if(at!=bundle.tileImages.end())t.image=imagePaths.at(at->second).u8string();
        }
        EndDrag(false);if(app.sharedClock)TickTimers();
        auto candidate=merge?Merge(app.tiles,bundle.tiles):bundle.tiles;ValidateTiles(candidate);
        auto previous=app.tiles;std::set<int> previousIds;for(const auto& t:previous)previousIds.insert(t.id);
        Status(L"[JSON 匯入 3/4] 備份並保存內嵌圖片");
        auto backup=app.directory/(L"import-before-"+std::to_wstring(GetTickCount64())+L".hexdesk");
        Log(L"匯入備份輸出："+backup.wstring());
        AtomicOutput(backup,[&](std::ostream& out){Save(out,previous);});
        fs::create_directories(app.directory/L"assets");
        for(const auto& item:bundle.images) {
            const auto& imagePath=imagePaths.at(item.first);
            Log(L"圖片還原輸入："+path.wstring()+L"（"+Wide(item.first)+L"）；輸出："+imagePath.wstring());
            if(fs::exists(imagePath)) {
                if(ReadBytes(imagePath,ImageLimit)!=item.second.bytes)throw std::runtime_error("既有圖片資源內容不符");
            }else AtomicOutput(imagePath,[&](std::ostream& out){out.write(item.second.bytes.data(),static_cast<std::streamsize>(item.second.bytes.size()));});
        }
        app.tiles=candidate;SaveState();
        if(app.savingFailed){app.tiles=previous;throw std::runtime_error("配置儲存失敗，已保留原版面");}
        auto oldViews=std::move(app.views);app.views.clear();
        try {for(auto& tile:app.tiles)CreateTileWindow(tile);}
        catch(...) {
            for(auto& view:app.views)DestroyWindow(view.second.window);app.views.clear();
            app.views=std::move(oldViews);app.tiles=previous;SaveState();throw;
        }
        for(auto& view:oldViews)DestroyWindow(view.second.window);oldViews.clear();
        app.selection.clear();for(auto& tile:app.tiles)if(!merge || !previousIds.count(tile.id))app.selection.insert(tile.id);
        app.primary=app.selection.empty()?0:*app.selection.begin();app.nextId=1;for(const auto& t:app.tiles)app.nextId=std::max(app.nextId,t.id+1);
        UpdateClock();UpdateAnimation();RefreshControls();Repaint();
        Status(L"[JSON 匯入 4/4] "+std::wstring(merge?L"合併":L"覆蓋")+L"完成；共 "+std::to_wstring(app.tiles.size())+L" 格。"+
            (missingFonts?L" 部分字體使用系統替代字體。":L""));return true;
    }catch(const std::exception& e){Error(L"JSON 匯入失敗："+Wide(e.what()));return false;}
}
void ChooseExport() {
    wchar_t name[32768]=L"DeskTiles-config.json";OPENFILENAMEW dialog{sizeof(dialog)};
    dialog.hwndOwner=app.control;dialog.lpstrFile=name;dialog.nMaxFile=32768;
    dialog.lpstrFilter=L"桌面拼格 JSON 配置\0*.json\0";dialog.lpstrDefExt=L"json";
    dialog.Flags=OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    if(GetSaveFileNameW(&dialog))ExportConfig(name);
}
void ChooseImport(bool merge) {
    wchar_t name[32768]{};OPENFILENAMEW dialog{sizeof(dialog)};
    dialog.hwndOwner=app.control;dialog.lpstrFile=name;dialog.nMaxFile=32768;
    dialog.lpstrFilter=L"桌面拼格 JSON 配置\0*.json\0";dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    if(GetOpenFileNameW(&dialog)) {
        if(!merge && MessageBoxW(app.control,L"以選取的 JSON 覆蓋目前整體配置？會先備份目前版面。尚未套用的編輯請先套用。",L"匯入：覆蓋配置",MB_YESNO|MB_ICONQUESTION)!=IDYES)return;
        ImportConfig(name,merge);
    }
}
void CreateControls() {
    app.titleFont=CreateFontW(-32,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    SendMessageW(Widget(215,L"STATIC",L"",SS_ICON|SS_REALSIZEIMAGE,26,13,52,64),STM_SETICON,reinterpret_cast<WPARAM>(app.brandIcon),0);
    SendMessageW(Widget(210,L"STATIC",L"DESKTILES",0,92,23,350,42),WM_SETFONT,reinterpret_cast<WPARAM>(app.titleFont),TRUE);
    Widget(211,L"STATIC",L"桌面上的一小格，留給重要的事。",0,28,80,580,26);
    Widget(IdConfig,L"BUTTON",L"配置 JSON…",WS_TABSTOP,475,25,142,36);
    Widget(212,L"STATIC",L"桌面圖形",0,26,115,160,24);
    Widget(IdList,L"LISTBOX",L"",WS_TABSTOP|WS_VSCROLL|LBS_NOTIFY|LBS_NOINTEGRALHEIGHT|WS_BORDER,26,147,175,207);
    Widget(IdNewShape,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,26,366,175,160);
    for(auto name:{L"六角形（預設）",L"三角形",L"矩形（3:2）"})SendMessageW(app.widgets[IdNewShape],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(name));
    SendMessageW(app.widgets[IdNewShape],CB_SETCURSEL,0,0);
    Widget(IdAdd,L"BUTTON",L"＋ 新增圖形",WS_TABSTOP,26,408,175,34);
    Widget(IdAppearance,L"BUTTON",L"邊框與風格…",WS_TABSTOP,26,449,175,34);
    Widget(IdLayout,L"BUTTON",L"大小與文字…",WS_TABSTOP,26,490,175,34);
    Widget(IdBackground,L"BUTTON",L"背景與透明度…",WS_TABSTOP,26,531,175,34);
    Widget(IdDelete,L"BUTTON",L"刪除選取…",WS_TABSTOP,26,572,175,34);
    Widget(205,L"STATIC",L"請先選取圖形",0,227,115,390,25);
    Widget(IdType,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,227,150,390,180);
    for(auto label:{L"文字",L"圖片",L"計時器"}) SendMessageW(app.widgets[IdType],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label));
    SendMessageW(app.widgets[IdType],CB_SETCURSEL,0,0);
    Widget(201,L"STATIC",L"文字內容",0,227,200,390,23);
    Widget(IdText,L"EDIT",L"",WS_TABSTOP|WS_VSCROLL|ES_MULTILINE|ES_AUTOVSCROLL|ES_WANTRETURN,227,232,390,190);
    SendMessageW(app.widgets[IdText],EM_SETLIMITTEXT,16000,0);
    Widget(202,L"STATIC",L"圖片絕對路徑 · 等比例裁切填滿",0,227,200,390,23);
    Widget(IdImage,L"EDIT",L"",WS_TABSTOP|ES_AUTOHSCROLL,227,232,390,32);
    Widget(IdBrowse,L"BUTTON",L"瀏覽圖片…",WS_TABSTOP,227,278,150,36);
    Widget(203,L"STATIC",L"計時方式",0,227,200,390,23);
    Widget(IdMode,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,227,232,390,130);
    for(auto label:{L"倒數計時",L"正向計時"}) SendMessageW(app.widgets[IdMode],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label));
    Widget(204,L"STATIC",L"倒數總秒數（正向計時不受此值限制）",0,227,280,390,24);
    Widget(IdSeconds,L"EDIT",L"300",WS_TABSTOP|ES_AUTOHSCROLL,227,313,390,32);
    SendMessageW(app.widgets[IdSeconds],EM_SETLIMITTEXT,12,0);
    Widget(IdStart,L"BUTTON",L"開始計時",WS_TABSTOP,227,366,185,38);
    Widget(IdReset,L"BUTTON",L"重設計時",WS_TABSTOP,432,366,185,38);
    Widget(IdApply,L"BUTTON",L"套用至所有選取格",WS_TABSTOP|BS_DEFPUSHBUTTON,227,445,390,42);
    Widget(IdTop,L"BUTTON",L"選取格置頂",WS_TABSTOP,227,502,185,38);
    Widget(IdDesktop,L"BUTTON",L"選取格固定於桌面",WS_TABSTOP,432,502,185,38);
    Widget(213,L"STATIC",L"Shift＋點擊多選 · 拖曳整組 · 靠近自動吸附\r\n雙擊編輯 · Esc 取消選取 · 關閉本視窗後仍會繼續顯示",0,227,552,395,48);
    Widget(214,L"STATIC",(L"資料位置："+app.directory.wstring()).c_str(),SS_PATHELLIPSIS,26,615,590,25);
    Widget(200,L"STATIC",L"準備就緒",SS_PATHELLIPSIS,26,648,590,25);
}
void EscapeSelection() {
    EndDrag(false);app.selection.clear();app.primary=0;Repaint();RefreshControls();
}
HICON CreateAppIcon(int size=32) {
    HICON icon=static_cast<HICON>(LoadImageW(app.instance,MAKEINTRESOURCEW(101),IMAGE_ICON,size,size,0));
    if(!icon)throw std::runtime_error("無法載入黑白像素 Logo 資源");return icon;
}
LRESULT CALLBACK TileProc(HWND window,UINT message,WPARAM w,LPARAM l) {
    if(message==WM_NCCREATE) {
        auto* create=reinterpret_cast<CREATESTRUCTW*>(l);
        SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    }
    int id=static_cast<int>(GetWindowLongPtrW(window,GWLP_USERDATA));
    switch(message) {
    case WM_ERASEBKGND:return 1;
    case WM_PAINT: {
        ++app.paintCount;
        PAINTSTRUCT paint;HDC dc=BeginPaint(window,&paint);
        (void)dc;if(auto* tile=Find(id))PresentTile(*tile);EndPaint(window,&paint);return 0;
    }
    case WM_PRINTCLIENT:if(auto* tile=Find(id)) DrawTile(reinterpret_cast<HDC>(w),*tile);return 0;
    case WM_LBUTTONDOWN: {
        POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)};ClientToScreen(window,&p);
        SetFocus(window);BeginDrag(id,{double(p.x),double(p.y)},(w&MK_SHIFT)!=0);return 0;
    }
    case WM_MOUSEMOVE:if(app.dragging) {
        POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)};ClientToScreen(window,&p);
        DragTo({double(p.x),double(p.y)});return 0;
    }break;
    case WM_LBUTTONUP: {
        bool clicked=app.dragging && !app.moved;
        EndDrag(true);
        if(clicked && app.selection.size()>1 && !(w&MK_SHIFT)) {
            Select(app.selection,id,false);app.primary=id;RefreshControls();Repaint();
        }
        return 0;
    }
    case WM_CAPTURECHANGED:EndDrag(false);return 0;
    case WM_CANCELMODE:EndDrag(false);return 0;
    case WM_LBUTTONDBLCLK:EndDrag(false);app.selection={id};app.primary=id;ShowControl();Repaint();return 0;
    case WM_CONTEXTMENU: {
        POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)};if(p.x==-1) GetCursorPos(&p);
        Popup(window,id,p);return 0;
    }
    case WM_KEYDOWN:
        if(w==VK_ESCAPE) EscapeSelection();
        if(w==VK_F2) ShowControl();
        return 0;
    case WM_CLOSE:return 0;
    }
    return DefWindowProcW(window,message,w,l);
}
void SmokeTick();
LRESULT CALLBACK ControlProc(HWND window,UINT message,WPARAM w,LPARAM l) {
    if(message==app.taskbarCreated && app.taskbarCreated) {
        AddTray();
        for(auto& t:app.tiles) if(!IsWindow(app.views[t.id].window)) CreateTileWindow(t);
        return 0;
    }
    switch(message) {
    case WM_ERASEBKGND: {RECT r;GetClientRect(window,&r);FillRect(reinterpret_cast<HDC>(w),&r,app.background);return 1;}
    case WM_CTLCOLORSTATIC:SetBkMode(reinterpret_cast<HDC>(w),TRANSPARENT);SetTextColor(reinterpret_cast<HDC>(w),RGB(42,62,69));return reinterpret_cast<LRESULT>(app.background);
    case WM_COMMAND:
        if(LOWORD(w)==IdList && HIWORD(w)==LBN_SELCHANGE) {
            int at=static_cast<int>(SendMessageW(app.widgets[IdList],LB_GETCURSEL,0,0));
            if(at!=LB_ERR) {
                int id=static_cast<int>(SendMessageW(app.widgets[IdList],LB_GETITEMDATA,at,0));
                Select(app.selection,id,(GetKeyState(VK_SHIFT)&0x8000)!=0);app.primary=id;RefreshControls();Repaint();
            }
        } else if(LOWORD(w)==IdType && HIWORD(w)==CBN_SELCHANGE) UpdateContentVisibility();
        else if(HIWORD(w)==BN_CLICKED) Command(LOWORD(w));
        return 0;
    case WM_TIMER:if(w==1) TickTimers();else if(w==2) SmokeTick();else if(w==3) AnimateBorders();return 0;
    case TrayMessage:
        if(l==WM_LBUTTONDBLCLK || l==WM_LBUTTONUP) ShowControl();
        else if(l==WM_RBUTTONUP) {POINT p;GetCursorPos(&p);Popup(window,0,p,true);}return 0;
    case WM_KEYDOWN:if(w==VK_ESCAPE) EscapeSelection();return 0;
    case WM_CLOSE:ShowWindow(window,SW_HIDE);return 0;
    case WM_QUERYENDSESSION:return TRUE;
    case WM_ENDSESSION:if(w){if(app.sharedClock)TickTimers();SaveState();}return 0;
    }
    return DefWindowProcW(window,message,w,l);
}

void SmokeCheck(bool condition,const std::wstring& message) {
    ++app.smokeChecks;
    if(!condition) throw std::runtime_error(Utf8(L"整合測試失敗："+message));
    Log(L"PASS "+message);
}
unsigned long long FileTicks(FILETIME time) {ULARGE_INTEGER i;i.LowPart=time.dwLowDateTime;i.HighPart=time.dwHighDateTime;return i.QuadPart;}
unsigned long long CpuTicks() {
    FILETIME a,b,c,d;GetProcessTimes(GetCurrentProcess(),&a,&b,&c,&d);return FileTicks(c)+FileTicks(d);
}
void CaptureWindowPng(HWND window,const fs::path& path) {
    RECT r;GetWindowRect(window,&r);int width=r.right-r.left,height=r.bottom-r.top;
    HDC screen=GetDC(nullptr),dc=CreateCompatibleDC(screen);
    HBITMAP bitmap=CreateCompatibleBitmap(screen,width,height);auto old=SelectObject(dc,bitmap);
    PrintWindow(window,dc,0);Bitmap image(bitmap,nullptr);
    CLSID png{0x557cf406,0x1a04,0x11d3,{0x9a,0x73,0x00,0x00,0xf8,0x1e,0xf3,0x2e}};
    image.Save(path.c_str(),&png);SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(nullptr,screen);
}
void CapturePreview(const std::wstring& suffix=L"") {
    RECT r;GetWindowRect(app.control,&r);int width=r.right-r.left,height=r.bottom-r.top;
    HDC screen=GetDC(nullptr),dc=CreateCompatibleDC(screen);
    HBITMAP bitmap=CreateCompatibleBitmap(screen,width,height);auto old=SelectObject(dc,bitmap);
    PrintWindow(app.control,dc,0);
    Bitmap image(bitmap,nullptr);
    CLSID png{0x557cf406,0x1a04,0x11d3,{0x9a,0x73,0x00,0x00,0xf8,0x1e,0xf3,0x2e}};
    image.Save((app.directory/(L"control-preview"+suffix+L".png")).c_str(),&png);
    SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(nullptr,screen);
    double left=0,top=0,right=0,bottom=0;
    if(!app.tiles.empty()) {left=right=app.tiles[0].position.x;top=bottom=app.tiles[0].position.y;}
    for(size_t i=0;i<std::min<size_t>(3,app.tiles.size());++i) {
        const auto& t=app.tiles[i];left=std::min(left,t.position.x);top=std::min(top,t.position.y);
        right=std::max(right,t.position.x+t.size);bottom=std::max(bottom,t.position.y+TileHeight(t.size,t.shape));
    }
    Bitmap board(static_cast<INT>(std::ceil(right-left))+80,static_cast<INT>(std::ceil(bottom-top))+80,PixelFormat32bppARGB);Graphics g(&board);g.Clear(Color(255,233,239,237));
    HDC target=g.GetHDC();
    int i=0;
    for(auto& t:app.tiles) {
        if(i>=3) break;
        auto& frame=RenderTile(t);
        int x=40+static_cast<int>(std::lround(t.position.x-left)),y=40+static_cast<int>(std::lround(t.position.y-top));
        auto outline=BorderOutline(t.borderStyle,0,t.size,t.shape);std::vector<POINT> polygon;
        for(auto p:outline)polygon.push_back({x+LONG(std::lround(p.x)),y+LONG(std::lround(p.y))});
        HRGN clip=CreatePolygonRgn(polygon.data(),static_cast<int>(polygon.size()),WINDING);int state=SaveDC(target);SelectClipRgn(target,clip);
        BLENDFUNCTION blend{AC_SRC_OVER,0,255,AC_SRC_ALPHA};
        AlphaBlend(target,x,y,PixelWidth(t),PixelHeight(t),frame.dc,0,0,PixelWidth(t),PixelHeight(t),blend);
        RestoreDC(target,state);DeleteObject(clip);++i;
    }
    g.ReleaseHDC(target);
    board.Save((app.directory/(L"tiles-preview"+suffix+L".png")).c_str(),&png);
}
void SmokeNewFeatures() {
    Status(L"[新增功能驗證 1/4] 背景與透明互動");
    app.selection={app.tiles[0].id,app.tiles[1].id};app.primary=app.tiles[0].id;RefreshControls();
    auto originalText=app.tiles[0].text,originalImage=app.tiles[1].image;
    ShowBackground();SmokeCheck(IsWindowVisible(app.backgroundControl),L"開啟背景與透明度視窗");
    auto& background=app.backgroundWidgets;
    SetWindowTextW(background[IdBackgroundColor],L"#304050");SetWindowTextW(background[IdBackgroundAlpha],L"50");
    SmokeCheck(ApplyBackground() && app.tiles[0].backgroundColor==0x304050 &&
        app.tiles[1].backgroundTransparency==50,L"多選背景顏色與透明度套用");
    auto& textTile=app.tiles[0];int id=textTile.id;
    auto pixel=[](const Dib& dib,int x,int y){return static_cast<const DWORD*>(dib.bits)[y*dib.width+x];};
    int x=PixelWidth(textTile)/2,y=static_cast<int>(PixelHeight(textTile)*.88);
    auto half=pixel(RenderTile(textTile),x,y);
    SmokeCheck((half>>24)==128,L"背景半透明使用獨立 alpha");
    auto imagePixel=pixel(RenderTile(app.tiles[1]),PixelWidth(app.tiles[1])/2,PixelHeight(app.tiles[1])/2);
    SetWindowTextW(background[IdBackgroundAlpha],L"100");
    SmokeCheck(ApplyBackground() && app.tiles[0].backgroundTransparency==100 && app.tiles[1].backgroundTransparency==100,
        L"背景可批次完全透明");
    auto& clearFrame=RenderTile(textTile);
    SmokeCheck(pixel(clearFrame,x,y)==0,L"完全透明背景為真正 alpha 0");
    bool opaqueText=false,opaqueBorder=false;
    for(int py=clearFrame.height/3;py<clearFrame.height*3/4;++py)
        for(int px=clearFrame.width/4;px<clearFrame.width*3/4;++px)
            opaqueText|=pixel(clearFrame,px,py)==(0xFF000000|textTile.textColor);
    for(int py=0;py<12;++py)opaqueBorder|=(pixel(clearFrame,x,py)>>24)==255;
    SmokeCheck(opaqueText && opaqueBorder,L"透明背景不降低文字與邊框不透明度");
    SmokeCheck(pixel(RenderTile(app.tiles[1]),PixelWidth(app.tiles[1])/2,PixelHeight(app.tiles[1])/2)==imagePixel &&
        (imagePixel>>24)==255 && textTile.text==originalText && app.tiles[1].image==originalImage,L"背景透明保留圖片原色與內容");
    SetWindowTextW(background[IdBackgroundAlpha],L"101");
    SmokeCheck(!ApplyBackground() && textTile.backgroundTransparency==100,L"拒絕無效透明度並保留設定");
    RefreshBackground();
    SendMessageW(background[IdBackgroundAlphaCheck],BM_SETCHECK,BST_UNCHECKED,0);
    SetWindowTextW(background[IdBackgroundColor],L"#243C50");
    SmokeCheck(ApplyBackground() && textTile.backgroundColor==0x243C50 && textTile.backgroundTransparency==100,L"背景色可單獨套用而保留透明度");
    SendMessageW(background[IdBackgroundAlphaCheck],BM_SETCHECK,BST_CHECKED,0);RefreshBackground();
    UpdateWindow(app.backgroundControl);CaptureWindowPng(app.backgroundControl,app.directory/L"background-preview.png");

    HWND host=app.views[id].window;RECT bounds;GetWindowRect(host,&bounds);
    struct TestWindow {HWND value;~TestWindow(){if(value)DestroyWindow(value);}} underlay{
        CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,L"STATIC",L"",WS_POPUP|SS_WHITERECT,
            bounds.left,bounds.top,bounds.right-bounds.left,bounds.bottom-bounds.top,nullptr,nullptr,app.instance,nullptr)};
    SmokeCheck(underlay.value!=nullptr,L"建立透明合成驗證底板");
    SetWindowPos(underlay.value,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW);
    UpdateWindow(underlay.value);Position(textTile);PresentTile(textTile);UpdateWindow(host);DwmFlush();
    POINT hit{bounds.left+x,bounds.top+y};
    HDC screen=GetDC(nullptr);auto color=GetPixel(screen,hit.x,hit.y);ReleaseDC(nullptr,screen);
    Log(L"透明合成實測：背景像素="+std::to_wstring(color)+L"；命中 HWND="+std::to_wstring(reinterpret_cast<UINT_PTR>(WindowFromPoint(hit)))+
        L"；預期 HWND="+std::to_wstring(reinterpret_cast<UINT_PTR>(host)));
    SmokeCheck(color==RGB(255,255,255),L"實際桌面合成完全顯示下方白底");
    SmokeCheck(WindowFromPoint(hit)==host,L"完全透明空白區仍命中六角形互動視窗");
    DestroyWindow(underlay.value);underlay.value=nullptr;
    app.selection.clear();SendMessageW(host,WM_LBUTTONDOWN,0,MAKELPARAM(x,y));SendMessageW(host,WM_LBUTTONUP,0,MAKELPARAM(x,y));
    SmokeCheck(app.selection==std::set<int>{id},L"透明空白區的視窗訊息可單選");
    app.selection.clear();for(const auto& t:app.tiles)app.selection.insert(t.id);
    auto originalPosition=textTile.position,relative=app.tiles[1].position-textTile.position;
    SendMessageW(host,WM_LBUTTONDOWN,0,MAKELPARAM(x,y));SendMessageW(host,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(x+30,y+20));
    SendMessageW(host,WM_LBUTTONUP,0,MAKELPARAM(x+30,y+20));
    auto movedRelative=app.tiles[1].position-app.tiles[0].position;
    SmokeCheck(std::abs(app.tiles[0].position.x-originalPosition.x-30)<.001 &&
        std::abs(relative.x-movedRelative.x)<.001 && std::abs(relative.y-movedRelative.y)<.001,L"透明空白區可整組拖曳並保持相對位置");
    SmokeCheck(SetLayer(app.tiles[0],false) && SetLayer(app.tiles[0],true) &&
        app.tiles[0].backgroundTransparency==100,L"全透明圖形可往返桌面層與置頂");

    Status(L"[新增功能驗證 2/4] 呼吸燈");
    app.primary=id;app.selection={id};RefreshControls();ShowAppearance();auto& fields=app.appearanceWidgets;
    SendMessageW(fields[IdBorderStyle],CB_SETCURSEL,3,0);SetWindowTextW(fields[IdBorderColor],L"#CC88FF");
    SetWindowTextW(fields[IdRgbSpeed],L"4");SendMessageW(fields[IdAnimate],BM_SETCHECK,BST_CHECKED,0);
    SmokeCheck(ApplyAppearance() && app.tiles[0].borderStyle==BorderStyle::Breathing && app.tiles[0].borderColor==0xCC88FF &&
        app.tiles[0].rgbCycleSeconds==4 && app.animationTimer,L"呼吸燈可從介面設定顏色與速度");
    app.animationSecondsOverride=0;auto& dimFrame=RenderTile(app.tiles[0]);
    std::vector<DWORD> dim(static_cast<const DWORD*>(dimFrame.bits),static_cast<const DWORD*>(dimFrame.bits)+dimFrame.width*dimFrame.height);
    auto builds=app.bodyBuilds;app.animationSecondsOverride=2;auto& brightFrame=RenderTile(app.tiles[0]);
    bool changed=false,bodySame=true;
    for(int py=0;py<brightFrame.height;++py)for(int px=0;px<brightFrame.width;++px) {
        auto value=pixel(brightFrame,px,py);changed|=value!=dim[py*brightFrame.width+px];
        if(px>brightFrame.width/3 && px<brightFrame.width*2/3 && py>brightFrame.height/3 && py<brightFrame.height*2/3)
            bodySame&=value==dim[py*brightFrame.width+px];
    }
    SmokeCheck(changed && bodySame && builds==app.bodyBuilds,L"呼吸亮度變化只重繪邊框並重用內容快取");
    CapturePreview(L"-breathing");UpdateWindow(app.appearance);CaptureWindowPng(app.appearance,app.directory/L"breathing-control.png");
    SendMessageW(fields[IdAnimate],BM_SETCHECK,BST_UNCHECKED,0);
    SmokeCheck(ApplyAppearance() && !app.tiles[0].rgbAnimated,L"可停止呼吸並保留指定顏色");
    SendMessageW(fields[IdAnimate],BM_SETCHECK,BST_CHECKED,0);ApplyAppearance();app.animationSecondsOverride=-1;

    Status(L"[新增功能驗證 3/4] JSON 圖片還原與合併");
    auto jsonPath=app.directory/L"配置 with 圖片.json";
    SmokeCheck(ExportConfig(jsonPath),L"匯出 UTF-8 JSON 與圖片至中文路徑");
    auto exported=ReadBytes(jsonPath,JsonLimit);auto imageBytes=ReadBytes(app.directory/L"sample.png",ImageLimit);
    SmokeCheck(exported.find("image/png")!=std::string::npos && exported.find("sample.png")==std::string::npos,L"JSON 內嵌圖片而不保存來源路徑");
    fs::rename(app.directory/L"sample.png",app.directory/L"source-image-moved.png");
    SmokeCheck(ImportConfig(jsonPath,false) && app.tiles.size()==3,L"覆蓋匯入還原整體配置");
    auto restoredPath=fs::u8path(app.tiles[1].image);
    SmokeCheck(restoredPath.parent_path()==app.directory/L"assets" && ReadBytes(restoredPath,ImageLimit)==imageBytes &&
        app.views[app.tiles[1].id].image && !app.views[app.tiles[1].id].imageFailed,L"原圖片移走後仍從 JSON 完整還原及顯示");
    SmokeCheck(app.tiles[0].backgroundTransparency==100 && app.tiles[0].borderStyle==BorderStyle::Breathing &&
        app.tiles[0].fontFamily=="Consolas" && !app.tiles[2].running,L"JSON 還原背景、呼吸、字體與暫停計時器");
    auto prior=app.tiles;std::stringstream before;Save(before,prior);
    SmokeCheck(ImportConfig(jsonPath,true) && app.tiles.size()==6 && app.selection.size()==3,L"JSON 可合併並選取新加入格");
    std::stringstream retained;Save(retained,std::vector<Tile>(app.tiles.begin(),app.tiles.begin()+3));
    bool disjoint=true;std::set<int> ids;for(const auto& t:app.tiles)ids.insert(t.id);
    for(size_t i=0;i<3;++i)for(size_t j=3;j<6;++j)disjoint&=!Overlaps(app.tiles[i].position,app.tiles[j].position,app.tiles[i].size,app.tiles[j].size);
    SmokeCheck(before.str()==retained.str() && ids.size()==6 && disjoint,L"合併保留舊格並分配新 ID 與無碰撞位置");
    auto importedOffset=app.tiles[4].position-app.tiles[3].position,sourceOffset=prior[1].position-prior[0].position;
    SmokeCheck(std::abs(importedOffset.x-sourceOffset.x)<.001 && std::abs(importedOffset.y-sourceOffset.y)<.001 &&
        app.tiles[4].image==prior[1].image,L"合併保持匯入群組相對位置並共用還原圖片");
    SmokeCheck(ImportConfig(jsonPath,false) && app.tiles.size()==3,L"再次覆蓋會取代合併版面");
    bool backedUp=false;for(const auto& file:fs::directory_iterator(app.directory))if(file.path().filename().wstring().find(L"import-before-")==0)backedUp=true;
    SmokeCheck(backedUp,L"匯入保留先前版面備份");

    Status(L"[新增功能驗證 4/4] 錯誤保護與 v4 保存");
    std::stringstream stable;Save(stable,app.tiles);auto disk=ReadBytes(app.statePath,40*1024*1024);
    auto badPath=app.directory/L"invalid.json";
    AtomicOutput(badPath,[](std::ostream& out){out<<"{broken";});
    SmokeCheck(!ImportConfig(badPath,false),L"拒絕損壞 JSON");
    auto bad=Json::parse(exported);bad["images"]["image-1"]["data"]=Base64(std::string("\x89PNG\r\n\x1a\n",8));
    AtomicOutput(badPath,[&](std::ostream& out){out<<bad;});
    SmokeCheck(!ImportConfig(badPath,true),L"拒絕損壞內嵌圖片");
    std::stringstream unchanged;Save(unchanged,app.tiles);
    SmokeCheck(stable.str()==unchanged.str() && disk==ReadBytes(app.statePath,40*1024*1024),L"無效匯入不修改記憶體或磁碟配置");
    SmokeCheck(!ExportConfig(app.statePath) && disk==ReadBytes(app.statePath,40*1024*1024),L"匯出不能覆蓋內部版面檔案");
    HANDLE locked=CreateFileW(app.statePath.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    SmokeCheck(locked!=INVALID_HANDLE_VALUE,L"建立無法替換版面檔的儲存失敗情境");
    bool failedSave=ImportConfig(jsonPath,true);CloseHandle(locked);
    std::stringstream afterFailedSave;Save(afterFailedSave,app.tiles);
    SmokeCheck(!failedSave && stable.str()==afterFailedSave.str() && disk==ReadBytes(app.statePath,40*1024*1024),L"匯入儲存失敗時保留原記憶體與磁碟配置");
    auto& running=app.tiles[2];running.countdown=false;running.elapsed=7;running.running=true;UpdateClock();
    SmokeCheck(ImportConfig(jsonPath,true) && app.tiles[2].running && app.tiles[2].elapsed>=7 && !app.tiles[5].running,
        L"合併保留既有計時器運作且匯入計時器暫停");
    SmokeCheck(ImportConfig(jsonPath,false),L"覆蓋可還原計時器配置");
    auto missingFont=Json::parse(exported);missingFont["tiles"][0]["typography"]["family"]="HexDesk-Missing-Font-987";
    AtomicOutput(badPath,[&](std::ostream& out){out<<missingFont;});
    SmokeCheck(ImportConfig(badPath,false) && app.tiles[0].fontFamily=="HexDesk-Missing-Font-987",L"匯入缺少字體時保留名稱並使用替代字體");
    SmokeCheck(ImportConfig(jsonPath,false),L"可重新還原原字體配置");
    AtomicOutput(badPath,[](std::ostream& out){out<<BundleJson(Bundle{});});
    SmokeCheck(ImportConfig(badPath,false) && app.tiles.empty() && app.views.empty() && app.selection.empty(),L"空配置覆蓋後清除圖形與選取");
    SmokeCheck(ImportConfig(jsonPath,false) && app.tiles.size()==3,L"空配置後仍可匯入完整配置");
    std::stringstream latest;Save(latest,app.tiles);auto saved=Load(latest);
    SmokeCheck(saved[0].backgroundTransparency==100 && saved[0].borderStyle==BorderStyle::Breathing &&
        SameAppearance(saved[0],app.tiles[0]),L"v4 保存背景與呼吸燈");
    SmokeCheck(app.layeredErrors==0,L"透明顯示層更新全部成功");
}
void SmokeShapes() {
    Status(L"[多形狀驗證 1/4] 名稱與新增入口");
    SmokeCheck(TextOf(app.control)==L"桌面拼格 · DeskTiles" && TextOf(app.widgets[210])==L"DESKTILES",L"正式名稱顯示為桌面拼格 DeskTiles");
    SmokeCheck(SendMessageW(app.widgets[IdNewShape],CB_GETCURSEL,0,0)==0,L"新增形狀預設仍為六角形");
    auto original=app.tiles;auto imageSource=original[1].image;
    app.primary=original[0].id;app.selection={app.primary};RefreshControls();
    SendMessageW(app.widgets[IdNewShape],CB_SETCURSEL,1,0);Command(IdAdd);
    int triangle=app.tiles.back().id;
    SmokeCheck(app.tiles.size()==4 && Find(triangle)->shape==Shape::Triangle,L"控制視窗可新增三角形");
    SendMessageW(app.widgets[IdNewShape],CB_SETCURSEL,2,0);Command(IdAdd);
    int rectangle=app.tiles.back().id;
    SmokeCheck(app.tiles.size()==5 && Find(rectangle)->shape==Shape::Rectangle,L"控制視窗可新增矩形");
    Command(IdAddHex);SmokeCheck(app.tiles.back().shape==Shape::Hexagon,L"右鍵選單命令可新增六角形");DeleteSelected();
    Command(IdAddTriangle);SmokeCheck(app.tiles.back().shape==Shape::Triangle,L"右鍵選單命令可新增三角形");DeleteSelected();
    Command(IdAddRectangle);SmokeCheck(app.tiles.back().shape==Shape::Rectangle,L"右鍵選單命令可新增矩形");DeleteSelected();
    app.selection={original[1].id,original[2].id};DeleteSelected();
    RECT work;SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);
    Point base{double(work.left+70),double(work.top+130)};
    app.tiles[0].position=base;Find(triangle)->position=base+Point{300,0};
    Find(rectangle)->position=base+Point{300,TileHeight(Find(triangle)->size,Shape::Triangle)};
    for(auto& t:app.tiles)Position(t);
    auto triWindow=app.views[triangle].window,rectWindow=app.views[rectangle].window;
    HRGN region=CreateRectRgn(0,0,0,0);GetWindowRgn(triWindow,region);
    bool triangular=!PtInRegion(region,3,3) && PtInRegion(region,PixelWidth(*Find(triangle))/2,10);DeleteObject(region);
    SmokeCheck(triangular,L"三角形真實視窗區域排除外側空白角落");
    region=CreateRectRgn(0,0,0,0);GetWindowRgn(rectWindow,region);
    bool rectangular=PtInRegion(region,3,3);DeleteObject(region);RECT bounds;GetWindowRect(rectWindow,&bounds);
    SmokeCheck(rectangular && bounds.bottom-bounds.top==PixelHeight(*Find(rectangle)) &&
        PixelHeight(*Find(rectangle))==static_cast<int>(std::ceil(Find(rectangle)->size*2/3)),L"矩形視窗區域與 3:2 比例正確");

    Status(L"[多形狀驗證 2/4] 透明互動與吸附縮放");
    app.primary=triangle;app.selection={triangle};RefreshControls();
    SendMessageW(app.widgets[IdType],CB_SETCURSEL,0,0);SetWindowTextW(app.widgets[IdText],L"三角形");ApplyContent();
    ShowBackground();SetWindowTextW(app.backgroundWidgets[IdBackgroundAlpha],L"100");ApplyBackground();
    auto& clear=RenderTile(*Find(triangle));int tx=clear.width/2,ty=static_cast<int>(clear.height*.92);
    SmokeCheck((static_cast<DWORD*>(clear.bits)[ty*clear.width+tx]>>24)==0,L"三角形空白背景可真正完全透明");
    Position(*Find(triangle));PresentTile(*Find(triangle));UpdateWindow(triWindow);DwmFlush();
    POINT hit{tx,ty};ClientToScreen(triWindow,&hit);
    SmokeCheck(WindowFromPoint(hit)==triWindow,L"全透明三角形仍接收滑鼠命中");
    app.selection.clear();
    auto hexWindow=app.views[app.tiles[0].id].window;
    SendMessageW(hexWindow,WM_LBUTTONDOWN,0,MAKELPARAM(130,190));SendMessageW(hexWindow,WM_LBUTTONUP,0,MAKELPARAM(130,190));
    SendMessageW(triWindow,WM_LBUTTONDOWN,MK_SHIFT,MAKELPARAM(tx,ty));SendMessageW(triWindow,WM_LBUTTONUP,MK_SHIFT,MAKELPARAM(tx,ty));
    SmokeCheck(app.selection.size()==2 && app.selection.count(triangle),L"不同形狀支援 Shift 多選");
    auto target=Find(rectangle)->position+Point{0,-TileHeight(Find(triangle)->size,Shape::Triangle)};
    app.selection={triangle};BeginDrag(triangle,{0,0},false);DragTo(target-Find(triangle)->position+Point{3,4});EndDrag(true);
    SmokeCheck(std::abs(Find(triangle)->position.x-target.x)<.001 && std::abs(Find(triangle)->position.y-target.y)<.001 &&
        !Overlaps(Find(triangle)->position,Find(rectangle)->position,Find(triangle)->size,Find(rectangle)->size,Shape::Triangle,Shape::Rectangle),L"三角形底邊可吸附矩形頂邊並置中對齊");
    app.selection.clear();for(const auto& t:app.tiles)app.selection.insert(t.id);app.primary=app.tiles[0].id;
    auto relative=Find(rectangle)->position-Find(triangle)->position,oldPosition=app.tiles[0].position;
    BeginDrag(app.primary,{0,0},false);DragTo({25,30});EndDrag(true);auto after=Find(rectangle)->position-Find(triangle)->position;
    SmokeCheck(std::abs(after.x-relative.x)<.001 && std::abs(after.y-relative.y)<.001 &&
        app.tiles[0].position.x==oldPosition.x+25,L"混合三種形狀可整組拖曳並維持相對位置");
    ShowLayout();auto& layout=app.layoutWidgets;SendMessageW(layout[IdResizeCheck],BM_SETCHECK,BST_CHECKED,0);
    SendMessageW(layout[IdTextCheck],BM_SETCHECK,BST_CHECKED,0);SetWindowTextW(layout[IdTileSize],L"240");
    SetWindowTextW(layout[IdFontFamily],L"Consolas");SetWindowTextW(layout[IdFontSize],L"18");SetWindowTextW(layout[IdTextColor],L"#FFE09A");
    SmokeCheck(ApplyLayout() && Find(triangle)->shape==Shape::Triangle && Find(rectangle)->shape==Shape::Rectangle &&
        std::all_of(app.tiles.begin(),app.tiles.end(),[](const Tile& t){return t.size==240 && t.fontSize==18;}),L"混合形狀可批次縮放與套用字體");
    SmokeCheck(SetLayer(*Find(triangle),false) && SetLayer(*Find(triangle),true) && SetLayer(*Find(rectangle),false) &&
        SetLayer(*Find(rectangle),true),L"三角形與矩形均可切換桌面層及置頂");

    Status(L"[多形狀驗證 3/4] 內容與燈效");
    app.primary=triangle;app.selection={triangle};RefreshControls();SendMessageW(app.widgets[IdType],CB_SETCURSEL,1,0);
    SetWindowTextW(app.widgets[IdImage],Wide(imageSource).c_str());ApplyContent();
    SmokeCheck(app.views[triangle].image && !app.views[triangle].imageFailed,L"三角形可顯示及裁切圖片");
    app.primary=rectangle;app.selection={rectangle};RefreshControls();SendMessageW(app.widgets[IdType],CB_SETCURSEL,2,0);
    SetWindowTextW(app.widgets[IdSeconds],L"600");SendMessageW(app.widgets[IdMode],CB_SETCURSEL,0,0);ApplyContent();
    ToggleTimer();app.lastTick-=std::chrono::seconds(2);TickTimers();ToggleTimer();
    SmokeCheck(!Find(rectangle)->running && Find(rectangle)->elapsed>=2,L"矩形可運行與暫停計時器");
    app.selection.clear();for(auto& t:app.tiles)app.selection.insert(t.id);app.primary=app.tiles[0].id;RefreshControls();ShowAppearance();
    auto& fields=app.appearanceWidgets;SetWindowTextW(fields[IdBorderColor],L"#66ECC1");SetWindowTextW(fields[IdRgbSpeed],L"6");
    SendMessageW(fields[IdAnimate],BM_SETCHECK,BST_CHECKED,0);
    for(int style:{1,2,3}) {
        SendMessageW(fields[IdBorderStyle],CB_SETCURSEL,style,0);bool applied=ApplyAppearance();
        for(auto& t:app.tiles)PresentTile(t);
        SmokeCheck(applied && app.layeredErrors==0,L"三種形狀套用邊框風格 "+std::to_wstring(style));
    }
    app.animationSecondsOverride=0;auto& initial=RenderTile(*Find(rectangle));
    std::vector<DWORD> first(static_cast<DWORD*>(initial.bits),static_cast<DWORD*>(initial.bits)+initial.width*initial.height);
    auto bodies=app.bodyBuilds;app.animationSecondsOverride=3;auto& next=RenderTile(*Find(rectangle));
    SmokeCheck(!std::equal(first.begin(),first.end(),static_cast<DWORD*>(next.bits)) && bodies==app.bodyBuilds,L"矩形呼吸燈只更新燈效並重用內容快取");
    app.animationSecondsOverride=3;Repaint();CapturePreview(L"-shapes");app.animationSecondsOverride=-1;

    Status(L"[多形狀驗證 4/4] 配置保存與合併");
    auto shapeJson=app.directory/L"mixed-shapes.json";SmokeCheck(ExportConfig(shapeJson),L"匯出三種形狀及圖片至 JSON v2");
    SmokeCheck(ImportConfig(shapeJson,false) && app.tiles.size()==3 && app.tiles[1].shape==Shape::Triangle &&
        app.tiles[2].shape==Shape::Rectangle,L"覆蓋匯入保留所有圖形種類");
    SmokeCheck(ImportConfig(shapeJson,true) && app.tiles.size()==6 && app.tiles[4].shape==Shape::Triangle &&
        app.tiles[5].shape==Shape::Rectangle,L"合併匯入保留混合形狀");
    bool valid=true;for(size_t i=0;i<app.tiles.size();++i)for(size_t j=i+1;j<app.tiles.size();++j) {
        const auto& a=app.tiles[i];const auto& b=app.tiles[j];valid&=!Overlaps(a.position,b.position,a.size,b.size,a.shape,b.shape);
    }
    SmokeCheck(valid,L"混合形狀合併後全版面沒有重疊");
    std::ifstream savedFile(app.statePath,std::ios::binary);auto saved=Load(savedFile);savedFile.close();
    SmokeCheck(saved.size()==6 && saved[1].shape==Shape::Triangle && saved[2].shape==Shape::Rectangle,L"v5 本機版面保存圖形種類");
    SmokeCheck(ImportConfig(app.directory/L"配置 with 圖片.json",false) && app.tiles.size()==3,L"完成多形狀測試後還原測量版面");
    SendMessageW(app.widgets[IdNewShape],CB_SETCURSEL,0,0);
}
void SmokeTick() {
    try {
        if(app.smokeStep==0) {
            KillTimer(app.control,2);
            SmokeCheck(app.tiles.size()==3,L"建立三個初始六角形");
            auto one=app.tiles[0].id,two=app.tiles[1].id;
            HWND window=app.views[one].window;
            SendMessageW(window,WM_LBUTTONDOWN,0,MAKELPARAM(80,69));
            SendMessageW(window,WM_LBUTTONUP,0,MAKELPARAM(80,69));
            SendMessageW(app.views[two].window,WM_LBUTTONDOWN,MK_SHIFT,MAKELPARAM(80,69));
            SendMessageW(app.views[two].window,WM_LBUTTONUP,MK_SHIFT,MAKELPARAM(80,69));
            SmokeCheck(app.selection.size()==2,L"Shift 多選實際視窗訊息");
            auto relative=app.tiles[1].position-app.tiles[0].position;
            auto old=app.tiles[0].position;
            BeginDrag(one,{0,0},false);DragTo({-200,210});EndDrag(true);
            auto current=app.tiles[1].position-app.tiles[0].position;
            SmokeCheck(std::abs(relative.x-current.x)<.001 && std::abs(relative.y-current.y)<.001 &&
                app.tiles[0].position.y!=old.y,L"整組拖曳並維持相對位置");
            auto target=app.tiles[2].position+Point{-120,Height/2};
            auto desired=target-app.tiles[0].position;
            BeginDrag(one,{0,0},false);DragTo(desired+Point{3,-2});EndDrag(true);
            SmokeCheck(std::abs(app.tiles[0].position.x-target.x)<.001 &&
                std::abs(app.tiles[0].position.y-target.y)<.001,L"整組拖曳靠近鄰邊自動吸附");
            auto stable=app.tiles[0].position;
            desired=app.tiles[2].position-stable;
            BeginDrag(one,{0,0},false);DragTo(desired);EndDrag(true);
            SmokeCheck(app.tiles[0].position.x==stable.x && app.tiles[0].position.y==stable.y,L"重疊放置回復原位");
            auto& first=app.tiles[0];
            SmokeCheck(SetLayer(first,false) && GetParent(window)!=nullptr,L"固定於 Explorer 桌面層");
            SmokeCheck(SetLayer(first,true) && GetParent(window)==nullptr &&
                (GetWindowLongPtrW(window,GWL_EXSTYLE)&WS_EX_TOPMOST),L"切回真正置頂視窗");
            app.selection={one};app.primary=one;RefreshControls();
            SendMessageW(app.widgets[IdType],CB_SETCURSEL,0,0);
            SetWindowTextW(app.widgets[IdText],L"今天的重點\r\n專心完成一件事");ApplyContent();
            SmokeCheck(first.text==Utf8(L"今天的重點\r\n專心完成一件事"),L"方形控制視窗套用文字");
            app.selection={one,two};RefreshControls();ApplyContent();
            SmokeCheck(app.tiles[1].text==app.tiles[0].text,L"批次套用多選內容");
            Bitmap sample(300,180,PixelFormat32bppARGB);Graphics image(&sample);image.Clear(Color(255,61,137,126));
            SolidBrush brush(Color(255,235,201,116));image.FillEllipse(&brush,90,30,120,120);
            CLSID png{0x557cf406,0x1a04,0x11d3,{0x9a,0x73,0x00,0x00,0xf8,0x1e,0xf3,0x2e}};
            auto path=app.directory/L"sample.png";sample.Save(path.c_str(),&png);
            app.primary=two;app.selection={two};RefreshControls();
            SendMessageW(app.widgets[IdType],CB_SETCURSEL,1,0);SetWindowTextW(app.widgets[IdImage],path.c_str());ApplyContent();
            SmokeCheck(app.views[two].image && !app.views[two].imageFailed,L"圖片縮圖與裁切載入");
            auto third=app.tiles[2].id;app.selection={third};app.primary=third;RefreshControls();
            SendMessageW(app.widgets[IdType],CB_SETCURSEL,2,0);SetWindowTextW(app.widgets[IdSeconds],L"1");
            SendMessageW(app.widgets[IdMode],CB_SETCURSEL,0,0);ApplyContent();ToggleTimer();
            SmokeCheck(app.sharedClock && app.tiles[2].running,L"啟動共用計時器");
            ShowControl();app.smokeStep=1;SetTimer(app.control,2,1800,nullptr);
        } else if(app.smokeStep==1) {
            KillTimer(app.control,2);
            TickTimers();
            SmokeCheck(!app.tiles[2].running && app.tiles[2].notified && !app.sharedClock,L"倒數完成且停用共用時脈");
            SaveState();
            std::ifstream in(app.statePath,std::ios::binary);auto loaded=Load(in);
            SmokeCheck(loaded.size()==app.tiles.size() && loaded[0].text==app.tiles[0].text,L"磁碟保存與重載");
            in.close();
            app.selection={app.tiles[2].id};app.primary=app.tiles[2].id;
            auto& timer=app.tiles[2];timer.countdown=false;timer.elapsed=0;ToggleTimer();
            app.lastTick-=std::chrono::milliseconds(2200);TickTimers();ToggleTimer();
            SmokeCheck(!timer.running && timer.elapsed>=2 && !app.sharedClock,L"正向計時與暫停");
            Command(IdReset);SmokeCheck(timer.elapsed==0 && !timer.running,L"重設計時器");
            timer.countdown=true;timer.elapsed=timer.duration;timer.notified=true;
            size_t oldCount=app.tiles.size();AddTile();
            SmokeCheck(app.tiles.size()==oldCount+1,L"新增相鄰六角形");
            DeleteSelected();SmokeCheck(app.tiles.size()==oldCount,L"刪除選取六角形");
            app.selection={app.tiles[0].id,app.tiles[1].id};app.primary=app.tiles[0].id;RefreshControls();Repaint();
            ShowAppearance();
            SmokeCheck(IsWindowVisible(app.appearance),L"開啟邊框設定視窗");
            auto& fields=app.appearanceWidgets;
            SendMessageW(fields[IdBorderStyle],CB_SETCURSEL,1,0);
            SetWindowTextW(fields[IdBorderColor],L"#E6B85C");SetWindowTextW(fields[IdBorderWidth],L"4");
            SmokeCheck(ApplyAppearance() && app.tiles[0].borderStyle==BorderStyle::Pixel &&
                SameAppearance(app.tiles[0],app.tiles[1]) && app.tiles[1].kind==Kind::Image,L"批次變更外觀並保留各格內容");
            SetWindowTextW(fields[IdBorderWidth],L"0");
            SmokeCheck(!ApplyAppearance() && app.tiles[0].borderWidth==4,L"無效厚度不變更外觀");
            RefreshAppearance();SendMessageW(fields[IdBorderStyle],CB_SETCURSEL,2,0);
            SmokeCheck(ApplyAppearance() && app.animationTimer,L"RGB 共用動畫時脈啟用");
            SendMessageW(fields[IdAnimate],BM_SETCHECK,BST_UNCHECKED,0);
            SmokeCheck(ApplyAppearance() && !app.animationTimer,L"靜態彩虹停用動畫時脈");
            for(size_t i=0;i<3;++i) {
                app.primary=app.tiles[i].id;app.selection={app.primary};RefreshControls();
                SendMessageW(fields[IdBorderStyle],CB_SETCURSEL,i,0);
                SetWindowTextW(fields[IdBorderColor],i==0?L"#E6B85C":i==1?L"#66ECC1":L"#FF0000");
                SetWindowTextW(fields[IdBorderWidth],i==2?L"5":L"4");
                SetWindowTextW(fields[IdRgbSpeed],L"6");SendMessageW(fields[IdAnimate],BM_SETCHECK,BST_CHECKED,0);
                SmokeCheck(ApplyAppearance(),L"套用風格 "+std::to_wstring(i));
            }
            std::ifstream appearanceFile(app.statePath,std::ios::binary);auto appearanceLoaded=Load(appearanceFile);appearanceFile.close();
            bool appearanceSaved=true;for(size_t i=0;i<3;++i)appearanceSaved&=SameAppearance(app.tiles[i],appearanceLoaded[i]);
            SmokeCheck(appearanceSaved,L"外觀設定磁碟保存與重載");
            UpdateWindow(app.appearance);CaptureWindowPng(app.appearance,app.directory/L"appearance-preview.png");
            app.selection.clear();for(const auto& t:app.tiles)app.selection.insert(t.id);app.primary=app.tiles[0].id;
            ShowLayout();SmokeCheck(IsWindowVisible(app.layout),L"開啟大小與文字設定視窗");
            auto& layout=app.layoutWidgets;
            SmokeCheck(SendMessageW(layout[IdFontFamily],CB_GETCOUNT,0,0)>0,L"載入本機字體清單");
            SendMessageW(layout[IdResizeCheck],BM_SETCHECK,BST_CHECKED,0);
            SendMessageW(layout[IdTextCheck],BM_SETCHECK,BST_UNCHECKED,0);
            SetWindowTextW(layout[IdTileSize],L"240");
            auto resizeAnchor=app.tiles[0].position;
            SmokeCheck(ApplyLayout() && std::all_of(app.tiles.begin(),app.tiles.end(),[](const Tile& t){return t.size==240;}),L"多選六角形一併調整大小");
            SmokeCheck(app.tiles[0].position.x==resizeAnchor.x && app.tiles[0].position.y==resizeAnchor.y && app.tiles[2].fontSize==22,L"縮放保持主格位置與各格原文字樣式");
            RECT tileRect;GetWindowRect(app.views[app.tiles[0].id].window,&tileRect);
            SmokeCheck(tileRect.right-tileRect.left==240 && tileRect.bottom-tileRect.top==PixelHeight(app.tiles[0]),L"實際視窗大小隨設定變更");
            auto& picture=app.views[app.tiles[1].id];
            SmokeCheck(picture.image && picture.image->GetWidth()==240 && picture.image->GetHeight()==static_cast<UINT>(PixelHeight(app.tiles[1])),L"圖片縮圖依新尺寸更新");
            app.selection={app.tiles[0].id};RefreshControls();SetWindowTextW(layout[IdTileSize],L"480");
            SmokeCheck(!ApplyLayout() && app.tiles[0].size==240,L"尺寸重疊時保留原狀");
            SendMessageW(layout[IdTextCheck],BM_SETCHECK,BST_CHECKED,0);
            SetWindowTextW(layout[IdFontFamily],L"Consolas");SetWindowTextW(layout[IdFontSize],L"36");SetWindowTextW(layout[IdTextColor],L"#FF00CC");
            auto unchangedFont=app.tiles[0].fontFamily;auto unchangedSize=app.tiles[0].fontSize;auto unchangedColor=app.tiles[0].textColor;
            SmokeCheck(!ApplyLayout() && app.tiles[0].size==240 && app.tiles[0].fontFamily==unchangedFont &&
                app.tiles[0].fontSize==unchangedSize && app.tiles[0].textColor==unchangedColor,L"大小與文字一併套用失敗時整批回復");
            app.selection.clear();for(const auto& t:app.tiles)app.selection.insert(t.id);RefreshControls();
            SetWindowTextW(layout[IdTileSize],L"260");SetWindowTextW(layout[IdFontFamily],L"Consolas");
            SetWindowTextW(layout[IdFontSize],L"18");SetWindowTextW(layout[IdTextColor],L"#FFCC66");
            SmokeCheck(ApplyLayout() && std::all_of(app.tiles.begin(),app.tiles.end(),[](const Tile& t){return t.size==260 && t.fontSize==18 && t.textColor==0xFFCC66;}),L"大小與文字可在多選時同時套用");
            SendMessageW(layout[IdResizeCheck],BM_SETCHECK,BST_UNCHECKED,0);
            SendMessageW(layout[IdTextCheck],BM_SETCHECK,BST_CHECKED,0);
            SetWindowTextW(layout[IdFontFamily],L"Consolas");SetWindowTextW(layout[IdFontSize],L"24");SetWindowTextW(layout[IdTextColor],L"#FFE09A");
            std::string originalText=app.tiles[0].text,originalImage=app.tiles[1].image;
            SmokeCheck(ApplyLayout() && std::all_of(app.tiles.begin(),app.tiles.end(),[](const Tile& t){return t.fontFamily=="Consolas" && t.fontSize==24 && t.textColor==0xFFE09A;}),L"字體、字級與文字顏色多選套用");
            SmokeCheck(app.tiles[0].text==originalText && app.tiles[1].image==originalImage && app.tiles[2].kind==Kind::Timer && app.tiles[0].size==260,L"套用文字樣式保留內容、尺寸與計時器");
            SetWindowTextW(layout[IdFontSize],L"0");SmokeCheck(!ApplyLayout() && app.tiles[0].fontSize==24,L"無效字級不變更設定");
            RefreshLayout();SetWindowTextW(layout[IdTextColor],L"#XYZXYZ");SmokeCheck(!ApplyLayout() && app.tiles[0].textColor==0xFFE09A,L"無效文字色碼不變更設定");
            RefreshLayout();SetWindowTextW(layout[IdFontFamily],L"HexDesk-Missing-Font-987");SmokeCheck(!ApplyLayout() && app.tiles[0].fontFamily=="Consolas",L"拒絕不可用字體");
            app.primary=app.tiles[2].id;app.selection={app.primary};RefreshControls();
            SendMessageW(layout[IdResizeCheck],BM_SETCHECK,BST_CHECKED,0);SendMessageW(layout[IdTextCheck],BM_SETCHECK,BST_UNCHECKED,0);
            SetWindowTextW(layout[IdTileSize],L"180");SmokeCheck(ApplyLayout() && app.tiles[2].size==180,L"單格尺寸可與其他格不同");
            auto mixedTarget=app.tiles[0].position+Neighbors(app.tiles[0].size,app.tiles[2].size)[1];
            BeginDrag(app.tiles[2].id,{0,0},false);DragTo(mixedTarget-app.tiles[2].position+Point{3,-2});EndDrag(true);
            SmokeCheck(std::abs(app.tiles[2].position.x-mixedTarget.x)<.001 && std::abs(app.tiles[2].position.y-mixedTarget.y)<.001,L"不同尺寸六角形實際拖曳吸附");
            SmokeCheck(SetLayer(app.tiles[2],false) && SetLayer(app.tiles[2],true) && PixelWidth(app.tiles[2])==180,L"調整尺寸後可切換桌面層與置頂");
            SaveState();std::ifstream layoutFile(app.statePath,std::ios::binary);auto layoutLoaded=Load(layoutFile);layoutFile.close();
            bool layoutSaved=true;for(size_t i=0;i<3;++i)layoutSaved&=SameTypography(app.tiles[i],layoutLoaded[i]) && app.tiles[i].size==layoutLoaded[i].size;
            SmokeCheck(layoutSaved,L"v3 尺寸與字體設定磁碟保存與重載");
            RefreshLayout();SendMessageW(layout[IdTextCheck],BM_SETCHECK,BST_CHECKED,0);UpdateLayoutControls();
            UpdateWindow(app.layout);CaptureWindowPng(app.layout,app.directory/L"layout-preview.png");
            SmokeNewFeatures();
            SmokeShapes();
            app.selection={app.tiles[0].id,app.tiles[1].id};app.primary=app.tiles[0].id;RefreshControls();Repaint();
            CapturePreview();
            SmokeCheck(fs::exists(app.directory/L"control-preview.png"),L"產出實際控制視窗截圖");
            app.animationSecondsOverride=0;CapturePreview(L"-styles");
            auto bodies=app.bodyBuilds;app.animationSecondsOverride=1;CapturePreview(L"-rgb-next");
            std::ifstream frameA(app.directory/L"tiles-preview-styles.png",std::ios::binary),frameB(app.directory/L"tiles-preview-rgb-next.png",std::ios::binary);
            std::string a((std::istreambuf_iterator<char>(frameA)),{}),b((std::istreambuf_iterator<char>(frameB)),{});
            SmokeCheck(!a.empty() && !b.empty() && a!=b,L"RGB 不同時間呈現不同燈條色彩");
            SmokeCheck(bodies==app.bodyBuilds,L"RGB 畫面重用內容快取");
            app.animationSecondsOverride=-1;
            for(auto& view:app.views) {EnableWindow(view.second.window,FALSE);UpdateWindow(view.second.window);}
            EnableWindow(app.control,FALSE);UpdateWindow(app.control);EnableWindow(app.appearance,FALSE);UpdateWindow(app.appearance);
            EnableWindow(app.layout,FALSE);UpdateWindow(app.layout);
            EnableWindow(app.backgroundControl,FALSE);UpdateWindow(app.backgroundControl);
            app.rgbBodyStart=app.bodyBuilds;app.rgbFrameStart=app.animationFrames;app.rgbPaintStart=app.paintCount;
            app.idlePaintStart=app.paintCount;app.idleCpuStart=CpuTicks();app.idleWallStart=GetTickCount64();app.smokeStep=2;
            SetTimer(app.control,2,10000,nullptr);
        } else if(app.smokeStep==2) {
            KillTimer(app.control,2);
            app.rgbSampleWall=double(GetTickCount64()-app.idleWallStart)/1000;
            app.rgbSampleCpu=double(CpuTicks()-app.idleCpuStart)/10000000.0/app.rgbSampleWall*100;
            app.rgbSampleFrames=app.animationFrames-app.rgbFrameStart;app.rgbSamplePaints=app.paintCount-app.rgbPaintStart;
            SmokeCheck(app.rgbSampleFrames>1 && app.rgbSamplePaints>1,L"實際 RGB 計時事件持續更新畫面");
            SmokeCheck(app.bodyBuilds==app.rgbBodyStart,L"RGB 播放期間未重建文字與圖片快取");
            for(auto& t:app.tiles)t.rgbAnimated=false;
            UpdateAnimation();SmokeCheck(!app.animationTimer,L"全部停止 RGB 後移除動畫時脈");
            AddTile();app.tiles.back().borderStyle=BorderStyle::Rgb;UpdateAnimation();
            SmokeCheck(app.animationTimer,L"新增動態圖形啟用時脈");
            DeleteSelected();SmokeCheck(!app.animationTimer,L"刪除最後動態圖形停用時脈");
            Repaint();for(auto& view:app.views)UpdateWindow(view.second.window);
            app.idlePaintStart=app.paintCount;app.idleCpuStart=CpuTicks();app.idleWallStart=GetTickCount64();app.smokeStep=3;
            SetTimer(app.control,2,5000,nullptr);
        } else {
            KillTimer(app.control,2);
            double wall=double(GetTickCount64()-app.idleWallStart)/1000;
            double cpu=double(CpuTicks()-app.idleCpuStart)/10000000.0;
            DWORD handles=0;GetProcessHandleCount(GetCurrentProcess(),&handles);
            PROCESS_MEMORY_COUNTERS_EX counters{};counters.cb=sizeof(counters);
            SmokeCheck(K32GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),sizeof(counters))!=FALSE,L"取得實際程序資源資訊");
            std::ofstream report(app.directory/L"integration-results.txt");
            report << "PASS checks=" << app.smokeChecks << "\nidle_sample_seconds=" << wall
                   << "\nidle_cpu_one_core_percent=" << cpu/wall*100
                   << "\nworking_set_mib=" << counters.WorkingSetSize/1048576.0
                   << "\nprivate_mib=" << counters.PrivateUsage/1048576.0 << "\nhandles=" << handles
                   << "\ngdi_objects=" << GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS)
                   << "\nidle_tile_paints=" << app.paintCount-app.idlePaintStart
                   << "\nrgb_sample_seconds=" << app.rgbSampleWall << "\nrgb_cpu_one_core_percent=" << app.rgbSampleCpu
                   << "\nrgb_timer_frames=" << app.rgbSampleFrames << "\nrgb_tile_paints=" << app.rgbSamplePaints << '\n';
            app.smokeDone=true;app.smokeStep=3;Command(IdExit);
        }
    } catch(const std::exception& e) {
        KillTimer(app.control,2);Error(Wide(e.what()));app.processExit=1;app.smokeDone=true;Command(IdExit);
    }
}
bool ActivateExisting(const fs::path& lockPath) {
    DWORD session=0;wchar_t key[CCH_RM_SESSION_KEY+1]{};
    if(RmStartSession(&session,0,key)!=ERROR_SUCCESS)return false;
    struct SessionGuard {DWORD handle;~SessionGuard(){RmEndSession(handle);}} guard{session};
    auto name=lockPath.wstring();LPCWSTR file=name.c_str();
    // Query owners only. Never shut down or restart another process.
    if(RmRegisterResources(session,1,&file,0,nullptr,0,nullptr)!=ERROR_SUCCESS)return false;
    UINT needed=0,count=0;DWORD reasons=0;std::vector<RM_PROCESS_INFO> processes;
    DWORD result=RmGetList(session,&needed,&count,nullptr,&reasons);
    for(int attempt=0;result==ERROR_MORE_DATA && attempt<3;++attempt) {
        if(needed>4096)return false;processes.resize(needed);count=needed;
        result=RmGetList(session,&needed,&count,processes.data(),&reasons);
    }
    if(result!=ERROR_SUCCESS || !count)return false;
    struct Search {std::set<DWORD> owners;HWND window=nullptr;DWORD owner=0;} search;
    for(UINT i=0;i<count;++i)search.owners.insert(processes[i].Process.dwProcessId);
    EnumWindows([](HWND window,LPARAM context)->BOOL {
        auto& found=*reinterpret_cast<Search*>(context);DWORD owner=0;GetWindowThreadProcessId(window,&owner);
        if(!found.owners.count(owner))return TRUE;
        wchar_t klass[128]{};GetClassNameW(window,klass,128);
        if(wcscmp(klass,ControlClass)!=0)return TRUE;
        found.window=window;found.owner=owner;return FALSE;
    },reinterpret_cast<LPARAM>(&search));
    if(!search.window)return false;
    DWORD_PTR response=0;
    if(!SendMessageTimeoutW(search.window,WM_NULL,0,0,SMTO_ABORTIFHUNG|SMTO_BLOCK,1500,&response))return false;
    AllowSetForegroundWindow(search.owner);
    ShowWindowAsync(search.window,IsIconic(search.window)?SW_RESTORE:SW_SHOW);
    HWND target=GetLastActivePopup(search.window);
    if(!target || !IsWindowVisible(target))target=search.window;
    if(!SetForegroundWindow(target)) {
        FLASHWINFO flash{sizeof(flash),target,FLASHW_TRAY,2,0};FlashWindowEx(&flash);
    }
    return true;
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int) {
    app.instance=instance;
    SetProcessDPIAware();
    int argc=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    bool customDirectory=false,showExisting=false;
    wchar_t local[32768]{};GetEnvironmentVariableW(L"LOCALAPPDATA",local,32768);
    app.directory=fs::path(local)/L"HexDesk";
    for(int i=1;i<argc;++i) {
        if(std::wstring(argv[i])==L"--data-dir" && i+1<argc) {app.directory=fs::absolute(argv[++i]);customDirectory=true;}
        else if(std::wstring(argv[i])==L"--smoke-test") app.smoke=true;
        else if(std::wstring(argv[i])==L"--show-existing")showExisting=true;
    }
    LocalFree(argv);
    if(app.smoke && !customDirectory) {MessageBoxW(nullptr,L"測試必須指定 --data-dir，避免修改使用者版面。",L"桌面拼格",MB_OK);return 2;}
    app.statePath=app.directory/L"layout.hexdesk";app.logPath=app.directory/L"session.log";
    HANDLE lock=INVALID_HANDLE_VALUE;ULONG_PTR gdiplus=0;
    try {
        if(showExisting)return ActivateExisting(app.directory/L"session.lock")?0:3;
        fs::create_directories(app.directory);
        lock=CreateFileW((app.directory/L"session.lock").c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(lock==INVALID_HANDLE_VALUE) {
            DWORD error=GetLastError();bool occupied=error==ERROR_SHARING_VIOLATION || error==ERROR_LOCK_VIOLATION;
            if(occupied && ActivateExisting(app.directory/L"session.lock"))return 0;
            if(!app.smoke) {
                auto message=occupied?L"這個資料目錄已在使用中，但無法帶回控制視窗。請從系統匣開啟原本的桌面拼格；要切換新版時，先儲存並結束舊版。":
                    L"無法開啟資料鎖。請確認下方資料目錄可寫入，或改用可寫入的資料目錄。";
                auto details=std::wstring(message)+L"\n\n資料位置："+app.directory.wstring()+L"\nWindows 錯誤碼："+std::to_wstring(error);
                MessageBoxW(nullptr,details.c_str(),L"桌面拼格",MB_OK|MB_ICONINFORMATION);
            }
            return 2;
        }
        app.log.open(app.logPath,std::ios::binary|std::ios::app);
        if(!app.log) throw std::runtime_error("無法開啟日誌");
        app.font=CreateFontW(-18,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft JhengHei UI");
        if(!app.font)throw std::runtime_error("無法建立介面字體");
        app.progress=CreateWindowExW(WS_EX_TOPMOST|WS_EX_TOOLWINDOW,L"STATIC",L"",WS_POPUP|WS_BORDER|SS_LEFT,
            130,130,1000,235,nullptr,nullptr,instance,nullptr);
        SendMessageW(app.progress,WM_SETFONT,reinterpret_cast<WPARAM>(app.font),TRUE);
        Log(L"輸入絕對路徑："+app.statePath.wstring());
        Log(L"輸出絕對路徑："+app.statePath.wstring()+L"；"+app.logPath.wstring());
        Progress(1,L"確認輸入與輸出路徑");ShowWindow(app.progress,SW_SHOWNORMAL);UpdateWindow(app.progress);
        GdiplusStartupInput startup;if(GdiplusStartup(&gdiplus,&startup,nullptr)!=Ok) throw std::runtime_error("GDI+ 初始化失敗");
        app.icon=CreateAppIcon();app.smallIcon=CreateAppIcon(16);app.brandIcon=CreateAppIcon(64);
        Progress(2,L"載入版面與內容");
        if(fs::exists(app.statePath)) {
            if(fs::file_size(app.statePath)>40*1024*1024) throw std::runtime_error("版面檔案過大，已保留原始檔案");
            std::ifstream input(app.statePath,std::ios::binary);app.tiles=Load(input);
            for(auto& t:app.tiles) {Wide(t.text);Wide(t.image);Wide(t.fontFamily);app.nextId=std::max(app.nextId,t.id+1);}
        } else {
            RECT area;SystemParametersInfoW(SPI_GETWORKAREA,0,&area,0);
            Point base{double(area.left+90),double(area.top+160)};
            for(int i=0;i<3;++i) {
                Tile t;t.id=i+1;t.position=base+Point{double(i*120),i==1?Height/2:0};
                t.kind=static_cast<Kind>(i);
                if(t.kind==Kind::Timer){t.fontSize=22;t.textColor=0x71EDC4;}
                if(i==0)t.text="今天的重點\n雙擊開始編輯";
                app.tiles.push_back(t);
            }
            app.nextId=4;
        }
        Progress(3,L"建立 Windows 顯示與控制視窗");
        WNDCLASSW tileClass{};tileClass.hInstance=instance;tileClass.lpszClassName=TileClass;
        tileClass.lpfnWndProc=TileProc;tileClass.hCursor=LoadCursor(nullptr,IDC_ARROW);tileClass.style=CS_DBLCLKS;
        RegisterClassW(&tileClass);
        WNDCLASSW visualClass{};visualClass.hInstance=instance;visualClass.lpszClassName=VisualClass;visualClass.lpfnWndProc=DefWindowProcW;
        RegisterClassW(&visualClass);
        WNDCLASSW controlClass{};controlClass.hInstance=instance;controlClass.lpszClassName=ControlClass;
        controlClass.lpfnWndProc=ControlProc;controlClass.hCursor=LoadCursor(nullptr,IDC_ARROW);
        controlClass.hIcon=app.icon;RegisterClassW(&controlClass);
        WNDCLASSW appearanceClass=controlClass;appearanceClass.lpszClassName=AppearanceClass;
        appearanceClass.lpfnWndProc=AppearanceProc;RegisterClassW(&appearanceClass);
        WNDCLASSW layoutClass=controlClass;layoutClass.lpszClassName=LayoutClass;
        layoutClass.lpfnWndProc=LayoutProc;RegisterClassW(&layoutClass);
        WNDCLASSW backgroundClass=controlClass;backgroundClass.lpszClassName=BackgroundClass;
        backgroundClass.lpfnWndProc=BackgroundProc;RegisterClassW(&backgroundClass);
        app.background=CreateSolidBrush(RGB(243,246,243));
        RECT controlRect{0,0,UiWidth(646),695};AdjustWindowRectEx(&controlRect,WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,FALSE,0);
        RECT work;SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);
        int x=std::max(work.left,work.right-(controlRect.right-controlRect.left)-50);
        app.control=CreateWindowExW(0,ControlClass,L"桌面拼格 · DeskTiles",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,
            x,work.top+35,controlRect.right-controlRect.left,controlRect.bottom-controlRect.top,nullptr,nullptr,instance,nullptr);
        if(!app.control)throw std::runtime_error("建立控制視窗失敗");
        SendMessageW(app.control,WM_SETICON,ICON_SMALL,reinterpret_cast<LPARAM>(app.smallIcon));
        SendMessageW(app.control,WM_SETICON,ICON_BIG,reinterpret_cast<LPARAM>(app.icon));
        CreateControls();
        for(auto& tile:app.tiles) CreateTileWindow(tile);
        if(!app.tiles.empty()){app.primary=app.tiles.front().id;app.selection={app.primary};}
        Progress(4,L"準備系統匣與操作選單");
        app.taskbarCreated=RegisterWindowMessageW(L"TaskbarCreated");AddTray();
        RefreshControls();Repaint();SaveState();UpdateAnimation();
        Progress(5,L"完成，等待操作");DestroyWindow(app.progress);app.progress=nullptr;
        ShowControl();
        if(app.smoke) SetTimer(app.control,2,350,nullptr);
        MSG message{};
        while(GetMessageW(&message,nullptr,0,0)>0) {
            if(app.backgroundControl && IsWindowVisible(app.backgroundControl) &&
                (message.hwnd==app.backgroundControl || IsChild(app.backgroundControl,message.hwnd))) {
                if(message.message==WM_KEYDOWN && message.wParam==VK_ESCAPE){ShowWindow(app.backgroundControl,SW_HIDE);continue;}
                if(IsDialogMessageW(app.backgroundControl,&message))continue;
            }
            if(app.layout && IsWindowVisible(app.layout) && (message.hwnd==app.layout || IsChild(app.layout,message.hwnd))) {
                if(message.message==WM_KEYDOWN && message.wParam==VK_ESCAPE){ShowWindow(app.layout,SW_HIDE);continue;}
                if(IsDialogMessageW(app.layout,&message))continue;
            }
            if(app.appearance && IsWindowVisible(app.appearance) &&
                (message.hwnd==app.appearance || IsChild(app.appearance,message.hwnd))) {
                if(message.message==WM_KEYDOWN && message.wParam==VK_ESCAPE){ShowWindow(app.appearance,SW_HIDE);continue;}
                if(IsDialogMessageW(app.appearance,&message))continue;
            }
            if(message.message==WM_KEYDOWN && message.wParam==VK_ESCAPE) {EscapeSelection();continue;}
            if(!IsDialogMessageW(app.control,&message)) {TranslateMessage(&message);DispatchMessageW(&message);}
        }
    } catch(const std::exception& e) {
        app.processExit=1;
        auto error=Wide(e.what());Log(L"ERROR: "+error);
        if(!app.smoke)MessageBoxW(nullptr,(error+L"\n\n資料位置："+app.statePath.wstring()).c_str(),L"DeskTiles 啟動失敗",MB_OK|MB_ICONERROR);
    }
    if(app.progress) DestroyWindow(app.progress);
    Shell_NotifyIconW(NIM_DELETE,&app.tray);
    for(auto& view:app.views) {if(IsWindow(view.second.window))DestroyWindow(view.second.window);view.second.image.reset();}
    app.views.clear();
    if(app.appearance)DestroyWindow(app.appearance);
    if(app.layout)DestroyWindow(app.layout);
    if(app.backgroundControl)DestroyWindow(app.backgroundControl);
    if(app.control)DestroyWindow(app.control);
    if(app.font)DeleteObject(app.font);if(app.titleFont)DeleteObject(app.titleFont);if(app.background)DeleteObject(app.background);
    if(app.icon)DestroyIcon(app.icon);
    if(app.smallIcon)DestroyIcon(app.smallIcon);if(app.brandIcon)DestroyIcon(app.brandIcon);
    if(gdiplus)GdiplusShutdown(gdiplus);
    if(lock!=INVALID_HANDLE_VALUE)CloseHandle(lock);
    return app.processExit;
}
