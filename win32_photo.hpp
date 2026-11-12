#pragma once
// Included by win32.cpp after the window, image and storage helpers.

namespace photo_crop {
enum { FocusX=401, FocusY, Zoom, Reset };

struct Editor {
    Tile tile;
    std::unique_ptr<Bitmap> source;
    RECT canvas{};
    POINT dragPoint{};
    Point dragFocus{};
    bool dragging=false;
};

bool ReadFields(HWND dialog,Point& focus,double& zoom) {
    BOOL validX=FALSE,validY=FALSE,validZoom=FALSE;
    const UINT x=GetDlgItemInt(dialog,FocusX,&validX,FALSE);
    const UINT y=GetDlgItemInt(dialog,FocusY,&validY,FALSE);
    const UINT scale=GetDlgItemInt(dialog,Zoom,&validZoom,FALSE);
    if(!validX||!validY||!validZoom||x>100||y>100||scale<100||scale>400)return false;
    focus={x/100.,y/100.};zoom=scale/100.;
    return true;
}

void WriteFocus(HWND dialog,Point focus) {
    SetDlgItemInt(dialog,FocusX,static_cast<UINT>(std::lround(focus.x*100)),FALSE);
    SetDlgItemInt(dialog,FocusY,static_cast<UINT>(std::lround(focus.y*100)),FALSE);
}

PhotoFrame CanvasTile(const Editor& editor) {
    const double height=TileHeight(editor.tile);
    const double scale=std::min((editor.canvas.right-editor.canvas.left-16)/editor.tile.size,
        (editor.canvas.bottom-editor.canvas.top-16)/height);
    const double w=editor.tile.size*scale,h=height*scale;
    return {(editor.canvas.left+editor.canvas.right-w)/2,(editor.canvas.top+editor.canvas.bottom-h)/2,w,h};
}

INT_PTR CALLBACK DialogProc(HWND dialog,UINT message,WPARAM w,LPARAM l) {
    if(message==WM_INITDIALOG) {
        auto* editor=reinterpret_cast<Editor*>(l);
        SetWindowLongPtrW(dialog,DWLP_USER,reinterpret_cast<LONG_PTR>(editor));
        SetWindowTextW(dialog,LayoutLabel(L"調整照片裁切",L"Adjust photo crop"));
        RECT bounds{};GetClientRect(dialog,&bounds);
        const int bottom=bounds.bottom, right=bounds.right;
        editor->canvas={12,12,right-12,bottom-117};
        auto control=[&](const wchar_t* type,const wchar_t* text,DWORD style,int id,int x,int y,int width,int height){
            HWND widget=CreateWindowExW(wcscmp(type,L"EDIT")==0?WS_EX_CLIENTEDGE:0,type,text,WS_CHILD|WS_VISIBLE|style,
                x,y,width,height,dialog,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),app.instance,nullptr);
            SendMessageW(widget,WM_SETFONT,reinterpret_cast<WPARAM>(app.font),TRUE);
            return widget;
        };
        control(L"STATIC",LayoutLabel(L"拖曳照片來調整構圖。",L"Drag the photo to reframe it."),0,0,12,bottom-110,right-24,23);
        const int column=(right-24)/3;
        auto number=[&](int id,const wchar_t* label,int x){
            control(L"STATIC",label,0,0,x,bottom-82,column-8,20);
            auto edit=control(L"EDIT",L"",WS_TABSTOP|ES_NUMBER,id,x,bottom-62,column-12,25);
            SendMessageW(edit,EM_SETLIMITTEXT,3,0);
        };
        number(FocusX,LayoutLabel(L"水平 %",L"Horizontal %"),12);
        number(FocusY,LayoutLabel(L"垂直 %",L"Vertical %"),12+column);
        number(Zoom,LayoutLabel(L"縮放 %",L"Zoom %"),12+2*column);
        control(L"BUTTON",LayoutLabel(L"重設",L"Reset"),WS_TABSTOP,Reset,12,bottom-31,78,26);
        control(L"BUTTON",LayoutLabel(L"套用",L"Apply"),WS_TABSTOP|BS_DEFPUSHBUTTON,IDOK,right-178,bottom-31,78,26);
        control(L"BUTTON",LayoutLabel(L"取消",L"Cancel"),WS_TABSTOP,IDCANCEL,right-90,bottom-31,78,26);
        WriteFocus(dialog,editor->tile.photoFocus);
        SetDlgItemInt(dialog,Zoom,static_cast<UINT>(std::lround(editor->tile.photoZoom*100)),FALSE);
        return TRUE;
    }
    auto* editor=reinterpret_cast<Editor*>(GetWindowLongPtrW(dialog,DWLP_USER));
    if(!editor)return FALSE;
    switch(message) {
    case WM_PAINT:{
        PAINTSTRUCT paint{};HDC dc=BeginPaint(dialog,&paint);
        FillRect(dc,&editor->canvas,GetSysColorBrush(COLOR_3DSHADOW));
        {
            Graphics g(dc);
            g.SetSmoothingMode(SmoothingModeAntiAlias);
            g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
            const auto tile=CanvasTile(*editor);
            const auto outline=BorderOutline(editor->tile.borderStyle,0,editor->tile.size,editor->tile.shape,editor->tile.rectangleHeight);
            std::vector<PointF> points;
            for(auto p:outline)points.emplace_back(static_cast<float>(tile.x+p.x*tile.width/editor->tile.size),
                static_cast<float>(tile.y+p.y*tile.height/TileHeight(editor->tile)));
            GraphicsPath clip;clip.AddPolygon(points.data(),static_cast<int>(points.size()));g.SetClip(&clip);
            Point focus=editor->tile.photoFocus;double zoom=editor->tile.photoZoom;
            ReadFields(dialog,focus,zoom);
            const auto crop=FitPhoto(editor->source->GetWidth(),editor->source->GetHeight(),tile.width,tile.height,focus,zoom);
            g.DrawImage(editor->source.get(),static_cast<float>(tile.x+crop.x),static_cast<float>(tile.y+crop.y),
                static_cast<float>(crop.width),static_cast<float>(crop.height));
        }
        EndPaint(dialog,&paint);return TRUE;
    }
    case WM_LBUTTONDOWN:{
        POINT cursor{GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        double zoom=1;
        if(PtInRect(&editor->canvas,cursor)&&ReadFields(dialog,editor->dragFocus,zoom)){
            editor->dragPoint=cursor;SetCapture(dialog);editor->dragging=GetCapture()==dialog;
        }
        return TRUE;
    }
    case WM_MOUSEMOVE:
        if(editor->dragging) {
            Point focus=editor->dragFocus;double zoom=1;
            ReadFields(dialog,focus,zoom);
            const auto tile=CanvasTile(*editor);
            const auto crop=FitPhoto(editor->source->GetWidth(),editor->source->GetHeight(),tile.width,tile.height,editor->dragFocus,zoom);
            const double overflowX=crop.width-tile.width,overflowY=crop.height-tile.height;
            if(overflowX>1)focus.x=std::clamp(editor->dragFocus.x-(GET_X_LPARAM(l)-editor->dragPoint.x)/overflowX,0.,1.);
            if(overflowY>1)focus.y=std::clamp(editor->dragFocus.y-(GET_Y_LPARAM(l)-editor->dragPoint.y)/overflowY,0.,1.);
            WriteFocus(dialog,focus);
        }
        return TRUE;
    case WM_LBUTTONUP:
        if(editor->dragging)ReleaseCapture();
        return TRUE;
    case WM_CAPTURECHANGED:editor->dragging=false;return TRUE;
    case WM_CLOSE:EndDialog(dialog,IDCANCEL);return TRUE;
    case WM_COMMAND:
        switch(LOWORD(w)) {
        case IDOK:
            if(!ReadFields(dialog,editor->tile.photoFocus,editor->tile.photoZoom)){
                MessageBoxW(dialog,LayoutLabel(L"焦點須為 0～100%，縮放須為 100～400%。",
                    L"Use 0–100% for the focus and 100–400% for zoom."),LayoutLabel(L"照片裁切",L"Photo crop"),MB_OK|MB_ICONWARNING);
                return TRUE;
            }
            EndDialog(dialog,IDOK);return TRUE;
        case IDCANCEL:EndDialog(dialog,IDCANCEL);return TRUE;
        case Reset:WriteFocus(dialog,{.5,.5});SetDlgItemInt(dialog,Zoom,100,FALSE);return TRUE;
        case FocusX:case FocusY:case Zoom:
            if(HIWORD(w)==EN_CHANGE)InvalidateRect(dialog,&editor->canvas,FALSE);
            return TRUE;
        }
    }
    return FALSE;
}
} // namespace photo_crop

