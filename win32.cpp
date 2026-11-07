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
#include "localization.hpp"
#include "startup.hpp"
using namespace desktile_ui;
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
    IdAddHex=150,IdAddTriangle=151,IdAddRectangle=152,IdNewShape=153,
    IdLanguage=154,IdEnglish=155,IdTraditionalChinese=156,IdLanguageLabel=157,IdAlarmSettings=158,IdStartup=159,
    IdAlarmDate=160,IdAlarmTime=161,IdAlarmRepeat=162,IdAlarmEnabled=163,IdAlarmApply=164,IdAlarmClose=165,
    IdAlarmDay0=170,IdReminderText=180,IdDismiss=181;
int PixelWidth(const Tile& tile){return static_cast<int>(std::ceil(tile.size));}
int UiWidth(int value){return static_cast<int>(std::lround(value*1.25));}
int PixelHeight(const Tile& tile){return static_cast<int>(std::ceil(TileHeight(tile.size,tile.shape)));}
std::wstring ShapeName(Shape shape){return shape==Shape::Triangle?Tr(L"三角形"):shape==Shape::Rectangle?Tr(L"矩形"):Tr(L"六角形");}
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
struct ReminderEntry {int id;bool alarm;std::wstring text;};
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
    HWND alarmControl=nullptr,reminder=nullptr;
    std::map<int,HWND> alarmWidgets;
    std::vector<ReminderEntry> reminderMessages;
    bool startupLaunch=false,featureOnly=false;
    HWND backgroundControl=nullptr;
    std::map<int,HWND> backgroundWidgets;
    std::vector<Tile> tiles;
    std::map<int,TileView> views;
    std::set<int> selection;
    int primary=0,nextId=1;
    fs::path directory,statePath,logPath;
    std::ofstream log;
    bool loadingControls=false,dragging=false,moved=false,savingFailed=false,exiting=false;
    bool smoke=false,sharedClock=false,interactionOnly=false,demoCapture=false,languageOnly=false;
    std::map<HWND,const Translation*> uiLabels;
    Point dragStart,dragDelta;
    HWND dragWindow=nullptr;
    bool dragShift=false,dragWasSelected=false,dragDoubleClick=false;
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
void ShowAlarm();
void RefreshAlarm();
void RefreshStartup();
void ToggleStartup();
void RefreshReminder();
void NotifyTile(const Tile&,bool alarm,bool preview=false);
std::wstring AlarmSummary(const Alarm&);
std::wstring AlarmCaption(const Alarm&);
void ShowBackground();
void RefreshBackground();
void ChooseExport();
void ChooseImport(bool merge);
bool SwitchLanguage(Language value,bool persist=true);
void AtomicOutput(const fs::path&,const std::function<void(std::ostream&)>&);
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
    if(!app.smoke) MessageBoxW(app.control,message.c_str(),Tr(L"桌面拼格"),MB_OK|MB_ICONWARNING);
}
void Progress(int n,const std::wstring& message) {
    std::wstring text=L"["+std::to_wstring(n)+L"/5] "+message;
    Log(text);
    SetWindowTextW(app.progress,(Tr(L"DeskTiles 啟動進度\r\n\r\n")+text+Tr(L"\r\n\r\n輸入：")+
        app.statePath.wstring()+Tr(L"\r\n輸出：")+app.statePath.wstring()+Tr(L"\r\n日誌：")+app.logPath.wstring()).c_str());
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
    // Both modes remain top-level popups, so positions are always screen coordinates.
    SetWindowPos(window,t.topmost?HWND_TOPMOST:HWND_BOTTOM,point.x,point.y,PixelWidth(t),PixelHeight(t),
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
        Log(Tr(L"桌面宿主搜尋失敗：未找到 SHELLDLL_DefView。"));
        EnumWindows([](HWND w,LPARAM)->BOOL {
            wchar_t klass[256]{};GetClassNameW(w,klass,256);
            if(wcsstr(klass,L"Worker") || wcsstr(klass,L"Progman")) Log(std::wstring(Tr(L"桌面候選："))+klass);
            return TRUE;
        },0);
        return false;
    }
    // Keep DWM composition on a top-level surface. Reparenting into Explorer's
    // no-redirection window prevents a nested layered child from reaching the screen.
    // An Explorer-owned popup stays above the desktop and below ordinary app windows.
    HWND originalOwner=GetWindow(window,GW_OWNER);
    SetWindowPos(window,HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
    SetLastError(0);
    SetWindowLongPtrW(window,GWLP_HWNDPARENT,reinterpret_cast<LONG_PTR>(host));
    DWORD parentError=GetLastError();
    if(GetWindow(window,GW_OWNER)!=host) {
        Log(Tr(L"桌面關聯失敗，Windows 錯誤碼：")+std::to_wstring(parentError));
        SetWindowLongPtrW(window,GWLP_HWNDPARENT,reinterpret_cast<LONG_PTR>(originalOwner));Position(t);return false;
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
    Log(Tr(L"[圖片 1/2] 輸入：")+Wide(tile.image)+Tr(L"；輸出：記憶體縮圖（")+std::to_wstring(tileWidth)+L" × "+std::to_wstring(tileHeight)+L"）");
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
        Log(Tr(L"[圖片 2/2] 縮圖完成，已釋放原始解碼器。"));
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
int AvailableTextStyle(const FontFamily& family,bool bold=false) {
    if(family.GetLastStatus()!=Ok)return -1;
    if(bold && family.IsStyleAvailable(FontStyleBold))return FontStyleBold;
    for(int style:{int(FontStyleRegular),int(FontStyleBold),int(Gdiplus::FontStyleItalic),int(FontStyleBold|Gdiplus::FontStyleItalic)})
        if(family.IsStyleAvailable(style))return style;
    return -1;
}
bool CoversTextGlyphs(const FontFamily& family,int style,const std::wstring& text) {
    if(text.empty())return true;
    wchar_t name[LF_FACESIZE]{};if(family.GetFamilyName(name)!=Ok)return false;
    HFONT font=CreateFontW(-16,0,0,0,(style&FontStyleBold)?FW_BOLD:FW_NORMAL,
        (style&Gdiplus::FontStyleItalic)!=0,FALSE,FALSE,DEFAULT_CHARSET,0,0,0,0,name);
    if(!font)return false;
    HDC dc=CreateCompatibleDC(nullptr);if(!dc){DeleteObject(font);return false;}
    auto old=SelectObject(dc,font);std::vector<WORD> glyphs(text.size());
    DWORD count=GetGlyphIndicesW(dc,text.data(),static_cast<int>(text.size()),glyphs.data(),GGI_MARK_NONEXISTING_GLYPHS);
    SelectObject(dc,old);DeleteDC(dc);DeleteObject(font);
    if(count==GDI_ERROR)return false;
    for(size_t i=0;i<text.size();++i)if(text[i]>L' ' && glyphs[i]==0xFFFF)return false;
    return true;
}
void DrawFittedText(Graphics& graphics,const std::wstring& text,const FontFamily* family,int style,
                    float requestedSize,const RectF& box,const SolidBrush* brush) {
    if(text.empty() || box.Width<=0 || box.Height<=0)return;
    FontFamily cjkFallback(L"Microsoft JhengHei UI");
    int fallbackStyle=AvailableTextStyle(cjkFallback,(style&FontStyleBold)!=0);
    // GDI+ can measure a missing CJK glyph using Latin advances, then paint a wider fallback glyph.
    // Select a font covering the whole text before both measurement and rendering.
    if(!CoversTextGlyphs(*family,style,text) && fallbackStyle>=0 && CoversTextGlyphs(cjkFallback,fallbackStyle,text)) {
        family=&cjkFallback;style=fallbackStyle;
    }
    StringFormat format;format.SetAlignment(StringAlignmentCenter);format.SetLineAlignment(StringAlignmentCenter);
    format.SetFormatFlags(Gdiplus::StringFormatFlagsLineLimit);
    auto fits=[&](float size) {
        Gdiplus::Font font(family,size,style,UnitPixel);
        if(font.GetLastStatus()!=Ok || font.GetHeight(&graphics)>box.Height+.01f)return false;
        RectF measured;INT characters=0,lines=0;
        auto status=graphics.MeasureString(text.c_str(),static_cast<INT>(text.size()),&font,
            RectF(0,0,box.Width,65536),&format,&measured,&characters,&lines);
        return status==Ok && characters==static_cast<INT>(text.size()) && measured.Height<=box.Height+.01f;
    };
    float size=requestedSize;
    if(!fits(size)) {
        float low=std::min(8.0f,size),high=size;
        // A bounded search only runs when the static text cache is rebuilt.
        for(int step=0;step<9;++step){float middle=(low+high)/2;if(fits(middle))low=middle;else high=middle;}
        size=low;
    }
    Gdiplus::Font font(family,size,style,UnitPixel);
    format.SetTrimming(StringTrimmingEllipsisCharacter);
    graphics.DrawString(text.c_str(),static_cast<INT>(text.size()),&font,box,&format,brush);
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
    FontFamily cjkFallback(L"Microsoft JhengHei UI");
    const FontFamily* family=AvailableTextStyle(requested)>=0?&requested:
        AvailableTextStyle(cjkFallback)>=0?&cjkFallback:FontFamily::GenericSansSerif();
    int normalStyle=AvailableTextStyle(*family);
    float captionSize=static_cast<float>(std::clamp(tile.fontSize*.65,8.0,14.0));
    float contentSize=static_cast<float>(tile.fontSize);
    int clockStyle=AvailableTextStyle(*family,true);
    SolidBrush muted(BorderColor(tile.textColor,175)),content(BorderColor(tile.textColor));
    float w=static_cast<float>(tile.size),h=static_cast<float>(TileHeight(tile.size,tile.shape));
    bool triangle=tile.shape==Shape::Triangle;
    g.SetClip(polygon.get());
    std::wstring label=L"#"+std::to_wstring(tile.id)+L"  "+(tile.topmost?Tr(L"置頂"):Tr(L"桌面"));
    if(tile.kind!=Kind::Image || !app.views[tile.id].image)
        DrawFittedText(g,label,family,normalStyle,captionSize,triangle?RectF(w*.35f,h*.32f,w*.3f,h*.14f):RectF(w*.21f,h*.1f,w*.58f,h*.17f),&muted);
    if(tile.kind==Kind::Text) {
        auto text=Wide(tile.text);DrawFittedText(g,text,family,normalStyle,contentSize,triangle?RectF(w*.26f,h*.5f,w*.48f,h*.35f):RectF(w*.14f,h*.28f,w*.72f,h*.5f),&content);
    } else if(tile.kind==Kind::Timer || tile.kind==Kind::Alarm) {
        bool alarm=tile.kind==Kind::Alarm,hasText=!tile.text.empty();
        auto time=Wide(alarm?AlarmTimeText(tile.alarm.minute):Clock(tile));
        auto timeBox=hasText?(triangle?RectF(w*.23f,h*.48f,w*.54f,h*.18f):RectF(w*.09f,h*.28f,w*.82f,h*.23f)):
            (triangle?RectF(w*.23f,h*.53f,w*.54f,h*.25f):RectF(w*.07f,h*.32f,w*.86f,h*.32f));
        DrawFittedText(g,time,family,clockStyle,contentSize,timeBox,&content);
        if(hasText)DrawFittedText(g,Wide(tile.text),family,normalStyle,contentSize*.75f,
            triangle?RectF(w*.18f,h*.69f,w*.64f,h*.17f):RectF(w*.14f,h*.55f,w*.72f,h*.22f),&content);
        std::wstring state=alarm?AlarmCaption(tile.alarm):tile.running?Tr(L"計時中"):
            ((tile.countdown && tile.elapsed>=tile.duration)?Tr(L"時間到"):Tr(L"已暫停"));
        auto stateBox=hasText?(triangle?RectF(w*.15f,h*.89f,w*.7f,h*.10f):RectF(w*.12f,h*.82f,w*.76f,h*.12f)):
            (triangle?RectF(w*.18f,h*.8f,w*.64f,h*.13f):RectF(w*.18f,h*.67f,w*.64f,h*.17f));
        DrawFittedText(g,state,family,normalStyle,captionSize,stateBox,&muted);
    } else if(!app.views[tile.id].image) {
        const wchar_t* hint=app.views[tile.id].imageFailed?Tr(L"圖片無法讀取\n雙擊重新選擇"):Tr(L"雙擊選擇圖片");
        DrawFittedText(g,hint,family,normalStyle,contentSize,triangle?RectF(w*.26f,h*.5f,w*.48f,h*.35f):RectF(w*.15f,h*.30f,w*.70f,h*.45f),&content);
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
        if(!view.presentationFailed)Error(Tr(L"透明畫面更新失敗，Windows 錯誤：")+std::to_wstring(GetLastError()));
        view.presentationFailed=true;
    }else view.presentationFailed=false;
}
void Repaint() {
    for(auto& item:app.views) {item.second.surfaceDirty=true;InvalidateRect(item.second.window,nullptr,FALSE);}
}
void UpdateSelectionVisuals(const std::set<int>& before) {
    for(auto& view:app.views)if(before.count(view.first)!=app.selection.count(view.first)) {
        InvalidateRect(view.second.window,nullptr,FALSE);UpdateWindow(view.second.window);
    }
}
void UpdateTileRegion(Tile& tile) {
    auto shape=BorderOutline(tile.borderStyle,0,tile.size,tile.shape);std::vector<POINT> points;
    for(auto p:shape)points.push_back({LONG(std::lround(p.x)),LONG(std::lround(p.y))});
    SetWindowRgn(app.views[tile.id].window,CreatePolygonRgn(points.data(),static_cast<int>(points.size()),WINDING),TRUE);
}
bool ConfigureTileComposition(HWND window) {
    // A zero-alpha host retains polygon hit testing. Its layered child draws content.
    // Extend the glass frame instead of suppressing the host's redirection surface.
    MARGINS glass{-1,-1,-1,-1};
    auto result=DwmExtendFrameIntoClientArea(window,&glass);
    if(FAILED(result))Log(L"DwmExtendFrameIntoClientArea HRESULT="+std::to_wstring(result));
    return SUCCEEDED(result);
}
void CreateTileWindow(Tile& t) {
    app.views[t.id].surfaceDirty=true;
    auto title=L"DeskTiles · "+ShapeName(t.shape);
    HWND window=CreateWindowExW(WS_EX_TOOLWINDOW,TileClass,title.c_str(),WS_POPUP,
        0,0,PixelWidth(t),PixelHeight(t),nullptr,nullptr,app.instance,reinterpret_cast<void*>(static_cast<INT_PTR>(t.id)));
    if(!window) throw std::runtime_error("建立圖形視窗失敗");
    app.views[t.id].window=window;
    if(!ConfigureTileComposition(window))throw std::runtime_error("無法建立透明顯示層");
    app.views[t.id].visual=CreateWindowExW(WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE,VisualClass,L"",WS_CHILD|WS_VISIBLE,
        0,0,PixelWidth(t),PixelHeight(t),window,nullptr,app.instance,nullptr);
    if(!app.views[t.id].visual)throw std::runtime_error("無法建立透明顯示層");
    UpdateTileRegion(t);
    LoadImage(t);
    if(!SetLayer(t,t.topmost)) {
        t.topmost=true; SetLayer(t,true);
        Error(Tr(L"桌面宿主無法使用，此格暫以置頂顯示。可從右鍵重試固定於桌面層。"));
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
        if(!app.savingFailed) Error(LocalizeError(Wide(e.what())));
        app.savingFailed=true;
    }
}
void ShowControl() {
    RefreshControls(); ShowWindow(app.control,SW_SHOWNORMAL); SetForegroundWindow(app.control);
}
void UpdateClock() {
    bool running=false;
    for(auto& t:app.tiles) if((t.kind==Kind::Timer && t.running)||(t.kind==Kind::Alarm&&t.alarm.enabled&&t.alarm.nextDue)) running=true;
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
    auto steady=std::chrono::steady_clock::now();
    double seconds=std::chrono::duration<double>(steady-app.lastTick).count();app.lastTick=steady;
    auto now=static_cast<std::int64_t>(std::time(nullptr));std::vector<std::pair<int,bool>> due;
    for(auto& tile:app.tiles) {
        bool repaint=false;
        if(tile.kind==Kind::Timer&&tile.running) {
            if(Advance(tile,seconds))due.push_back({tile.id,false});repaint=true;
        } else if(tile.kind==Kind::Alarm&&AdvanceAlarm(tile.alarm,now)) {due.push_back({tile.id,true});repaint=true;}
        if(repaint){app.views[tile.id].surfaceDirty=true;InvalidateRect(app.views[tile.id].window,nullptr,FALSE);}
    }
    UpdateClock();
    if(!due.empty()) {
        // Persist the consumed occurrence before presenting nonmodal notifications.
        SaveState();
        for(auto item:due)if(auto* tile=Find(item.first)) {
            Status(Tr(L"圖形 #")+std::to_wstring(tile->id)+(item.second?Tr(L"：鬧鐘時間到。"):Tr(L"：倒數結束。")));
            NotifyTile(*tile,item.second);
            if(tile->id==app.primary && item.second)SetWindowTextW(app.widgets[245],AlarmSummary(tile->alarm).c_str());
            if(tile->id==app.primary && !item.second)SetWindowTextW(app.widgets[IdStart],Tr(L"開始計時"));
        }
    }
}
void EndDrag(bool commit) {
    if(!app.dragging)return;
    Point delta=app.dragDelta;HWND owner=app.dragWindow;auto previousSelection=app.selection;
    bool moved=app.moved,shift=app.dragShift,wasSelected=app.dragWasSelected,edit=app.dragDoubleClick;
    int id=app.primary;app.dragging=false;app.dragWindow=nullptr;
    if(commit && moved && ValidDelta(app.dragOriginal,app.dragSelection,delta)) {
        Move(app.tiles,app.dragSelection,delta);SaveState();
        Status(Tr(L"[拖曳 2/2] 已移動 ")+std::to_wstring(app.dragSelection.size())+TrCounted(L" 個圖形；相對位置保持不變。",app.dragSelection.size()));
    }else if(commit && moved)Status(Tr(L"[拖曳 2/2] 位置重疊，已回到拖曳前位置。"));
    if(commit && !moved) {
        if(shift && wasSelected)app.selection.erase(id);
        else if(!shift)app.selection={id};
        app.primary=app.selection.count(id)?id:app.selection.empty()?0:*app.selection.begin();
    }
    for(auto& tile:app.tiles)Position(tile);
    app.dragOriginal.clear();app.dragSelection.clear();
    if(GetCapture()==owner)ReleaseCapture();
    RefreshControls();UpdateSelectionVisuals(previousSelection);
    if(commit && !moved && edit && !shift)ShowControl();
}
void BeginDrag(int id,Point cursor,bool shift,bool doubleClick=false) {
    EndDrag(false);
    if(!Find(id))return;
    auto previousSelection=app.selection;
    app.dragWasSelected=app.selection.count(id)!=0;app.dragShift=shift;app.dragDoubleClick=doubleClick;
    if(!app.dragWasSelected) {
        if(!shift)app.selection.clear();
        app.selection.insert(id);
    }
    app.primary=id;RefreshControls();UpdateSelectionVisuals(previousSelection);
    UpdateWindow(app.control);
    app.dragStart=cursor;app.dragDelta={};app.dragOriginal=app.tiles;
    app.dragSelection=app.selection;app.dragging=true;app.moved=false;app.dragWindow=app.views[id].window;
    SetCapture(app.dragWindow);
    if(GetCapture()!=app.dragWindow)EndDrag(false);
}
void DragTo(Point cursor) {
    if(!app.dragging)return;
    auto desired=cursor-app.dragStart;
    if(!app.moved && std::abs(desired.x)<GetSystemMetrics(SM_CXDRAG) &&
                      std::abs(desired.y)<GetSystemMetrics(SM_CYDRAG))return;
    if(!app.moved) {
        Log(Tr(L"拖曳輸入：目前選取圖形；輸出：")+app.statePath.wstring());
        Status(Tr(L"[拖曳 1/2] 移動預覽；放開滑鼠後保存。"));
    }
    app.moved=true;app.dragDelta=Snap(app.dragOriginal,app.dragSelection,desired);
    for(auto& tile:app.tiles)if(app.dragSelection.count(tile.id))Position(tile);
}
std::wstring KindName(Kind k) { return k==Kind::Text?Tr(L"文字"):k==Kind::Image?Tr(L"圖片"):k==Kind::Alarm?Tr(L"鬧鐘"):Tr(L"計時器"); }
void UpdateContentVisibility() {
    int type=static_cast<int>(SendMessageW(app.widgets[IdType],CB_GETCURSEL,0,0));
    for(int id:{IdText,201})ShowWindow(app.widgets[id],type!=1?SW_SHOW:SW_HIDE);
    SetWindowPos(app.widgets[IdText],nullptr,UiWidth(227),232,UiWidth(390),type==0?190:54,SWP_NOZORDER|SWP_NOACTIVATE);
    for(int id:{IdAlarmSettings,245})ShowWindow(app.widgets[id],type==3?SW_SHOW:SW_HIDE);
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
    std::wstring summary=tile ? Tr(L"編輯 #")+std::to_wstring(tile->id)+Tr(L" · 已選取 ")+std::to_wstring(app.selection.size())+TrCounted(L" 格",app.selection.size()) : Tr(L"請先選取一個圖形");
    SetWindowTextW(app.widgets[205],summary.c_str());
    for(int id:{IdType,IdText,IdImage,IdBrowse,IdMode,IdSeconds,IdApply,IdDelete,IdStart,IdReset,IdTop,IdDesktop,IdAppearance,IdLayout,IdBackground,IdAlarmSettings})
        EnableWindow(app.widgets[id],tile!=nullptr);
    if(tile) {
        SendMessageW(app.widgets[IdType],CB_SETCURSEL,static_cast<int>(tile->kind),0);
        SetWindowTextW(app.widgets[IdText],Wide(tile->text).c_str());
        SetWindowTextW(app.widgets[IdImage],Wide(tile->image).c_str());
        SendMessageW(app.widgets[IdMode],CB_SETCURSEL,tile->countdown?0:1,0);
        SetWindowTextW(app.widgets[IdSeconds],std::to_wstring(static_cast<int>(tile->duration)).c_str());
        SetWindowTextW(app.widgets[IdStart],tile->running?Tr(L"暫停計時"):Tr(L"開始計時"));
    }
    UpdateContentVisibility();
    app.loadingControls=false;
    RefreshAppearance();
    RefreshLayout();
    RefreshBackground();RefreshAlarm();
}
void AddTile(Shape shape) {
    if(app.tiles.size()>=500) { Error(Tr(L"第一版最多支援 500 格。")); return; }
    Log(Tr(L"新增圖形輸入：控制視窗／右鍵選單；輸出：")+app.statePath.wstring());
    Status(Tr(L"[新增 1/3] 尋找 ")+ShapeName(shape)+Tr(L" 的可用相鄰位置"));
    Tile tile;tile.id=app.nextId++;tile.shape=shape;tile.text=Utf8(Tr(L"新文字"));
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
    if(!found) { Error(Tr(L"附近沒有可用的位置，請先移動部分圖形。")); return; }
    Status(Tr(L"[新增 2/3] 建立圖形與互動區域"));app.tiles.push_back(tile);CreateTileWindow(app.tiles.back());
    app.selection={tile.id}; app.primary=tile.id; Repaint(); RefreshControls(); SaveState();
    Status(Tr(L"[新增 3/3] 已新增 ")+ShapeName(shape)+L" #"+std::to_wstring(tile.id)+(app.savingFailed?Tr(L"，儲存失敗。"):Tr(L" 並儲存。")));
}
void ApplyContent() {
    auto* primary=Find(app.primary); if(!primary || app.selection.empty()) return;
    int kind=static_cast<int>(SendMessageW(app.widgets[IdType],CB_GETCURSEL,0,0));
    if(kind<0 || kind>3) return;
    if(kind==3 && primary->kind!=Kind::Alarm){ShowAlarm();return;}
    Log(Tr(L"內容輸入：控制視窗；輸出：")+app.statePath.wstring());Status(Tr(L"[內容 1/3] 驗證設定"));
    auto text=Utf8(TextOf(app.widgets[IdText])); auto image=Utf8(TextOf(app.widgets[IdImage]));
    double duration=300; bool countdown=SendMessageW(app.widgets[IdMode],CB_GETCURSEL,0,0)==0;
    if(kind==2) {
        auto value=TextOf(app.widgets[IdSeconds]); size_t end=0;
        try { duration=std::stod(value,&end); } catch(...) {duration=0;}
        if(end!=value.size() || !std::isfinite(duration) || duration<1 || duration>8640000) {
            Error(Tr(L"請輸入 1 到 8640000 之間的秒數。")); return;
        }
    }
    if(kind==1) {
        if(image.empty() || !fs::is_regular_file(fs::u8path(image))) { Error(Tr(L"請先選擇存在的圖片檔案。")); return; }
        image=fs::absolute(fs::u8path(image)).u8string();
        std::unique_ptr<Bitmap> probe(Bitmap::FromFile(Wide(image).c_str()));
        if(!probe || probe->GetLastStatus()!=Ok) { Error(Tr(L"圖片無法讀取，請選擇 PNG、JPEG 或 BMP。")); return; }
    }
    if(app.sharedClock) TickTimers();
    Status(Tr(L"[內容 2/3] 套用文字與內容"));
    for(auto& t:app.tiles) if(app.selection.count(t.id)) {
        bool reset= t.kind!=Kind::Timer || t.duration!=duration || t.countdown!=countdown;
        if(kind==3&&t.kind!=Kind::Alarm){t.alarm=primary->alarm;ArmAlarm(t.alarm,static_cast<std::int64_t>(std::time(nullptr)));}
        if(kind!=3)t.alarm.nextDue=0;
        t.kind=static_cast<Kind>(kind);t.text=text;
        if(kind==1) {t.image=image;app.views[t.id].imagePath.clear();LoadImage(t);}
        else {app.views[t.id].image.reset(); app.views[t.id].imagePath.clear();}
        if(kind==2) {t.duration=duration;t.countdown=countdown;if(reset){t.elapsed=0;t.running=false;t.notified=false;}}
        else t.running=false;
    }
    UpdateClock(); SaveState(); RefreshControls(); Repaint();
    Status(std::wstring(Tr(L"[內容 3/3] "))+(app.savingFailed?Tr(L"已更新顯示，但儲存失敗。"):Tr(L"已套用至 ")+std::to_wstring(app.selection.size())+TrCounted(L" 個圖形。",app.selection.size())));
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
    auto message=Tr(L"確定刪除選取的 ")+std::to_wstring(app.selection.size())+TrCounted(L" 個圖形？",app.selection.size());
    if(!app.smoke && MessageBoxW(app.control,message.c_str(),Tr(L"刪除圖形"),MB_YESNO|MB_ICONQUESTION)!=IDYES) return;
    for(auto id:app.selection) {DestroyWindow(app.views[id].window);app.views.erase(id);}
    app.tiles.erase(std::remove_if(app.tiles.begin(),app.tiles.end(),[](const Tile& t){return app.selection.count(t.id)!=0;}),app.tiles.end());
    app.selection.clear(); app.primary=0; UpdateClock();UpdateAnimation(); RefreshControls(); SaveState();
}
void ChangeSelectedLayer(bool top) {
    int failed=0;
    for(auto& t:app.tiles) if(app.selection.count(t.id) && !SetLayer(t,top)) ++failed;
    Repaint(); SaveState();
    if(failed) {
        Status(Tr(L"層級切換：")+std::to_wstring(app.selection.size()-failed)+Tr(L" 格完成，")+std::to_wstring(failed)+Tr(L" 格保留原設定。"));
        Error(Tr(L"部分格子無法切換層級，已保留原本設定。請確認 Windows Explorer 正在執行。"));
    } else Status(top?Tr(L"選取圖形已設為永遠置頂。"):Tr(L"選取圖形已固定於桌面層。"));
}
void AddTray() {
    app.tray={sizeof(NOTIFYICONDATAW)};app.tray.hWnd=app.control;app.tray.uID=1;
    app.tray.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP; app.tray.uCallbackMessage=TrayMessage;
    app.tray.hIcon=app.smallIcon; wcscpy_s(app.tray.szTip,Tr(L"桌面拼格 · DeskTiles"));
    Shell_NotifyIconW(NIM_ADD,&app.tray);
}
HMENU BuildTileMenu(bool trayMenu) {
    HMENU menu=CreatePopupMenu();
    AppendMenuW(menu,MF_STRING,IdEdit,Tr(L"開啟控制視窗"));
    HMENU shapes=CreatePopupMenu();
    AppendMenuW(shapes,MF_STRING,IdAddHex,Tr(L"六角形（預設）"));AppendMenuW(shapes,MF_STRING,IdAddTriangle,Tr(L"三角形"));
    AppendMenuW(shapes,MF_STRING,IdAddRectangle,Tr(L"矩形（3:2）"));AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(shapes),Tr(L"新增相鄰圖形"));
    AppendMenuW(menu,MF_STRING,IdSelectAll,Tr(L"選取全部"));
    AppendMenuW(menu,MF_STRING,IdExport,Tr(L"匯出整體配置（含圖片）…"));
    AppendMenuW(menu,MF_STRING,IdImportReplace,Tr(L"匯入 JSON：覆蓋配置…"));
    AppendMenuW(menu,MF_STRING,IdImportMerge,Tr(L"匯入 JSON：合併配置…"));
    if(!trayMenu && !app.selection.empty()) {
        bool allTop=true,allDesktop=true,hasTimer=false;
        for(const auto& t:app.tiles) if(app.selection.count(t.id)) {
            allTop&=t.topmost; allDesktop&=!t.topmost; hasTimer|=t.kind==Kind::Timer;
        }
        AppendMenuW(menu,MF_SEPARATOR,0,nullptr);
        AppendMenuW(menu,MF_STRING,IdAppearance,Tr(L"邊框顏色、厚度與風格…"));
        AppendMenuW(menu,MF_STRING,IdLayout,Tr(L"圖形大小與文字樣式…"));
        AppendMenuW(menu,MF_STRING,IdBackground,Tr(L"背景顏色與透明度…"));
        AppendMenuW(menu,MF_STRING,IdAlarmSettings,Tr(L"鬧鐘設定…"));
        if(!allTop && !allDesktop) AppendMenuW(menu,MF_STRING|MF_DISABLED,0,Tr(L"顯示層級：混合狀態"));
        AppendMenuW(menu,MF_STRING|(allTop?MF_CHECKED:0),IdTop,Tr(L"顯示於最上層"));
        AppendMenuW(menu,MF_STRING|(allDesktop?MF_CHECKED:0),IdDesktop,Tr(L"固定於桌面層"));
        if(hasTimer) {
            AppendMenuW(menu,MF_SEPARATOR,0,nullptr);
            AppendMenuW(menu,MF_STRING,IdStart,Tr(L"開始／暫停選取的計時器"));
            AppendMenuW(menu,MF_STRING,IdReset,Tr(L"重設選取的計時器"));
        }
        AppendMenuW(menu,MF_SEPARATOR,0,nullptr);
        AppendMenuW(menu,MF_STRING,IdDelete,Tr(L"刪除選取圖形…"));
    }
    AppendMenuW(menu,MF_SEPARATOR,0,nullptr);
    AppendMenuW(menu,MF_STRING,IdShow,Tr(L"找回全部圖形（移至主螢幕並置頂）"));
    AppendMenuW(menu,MF_STRING,IdExit,Tr(L"儲存並結束桌面拼格"));
    HMENU languages=CreatePopupMenu();
    AppendMenuW(languages,MF_STRING|(language==Language::TraditionalChinese?MF_CHECKED:0),IdTraditionalChinese,L"繁體中文");
    AppendMenuW(languages,MF_STRING|(language==Language::English?MF_CHECKED:0),IdEnglish,L"English");
    AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(languages),Tr(L"語言"));
    return menu;
}
void Popup(HWND window,int id,POINT point,bool trayMenu=false) {
    EndDrag(false);
    if(!trayMenu && id) {
        if(!app.selection.count(id)) app.selection={id};
        app.primary=id; RefreshControls(); Repaint();
    }
    HMENU menu=BuildTileMenu(trayMenu);
    // Use the controller for menus so desktop tiles remain in the desktop band.
    SetForegroundWindow(app.control);
    int result=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,point.x,point.y,0,app.control,nullptr);
    DestroyMenu(menu); PostMessageW(window,WM_NULL,0,0);
    if(result) Command(result);
}
void Command(int id) {
    switch(id) {
    case IdEdit:ShowControl();break;
    case IdEnglish:SwitchLanguage(Language::English);break;
    case IdTraditionalChinese:SwitchLanguage(Language::TraditionalChinese);break;
    case IdAppearance:ShowAppearance();break;
    case IdLayout:ShowLayout();break;
    case IdBackground:ShowBackground();break;
    case IdAlarmSettings:ShowAlarm();break;
    case IdStartup:ToggleStartup();break;
    case IdExport:ChooseExport();break;
    case IdImportReplace:ChooseImport(false);break;
    case IdImportMerge:ChooseImport(true);break;
    case IdConfig: {
        RECT r;GetWindowRect(app.widgets[IdConfig],&r);HMENU menu=CreatePopupMenu();
        AppendMenuW(menu,MF_STRING,IdExport,Tr(L"匯出整體配置（含圖片）…"));
        AppendMenuW(menu,MF_STRING,IdImportReplace,Tr(L"匯入 JSON：覆蓋配置…"));
        AppendMenuW(menu,MF_STRING,IdImportMerge,Tr(L"匯入 JSON：合併配置…"));
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
        ofn.lpstrFilter=Tr(L"圖片檔案\0*.png;*.jpg;*.jpeg;*.bmp\0所有檔案\0*.*\0");
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
        SaveState();Repaint();Status(Tr(L"已將圖形移至主螢幕並置頂。")); break;
    }
    case IdExit:
        if(app.sharedClock) TickTimers();SaveState();
        if(app.savingFailed && !app.smoke) {
            if(MessageBoxW(app.control,Tr(L"版面尚未成功儲存，仍要結束嗎？"),Tr(L"儲存失敗"),MB_YESNO|MB_ICONWARNING)!=IDYES) break;
        }
        app.exiting=true; PostQuitMessage(app.processExit);break;
    }
}
HWND WidgetFor(HWND parent,std::map<int,HWND>& widgets,int id,const wchar_t* klass,const wchar_t* text,DWORD style,int x,int y,int w,int h) {
    if(wcscmp(klass,L"STATIC")==0 && (style&SS_TYPEMASK)!=SS_ICON)
        h=std::max(h,wcsstr(text,L"\n")?52:26);
    HWND window=CreateWindowExW(wcscmp(klass,L"EDIT")==0?WS_EX_CLIENTEDGE:0,klass,text,
        WS_CHILD|WS_VISIBLE|style,UiWidth(x),y,UiWidth(w),h,parent,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),app.instance,nullptr);
    SendMessageW(window,WM_SETFONT,reinterpret_cast<WPARAM>(app.font),TRUE);widgets[id]=window;
    if(wcscmp(klass,L"STATIC")==0 || wcscmp(klass,L"BUTTON")==0)
        if(auto entry=FindTranslation(text,true))app.uiLabels[window]=entry;
    if(id==IdDesktop)app.uiLabels[window]=FindTranslation(L"選取格固定於桌面");
    return window;
}
HWND Widget(int id,const wchar_t* klass,const wchar_t* text,DWORD style,int x,int y,int w,int h) {
    return WidgetFor(app.control,app.widgets,id,klass,text,style,x,y,w,h);
}
#include "win32_features.hpp"

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
    auto summary=tile?L"#"+std::to_wstring(tile->id)+Tr(L" · 選取 ")+std::to_wstring(app.selection.size())+TrCounted(L" 格",app.selection.size())+
        (mixed?Tr(L" · 混合設定，以此格為範本"):L""):Tr(L"請先選取圖形");
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
    Log(Tr(L"邊框設定輸入：目前選取圖形；輸出：")+app.statePath.wstring());
    Status(Tr(L"[邊框 1/3] 檢查顏色、厚度與風格"));
    int style=static_cast<int>(SendMessageW(f[IdBorderStyle],CB_GETCURSEL,0,0));
    std::uint32_t color=0;double width=0,cycle=0;
    if(style<0 || style>3 || !ParseColor(Utf8(TextOf(f[IdBorderColor])),color)) {
        Error(Tr(L"顏色請輸入六位十六進位，例如 #66ECC1。"));return false;
    }
    if(!ReadNumber(f[IdBorderWidth],1,12,width)) {Error(Tr(L"邊框厚度請輸入 1～12 像素。"));return false;}
    if(!ReadNumber(f[IdRgbSpeed],2,60,cycle)) {Error(Tr(L"燈效循環時間請輸入 2～60 秒。"));return false;}
    bool animated=SendMessageW(f[IdAnimate],BM_GETCHECK,0,0)==BST_CHECKED;
    Status(Tr(L"[邊框 2/3] 套用至 ")+std::to_wstring(app.selection.size())+TrCounted(L" 格",app.selection.size()));
    for(auto& tile:app.tiles)if(app.selection.count(tile.id)) {
        tile.borderStyle=static_cast<BorderStyle>(style);tile.borderColor=color;tile.borderWidth=width;
        tile.rgbCycleSeconds=cycle;tile.rgbAnimated=animated;UpdateTileRegion(tile);
    }
    UpdateAnimation();Repaint();SaveState();RefreshAppearance();
    auto status=app.savingFailed?Tr(L"[邊框 3/3] 已更新顯示，但儲存失敗。"):Tr(L"[邊框 3/3] 外觀已更新並儲存。");
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
        app.appearance=CreateWindowExW(WS_EX_DLGMODALFRAME,AppearanceClass,Tr(L"DeskTiles · 邊框與風格"),style,
            x,y,bounds.right-bounds.left,bounds.bottom-bounds.top,app.control,nullptr,app.instance,nullptr);
        auto field=[](int id,const wchar_t* type,const wchar_t* text,DWORD style,int x,int y,int w,int h) {
            return WidgetFor(app.appearance,app.appearanceWidgets,id,type,text,style,x,y,w,h);
        };
        field(220,L"STATIC",L"",0,22,20,406,25);
        field(222,L"STATIC",Tr(L"邊框風格"),0,22,65,110,24);
        field(IdBorderStyle,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,142,60,280,150);
        for(auto name:{Tr(L"一般實色"),Tr(L"像素風"),Tr(L"RGB 動態燈條"),Tr(L"單色呼吸燈")})
            SendMessageW(app.appearanceWidgets[IdBorderStyle],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(name));
        field(223,L"STATIC",Tr(L"邊框顏色"),0,22,112,110,24);
        field(IdBorderColor,L"EDIT",L"#485F69",WS_TABSTOP|ES_AUTOHSCROLL,142,106,157,30);
        SendMessageW(app.appearanceWidgets[IdBorderColor],EM_SETLIMITTEXT,7,0);
        field(IdChooseColor,L"BUTTON",Tr(L"選擇顏色…"),WS_TABSTOP,310,104,112,34);
        field(224,L"STATIC",Tr(L"邊框厚度"),0,22,159,110,24);
        field(IdBorderWidth,L"EDIT",L"2",WS_TABSTOP|ES_AUTOHSCROLL,142,153,80,30);
        field(225,L"STATIC",Tr(L"像素（1～12）"),0,235,159,185,24);
        field(226,L"STATIC",Tr(L"燈效循環"),0,22,206,110,24);
        field(IdRgbSpeed,L"EDIT",L"6",WS_TABSTOP|ES_AUTOHSCROLL,142,200,80,30);
        field(227,L"STATIC",Tr(L"秒／圈（2～60）"),0,235,206,185,24);
        field(IdAnimate,L"BUTTON",Tr(L"播放燈效（取消後顯示靜態邊框）"),WS_TABSTOP|BS_AUTOCHECKBOX,22,249,400,28);
        field(228,L"STATIC",Tr(L"RGB 沿邊框流動；呼吸燈以設定色明暗循環。\r\n僅作用於邊框，背景與內容保持原設定。"),0,22,290,404,42);
        field(IdApplyBorder,L"BUTTON",Tr(L"套用至所有選取格"),WS_TABSTOP|BS_DEFPUSHBUTTON,22,348,270,38);
        field(IdCloseBorder,L"BUTTON",Tr(L"關閉"),WS_TABSTOP,309,348,113,38);
        field(221,L"STATIC",Tr(L"修改後按套用；可單選或多選。"),SS_PATHELLIPSIS,22,397,406,23);
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
    auto label=tile?L"#"+std::to_wstring(tile->id)+Tr(L" · 選取 ")+std::to_wstring(app.selection.size())+TrCounted(L" 格",app.selection.size())+(mixed?Tr(L" · 混合設定，以此格為範本"):L""):Tr(L"請先選取圖形");
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
    Log(Tr(L"大小與文字輸入：目前選取格；輸出：")+app.statePath.wstring());
    Status(Tr(L"[大小／文字 1/3] 驗證輸入與可用空間"));
    if(!resize && !typography){Error(Tr(L"請先勾選要套用的大小或文字樣式。"));return false;}
    double size=0,fontSize=0;std::uint32_t color=0;std::wstring family;
    if(resize && !ReadNumber(f[IdTileSize],80,480,size)){Error(Tr(L"圖形寬度請輸入 80～480 像素。"));return false;}
    if(typography) {
        family=TextOf(f[IdFontFamily]);FontFamily available(family.c_str());
        if(family.empty() || Utf8(family).size()>256 || AvailableTextStyle(available)<0) {
            Error(Tr(L"請選擇本機可用且支援繪製的字體。"));return false;
        }
        if(!ReadNumber(f[IdFontSize],8,96,fontSize)){Error(Tr(L"字級請輸入 8～96 像素。"));return false;}
        if(!ParseColor(Utf8(TextOf(f[IdTextColor])),color)){Error(Tr(L"文字顏色請輸入六位色碼，例如 #FFFFFF。"));return false;}
    }
    EndDrag(false);if(app.sharedClock)TickTimers();
    auto candidate=app.tiles;
    if(resize && !Resize(candidate,app.selection,app.primary,size)) {
        Error(Tr(L"調整後會與其他格子重疊。請選取整組一起調整，或先移開附近格子；設定已保留原狀。"));return false;
    }
    std::set<int> resized;
    for(auto& t:candidate)if(app.selection.count(t.id)) {
        if(t.size!=Find(t.id)->size)resized.insert(t.id);
        if(typography){t.fontFamily=Utf8(family);t.fontSize=fontSize;t.textColor=color;}
    }
    Status(Tr(L"[大小／文字 2/3] 更新 ")+std::to_wstring(app.selection.size())+TrCounted(L" 格",app.selection.size()));
    app.tiles=std::move(candidate);
    if(resize)for(auto& t:app.tiles)if(app.selection.count(t.id)) {
        Position(t);UpdateTileRegion(t);
        if(resized.count(t.id) && t.kind==Kind::Image){app.views[t.id].imagePath.clear();LoadImage(t);}
    }
    Repaint();SaveState();RefreshLayout();
    auto status=app.savingFailed?Tr(L"[大小／文字 3/3] 顯示已更新，但儲存失敗。"):Tr(L"[大小／文字 3/3] 已套用並儲存。");
    Status(status);SetWindowTextW(f[231],status);return !app.savingFailed;
}
int CALLBACK CollectFonts(const LOGFONTW* font,const TEXTMETRICW*,DWORD,LPARAM data) {
    if(font->lfFaceName[0] && font->lfFaceName[0]!=L'@') {
        FontFamily family(font->lfFaceName);
        if(AvailableTextStyle(family)>=0)reinterpret_cast<std::set<std::wstring>*>(data)->insert(font->lfFaceName);
    }
    return 1;
}
void ShowLayout() {
    if(app.selection.empty())return;
    ShowControl();
    if(!app.layout) {
        RECT bounds{0,0,UiWidth(492),445};DWORD style=WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU;
        AdjustWindowRect(&bounds,style,FALSE);RECT parent;GetWindowRect(app.control,&parent);
        app.layout=CreateWindowExW(WS_EX_DLGMODALFRAME,LayoutClass,Tr(L"DeskTiles · 大小與文字"),style,
            parent.left+40,parent.top+65,bounds.right-bounds.left,bounds.bottom-bounds.top,app.control,nullptr,app.instance,nullptr);
        auto field=[](int id,const wchar_t* type,const wchar_t* text,DWORD style,int x,int y,int w,int h) {
            return WidgetFor(app.layout,app.layoutWidgets,id,type,text,style,x,y,w,h);
        };
        field(230,L"STATIC",L"",0,22,20,454,26);
        field(IdResizeCheck,L"BUTTON",Tr(L"套用大小"),WS_TABSTOP|BS_AUTOCHECKBOX,22,60,115,30);
        field(IdTileSize,L"EDIT",L"160",WS_TABSTOP|ES_AUTOHSCROLL,150,60,80,30);
        field(232,L"STATIC",Tr(L"寬度 px（80～480）"),0,242,65,226,24);
        field(IdTextCheck,L"BUTTON",Tr(L"套用文字樣式（含計時數字）"),WS_TABSTOP|BS_AUTOCHECKBOX,22,108,442,30);
        SendMessageW(app.layoutWidgets[IdTextCheck],BM_SETCHECK,BST_CHECKED,0);
        field(233,L"STATIC",Tr(L"字體"),0,22,160,110,24);
        field(IdFontFamily,L"COMBOBOX",L"",WS_TABSTOP|WS_VSCROLL|CBS_DROPDOWN|CBS_AUTOHSCROLL|CBS_SORT,150,155,318,250);
        SendMessageW(app.layoutWidgets[IdFontFamily],CB_LIMITTEXT,31,0);
        field(234,L"STATIC",Tr(L"字級"),0,22,207,110,24);
        field(IdFontSize,L"EDIT",L"14",WS_TABSTOP|ES_AUTOHSCROLL,150,201,80,30);
        field(235,L"STATIC",Tr(L"像素（8～96）"),0,242,207,226,24);
        field(236,L"STATIC",Tr(L"文字顏色"),0,22,254,110,24);
        field(IdTextColor,L"EDIT",L"#EBF4F5",WS_TABSTOP|ES_AUTOHSCROLL,150,248,170,30);
        SendMessageW(app.layoutWidgets[IdTextColor],EM_SETLIMITTEXT,7,0);
        field(IdChooseTextColor,L"BUTTON",Tr(L"選擇顏色…"),WS_TABSTOP,334,246,134,34);
        field(237,L"STATIC",Tr(L"只套用勾選項目。主選取格位置保持不變。\r\n多選縮放會等比例調整位置；空間不足時保留原狀。"),0,22,303,448,45);
        field(IdApplyLayout,L"BUTTON",Tr(L"套用至所有選取格"),WS_TABSTOP|BS_DEFPUSHBUTTON,22,365,290,38);
        field(IdCloseLayout,L"BUTTON",Tr(L"關閉"),WS_TABSTOP,330,365,138,38);
        field(231,L"STATIC",Tr(L"修改後按套用；內容文字不會被覆寫。"),SS_PATHELLIPSIS,22,417,448,22);
        Status(Tr(L"[字體清單 1/2] 讀取本機已安裝字體"));
        std::set<std::wstring> names;LOGFONTW query{};query.lfCharSet=DEFAULT_CHARSET;
        HDC dc=GetDC(app.layout);EnumFontFamiliesExW(dc,&query,CollectFonts,reinterpret_cast<LPARAM>(&names),0);ReleaseDC(app.layout,dc);
        for(const auto& name:names)SendMessageW(app.layoutWidgets[IdFontFamily],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(name.c_str()));
        Status(Tr(L"[字體清單 2/2] 已讀取 ")+std::to_wstring(names.size())+TrCounted(L" 個字體。",names.size()));
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
    auto summary=tile?L"#"+std::to_wstring(tile->id)+Tr(L" · 選取 ")+std::to_wstring(app.selection.size())+TrCounted(L" 格",app.selection.size())+
        (mixed?Tr(L" · 混合設定"):L""):Tr(L"請選取圖形");
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
    Log(Tr(L"背景輸入：選取圖形；輸出：")+app.statePath.wstring());Status(Tr(L"[背景 1/3] 驗證設定"));
    std::uint32_t value=0;double transparency=0;
    if(!color && !alpha){Error(Tr(L"請勾選要套用的項目。"));return false;}
    if(color && !ParseColor(Utf8(TextOf(f[IdBackgroundColor])),value)){Error(Tr(L"背景顏色請輸入六位色碼。"));return false;}
    if(alpha && !ReadNumber(f[IdBackgroundAlpha],0,100,transparency)){Error(Tr(L"背景透明度請輸入 0～100%。"));return false;}
    Status(Tr(L"[背景 2/3] 套用至 ")+std::to_wstring(app.selection.size())+TrCounted(L" 格",app.selection.size()));
    for(auto& tile:app.tiles)if(app.selection.count(tile.id)) {
        if(color)tile.backgroundColor=value;if(alpha)tile.backgroundTransparency=transparency;
    }
    Repaint();SaveState();RefreshBackground();
    auto status=app.savingFailed?Tr(L"[背景 3/3] 已更新顯示，但儲存失敗。"):Tr(L"[背景 3/3] 已更新並儲存。");
    Status(status);SetWindowTextW(f[241],status);return !app.savingFailed;
}
void ShowBackground() {
    if(app.selection.empty())return;ShowControl();
    if(!app.backgroundControl) {
        RECT bounds{0,0,UiWidth(478),325};DWORD style=WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU;AdjustWindowRect(&bounds,style,FALSE);
        RECT parent;GetWindowRect(app.control,&parent);
        app.backgroundControl=CreateWindowExW(WS_EX_DLGMODALFRAME,BackgroundClass,Tr(L"DeskTiles · 背景與透明度"),style,
            parent.left+40,parent.top+90,bounds.right-bounds.left,bounds.bottom-bounds.top,app.control,nullptr,app.instance,nullptr);
        auto field=[](int id,const wchar_t* type,const wchar_t* text,DWORD style,int x,int y,int w,int h) {
            return WidgetFor(app.backgroundControl,app.backgroundWidgets,id,type,text,style,x,y,w,h);
        };
        field(240,L"STATIC",L"",0,22,20,434,25);
        field(IdBackgroundColorCheck,L"BUTTON",Tr(L"套用背景色"),WS_TABSTOP|BS_AUTOCHECKBOX,22,65,125,30);
        field(IdBackgroundColor,L"EDIT",L"#172730",WS_TABSTOP|ES_AUTOHSCROLL,160,65,148,30);
        field(IdChooseBackgroundColor,L"BUTTON",Tr(L"選擇顏色…"),WS_TABSTOP,322,63,134,34);
        SendMessageW(app.backgroundWidgets[IdBackgroundColor],EM_SETLIMITTEXT,7,0);
        field(IdBackgroundAlphaCheck,L"BUTTON",Tr(L"套用透明度"),WS_TABSTOP|BS_AUTOCHECKBOX,22,117,125,30);
        field(IdBackgroundAlpha,L"EDIT",L"0",WS_TABSTOP|ES_AUTOHSCROLL,160,117,80,30);
        field(242,L"STATIC",Tr(L"%（0 不透明，100 全透明）"),0,251,122,218,25);
        field(243,L"STATIC",Tr(L"只影響背景；文字、圖片與邊框保持原樣。\r\n完全透明處仍可選取、拖曳和使用右鍵選單。"),0,22,175,434,45);
        field(IdApplyBackground,L"BUTTON",Tr(L"套用至所有選取格"),WS_TABSTOP|BS_DEFPUSHBUTTON,22,240,285,38);
        field(IdCloseBackground,L"BUTTON",Tr(L"關閉"),WS_TABSTOP,323,240,133,38);
        field(241,L"STATIC",Tr(L"修改後按套用。"),SS_PATHELLIPSIS,22,296,434,23);
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

Json ReadUiPreferences() {
    auto path=app.directory/L"ui-settings.json";
    if(!fs::exists(path))return Json::object();
    if(fs::file_size(path)>4096)throw std::runtime_error(Utf8(Tr(L"介面設定檔過大")));
    std::ifstream file(path,std::ios::binary);Json value=Json::parse(file);
    if(!value.is_object())throw std::runtime_error("UI settings must be an object");
    return value;
}
Language LoadLanguagePreference() {
    try {auto prefs=ReadUiPreferences();return LanguageFromCode(prefs.value("language",std::string("zh-TW")));}
    catch(...) {OutputDebugStringW(Tr(L"介面設定無效，使用繁體中文。"));return Language::TraditionalChinese;}
}
void SaveLanguagePreference(Language value) {
    Json prefs=Json::object();try{prefs=ReadUiPreferences();}catch(...){}
    prefs["version"]=1;prefs["language"]=LanguageCode(value);
    AtomicOutput(app.directory/L"ui-settings.json",[&](std::ostream& out){out<<std::setw(2)<<prefs<<'\n';});
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
        Log(Tr(L"JSON 匯出輸入：")+app.statePath.wstring()+Tr(L"；輸出：")+path.wstring());
        Status(Tr(L"[JSON 匯出 1/3] 整理配置與圖片"));if(app.sharedClock)TickTimers();
        auto bundle=Pack(app.tiles,[](const std::string& name){
            Log(Tr(L"圖片匯出輸入：")+Wide(name));Asset asset;asset.bytes=ReadBytes(fs::u8path(name),ImageLimit);ValidateAsset(asset);return asset;
        });
        Status(Tr(L"[JSON 匯出 2/3] 驗證內嵌圖片與配置"));auto json=BundleJson(bundle);ReadBundle(json);
        Status(Tr(L"[JSON 匯出 3/3] 寫入 ")+path.wstring());
        AtomicOutput(path,[&](std::ostream& out){out<<std::setw(2)<<json<<'\n';});
        Status(Tr(L"[JSON 匯出 3/3] 已完成：")+path.wstring());return true;
    }catch(const std::exception& e){Error(Tr(L"JSON 匯出失敗：")+LocalizeError(Wide(e.what())));return false;}
}
bool ImportConfig(const fs::path& requested,bool merge) {
    try {
        auto path=fs::absolute(requested);Log(Tr(L"JSON 匯入輸入：")+path.wstring()+Tr(L"；輸出：")+app.statePath.wstring()+Tr(L"；圖片目錄：")+(app.directory/L"assets").wstring());
        Status(Tr(L"[JSON 匯入 1/4] 讀取檔案"));
        if(!fs::is_regular_file(path) || fs::file_size(path)>JsonLimit)throw std::runtime_error("JSON 不存在或超過 128 MiB");
        std::ifstream in(path,std::ios::binary);auto bundle=ParseBundle(in);in.close();
        Status(Tr(L"[JSON 匯入 2/4] 驗證配置與圖片"));
        std::map<std::string,fs::path> imagePaths;int missingFonts=0;
        for(const auto& item:bundle.images) {
            ValidateAsset(item.second);auto extension=item.second.mime=="image/png"?L".png":item.second.mime=="image/jpeg"?L".jpg":L".bmp";
            imagePaths[item.first]=app.directory/L"assets"/(Wide(Digest(item.second.bytes))+extension);
        }
        for(auto& t:bundle.tiles) {
            Wide(t.text);auto family=Wide(t.fontFamily);FontFamily font(family.c_str());
            if(AvailableTextStyle(font)<0)++missingFonts;
            auto at=bundle.tileImages.find(t.id);if(at!=bundle.tileImages.end())t.image=imagePaths.at(at->second).u8string();
        }
        EndDrag(false);if(app.sharedClock)TickTimers();
        auto candidate=merge?Merge(app.tiles,bundle.tiles):bundle.tiles;ValidateTiles(candidate);
        for(auto& tile:candidate)if(tile.kind==Kind::Alarm)ArmAlarm(tile.alarm,static_cast<std::int64_t>(std::time(nullptr)));
        auto previous=app.tiles;std::set<int> previousIds;for(const auto& t:previous)previousIds.insert(t.id);
        Status(Tr(L"[JSON 匯入 3/4] 備份並保存內嵌圖片"));
        auto backup=app.directory/(L"import-before-"+std::to_wstring(GetTickCount64())+L".hexdesk");
        Log(Tr(L"匯入備份輸出：")+backup.wstring());
        AtomicOutput(backup,[&](std::ostream& out){Save(out,previous);});
        fs::create_directories(app.directory/L"assets");
        for(const auto& item:bundle.images) {
            const auto& imagePath=imagePaths.at(item.first);
            Log(Tr(L"圖片還原輸入：")+path.wstring()+L"（"+Wide(item.first)+Tr(L"）；輸出：")+imagePath.wstring());
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
        Status(Tr(L"[JSON 匯入 4/4] ")+std::wstring(merge?Tr(L"合併"):Tr(L"覆蓋"))+Tr(L"完成；共 ")+std::to_wstring(app.tiles.size())+TrCounted(L" 格。",app.tiles.size())+
            (missingFonts?Tr(L" 部分字體使用系統替代字體。"):L""));return true;
    }catch(const std::exception& e){Error(Tr(L"JSON 匯入失敗：")+LocalizeError(Wide(e.what())));return false;}
}
void ChooseExport() {
    wchar_t name[32768]=L"DeskTiles-config.json";OPENFILENAMEW dialog{sizeof(dialog)};
    dialog.hwndOwner=app.control;dialog.lpstrFile=name;dialog.nMaxFile=32768;
    dialog.lpstrFilter=Tr(L"桌面拼格 JSON 配置\0*.json\0");dialog.lpstrDefExt=L"json";
    dialog.Flags=OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    if(GetSaveFileNameW(&dialog))ExportConfig(name);
}
void ChooseImport(bool merge) {
    wchar_t name[32768]{};OPENFILENAMEW dialog{sizeof(dialog)};
    dialog.hwndOwner=app.control;dialog.lpstrFile=name;dialog.nMaxFile=32768;
    dialog.lpstrFilter=Tr(L"桌面拼格 JSON 配置\0*.json\0");dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    if(GetOpenFileNameW(&dialog)) {
        if(!merge && MessageBoxW(app.control,Tr(L"以選取的 JSON 覆蓋目前整體配置？會先備份目前版面。尚未套用的編輯請先套用。"),Tr(L"匯入：覆蓋配置"),MB_YESNO|MB_ICONQUESTION)!=IDYES)return;
        ImportConfig(name,merge);
    }
}
void CreateControls() {
    app.titleFont=CreateFontW(-32,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    SendMessageW(Widget(215,L"STATIC",L"",SS_ICON|SS_REALSIZEIMAGE,26,13,52,64),STM_SETICON,reinterpret_cast<WPARAM>(app.brandIcon),0);
    SendMessageW(Widget(210,L"STATIC",L"DESKTILES",0,92,23,250,42),WM_SETFONT,reinterpret_cast<WPARAM>(app.titleFont),TRUE);
    Widget(211,L"STATIC",Tr(L"桌面上的一小格，留給重要的事。"),0,28,80,430,26);
    Widget(IdConfig,L"BUTTON",Tr(L"配置 JSON…"),WS_TABSTOP,475,78,142,30);
    Widget(IdLanguageLabel,L"STATIC",Tr(L"語言"),0,350,31,110,26);
    Widget(IdLanguage,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,475,25,142,130);
    for(auto label:{L"繁體中文",L"English"})SendMessageW(app.widgets[IdLanguage],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label));
    SendMessageW(app.widgets[IdLanguage],CB_SETCURSEL,language==Language::English?1:0,0);
    Widget(212,L"STATIC",Tr(L"桌面圖形"),0,26,115,160,24);
    Widget(IdList,L"LISTBOX",L"",WS_TABSTOP|WS_VSCROLL|LBS_NOTIFY|LBS_NOINTEGRALHEIGHT|WS_BORDER,26,147,175,207);
    Widget(IdNewShape,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,26,366,175,160);
    for(auto name:{Tr(L"六角形（預設）"),Tr(L"三角形"),Tr(L"矩形（3:2）")})SendMessageW(app.widgets[IdNewShape],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(name));
    SendMessageW(app.widgets[IdNewShape],CB_SETCURSEL,0,0);
    Widget(IdAdd,L"BUTTON",Tr(L"＋ 新增圖形"),WS_TABSTOP,26,408,175,34);
    Widget(IdAppearance,L"BUTTON",Tr(L"邊框與風格…"),WS_TABSTOP,26,449,175,34);
    Widget(IdLayout,L"BUTTON",Tr(L"大小與文字…"),WS_TABSTOP,26,490,175,34);
    Widget(IdBackground,L"BUTTON",Tr(L"背景與透明度…"),WS_TABSTOP,26,531,175,34);
    Widget(IdDelete,L"BUTTON",Tr(L"刪除選取…"),WS_TABSTOP,26,572,175,34);
    Widget(205,L"STATIC",Tr(L"請先選取圖形"),0,227,115,390,25);
    Widget(IdType,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,227,150,390,180);
    for(auto label:{Tr(L"文字"),Tr(L"圖片"),Tr(L"計時器"),Tr(L"鬧鐘")}) SendMessageW(app.widgets[IdType],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label));
    SendMessageW(app.widgets[IdType],CB_SETCURSEL,0,0);
    Widget(201,L"STATIC",Tr(L"文字內容"),0,227,200,390,23);
    Widget(IdText,L"EDIT",L"",WS_TABSTOP|WS_VSCROLL|ES_MULTILINE|ES_AUTOVSCROLL|ES_WANTRETURN,227,232,390,190);
    SendMessageW(app.widgets[IdText],EM_SETLIMITTEXT,16000,0);
    Widget(202,L"STATIC",Tr(L"圖片絕對路徑 · 等比例裁切填滿"),0,227,200,390,23);
    Widget(IdImage,L"EDIT",L"",WS_TABSTOP|ES_AUTOHSCROLL,227,232,390,32);
    Widget(IdBrowse,L"BUTTON",Tr(L"瀏覽圖片…"),WS_TABSTOP,227,278,150,36);
    Widget(203,L"STATIC",Tr(L"計時方式"),0,227,298,95,26);
    Widget(IdMode,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,327,296,290,130);
    for(auto label:{Tr(L"倒數計時"),Tr(L"正向計時")}) SendMessageW(app.widgets[IdMode],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label));
    Widget(204,L"STATIC",Tr(L"倒數總秒數"),0,227,340,130,26);
    Widget(IdSeconds,L"EDIT",L"300",WS_TABSTOP|ES_AUTOHSCROLL,367,338,250,32);
    SendMessageW(app.widgets[IdSeconds],EM_SETLIMITTEXT,12,0);
    Widget(IdStart,L"BUTTON",Tr(L"開始計時"),WS_TABSTOP,227,384,185,34);
    Widget(IdReset,L"BUTTON",Tr(L"重設計時"),WS_TABSTOP,432,384,185,34);
    Widget(IdAlarmSettings,L"BUTTON",Tr(L"鬧鐘設定…"),WS_TABSTOP,227,310,390,42);
    Widget(245,L"STATIC",L"",0,227,365,390,52);
    Widget(IdStartup,L"BUTTON",Tr(L"Windows 登入後自動啟動（需同意）"),WS_TABSTOP|BS_AUTOCHECKBOX,26,685,590,32);
    RefreshStartup();
    Widget(IdApply,L"BUTTON",Tr(L"套用至所有選取格"),WS_TABSTOP|BS_DEFPUSHBUTTON,227,445,390,42);
    Widget(IdTop,L"BUTTON",Tr(L"選取格置頂"),WS_TABSTOP,227,502,185,38);
    Widget(IdDesktop,L"BUTTON",Tr(L"選取格固定於桌面"),WS_TABSTOP,432,502,185,38);
    Widget(213,L"STATIC",Tr(L"Shift＋點擊多選 · 拖曳整組 · 靠近自動吸附\r\n雙擊編輯 · Esc 取消選取 · 關閉本視窗後仍會繼續顯示"),0,227,552,395,48);
    Widget(214,L"STATIC",(Tr(L"資料位置：")+app.directory.wstring()).c_str(),SS_PATHELLIPSIS,26,615,590,25);
    Widget(200,L"STATIC",Tr(L"準備就緒"),SS_PATHELLIPSIS,26,648,590,25);
}

struct UiDraft {
    HWND window=nullptr;std::wstring text;int kind=0;LRESULT choice=0;DWORD start=0,end=0;bool modified=false;
};
std::vector<UiDraft> CaptureUiDrafts() {
    std::vector<UiDraft> drafts;
    for(auto fields:{&app.widgets,&app.appearanceWidgets,&app.layoutWidgets,&app.backgroundWidgets,&app.alarmWidgets})for(auto item:*fields) {
        if(item.first==IdLanguage)continue;
        wchar_t type[32]{};GetClassNameW(item.second,type,32);UiDraft draft;draft.window=item.second;
        if(_wcsicmp(type,L"EDIT")==0) {
            draft.kind=1;draft.text=TextOf(item.second);draft.modified=SendMessageW(item.second,EM_GETMODIFY,0,0)!=0;
            SendMessageW(item.second,EM_GETSEL,reinterpret_cast<WPARAM>(&draft.start),reinterpret_cast<LPARAM>(&draft.end));
        }else if(_wcsicmp(type,L"COMBOBOX")==0) {
            draft.kind=2;draft.choice=SendMessageW(item.second,CB_GETCURSEL,0,0);draft.text=TextOf(item.second);
            if(item.first==IdFontFamily)draft.kind=3;
        }else if(_wcsicmp(type,L"BUTTON")==0 && (GetWindowLongPtrW(item.second,GWL_STYLE)&BS_TYPEMASK)==BS_AUTOCHECKBOX) {
            draft.kind=4;draft.choice=SendMessageW(item.second,BM_GETCHECK,0,0);
        }
        if(draft.kind)drafts.push_back(std::move(draft));
    }
    return drafts;
}
void RestoreUiDrafts(const std::vector<UiDraft>& drafts) {
    for(const auto& draft:drafts)if(IsWindow(draft.window)) {
        if(draft.kind==1) {
            SetWindowTextW(draft.window,draft.text.c_str());SendMessageW(draft.window,EM_SETSEL,draft.start,draft.end);
            SendMessageW(draft.window,EM_SETMODIFY,draft.modified,0);
        }else if(draft.kind==2 || draft.kind==3) {
            SendMessageW(draft.window,CB_SETCURSEL,draft.choice,0);
            if(draft.kind==3)SetWindowTextW(draft.window,draft.text.c_str());
        }else SendMessageW(draft.window,BM_SETCHECK,draft.choice,0);
    }
    UpdateContentVisibility();UpdateAppearanceControls();UpdateLayoutControls();UpdateBackgroundControls();UpdateAlarmControls();
}
void ResetChoices(HWND window,std::initializer_list<const wchar_t*> keys) {
    if(!window)return;auto current=SendMessageW(window,CB_GETCURSEL,0,0);
    SendMessageW(window,CB_RESETCONTENT,0,0);
    for(auto key:keys)SendMessageW(window,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(Tr(key)));
    SendMessageW(window,CB_SETCURSEL,current,0);
}
bool SwitchLanguage(Language value,bool persist) {
    if(value==language) {if(app.widgets.count(IdLanguage))SendMessageW(app.widgets[IdLanguage],CB_SETCURSEL,value==Language::English?1:0,0);return true;}
    auto drafts=CaptureUiDrafts();HWND focus=GetFocus();
    auto listTop=app.widgets.count(IdList)?SendMessageW(app.widgets[IdList],LB_GETTOPINDEX,0,0):0;
    Log(Tr(L"語言設定輸入：介面選擇；輸出：")+(app.directory/L"ui-settings.json").wstring());
    Status(Tr(L"[語言 1/3] 保存介面偏好"));
    if(persist)try{SaveLanguagePreference(value);}catch(const std::exception& e) {
        Error(Tr(L"無法保存語言設定：")+LocalizeError(Wide(e.what())));
        SendMessageW(app.widgets[IdLanguage],CB_SETCURSEL,language==Language::English?1:0,0);return false;
    }
    EndDrag(false);language=value;
    SetThreadUILanguage(value==Language::English?MAKELANGID(LANG_ENGLISH,SUBLANG_ENGLISH_US):MAKELANGID(LANG_CHINESE,SUBLANG_CHINESE_TRADITIONAL));
    Status(Tr(L"[語言 2/3] 更新介面"));app.loadingControls=true;
    for(auto label:app.uiLabels)if(IsWindow(label.first))SetWindowTextW(label.first,language==Language::English?label.second->english:label.second->chinese);
    SetWindowTextW(app.control,Tr(L"桌面拼格 · DeskTiles"));
    SetWindowTextW(app.widgets[214],(Tr(L"資料位置：")+app.directory.wstring()).c_str());
    if(app.appearance)SetWindowTextW(app.appearance,Tr(L"DeskTiles · 邊框與風格"));
    if(app.layout)SetWindowTextW(app.layout,Tr(L"DeskTiles · 大小與文字"));
    if(app.alarmControl)SetWindowTextW(app.alarmControl,Tr(L"DeskTiles · 鬧鐘設定"));
    RefreshReminder();
    if(app.backgroundControl)SetWindowTextW(app.backgroundControl,Tr(L"DeskTiles · 背景與透明度"));
    ResetChoices(app.widgets[IdNewShape],{L"六角形（預設）",L"三角形",L"矩形（3:2）"});
    ResetChoices(app.widgets[IdType],{L"文字",L"圖片",L"計時器",L"鬧鐘"});
    ResetChoices(app.widgets[IdMode],{L"倒數計時",L"正向計時"});
    if(app.appearance)ResetChoices(app.appearanceWidgets[IdBorderStyle],{L"一般實色",L"像素風",L"RGB 動態燈條",L"單色呼吸燈"});
    if(app.alarmControl)ResetChoices(app.alarmWidgets[IdAlarmRepeat],{L"單次",L"每天",L"指定星期"});
    RefreshControls();RestoreUiDrafts(drafts);
    SendMessageW(app.widgets[IdLanguage],CB_SETCURSEL,language==Language::English?1:0,0);
    SendMessageW(app.widgets[IdList],LB_SETTOPINDEX,listTop,0);
    wcscpy_s(app.tray.szTip,Tr(L"桌面拼格 · DeskTiles"));app.tray.uFlags=NIF_TIP;Shell_NotifyIconW(NIM_MODIFY,&app.tray);app.tray.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP;
    for(auto& tile:app.tiles)SetWindowTextW(app.views[tile.id].window,(L"DeskTiles · "+ShapeName(tile.shape)).c_str());
    Repaint();for(auto& view:app.views)UpdateWindow(view.second.window);
    app.loadingControls=false;if(focus && IsWindow(focus))SetFocus(focus);
    Status(Tr(L"[語言 3/3] 已切換為繁體中文"));return true;
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
    case WM_MOUSEACTIVATE:if(auto* tile=Find(id);tile && !tile->topmost)return MA_NOACTIVATE;break;
    case WM_DWMCOMPOSITIONCHANGED:
        if(!ConfigureTileComposition(window))Error(Tr(L"無法建立透明顯示層"));
        InvalidateRect(window,nullptr,FALSE);return 0;
    case WM_ERASEBKGND:return 1;
    case WM_PAINT: {
        ++app.paintCount;
        PAINTSTRUCT paint;HDC dc=BeginPaint(window,&paint);
        RECT clear;GetClientRect(window,&clear);PatBlt(dc,0,0,clear.right,clear.bottom,BLACKNESS);if(auto* tile=Find(id))PresentTile(*tile);EndPaint(window,&paint);return 0;
    }
    case WM_PRINTCLIENT:if(auto* tile=Find(id)) DrawTile(reinterpret_cast<HDC>(w),*tile);return 0;
    case WM_APP+17:
        if(app.demoCapture)if(auto* tile=Find(id)) {
            auto& frame=RenderTile(*tile);
            Bitmap bitmap(frame.width,frame.height,frame.width*4,PixelFormat32bppPARGB,static_cast<BYTE*>(frame.bits));
            CLSID png{0x557cf406,0x1a04,0x11d3,{0x9a,0x73,0x00,0x00,0xf8,0x1e,0xf3,0x2e}};
            return bitmap.Save((app.directory/(L"demo-tile-"+std::to_wstring(id)+L".png")).c_str(),&png)==Ok;
        }
        return 0;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK: {
        POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)};ClientToScreen(window,&p);
        if(auto* tile=Find(id);tile && tile->topmost)SetFocus(window);BeginDrag(id,{double(p.x),double(p.y)},(w&MK_SHIFT)!=0,message==WM_LBUTTONDBLCLK);return 0;
    }
    case WM_MOUSEMOVE:if(app.dragging && app.dragWindow==window) {
        POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)};ClientToScreen(window,&p);
        DragTo({double(p.x),double(p.y)});return 0;
    }break;
    case WM_LBUTTONUP: {
        if(app.dragging && app.dragWindow==window) {
            POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)};ClientToScreen(window,&p);
            DragTo({double(p.x),double(p.y)});EndDrag(true);
        }
        return 0;
    }
    case WM_CAPTURECHANGED:if(app.dragWindow==window)EndDrag(false);return 0;
    case WM_CANCELMODE:if(app.dragWindow==window)EndDrag(false);return 0;
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
        } else if(LOWORD(w)==IdLanguage && HIWORD(w)==CBN_SELCHANGE && !app.loadingControls) {
            SwitchLanguage(SendMessageW(app.widgets[IdLanguage],CB_GETCURSEL,0,0)==1?Language::English:Language::TraditionalChinese);
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
    SendMessageW(host,WM_LBUTTONUP,0,MAKELPARAM(x,y));
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
    SmokeCheck(TextOf(app.control)==Tr(L"桌面拼格 · DeskTiles") && TextOf(app.widgets[210])==L"DESKTILES",L"正式名稱顯示為桌面拼格 DeskTiles");
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
void SmokeInteraction() {
    Log(L"互動測試輸入：專用測試版面；輸出："+app.directory.wstring());
    auto original=app.tiles;auto originalSelection=app.selection;int originalPrimary=app.primary;
    auto reset=[&]() {
        EndDrag(false);app.tiles=original;app.selection=originalSelection;app.primary=originalPrimary;
        for(auto& tile:app.tiles)Position(tile);RefreshControls();Repaint();
    };
    auto send=[&](int id,UINT message,WPARAM keys,Point screen) {
        POINT p{LONG(std::lround(screen.x)),LONG(std::lround(screen.y))};
        ScreenToClient(app.views[id].window,&p);
        SendMessageW(app.views[id].window,message,keys,MAKELPARAM(p.x,p.y));
    };
    auto nearPosition=[](Point a,Point b){return std::hypot(a.x-b.x,a.y-b.y)<1.1;};
    std::vector<std::pair<bool,std::wstring>> results;
    auto check=[&](bool ok,const std::wstring& name){results.push_back({ok,name});Log((ok?L"PASS ":L"FAIL ")+name);};
    int one=original[0].id,two=original[1].id,three=original[2].id;
    Point delta{-60,100};
    auto group=[&](){app.selection={one,two,three};app.primary=two;RefreshControls();};
    auto start=original[0].position+Point{80,69};
    Status(L"[互動回歸 1/3] 立即選取與快速拖曳");
    group();send(one,WM_LBUTTONDOWN,MK_LBUTTON,start);
    check(app.primary==one && app.selection.size()==3 && TextOf(app.widgets[IdText])==Wide(original[0].text),L"按下立即更新主選取與內容面板，保留多選");
    send(one,WM_LBUTTONUP,0,start+delta);
    check(nearPosition(app.tiles[0].position,original[0].position+delta),L"快速拖曳只收到放開座標仍提交最後位置");
    reset();group();send(one,WM_LBUTTONDBLCLK,MK_LBUTTON,start);
    check(app.selection.size()==3 && app.primary==one,L"第二次按下不提前清除多選");
    send(one,WM_MOUSEMOVE,MK_LBUTTON,start+delta);send(one,WM_LBUTTONUP,0,start+delta);
    check(nearPosition(app.tiles[0].position,original[0].position+delta) && nearPosition(app.tiles[1].position,original[1].position+delta),L"快速連按後拖曳仍移動整組");
    reset();group();send(one,WM_LBUTTONDOWN,MK_SHIFT|MK_LBUTTON,start);
    send(one,WM_MOUSEMOVE,MK_SHIFT|MK_LBUTTON,start+delta);send(one,WM_LBUTTONUP,MK_SHIFT,start+delta);
    check(app.selection.size()==3 && nearPosition(app.tiles[0].position,original[0].position+delta),L"按住 Shift 拖曳已選取格仍保留整組");
    reset();group();send(one,WM_LBUTTONDOWN,MK_SHIFT|MK_LBUTTON,start);send(one,WM_LBUTTONUP,MK_SHIFT,start);
    check(app.selection==std::set<int>{two,three} && app.selection.count(app.primary),L"Shift 單擊取消選取後主格仍有效");
    reset();group();send(one,WM_LBUTTONDOWN,MK_LBUTTON,start);send(one,WM_MOUSEMOVE,MK_LBUTTON,start+delta);
    SetCapture(app.control);
    check(!app.dragging && nearPosition(app.tiles[0].position,original[0].position),L"失去滑鼠擷取取消預覽且不保存錯誤位置");
    ReleaseCapture();send(one,WM_LBUTTONDOWN,MK_LBUTTON,start);send(one,WM_MOUSEMOVE,MK_LBUTTON,start+delta);send(one,WM_LBUTTONUP,0,start+delta);
    check(nearPosition(app.tiles[0].position,original[0].position+delta),L"取消後可立即重新拖曳");
    reset();
    Status(L"[互動回歸 2/3] 小圖形與大字級的實際像素");
    for(auto shape:{Shape::Hexagon,Shape::Triangle,Shape::Rectangle})for(auto kind:{Kind::Text,Kind::Timer}) {
        auto& tile=app.tiles[0];tile.shape=shape;tile.kind=kind;tile.size=80;tile.fontSize=96;
        tile.text="A";tile.textColor=0xFFFFFF;tile.backgroundTransparency=100;tile.elapsed=0;
        Bitmap bitmap(PixelWidth(tile),PixelHeight(tile),PixelFormat32bppARGB);Graphics graphics(&bitmap);DrawTileBody(graphics,tile);
        int pixels=0;for(int y=0;y<PixelHeight(tile);++y)for(int x=0;x<PixelWidth(tile);++x) {
            Color color;bitmap.GetPixel(x,y,&color);if(color.GetA()>230 && color.GetR()>230 && color.GetG()>230 && color.GetB()>230)++pixels;
        }
        check(pixels>4,L"80px "+ShapeName(shape)+(kind==Kind::Text?L"文字":L"計時器")+L" 在 96px 設定仍有可讀內容像素");
    }
    reset();app.selection={two};app.primary=two;RefreshControls();
    send(one,WM_LBUTTONDOWN,MK_LBUTTON,start);
    check(app.selection==std::set<int>{one} && app.primary==one,L"拖曳未選取格時立即切換單選");EndDrag(false);
    reset();group();send(one,WM_LBUTTONDOWN,MK_LBUTTON,start);
    SendMessageW(app.views[two].window,WM_CAPTURECHANGED,0,0);
    check(app.dragging && app.dragWindow==app.views[one].window,L"其他圖形的舊擷取事件不取消目前拖曳");EndDrag(false);
    reset();app.selection={one};app.primary=one;send(one,WM_LBUTTONDOWN,MK_LBUTTON,start);
    send(one,WM_LBUTTONUP,0,start);send(one,WM_LBUTTONDBLCLK,MK_LBUTTON,start);send(one,WM_LBUTTONUP,0,start);
    check(IsWindowVisible(app.control) && !app.dragging,L"普通雙擊放開後仍可開啟控制視窗");
    reset();group();
    check(SetLayer(app.tiles[0],false),L"桌面層拖曳前可附加 Explorer");
    send(one,WM_LBUTTONDOWN,MK_LBUTTON,start);send(one,WM_MOUSEMOVE,MK_LBUTTON,start+delta);send(one,WM_LBUTTONUP,0,start+delta);
    check(nearPosition(app.tiles[0].position,original[0].position+delta),L"桌面層與置頂混合多選仍可拖曳");
    SetLayer(app.tiles[0],true);reset();
    for(const auto& name:{std::string("Microsoft JhengHei UI"),std::string("Consolas"),std::string("HexDesk-Missing-Font-987")}) {
        auto& tile=app.tiles[0];tile.kind=Kind::Text;tile.shape=Shape::Rectangle;tile.size=240;
        tile.fontFamily=name;tile.fontSize=36;tile.text=Utf8(L"繁體中文 ABC\r\n第二行 123");tile.textColor=0xFFFFFF;tile.backgroundTransparency=100;
        Bitmap bitmap(PixelWidth(tile),PixelHeight(tile),PixelFormat32bppARGB);Graphics graphics(&bitmap);DrawTileBody(graphics,tile);
        int upper=0,lower=0;
        for(int y=int(PixelHeight(tile)*.28);y<int(PixelHeight(tile)*.78);++y)for(int x=int(tile.size*.14);x<int(tile.size*.86);++x) {
            Color color;bitmap.GetPixel(x,y,&color);
            if(color.GetA()>230 && color.GetR()>230 && color.GetG()>230 && color.GetB()>230) {
                if(y<PixelHeight(tile)*.53)++upper;else ++lower;
            }
        }
        check(upper>10 && lower>10 && tile.fontSize==36 && tile.fontFamily==name,L"中英多行可顯示，適配後保留字體設定："+Wide(name));
    }
    reset();
    auto glyphFrame=[&](const std::string& font,const std::wstring& text) {
        auto tile=original[0];tile.kind=Kind::Text;tile.shape=Shape::Hexagon;tile.size=180;tile.fontSize=96;
        tile.fontFamily=font;tile.text=Utf8(text);tile.textColor=0xFFFFFF;tile.backgroundTransparency=100;
        Bitmap bitmap(PixelWidth(tile),PixelHeight(tile),PixelFormat32bppARGB);Graphics graphics(&bitmap);DrawTileBody(graphics,tile);
        std::vector<Gdiplus::ARGB> pixels;for(int y=int(PixelHeight(tile)*.28);y<int(PixelHeight(tile)*.78);++y)for(int x=int(tile.size*.14);x<int(tile.size*.86);++x){Color color;bitmap.GetPixel(x,y,&color);pixels.push_back(color.GetValue());}
        return pixels;
    };
    auto chinese=glyphFrame("Microsoft JhengHei UI",L"繁體中文 ABC");
    check(chinese==glyphFrame("Consolas",L"繁體中文 ABC"),L"缺中文字體改用完整中文替代字體，量測與繪製一致");
    check(glyphFrame("Microsoft JhengHei UI",L"ABC")!=glyphFrame("Consolas",L"ABC"),L"所選字體支援英文時保留原本字形");
    reset();for(auto& tile:app.tiles)RenderTile(tile);
    auto cachedBodies=app.bodyBuilds;app.selection={two};app.primary=two;
    send(one,WM_LBUTTONDOWN,MK_LBUTTON,start);send(one,WM_LBUTTONUP,0,start);
    for(auto& tile:app.tiles)RenderTile(tile);
    check(app.bodyBuilds==cachedBodies,L"切換主選取重用文字圖片快取，僅更新選取邊框");
    reset();Status(L"[互動回歸 3/3] 還原專用測試版面與結果");
    bool passed=true;for(const auto& result:results){++app.smokeChecks;passed&=result.first;}
    std::ofstream report(app.directory/L"interaction-results.txt");
    report<<(passed?"PASS":"FAIL")<<" checks="<<results.size()<<'\n';
    for(const auto& result:results)report<<(result.first?"PASS ":"FAIL ")<<Utf8(result.second)<<'\n';
    if(!passed)throw std::runtime_error("拖曳／字體回歸測試失敗，詳見 interaction-results.txt");
}

void SmokeAlarmFeatures() {
    ShowControl();ShowWindow(app.control,SW_SHOW);UpdateWindow(app.control);
    auto original=app.tiles;auto selected=app.selection;int primary=app.primary,base=app.smokeChecks;
    auto serialize=[](const std::vector<Tile>& tiles){std::ostringstream out;Save(out,tiles);return out.str();};
    auto check=[](bool result,const std::wstring& name){SmokeCheck(result,name);};
    Log(L"[鬧鐘與啟動驗證 1/4] 計時器與鬧鐘文字；輸出："+app.directory.wstring());
    auto& tile=app.tiles[0];int id=tile.id;app.primary=id;app.selection={id};RefreshControls();
    SetWindowTextW(app.widgets[IdText],L"專注工作 / Focus");SendMessageW(app.widgets[IdType],CB_SETCURSEL,2,0);UpdateContentVisibility();
    SetWindowTextW(app.widgets[IdSeconds],L"300");ApplyContent();
    check(tile.kind==Kind::Timer&&tile.text==Utf8(L"專注工作 / Focus")&&IsWindowVisible(app.widgets[IdText]),L"計時器保留自訂文字且顯示文字編輯欄");
    ShowAlarm();auto& f=app.alarmWidgets;
    check(SendMessageW(f[IdAlarmRepeat],CB_GETCURSEL,0,0)==0,L"新鬧鐘預設單次");
    auto future=LocalTime(static_cast<std::int64_t>(std::time(nullptr))+180);
    SetWindowTextW(f[IdAlarmDate],Wide(DateText(future)).c_str());SetWindowTextW(f[IdAlarmTime],Wide(AlarmTimeText(future.tm_hour*60+future.tm_min)).c_str());
    SendMessageW(f[IdAlarmEnabled],BM_SETCHECK,BST_CHECKED,0);SetWindowTextW(app.widgets[IdText],L"下午開會 / Meeting");
    check(ApplyAlarmSettings()&&tile.kind==Kind::Alarm&&tile.alarm.repeat==AlarmRepeat::Once&&tile.alarm.enabled&&tile.alarm.nextDue>std::time(nullptr)&&app.sharedClock,
        L"從實際控制項建立未來單次鬧鐘並共用時脈");
    check(tile.text==Utf8(L"下午開會 / Meeting")&&IsWindowVisible(app.widgets[IdText]),L"鬧鐘與自訂文字可同時顯示並保存");
    SendMessageW(f[IdAlarmRepeat],CB_SETCURSEL,2,0);UpdateAlarmControls();
    for(int day=0;day<7;++day)SendMessageW(f[IdAlarmDay0+day],BM_SETCHECK,day==1||day==5?BST_CHECKED:BST_UNCHECKED,0);
    check(!IsWindowEnabled(f[IdAlarmDate])&&IsWindowEnabled(f[IdAlarmDay0]),L"指定星期只啟用星期選項");
    check(ApplyAlarmSettings()&&tile.alarm.weekdays==((1<<1)|(1<<5))&&tile.alarm.repeat==AlarmRepeat::Weekly,L"指定週一週五重複鬧鐘套用");
    auto stable=serialize(app.tiles);
    for(int day=0;day<7;++day)SendMessageW(f[IdAlarmDay0+day],BM_SETCHECK,BST_UNCHECKED,0);
    check(!ApplyAlarmSettings()&&serialize(app.tiles)==stable,L"無效星期集合不覆寫原鬧鐘");
    SendMessageW(f[IdAlarmRepeat],CB_SETCURSEL,0,0);SetWindowTextW(f[IdAlarmDate],L"2020-01-01");
    check(!ApplyAlarmSettings()&&serialize(app.tiles)==stable,L"過期單次鬧鐘不會意外啟用");
    RefreshAlarm();SendMessageW(f[IdAlarmRepeat],CB_SETCURSEL,1,0);
    check(ApplyAlarmSettings()&&tile.alarm.repeat==AlarmRepeat::Daily,L"每天重複模式可套用");

    Log(L"[鬧鐘與啟動驗證 2/4] 實際文字像素、到期與提醒視窗");
    auto saved=tile;
    for(auto shape:{Shape::Hexagon,Shape::Triangle,Shape::Rectangle})for(auto kind:{Kind::Alarm,Kind::Timer}) {
        tile.shape=shape;tile.kind=kind;tile.size=160;tile.fontSize=26;tile.textColor=0xFFFFFF;tile.backgroundColor=0;tile.backgroundTransparency=0;
        tile.text="ABC";app.views[id].surfaceDirty=true;auto& frame=RenderTile(tile);int content=0;
        double top=shape==Shape::Triangle?.74:.65,bottom=shape==Shape::Triangle?.84:.75;
        for(int y=int(frame.height*top);y<int(frame.height*bottom);++y)for(int x=frame.width/4;x<frame.width*3/4;++x)
            content+=static_cast<DWORD*>(frame.bits)[y*frame.width+x]==0xFFFFFFFF;
        check(content>4,L"時間與自訂文字在 "+ShapeName(shape)+L" 中具有獨立可見文字像素");
    }
    tile=saved;tile.kind=Kind::Alarm;tile.alarm.enabled=true;tile.alarm.repeat=AlarmRepeat::Once;
    tile.alarm.nextDue=static_cast<std::int64_t>(std::time(nullptr))-1;tile.text=Utf8(L"提醒測試 / Reminder");
    app.lastTick=std::chrono::steady_clock::now();TickTimers();
    check(!tile.alarm.enabled&&tile.alarm.lastFired>0&&!tile.alarm.nextDue,L"實際 WM_TIMER 路徑消耗單次鬧鐘並停用");
    auto last=tile.alarm.lastFired;TickTimers();check(tile.alarm.lastFired==last,L"後續計時不重複消耗鬧鐘");
    NotifyTile(tile,true,true);ShowWindow(app.reminder,SW_SHOW);UpdateWindow(app.reminder);CaptureWindowPng(app.reminder,app.directory/L"alarm-reminder.png");
    check(IsWindowVisible(app.reminder)&&TextOf(GetDlgItem(app.reminder,IdReminderText)).find(L"Reminder")!=std::wstring::npos,L"非阻塞提醒視窗顯示自訂文字");
    SendMessageW(app.reminder,WM_COMMAND,IdDismiss,0);
    check(!IsWindowVisible(app.reminder)&&app.reminderMessages.empty(),L"提醒可關閉且不結束程式");
    tile=saved;RefreshControls();ShowAlarm();ShowWindow(app.alarmControl,SW_SHOW);UpdateWindow(app.alarmControl);CaptureWindowPng(app.alarmControl,app.directory/L"alarm-settings.png");
    ShowWindow(app.alarmControl,SW_HIDE);

    Log(L"[鬧鐘與啟動驗證 3/4] 讀寫隔離登錄檔；絕不啟用真實自動啟動");
    auto before=ReadStartupValue();SendMessageW(app.widgets[IdStartup],BM_SETCHECK,BST_CHECKED,0);ToggleStartup();
    check(SendMessageW(app.widgets[IdStartup],BM_GETCHECK,0,0)==BST_UNCHECKED&&ReadStartupValue()==before,L"未取得使用者同意不得更動真實登入啟動值");
    auto command=StartupCommand();int argc=0;auto argv=CommandLineToArgvW(command.c_str(),&argc);
    check(argv&&argc==4&&std::wstring(argv[1])==L"--startup"&&std::wstring(argv[3])==app.directory.wstring(),L"開機啟動命令可正確還原絕對程式與資料路徑");if(argv)LocalFree(argv);
    check(!WriteStartupValue(true,command)&&ReadStartupValue()==before,L"底層登錄寫入也拒絕未同意的啟用");
    struct FixtureKey {
        std::wstring key=L"Software\\DeskTiles\\Test-"+std::to_wstring(GetCurrentProcessId());
        ~FixtureKey(){RegDeleteKeyValueW(HKEY_CURRENT_USER,key.c_str(),StartupValue);RegDeleteKeyW(HKEY_CURRENT_USER,key.c_str());}
    } isolated;
    check(WriteStartupValue(true,command,isolated.key.c_str(),true)&&ReadStartupValue(isolated.key.c_str())==command,L"專用測試登錄鍵可保存完整啟動命令");
    check(WriteStartupValue(false,command,isolated.key.c_str())&&ReadStartupValue(isolated.key.c_str()).empty(),L"專用測試登錄鍵可取消啟動命令");
    check(!WriteStartupValue(true,std::wstring(261,L'x'),isolated.key.c_str(),true),L"拒絕超過 Windows 登入命令長度上限的設定");
    check(ReadStartupValue()==before,L"隔離登錄測試後真實啟動設定保持不變");

    Log(L"[鬧鐘與啟動驗證 4/4] 還原專用配置並保存驗證結果");
    app.tiles=original;app.selection=selected;app.primary=primary;
    for(auto& item:app.tiles){UpdateTileRegion(item);Position(item);}UpdateClock();RefreshControls();Repaint();SaveState();
    std::ofstream report(app.directory/L"feature-results.txt");report<<"PASS checks="<<app.smokeChecks-base<<"\nAlarm UI/recurrence/text/reminder and isolated startup registration verified; real Run value unchanged.\n";
}

void SmokeLanguage() {
    int firstCheck=app.smokeChecks;Language initial=language;
    auto original=app.tiles;auto originalSelection=app.selection;int originalPrimary=app.primary;
    app.selection={app.tiles[0].id,app.tiles[1].id};app.primary=app.tiles[0].id;RefreshControls();
    ShowAppearance();ShowLayout();ShowBackground();
    auto drafts=CaptureUiDrafts();
    std::ifstream input(app.statePath,std::ios::binary);std::string layoutBefore((std::istreambuf_iterator<char>(input)),{});input.close();
    app.tiles[2].running=true;app.tiles[2].elapsed=12.5;UpdateClock();
    std::ostringstream modelBefore;Save(modelBefore,app.tiles);
    auto handles=std::vector<HWND>{app.control,app.appearance,app.layout,app.backgroundControl,app.widgets[IdText]};
    SetWindowTextW(app.widgets[IdText],L"未套用的草稿 / Unsaved draft");
    SetWindowTextW(app.widgets[IdImage],L"D:\\Draft 圖片.png");SetWindowTextW(app.widgets[IdSeconds],L"712");
    SendMessageW(app.widgets[IdType],CB_SETCURSEL,2,0);UpdateContentVisibility();
    SetWindowTextW(app.appearanceWidgets[IdBorderWidth],L"9");SetWindowTextW(app.appearanceWidgets[IdRgbSpeed],L"13");
    SendMessageW(app.appearanceWidgets[IdBorderStyle],CB_SETCURSEL,3,0);
    SetWindowTextW(app.layoutWidgets[IdFontFamily],L"Arial");SetWindowTextW(app.layoutWidgets[IdFontSize],L"42");
    SetWindowTextW(app.layoutWidgets[IdTextColor],L"#12AB34");
    SetWindowTextW(app.backgroundWidgets[IdBackgroundAlpha],L"37");
    SendMessageW(app.backgroundWidgets[IdBackgroundColorCheck],BM_SETCHECK,BST_UNCHECKED,0);
    SendMessageW(app.widgets[IdText],EM_SETSEL,2,7);SetFocus(app.widgets[IdLanguage]);
    auto pending=CaptureUiDrafts();
    SmokeCheck(pending.size()>=15 && TextOf(app.widgets[IdText])==L"未套用的草稿 / Unsaved draft" && TextOf(app.layoutWidgets[IdFontSize])==L"42",L"草稿驗證確實涵蓋實際輸入控制項");
    auto pendingPreserved=[&]() {
        auto now=CaptureUiDrafts();if(now.size()!=pending.size())return false;
        for(size_t i=0;i<now.size();++i)if(now[i].window!=pending[i].window || ((now[i].kind==1 || now[i].kind==3) && now[i].text!=pending[i].text) || now[i].choice!=pending[i].choice ||
            now[i].start!=pending[i].start || now[i].end!=pending[i].end || now[i].modified!=pending[i].modified)return false;
        return true;
    };
    Log(L"Language test input: isolated demo controls; output: "+(app.directory/L"language-results.txt").wstring());
    if(language==Language::English)SwitchLanguage(Language::TraditionalChinese);
    SendMessageW(app.widgets[IdLanguage],CB_SETCURSEL,1,0);
    SendMessageW(app.control,WM_COMMAND,MAKEWPARAM(IdLanguage,CBN_SELCHANGE),reinterpret_cast<LPARAM>(app.widgets[IdLanguage]));
    SmokeCheck(language==Language::English && TextOf(app.control)==L"DeskTiles",L"語言下拉選單即時切換英文");
    SmokeCheck(TextOf(app.widgets[IdAdd])==L"+ Add tile" && TextOf(app.widgets[IdConfig])==L"Layout JSON…" &&
        TextOf(app.widgets[IdLanguageLabel])==L"Language",L"主視窗按鈕及語言標籤完整英文");
    SmokeCheck(TextOf(app.appearance)==L"DeskTiles · Border style" && TextOf(app.layout)==L"DeskTiles · Size & text" &&
        TextOf(app.backgroundControl)==L"DeskTiles · Background",L"已開啟設定視窗同步英文");
    SmokeCheck(pendingPreserved(),L"切換保留所有尚未套用內容、字體、數值、核取及文字選取");
    SmokeCheck(GetFocus()==app.widgets[IdLanguage] && handles==std::vector<HWND>{app.control,app.appearance,app.layout,app.backgroundControl,app.widgets[IdText]},
        L"語言切換保留焦點及原視窗，無須重建或重啟");
    SmokeCheck(app.selection==std::set<int>{original[0].id,original[1].id} && app.primary==original[0].id,L"切換保留主格與多選");
    std::ostringstream modelAfter;Save(modelAfter,app.tiles);
    std::ifstream saved(app.statePath,std::ios::binary);std::string layoutAfter((std::istreambuf_iterator<char>(saved)),{});saved.close();
    SmokeCheck(modelBefore.str()==modelAfter.str() && layoutBefore==layoutAfter && app.tiles[2].running && app.tiles[2].elapsed==12.5,L"語言偏好不改動版面、字體、圖片、文字或計時狀態");
    SmokeCheck(LoadLanguagePreference()==Language::English && ReadUiPreferences().at("language")=="en",L"英文偏好獨立保存且可重新讀取");
    SmokeCheck(ShapeName(Shape::Triangle)==L"Triangle" && KindName(Kind::Timer)==L"Timer" &&
        std::wstring(Tr(L"已暫停"))==L"Paused",L"圖形名稱、內容與計時狀態英文");
    SmokeCheck(LocalizeError(L"JSON 配置內的圖形彼此重疊")==L"Tiles overlap in the JSON layout" &&
        LocalizeError(L"版面資料損壞，已保留原始檔案")==L"Layout data is corrupt; the original file was preserved",L"匯入及資料保護錯誤英文");
    auto hasChinese=[](const std::wstring& text){return std::any_of(text.begin(),text.end(),[](wchar_t c){return c>=0x3400 && c<=0x9FFF;});};
    bool translated=true;
    for(auto label:app.uiLabels)if(IsWindow(label.first) && GetDlgCtrlID(label.first)!=214)translated&=!hasChinese(TextOf(label.first));
    SmokeCheck(translated,L"所有主視窗與設定視窗固定標籤無漏譯中文字");
    auto choices=[](HWND window,int at){wchar_t value[256]{};SendMessageW(window,CB_GETLBTEXT,at,reinterpret_cast<LPARAM>(value));return std::wstring(value);};
    SmokeCheck(choices(app.widgets[IdNewShape],2)==L"Rectangle (3:2)" && choices(app.widgets[IdType],1)==L"Image" &&
        choices(app.widgets[IdMode],1)==L"Count up" && choices(app.appearanceWidgets[IdBorderStyle],3)==L"Breathing",L"形狀、內容、計時及邊框選項英文且順序不變");
    HMENU menu=BuildTileMenu(false);wchar_t entry[256]{};
    GetMenuStringW(menu,IdDesktop,entry,256,MF_BYCOMMAND);
    auto submenu=GetSubMenu(menu,GetMenuItemCount(menu)-1);
    SmokeCheck(std::wstring(entry)==L"Pin to desktop" && (GetMenuState(submenu,IdEnglish,MF_BYCOMMAND)&MF_CHECKED),L"右鍵與系統匣共用英文選單及語言核取狀態");DestroyMenu(menu);
    CaptureWindowPng(app.control,app.directory/L"language-english.png");
    CaptureWindowPng(app.appearance,app.directory/L"language-border-english.png");
    CaptureWindowPng(app.layout,app.directory/L"language-text-english.png");
    CaptureWindowPng(app.backgroundControl,app.directory/L"language-background-english.png");
    HANDLE preferenceLock=CreateFileW((app.directory/L"ui-settings.json").c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    SmokeCheck(preferenceLock!=INVALID_HANDLE_VALUE,L"建立語言設定保存失敗測試鎖");
    bool changed=SwitchLanguage(Language::TraditionalChinese);CloseHandle(preferenceLock);
    SmokeCheck(!changed && language==Language::English && LoadLanguagePreference()==Language::English && pendingPreserved(),L"設定無法保存時保留原語言及草稿");
    Command(IdTraditionalChinese);
    SmokeCheck(language==Language::TraditionalChinese && TextOf(app.widgets[IdAdd])==L"＋ 新增圖形" && pendingPreserved(),L"右鍵命令可切回繁體中文且保留草稿");
    SmokeCheck(LoadLanguagePreference()==Language::TraditionalChinese && TextOf(app.layout)==L"DeskTiles · 大小與文字",L"繁體中文偏好與設定視窗還原");
    if(initial!=language)SwitchLanguage(initial);
    app.tiles=original;app.selection=originalSelection;app.primary=originalPrimary;UpdateClock();RefreshControls();RestoreUiDrafts(drafts);Repaint();
    ShowWindow(app.appearance,SW_HIDE);ShowWindow(app.layout,SW_HIDE);ShowWindow(app.backgroundControl,SW_HIDE);
    std::ofstream report(app.directory/L"language-results.txt");report<<"PASS checks="<<app.smokeChecks-firstCheck<<"\nEnglish and Traditional Chinese UI, draft preservation, menus, errors, preference persistence and save failure verified.\n";
}

void SmokeTick() {
    try {
        if(app.smokeStep==0) {
            KillTimer(app.control,2);
            SmokeCheck(app.tiles.size()==3,L"建立三個初始六角形");
            if(!app.interactionOnly)SmokeAlarmFeatures();
            if(app.featureOnly){app.smokeDone=true;PostQuitMessage(0);return;}
            if(!app.interactionOnly)SmokeLanguage();
            if(app.languageOnly){SwitchLanguage(Language::English);app.smokeDone=true;Command(IdExit);return;}
            SmokeInteraction();
            if(app.interactionOnly){app.smokeDone=true;Command(IdExit);return;}
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
            SmokeCheck(SetLayer(first,false) && GetWindow(window,GW_OWNER)==DesktopHost() &&
                !(GetWindowLongPtrW(window,GWL_STYLE)&WS_CHILD) && !(GetWindowLongPtrW(window,GWL_EXSTYLE)&WS_EX_TOPMOST),
                L"Explorer 關聯的桌面圖形保持獨立透明合成並解除置頂");
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
        KillTimer(app.control,2);Error(LocalizeError(Wide(e.what())));app.processExit=1;app.smokeDone=true;Command(IdExit);
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
        else if(std::wstring(argv[i])==L"--interaction-test"){app.smoke=true;app.interactionOnly=true;}
        else if(std::wstring(argv[i])==L"--language-test"){app.smoke=true;app.languageOnly=true;}
        else if(std::wstring(argv[i])==L"--show-existing")showExisting=true;
        else if(std::wstring(argv[i])==L"--startup")app.startupLaunch=true;
        else if(std::wstring(argv[i])==L"--feature-test"){app.smoke=true;app.featureOnly=true;}
        else if(std::wstring(argv[i])==L"--demo-capture")app.demoCapture=true;
    }
    LocalFree(argv);
    if((app.smoke || app.demoCapture) && !customDirectory) {MessageBoxW(nullptr,Tr(L"測試／示範必須指定 --data-dir，避免修改使用者版面。"),Tr(L"桌面拼格"),MB_OK);return 2;}
    app.statePath=app.directory/L"layout.hexdesk";app.logPath=app.directory/L"session.log";
    language=LoadLanguagePreference();
    SetThreadUILanguage(language==Language::English?MAKELANGID(LANG_ENGLISH,SUBLANG_ENGLISH_US):MAKELANGID(LANG_CHINESE,SUBLANG_CHINESE_TRADITIONAL));
    HANDLE lock=INVALID_HANDLE_VALUE;ULONG_PTR gdiplus=0;
    try {
        if(showExisting)return ActivateExisting(app.directory/L"session.lock")?0:3;
        fs::create_directories(app.directory);
        lock=CreateFileW((app.directory/L"session.lock").c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(lock==INVALID_HANDLE_VALUE) {
            DWORD error=GetLastError();bool occupied=error==ERROR_SHARING_VIOLATION || error==ERROR_LOCK_VIOLATION;
            if(occupied && ActivateExisting(app.directory/L"session.lock"))return 0;
            if(!app.smoke) {
                auto message=occupied?Tr(L"這個資料目錄已在使用中，但無法帶回控制視窗。請從系統匣開啟原本的桌面拼格；要切換新版時，先儲存並結束舊版。"):
                    Tr(L"無法開啟資料鎖。請確認下方資料目錄可寫入，或改用可寫入的資料目錄。");
                auto details=std::wstring(message)+Tr(L"\n\n資料位置：")+app.directory.wstring()+Tr(L"\nWindows 錯誤碼：")+std::to_wstring(error);
                MessageBoxW(nullptr,details.c_str(),Tr(L"桌面拼格"),MB_OK|MB_ICONINFORMATION);
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
        Log(Tr(L"輸入絕對路徑：")+app.statePath.wstring());
        Log(Tr(L"輸出絕對路徑：")+app.statePath.wstring()+L"；"+app.logPath.wstring());
        Progress(1,Tr(L"確認輸入與輸出路徑"));ShowWindow(app.progress,SW_SHOWNORMAL);UpdateWindow(app.progress);
        GdiplusStartupInput startup;if(GdiplusStartup(&gdiplus,&startup,nullptr)!=Ok) throw std::runtime_error("GDI+ 初始化失敗");
        app.icon=CreateAppIcon();app.smallIcon=CreateAppIcon(16);app.brandIcon=CreateAppIcon(64);
        Progress(2,Tr(L"載入版面與內容"));
        if(fs::exists(app.statePath)) {
            if(fs::file_size(app.statePath)>40*1024*1024) throw std::runtime_error("版面檔案過大，已保留原始檔案");
            std::ifstream input(app.statePath,std::ios::binary);app.tiles=Load(input);
            for(auto& t:app.tiles) {Wide(t.text);Wide(t.image);Wide(t.fontFamily);app.nextId=std::max(app.nextId,t.id+1);}
        } else {
            RECT area;SystemParametersInfoW(SPI_GETWORKAREA,0,&area,0);
            Point base{double(area.left+90),double(area.top+160)};
            for(int i=0;i<3;++i) {
                Tile t;t.text=Utf8(Tr(L"新文字"));t.id=i+1;t.position=base+Point{double(i*120),i==1?Height/2:0};
                t.kind=static_cast<Kind>(i);
                if(t.kind==Kind::Timer){t.fontSize=22;t.textColor=0x71EDC4;}
                if(i==0)t.text=Utf8(Tr(L"今天的重點\n雙擊開始編輯"));
                app.tiles.push_back(t);
            }
            app.nextId=4;
        }
        for(auto& tile:app.tiles)if(tile.kind==Kind::Alarm)ArmAlarm(tile.alarm,static_cast<std::int64_t>(std::time(nullptr)));
        Progress(3,Tr(L"建立 Windows 顯示與控制視窗"));
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
        RECT controlRect{0,0,UiWidth(646),735};AdjustWindowRectEx(&controlRect,WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,FALSE,0);
        RECT work;SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);
        int x=std::max(work.left,work.right-(controlRect.right-controlRect.left)-50);
        app.control=CreateWindowExW(0,ControlClass,Tr(L"桌面拼格 · DeskTiles"),WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,
            x,work.top+35,controlRect.right-controlRect.left,controlRect.bottom-controlRect.top,nullptr,nullptr,instance,nullptr);
        if(!app.control)throw std::runtime_error("建立控制視窗失敗");
        SendMessageW(app.control,WM_SETICON,ICON_SMALL,reinterpret_cast<LPARAM>(app.smallIcon));
        SendMessageW(app.control,WM_SETICON,ICON_BIG,reinterpret_cast<LPARAM>(app.icon));
        CreateControls();
        for(auto& tile:app.tiles) CreateTileWindow(tile);
        if(!app.tiles.empty()){app.primary=app.tiles.front().id;app.selection={app.primary};}
        Progress(4,Tr(L"準備系統匣與操作選單"));
        app.taskbarCreated=RegisterWindowMessageW(L"TaskbarCreated");AddTray();
        RefreshControls();Repaint();SaveState();UpdateAnimation();
        Progress(5,Tr(L"完成，等待操作"));DestroyWindow(app.progress);app.progress=nullptr;
        if(!app.startupLaunch)ShowControl();
        if(app.smoke) SetTimer(app.control,2,350,nullptr);
        MSG message{};
        while(GetMessageW(&message,nullptr,0,0)>0) {
            if(app.alarmControl&&IsWindowVisible(app.alarmControl)&&(message.hwnd==app.alarmControl||IsChild(app.alarmControl,message.hwnd))) {
                if(message.message==WM_KEYDOWN&&message.wParam==VK_ESCAPE){ShowWindow(app.alarmControl,SW_HIDE);continue;}
                if(IsDialogMessageW(app.alarmControl,&message))continue;
            }
            if(app.reminder&&IsWindowVisible(app.reminder)&&(message.hwnd==app.reminder||IsChild(app.reminder,message.hwnd))) {
                if(message.message==WM_KEYDOWN&&message.wParam==VK_ESCAPE){ShowWindow(app.reminder,SW_HIDE);app.reminderMessages.clear();continue;}
                if(IsDialogMessageW(app.reminder,&message))continue;
            }
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
        auto error=LocalizeError(Wide(e.what()));Log(L"ERROR: "+error);
        if(!app.smoke)MessageBoxW(nullptr,(error+Tr(L"\n\n資料位置：")+app.statePath.wstring()).c_str(),Tr(L"DeskTiles 啟動失敗"),MB_OK|MB_ICONERROR);
    }
    if(app.progress) DestroyWindow(app.progress);
    Shell_NotifyIconW(NIM_DELETE,&app.tray);
    for(auto& view:app.views) {if(IsWindow(view.second.window))DestroyWindow(view.second.window);view.second.image.reset();}
    app.views.clear();
    if(app.alarmControl)DestroyWindow(app.alarmControl);if(app.reminder)DestroyWindow(app.reminder);
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