bool ApplyPhotoCrop(int id,Point focus,double zoom) {
    auto* tile=Find(id);
    if(!tile||tile->kind!=Kind::Image)return false;
    auto next=*tile;next.photoFocus=focus;next.photoZoom=zoom;
    if(!ValidPresentationSettings(next))return false;
    Log(std::wstring(LayoutLabel(L"照片裁切輸入：",L"Photo crop input: "))+Wide(tile->image)+
        LayoutLabel(L"；輸出：",L"; output: ")+app.statePath.wstring());
    Status(LayoutLabel(L"[裁切 1/3] 套用焦點與縮放",L"[Crop 1/3] Apply focus and zoom"));
    const auto previous=*tile;
    *tile=std::move(next);
    Status(LayoutLabel(L"[裁切 2/3] 儲存配置",L"[Crop 2/3] Save layout"));
    SaveState();
    if(app.savingFailed){*tile=previous;return false;}
    app.views[id].imagePath.clear();LoadImage(*tile);Repaint();
    Status(LayoutLabel(L"[裁切 3/3] 照片已更新",L"[Crop 3/3] Photo updated"));
    return true;
}

void ShowPhotoCrop() {
    const auto* tile=Find(app.primary);
    if(!tile||tile->kind!=Kind::Image||tile->image.empty())return;
    photo_crop::Editor editor{*tile,std::unique_ptr<Bitmap>(Bitmap::FromFile(Wide(tile->image).c_str()))};
    if(!editor.source||editor.source->GetLastStatus()!=Ok||!editor.source->GetWidth()||!editor.source->GetHeight()){
        Error(LayoutLabel(L"無法讀取照片。請重新選擇圖片。",L"Could not read the photo. Choose the image again."));
        return;
    }
    struct alignas(DWORD) Template { DLGTEMPLATE header; WORD menu=0,klass=0,title=0; } dialog{};
    dialog.header.style=WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME|DS_CENTER;
    dialog.header.cx=220;dialog.header.cy=180;
    const auto result=DialogBoxIndirectParamW(app.instance,&dialog.header,app.control,photo_crop::DialogProc,
        reinterpret_cast<LPARAM>(&editor));
    if(result==IDOK)ApplyPhotoCrop(editor.tile.id,editor.tile.photoFocus,editor.tile.photoZoom);
}
