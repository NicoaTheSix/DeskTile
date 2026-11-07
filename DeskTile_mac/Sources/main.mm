#import <Cocoa/Cocoa.h>
#import <ImageIO/ImageIO.h>
#import <CommonCrypto/CommonDigest.h>
#include <fstream>
#include <filesystem>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include "config.hpp"
using namespace hexdesk;
namespace fs=std::filesystem;
static NSString *S(const std::string& s){return [[NSString alloc] initWithBytes:s.data() length:s.size() encoding:NSUTF8StringEncoding] ?: @"";}
static std::string U(NSString *s){return s.UTF8String ?: "";}
static NSColor *Color(uint32_t c,double alpha=1){return [NSColor colorWithSRGBRed:((c>>16)&255)/255. green:((c>>8)&255)/255. blue:(c&255)/255. alpha:alpha];}
static double Top(){return NSMaxY(NSScreen.screens.firstObject.frame);}
static NSRect Frame(const Tile& t){return NSMakeRect(t.position.x,Top()-t.position.y-TileHeight(t.size,t.shape),t.size,TileHeight(t.size,t.shape));}
static NSBezierPath *Path(const Tile& t,double inset=0){auto pts=BorderOutline(t.borderStyle==BorderStyle::Pixel?BorderStyle::Pixel:BorderStyle::Solid,inset,t.size,t.shape);auto p=[NSBezierPath bezierPath];[p moveToPoint:NSMakePoint(pts[0].x,pts[0].y)];for(size_t i=1;i<pts.size();++i)[p lineToPoint:NSMakePoint(pts[i].x,pts[i].y)];[p closePath];return p;}
@class Controller;
static Controller *app;
@interface ControlWindow:NSWindow @end
@interface TileWindow:NSPanel @end
@implementation TileWindow
-(BOOL)canBecomeKeyWindow{return YES;}
-(BOOL)canBecomeMainWindow{return NO;}
@end
@interface TileView:NSView
@property int tileID;
@end
@interface FlippedView:NSView @end
@implementation FlippedView
-(BOOL)isFlipped{return YES;}
-(void)drawRect:(NSRect)rect{[Color(0xF3F7F5) setFill];NSRectFill(rect);}
@end
@interface Controller:NSObject<NSApplicationDelegate,NSWindowDelegate,NSTableViewDataSource,NSTableViewDelegate>{
@public
 std::vector<Tile> tiles;
 std::set<int> selected;
 std::vector<Tile> dragStart;
 NSPoint dragMouse;
 BOOL dragging;
 int primary;
 NSString *directory;
 int lockFD;
 BOOL ready;
 double lastTick;
 NSMutableDictionary<NSNumber*,TileWindow*> *windows;
 NSMutableDictionary<NSString*,NSImage*> *images;
 NSWindow *control;
 NSTableView *list;
 NSPopUpButton *shape,*kind,*mode;
 NSTextView *text;
 NSTextField *imageField,*seconds,*selectionLabel,*status;
 NSScrollView *textScroll;
 NSView *imageGroup,*timerGroup;
 NSStatusItem *tray;
 NSTimer *clock;
 NSMutableDictionary<NSString*,NSControl*> *fields;
 NSWindow *settings;
 NSString *settingType;
}
-(Tile*)tile:(int)ident;
-(void)refresh;
-(void)rebuild;
-(void)save;
-(void)show:(id)sender;
-(void)action:(id)sender;
-(void)tick:(NSTimer*)timer;
-(NSMenu*)menu;
-(NSImage*)image:(NSString*)path;
-(void)error:(NSString*)message;
@end
@implementation ControlWindow
-(void)cancelOperation:(id)sender{app->selected.clear();app->primary=0;[app refresh];}
@end
@implementation TileView
-(BOOL)isFlipped{return YES;}
-(BOOL)acceptsFirstResponder{return YES;}
-(NSView*)hitTest:(NSPoint)p{Tile *t=[app tile:self.tileID];return t&&[Path(*t) containsPoint:[self convertPoint:p fromView:self.superview]]?self:nil;}
-(void)drawRect:(NSRect)dirty{
 Tile *t=[app tile:self.tileID];if(!t)return;
 [NSGraphicsContext saveGraphicsState];NSBezierPath *outline=Path(*t);[outline addClip];
 [Color(t->backgroundColor,std::max(.001,1-t->backgroundTransparency/100)) setFill];[outline fill];
 if(t->kind==Kind::Image){NSImage *im=[app image:S(t->image)];if(im){NSSize sz=im.size;double scale=std::max(t->size/sz.width,TileHeight(t->size,t->shape)/sz.height);NSRect dest=NSMakeRect((t->size-sz.width*scale)/2,(TileHeight(t->size,t->shape)-sz.height*scale)/2,sz.width*scale,sz.height*scale);[im drawInRect:dest fromRect:NSZeroRect operation:NSCompositingOperationSourceOver fraction:1 respectFlipped:YES hints:nil];}}
 else{NSString *value=S(t->kind==Kind::Timer?Clock(*t):t->text);NSFont *font=[NSFont fontWithName:S(t->fontFamily) size:t->fontSize] ?: [NSFont systemFontOfSize:t->fontSize];NSMutableParagraphStyle *style=[NSMutableParagraphStyle new];style.alignment=NSTextAlignmentCenter;style.lineBreakMode=NSLineBreakByWordWrapping;NSDictionary *attrs=@{NSFontAttributeName:font,NSForegroundColorAttributeName:Color(t->textColor),NSParagraphStyleAttributeName:style};double inset=t->shape==Shape::Triangle?t->size*.28:t->size*.15;NSRect area=NSInsetRect(self.bounds,inset,12);if(t->shape==Shape::Triangle){area.origin.y=self.bounds.size.height*.44;area.size.height=self.bounds.size.height*.42;}NSRect measured=[value boundingRectWithSize:area.size options:NSStringDrawingUsesLineFragmentOrigin attributes:attrs];area.origin.y+=std::max(0.,(area.size.height-measured.size.height)/2);[value drawWithRect:area options:NSStringDrawingUsesLineFragmentOrigin attributes:attrs];}
 [NSGraphicsContext restoreGraphicsState];
 double now=NSProcessInfo.processInfo.systemUptime;uint32_t c=t->borderColor;double alpha=1;
 if(t->borderStyle==BorderStyle::Rgb)c=Rainbow(ColorHue(c)+(t->rgbAnimated?now/t->rgbCycleSeconds:0));
 if(t->borderStyle==BorderStyle::Breathing)alpha=BreathLevel(now,t->rgbCycleSeconds,t->rgbAnimated);
 [Color(c,alpha) setStroke];NSBezierPath *border=Path(*t,t->borderWidth/2);border.lineWidth=t->borderWidth;[border stroke];
 if(app->selected.count(t->id)){[Color(0x71EDC4) setStroke];NSBezierPath *p=Path(*t,t->borderWidth+3);p.lineWidth=1;CGFloat dash[]={4,3};[p setLineDash:dash count:2 phase:0];[p stroke];}
}
-(void)mouseDown:(NSEvent*)e{
 Tile *t=[app tile:self.tileID];if(!t)return;
 if(e.clickCount==2){app->selected={self.tileID};app->primary=self.tileID;[app refresh];[app show:nil];return;}
 BOOL shift=(e.modifierFlags&NSEventModifierFlagShift)!=0;
 if(shift)Select(app->selected,self.tileID,true);else if(!app->selected.count(self.tileID))Select(app->selected,self.tileID,false);
 app->primary=app->selected.count(self.tileID)?self.tileID:(app->selected.empty()?0:*app->selected.begin());
 app->dragStart=app->tiles;app->dragMouse=NSEvent.mouseLocation;app->dragging=NO;[app refresh];
}
-(void)mouseDragged:(NSEvent*)e{
 if(!app->selected.count(self.tileID))return;app->dragging=YES;NSPoint p=NSEvent.mouseLocation;
 hexdesk::Point desired{p.x-app->dragMouse.x,app->dragMouse.y-p.y};auto delta=Snap(app->dragStart,app->selected,desired);app->tiles=app->dragStart;Move(app->tiles,app->selected,delta);
 for(auto& t:app->tiles)[app->windows[@(t.id)] setFrame:Frame(t) display:YES];
}
-(void)mouseUp:(NSEvent*)e{
 if(app->dragging){if(!ValidDelta(app->dragStart,app->selected,[app tile:self.tileID]->position-std::find_if(app->dragStart.begin(),app->dragStart.end(),[&](auto& t){return t.id==self.tileID;})->position))app->tiles=app->dragStart;app->dragging=NO;[app rebuild];[app save];}
 else if(!(e.modifierFlags&NSEventModifierFlagShift)){app->selected={self.tileID};app->primary=self.tileID;[app refresh];}
}
-(void)rightMouseDown:(NSEvent*)e{if(!app->selected.count(self.tileID)){app->selected={self.tileID};app->primary=self.tileID;[app refresh];}[NSMenu popUpContextMenu:[app menu] withEvent:e forView:self];}
-(void)keyDown:(NSEvent*)e{if(e.keyCode==53){if(app->dragging){app->tiles=app->dragStart;app->dragging=NO;[app rebuild];}app->selected.clear();app->primary=0;[app refresh];}else [super keyDown:e];}
@end
static NSTextField *Label(NSView *v,NSString *s,NSRect r,double size=18){NSTextField *f=[NSTextField labelWithString:s];f.frame=r;f.font=[NSFont systemFontOfSize:size];f.textColor=Color(0x183E4D);[v addSubview:f];return f;}
static NSButton *Button(NSView *v,NSString *s,NSRect r,int tag){NSButton *b=[NSButton buttonWithTitle:s target:app action:@selector(action:)];b.frame=r;b.tag=tag;b.font=[NSFont systemFontOfSize:16];[v addSubview:b];return b;}
static NSPopUpButton *Popup(NSView *v,NSArray *items,NSRect r){NSPopUpButton *p=[[NSPopUpButton alloc] initWithFrame:r pullsDown:NO];[p addItemsWithTitles:items];p.font=[NSFont systemFontOfSize:18];[v addSubview:p];return p;}
static NSTextField *Field(NSView *v,NSString *s,NSRect r){NSTextField *f=[[NSTextField alloc] initWithFrame:r];f.stringValue=s;f.font=[NSFont systemFontOfSize:18];[v addSubview:f];return f;}
@implementation Controller
-(Tile*)tile:(int)ident{for(auto& t:tiles)if(t.id==ident)return &t;return nullptr;}
-(void)error:(NSString*)message{NSAlert *a=[NSAlert new];a.messageText=@"DeskTiles";a.informativeText=message;[a runModal];}
-(NSImage*)image:(NSString*)path{if(!path.length)return nil;NSImage *im=images[path];if(!im){im=[[NSImage alloc] initWithContentsOfFile:path];if(im)images[path]=im;}return im;}
-(void)validateImage:(NSData*)data{
 if(!data || !data.length || data.length>ImageLimit)throw std::runtime_error("圖片不存在、為空或超過 16 MiB");
 ImageMime(std::string((const char*)data.bytes,data.length));CGImageSourceRef src=CGImageSourceCreateWithData((__bridge CFDataRef)data,nullptr);if(!src)throw std::runtime_error("無法讀取圖片");
 NSDictionary *props=CFBridgingRelease(CGImageSourceCopyPropertiesAtIndex(src,0,nullptr));double w=[props[(id)kCGImagePropertyPixelWidth] doubleValue],h=[props[(id)kCGImagePropertyPixelHeight] doubleValue];CGImageRef im=nullptr;if(w>0&&h>0&&w*h<=64000000)im=CGImageSourceCreateImageAtIndex(src,0,nullptr);CFRelease(src);if(!im)throw std::runtime_error("圖片損壞或超過 6400 萬像素");CGImageRelease(im);
}
-(void)save{
 if(!ready)return;try{ValidateTiles(tiles);std::ostringstream out;Save(out,tiles);auto bytes=out.str();NSData *data=[NSData dataWithBytes:bytes.data() length:bytes.size()];NSString *path=[directory stringByAppendingPathComponent:@"layout.hexdesk"];NSFileManager *fm=NSFileManager.defaultManager;if([fm fileExistsAtPath:path]){NSData *old=[NSData dataWithContentsOfFile:path];NSError *backupError=nil;if(![old writeToFile:[directory stringByAppendingPathComponent:@"layout.backup"] options:NSDataWritingAtomic error:&backupError])throw std::runtime_error(U(backupError.localizedDescription));}NSError *err=nil;if(![data writeToFile:path options:NSDataWritingAtomic error:&err])throw std::runtime_error(U(err.localizedDescription));status.stringValue=@"已儲存版面。";}catch(const std::exception& e){[self error:S(e.what())];}
}
-(void)rebuild{
 NSMutableSet *keep=[NSMutableSet new];for(auto& t:tiles){NSNumber *key=@(t.id);[keep addObject:key];TileWindow *w=windows[key];if(!w){w=[[TileWindow alloc] initWithContentRect:Frame(t) styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:NO];w.opaque=NO;w.backgroundColor=NSColor.clearColor;w.hasShadow=NO;w.hidesOnDeactivate=NO;w.releasedWhenClosed=NO;w.collectionBehavior=NSWindowCollectionBehaviorCanJoinAllSpaces|NSWindowCollectionBehaviorStationary|NSWindowCollectionBehaviorFullScreenAuxiliary;TileView *view=[[TileView alloc] initWithFrame:NSMakeRect(0,0,t.size,TileHeight(t.size,t.shape))];view.tileID=t.id;w.contentView=view;windows[key]=w;}w.level=t.topmost?NSFloatingWindowLevel:CGWindowLevelForKey(kCGDesktopIconWindowLevelKey)-1;[w setFrame:Frame(t) display:YES];[w orderFrontRegardless];[w.contentView setNeedsDisplay:YES];}
 for(NSNumber *key in windows.allKeys)if(![keep containsObject:key]){[windows[key] close];[windows removeObjectForKey:key];}[self refresh];
}
-(void)refresh{
 [list reloadData];NSMutableIndexSet *indexes=[NSMutableIndexSet new];for(size_t i=0;i<tiles.size();++i)if(selected.count(tiles[i].id))[indexes addIndex:i];list.delegate=nil;[list selectRowIndexes:indexes byExtendingSelection:NO];list.delegate=self;
 Tile *t=[self tile:primary];selectionLabel.stringValue=t?[NSString stringWithFormat:@"編輯 #%d · 已選取 %lu 格",primary,selected.size()]:@"請先選取圖形";
 if(t){[kind selectItemAtIndex:(int)t->kind];text.string=S(t->text);imageField.stringValue=S(t->image);[mode selectItemAtIndex:t->countdown?0:1];seconds.stringValue=[NSString stringWithFormat:@"%.0f",t->duration];}
 [self contentVisibility];for(NSWindow *w in windows.allValues)[w.contentView setNeedsDisplay:YES];
}
-(NSInteger)numberOfRowsInTableView:(NSTableView*)table{return tiles.size();}
-(id)tableView:(NSTableView*)table objectValueForTableColumn:(NSTableColumn*)column row:(NSInteger)row{const Tile& t=tiles[row];return [NSString stringWithFormat:@"#%d %@ · %@",t.id,@[@"六角形",@"三角形",@"矩形"][(int)t.shape],@[@"文字",@"圖片",@"計時器"][(int)t.kind]];}
-(void)tableViewSelectionDidChange:(NSNotification*)n{selected.clear();[list.selectedRowIndexes enumerateIndexesUsingBlock:^(NSUInteger idx,BOOL *stop){if(idx<tiles.size())selected.insert(tiles[idx].id);}];primary=selected.empty()?0:*selected.begin();[self refresh];}
-(void)contentVisibility{textScroll.hidden=kind.indexOfSelectedItem!=0;imageGroup.hidden=kind.indexOfSelectedItem!=1;timerGroup.hidden=kind.indexOfSelectedItem!=2;}
-(void)changed:(id)sender{[self contentVisibility];}
-(void)show:(id)sender{[NSApp activateIgnoringOtherApps:YES];[control makeKeyAndOrderFront:nil];}
-(BOOL)windowShouldClose:(NSWindow*)window{[window orderOut:nil];return NO;}
-(void)createControl{
 control=[[ControlWindow alloc] initWithContentRect:NSMakeRect(0,0,820,710) styleMask:NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|NSWindowStyleMaskMiniaturizable backing:NSBackingStoreBuffered defer:NO];control.title=@"桌面拼格 · DeskTiles";control.delegate=self;control.backgroundColor=Color(0xF3F7F5);[control center];FlippedView *v=[[FlippedView alloc] initWithFrame:NSMakeRect(0,0,820,710)];control.contentView=v;
 NSImage *logo=[[NSImage alloc] initWithContentsOfFile:[NSBundle.mainBundle pathForResource:@"DeskTiles" ofType:@"icns"]];if(logo){NSImageView *iv=[[NSImageView alloc] initWithFrame:NSMakeRect(28,20,64,64)];iv.image=logo;[v addSubview:iv];NSApp.applicationIconImage=logo;}
 Label(v,@"DESKTILES",NSMakeRect(108,30,360,45),32);Label(v,@"桌面上的一小格，留給重要的事。",NSMakeRect(28,92,600,28));Button(v,@"配置 JSON…",NSMakeRect(620,30,170,36),20);
 Label(v,@"桌面圖形",NSMakeRect(28,130,225,26));selectionLabel=Label(v,@"請先選取圖形",NSMakeRect(280,130,510,26));
 NSScrollView *scroll=[[NSScrollView alloc] initWithFrame:NSMakeRect(28,166,225,210)];scroll.hasVerticalScroller=YES;scroll.borderType=NSBezelBorder;list=[[NSTableView alloc] initWithFrame:scroll.bounds];NSTableColumn *col=[[NSTableColumn alloc] initWithIdentifier:@"tile"];col.width=220;((NSTextFieldCell*)col.dataCell).font=[NSFont systemFontOfSize:17];[list addTableColumn:col];list.headerView=nil;list.rowHeight=28;list.allowsMultipleSelection=YES;list.dataSource=self;list.delegate=self;scroll.documentView=list;[v addSubview:scroll];
 shape=Popup(v,@[@"六角形（預設）",@"三角形",@"矩形（3:2）"],NSMakeRect(28,388,225,34));NSArray *names=@[@"＋ 新增圖形",@"邊框與風格…",@"大小與文字…",@"背景與透明度…",@"刪除選取…"];for(int i=0;i<5;++i)Button(v,names[i],NSMakeRect(28,432+i*42,225,34),i+1);
 kind=Popup(v,@[@"文字",@"圖片",@"計時器"],NSMakeRect(280,164,510,36));kind.target=self;kind.action=@selector(changed:);
 textScroll=[[NSScrollView alloc] initWithFrame:NSMakeRect(280,224,510,220)];textScroll.borderType=NSBezelBorder;textScroll.hasVerticalScroller=YES;text=[[NSTextView alloc] initWithFrame:textScroll.bounds];text.font=[NSFont systemFontOfSize:18];text.richText=NO;text.autoresizingMask=NSViewWidthSizable;text.textContainer.containerSize=NSMakeSize(490,CGFLOAT_MAX);text.textContainer.widthTracksTextView=YES;textScroll.documentView=text;[v addSubview:textScroll];
 imageGroup=[[FlippedView alloc] initWithFrame:NSMakeRect(280,224,510,220)];[v addSubview:imageGroup];Label(imageGroup,@"圖片路徑 · 等比例裁切填滿",NSMakeRect(0,0,510,26));imageField=Field(imageGroup,@"",NSMakeRect(0,40,510,34));Button(imageGroup,@"瀏覽圖片…",NSMakeRect(0,90,170,36),10);
 timerGroup=[[FlippedView alloc] initWithFrame:imageGroup.frame];[v addSubview:timerGroup];mode=Popup(timerGroup,@[@"倒數計時",@"正向計時"],NSMakeRect(0,0,510,36));Label(timerGroup,@"倒數總秒數（1～8640000）",NSMakeRect(0,50,510,26));seconds=Field(timerGroup,@"300",NSMakeRect(0,85,510,34));Button(timerGroup,@"開始／暫停",NSMakeRect(0,140,245,38),11);Button(timerGroup,@"重設計時",NSMakeRect(265,140,245,38),12);
 Button(v,@"套用至所有選取格",NSMakeRect(280,470,510,42),6);Button(v,@"選取格置頂",NSMakeRect(280,528,245,38),7);Button(v,@"選取格固定於桌面",NSMakeRect(545,528,245,38),8);
 Label(v,@"Shift＋點擊多選 · 拖曳整組 · 靠近自動吸附\n雙擊編輯 · Esc 取消選取 · 關閉本視窗後仍會繼續顯示",NSMakeRect(280,582,520,60),16);NSTextField *loc=Label(v,[@"資料位置：" stringByAppendingString:directory],NSMakeRect(28,650,760,24),14);loc.lineBreakMode=NSLineBreakByTruncatingMiddle;status=Label(v,@"準備就緒",NSMakeRect(28,681,760,22),14);
}
-(NSMenu*)menu{
 NSMenu *m=[NSMenu new];NSArray *titles=@[@"開啟控制視窗",@"新增六角形",@"新增三角形",@"新增矩形",@"選取全部",@"邊框與風格…",@"大小與文字…",@"背景與透明度…",@"顯示於最上層",@"固定於桌面層",@"開始／暫停計時器",@"重設計時器",@"刪除選取…",@"匯出整體配置（含圖片）…",@"匯入 JSON：覆蓋配置…",@"匯入 JSON：合併配置…",@"找回全部圖形",@"儲存並結束桌面拼格"];int tags[]={0,31,32,33,9,2,3,4,7,8,11,12,5,21,22,23,24,99};for(NSUInteger i=0;i<titles.count;++i){NSMenuItem *item=[[NSMenuItem alloc] initWithTitle:titles[i] action:@selector(action:) keyEquivalent:tags[i]==99?@"q":@""];item.target=self;item.tag=tags[i];[m addItem:item];}return m;
}
-(void)add:(Shape)newShape{
 if(tiles.size()>=500)throw std::runtime_error("最多只能有 500 格");Tile t;t.shape=newShape;t.fontFamily="PingFang TC";t.position={80,140};std::set<int> ids;for(auto& x:tiles)ids.insert(x.id);t.id=1;while(ids.count(t.id))++t.id;
 bool placed=tiles.empty();std::vector<Tile> bases=tiles;if(Tile *a=[self tile:primary])std::rotate(bases.begin(),std::find_if(bases.begin(),bases.end(),[&](auto& b){return b.id==a->id;}),bases.end());
 for(auto& base:bases){for(hexdesk::Point delta:Neighbors(base.size,t.size,base.shape,t.shape)){t.position=base.position+delta;bool ok=true;for(auto& other:tiles)if(Overlaps(t.position,other.position,t.size,other.size,t.shape,other.shape)){ok=false;break;}if(ok){placed=true;break;}}if(placed)break;}
 if(!placed)throw std::runtime_error("找不到可新增的位置");tiles.push_back(t);selected={t.id};primary=t.id;[self rebuild];[self save];
}
-(void)action:(id)sender{
 NSInteger tag=[sender tag];try{
 if(tag==0){[self show:nil];return;}if(tag==99){[NSApp terminate:nil];return;}
 if(tag==1 || (tag>=31&&tag<=33)){[self add:static_cast<Shape>(tag==1?shape.indexOfSelectedItem:tag-31)];return;}
 if(tag==9){selected.clear();for(auto& t:tiles)selected.insert(t.id);primary=tiles.empty()?0:tiles[0].id;[self refresh];return;}
 if(tag==20){NSMenu *m=[NSMenu new];for(NSMenuItem *item in [self menu].itemArray)if(item.tag>=21&&item.tag<=23)[m addItem:[item copy]];[m popUpMenuPositioningItem:nil atLocation:NSMakePoint(620,70) inView:control.contentView];return;}
 if(tag>=21&&tag<=23){[self config:tag];return;}
 if(tag==24){if(!tiles.empty()){double left=tiles[0].position.x,top=tiles[0].position.y;for(auto& t:tiles){left=std::min(left,t.position.x);top=std::min(top,t.position.y);}for(auto& t:tiles){t.position=t.position+hexdesk::Point{60-left,100-top};t.topmost=true;}[self rebuild];[self save];}return;}
 if(tag==10){NSOpenPanel *p=[NSOpenPanel openPanel];p.allowedFileTypes=@[@"png",@"jpg",@"jpeg",@"bmp"];if([p runModal]==NSModalResponseOK){[self validateImage:[NSData dataWithContentsOfURL:p.URL]];imageField.stringValue=p.URL.path;}return;}
 if(selected.empty())throw std::runtime_error("請先選取圖形");
 if(tag>=2&&tag<=4){[self openSettings:tag];return;}
 if(tag==5){NSAlert *a=[NSAlert new];a.messageText=@"刪除選取圖形？";a.informativeText=[NSString stringWithFormat:@"將刪除 %lu 格。",selected.size()];[a addButtonWithTitle:@"刪除"];[a addButtonWithTitle:@"取消"];if([a runModal]!=NSAlertFirstButtonReturn)return;tiles.erase(std::remove_if(tiles.begin(),tiles.end(),[&](const Tile& t){return selected.count(t.id);}),tiles.end());selected.clear();primary=0;}
 if(tag==6){auto candidate=tiles;double duration=seconds.doubleValue;if(kind.indexOfSelectedItem==2&&(!std::isfinite(duration)||duration<1||duration>8640000))throw std::runtime_error("倒數秒數必須介於 1～8640000");if(kind.indexOfSelectedItem==1)[self validateImage:[NSData dataWithContentsOfFile:imageField.stringValue]];
 for(auto& t:candidate)if(selected.count(t.id)){t.kind=static_cast<Kind>(kind.indexOfSelectedItem);if(t.kind==Kind::Text)t.text=U(text.string);if(t.kind==Kind::Image)t.image=U(imageField.stringValue);if(t.kind==Kind::Timer){t.countdown=mode.indexOfSelectedItem==0;t.duration=duration;t.elapsed=0;t.running=false;t.notified=false;}}ValidateTiles(candidate);tiles=candidate;}
 if(tag==7||tag==8)for(auto& t:tiles)if(selected.count(t.id))t.topmost=tag==7;
 if(tag==11)for(auto& t:tiles)if(selected.count(t.id)&&t.kind==Kind::Timer){if(t.countdown&&t.elapsed>=t.duration){t.elapsed=0;t.notified=false;}t.running=!t.running;}
 if(tag==12)for(auto& t:tiles)if(selected.count(t.id)&&t.kind==Kind::Timer){t.elapsed=0;t.running=false;t.notified=false;}
 [self rebuild];[self save];
 }catch(const std::exception& e){[self error:S(e.what())];}
}
-(void)openSettings:(NSInteger)type{
 if(settings){[settings close];settings=nil;}fields=[NSMutableDictionary new];settingType=@(type).stringValue;
 settings=[[NSWindow alloc] initWithContentRect:NSMakeRect(0,0,520,type==2?390:350) styleMask:NSWindowStyleMaskTitled|NSWindowStyleMaskClosable backing:NSBackingStoreBuffered defer:NO];settings.releasedWhenClosed=NO;settings.title=@{@2:@"DeskTiles · 邊框與風格",@3:@"DeskTiles · 大小與文字",@4:@"DeskTiles · 背景與透明度"}[@(type)];FlippedView *v=[[FlippedView alloc] initWithFrame:settings.contentView.bounds];settings.contentView=v;Tile *t=[self tile:primary];if(!t)t=[self tile:*selected.begin()];
 auto field=[&](NSString *key,NSString *label,NSString *value,int y){Label(v,label,NSMakeRect(24,y,240,30),16);NSTextField *f=Field(v,value,NSMakeRect(270,y,220,30));fields[key]=f;};
 auto check=[&](NSString *key,NSString *label,int y,BOOL value){NSButton *b=[NSButton checkboxWithTitle:label target:nil action:nil];b.frame=NSMakeRect(24,y,460,30);b.state=value?NSControlStateValueOn:NSControlStateValueOff;[v addSubview:b];fields[key]=b;};
 if(type==2){check(@"apply",@"套用邊框設定",15,YES);Label(v,@"邊框風格",NSMakeRect(24,60,240,30),16);auto p=Popup(v,@[@"實色",@"像素",@"RGB 彩虹",@"單色呼吸燈"],NSMakeRect(270,60,220,30));[p selectItemAtIndex:(int)t->borderStyle];fields[@"style"]=p;field(@"color",@"色碼 #RRGGBB",S(ColorText(t->borderColor)),105);field(@"width",@"厚度（1～12 px）",@(t->borderWidth).stringValue,150);field(@"cycle",@"動畫週期（2～60 秒）",@(t->rgbCycleSeconds).stringValue,195);check(@"animated",@"播放動畫",240,t->rgbAnimated);}
 if(type==3){check(@"sizeApply",@"套用圖形大小",10,YES);field(@"size",@"寬度（80～480 px）",@(t->size).stringValue,50);check(@"fontApply",@"套用文字樣式",95,YES);field(@"family",@"字體名稱",S(t->fontFamily),135);field(@"fontSize",@"字級（8～96 px）",@(t->fontSize).stringValue,180);field(@"color",@"文字色碼 #RRGGBB",S(ColorText(t->textColor)),225);}
 if(type==4){check(@"colorApply",@"套用背景色",15,YES);field(@"color",@"背景色碼 #RRGGBB",S(ColorText(t->backgroundColor)),60);check(@"alphaApply",@"套用透明度",115,YES);field(@"alpha",@"透明度（0～100%）",@(t->backgroundTransparency).stringValue,160);Label(v,@"只影響背景；文字、圖片及邊框維持原設定。",NSMakeRect(24,210,470,40),14);}
 NSButton *apply=Button(v,@"套用至所有選取格",NSMakeRect(240,settings.contentView.bounds.size.height-65,250,40),0);apply.action=@selector(applySettings:);[settings center];[settings makeKeyAndOrderFront:nil];
}
-(void)applySettings:(id)sender{
 try{if(selected.empty())throw std::runtime_error("請先選取圖形");auto candidate=tiles;int type=settingType.intValue;
 auto checked=[&](NSString *key){return [(NSButton*)fields[key] state]==NSControlStateValueOn;};
 auto number=[&](NSString *key,double lo,double hi){NSString *s=fields[key].stringValue;NSScanner *scanner=[NSScanner scannerWithString:s];double n=0;if(![scanner scanDouble:&n]||!scanner.isAtEnd||!std::isfinite(n)||n<lo||n>hi)throw std::runtime_error("數值無效或超出範圍");return n;};
 auto color=[&](){uint32_t c;if(!ParseColor(U(fields[@"color"].stringValue),c))throw std::runtime_error("請輸入 #RRGGBB 色碼");return c;};
 if(type==3&&checked(@"sizeApply")&&!Resize(candidate,selected,primary,number(@"size",80,480)))throw std::runtime_error("縮放會造成重疊，已保留原設定");
 for(auto& t:candidate)if(selected.count(t.id)){
 if(type==2&&checked(@"apply")){t.borderStyle=static_cast<BorderStyle>([(NSPopUpButton*)fields[@"style"] indexOfSelectedItem]);t.borderColor=color();t.borderWidth=number(@"width",1,12);t.rgbCycleSeconds=number(@"cycle",2,60);t.rgbAnimated=checked(@"animated");}
 if(type==3&&checked(@"fontApply")){t.fontFamily=U(fields[@"family"].stringValue);t.fontSize=number(@"fontSize",8,96);t.textColor=color();if(![NSFont fontWithName:S(t.fontFamily) size:t.fontSize])throw std::runtime_error("找不到此字體；請輸入 Mac 已安裝的字體名稱（例如 PingFang TC）");}
 if(type==4){if(checked(@"colorApply"))t.backgroundColor=color();if(checked(@"alphaApply"))t.backgroundTransparency=number(@"alpha",0,100);}}
 ValidateTiles(candidate);tiles=candidate;[self rebuild];[self save];[settings orderOut:nil];
 }catch(const std::exception& e){[self error:S(e.what())];}
}
-(void)config:(NSInteger)tag{
 if(tag==21){NSSavePanel *p=[NSSavePanel savePanel];p.allowedFileTypes=@[@"json"];p.nameFieldStringValue=@"DeskTiles配置.json";if([p runModal]!=NSModalResponseOK)return;Bundle bundle=Pack(tiles,[&](const std::string& path){NSData *data=[NSData dataWithContentsOfFile:S(path)];[self validateImage:data];return Asset{"",std::string((const char*)data.bytes,data.length)};});auto json=BundleJson(bundle).dump(2);if(json.size()>JsonLimit)throw std::runtime_error("JSON 超過 128 MiB");NSError *e=nil;if(![[NSData dataWithBytes:json.data() length:json.size()] writeToURL:p.URL options:NSDataWritingAtomic error:&e])throw std::runtime_error(U(e.localizedDescription));status.stringValue=@"已匯出含圖片的配置。";return;}
 NSOpenPanel *p=[NSOpenPanel openPanel];p.allowedFileTypes=@[@"json"];if([p runModal]!=NSModalResponseOK)return;
 NSDictionary *attr=[NSFileManager.defaultManager attributesOfItemAtPath:p.URL.path error:nil];if([attr fileSize]>JsonLimit)throw std::runtime_error("JSON 超過 128 MiB");std::ifstream in(U(p.URL.path));Bundle b=ParseBundle(in);
 for(const auto& entry:b.images){NSData *data=[NSData dataWithBytes:entry.second.bytes.data() length:entry.second.bytes.size()];[self validateImage:data];}
 if(tag==22){NSAlert *a=[NSAlert new];a.messageText=@"以匯入配置覆蓋目前版面？";a.informativeText=@"目前配置會先備份。";[a addButtonWithTitle:@"覆蓋匯入"];[a addButtonWithTitle:@"取消"];if([a runModal]!=NSAlertFirstButtonReturn)return;}
 NSString *assetDir=[directory stringByAppendingPathComponent:@"assets"];[NSFileManager.defaultManager createDirectoryAtPath:assetDir withIntermediateDirectories:YES attributes:nil error:nil];
 for(auto& t:b.tiles){auto key=b.tileImages.find(t.id);if(key!=b.tileImages.end()){auto& asset=b.images.at(key->second);unsigned char hash[CC_SHA256_DIGEST_LENGTH];CC_SHA256(asset.bytes.data(),(CC_LONG)asset.bytes.size(),hash);NSMutableString *name=[NSMutableString new];for(auto x:hash)[name appendFormat:@"%02x",x];NSString *path=[assetDir stringByAppendingPathComponent:name];NSError *err=nil;if(![[NSData dataWithBytes:asset.bytes.data() length:asset.bytes.size()] writeToFile:path options:NSDataWritingAtomic error:&err])throw std::runtime_error(U(err.localizedDescription));t.image=U(path);}}
 auto candidate=tag==23?Merge(tiles,b.tiles):b.tiles;[self save];NSString *path=[directory stringByAppendingPathComponent:@"layout.hexdesk"];NSString *backup=[directory stringByAppendingPathComponent:[NSString stringWithFormat:@"import-before-%@.hexdesk",NSUUID.UUID.UUIDString]];if([NSFileManager.defaultManager fileExistsAtPath:path]&&![NSFileManager.defaultManager copyItemAtPath:path toPath:backup error:nil])throw std::runtime_error("無法建立匯入前備份");tiles=candidate;selected.clear();primary=0;[images removeAllObjects];[self rebuild];[self save];status.stringValue=@"JSON 匯入完成；匯入計時器維持暫停。";
}
-(void)tick:(NSTimer*)timer{
 double now=NSProcessInfo.processInfo.systemUptime,delta=now-lastTick;lastTick=now;BOOL changed=NO;
 NSPoint mouse=NSEvent.mouseLocation;
 for(auto& t:tiles){if(Advance(t,delta)){NSBeep();status.stringValue=[NSString stringWithFormat:@"#%d 計時完成",t.id];changed=YES;}TileWindow *w=windows[@(t.id)];NSPoint local=NSMakePoint(mouse.x-w.frame.origin.x,NSMaxY(w.frame)-mouse.y);if(!dragging)w.ignoresMouseEvents=![Path(t) containsPoint:local];if(AnimatedBorder(t)||t.running||changed)[w.contentView setNeedsDisplay:YES];}
 static double saved=0;if(changed||now-saved>10){BOOL running=NO;for(auto& t:tiles)running|=t.running;if(changed||running)[self save];saved=now;}
}
-(void)activateExisting:(NSNotification*)note{[self show:nil];}
-(void)applicationDidFinishLaunching:(NSNotification*)note{
 [NSApp setAppearance:[NSAppearance appearanceNamed:NSAppearanceNameAqua]];windows=[NSMutableDictionary new];images=[NSMutableDictionary new];lockFD=-1;
 NSArray<NSString*> *args=NSProcessInfo.processInfo.arguments;NSUInteger i=[args indexOfObject:@"--data-dir"];directory=(i!=NSNotFound&&i+1<args.count)?[args[i+1] stringByStandardizingPath]:[NSHomeDirectory() stringByAppendingPathComponent:@"Library/Application Support/DeskTiles"];
 if(!directory.isAbsolutePath)directory=[NSFileManager.defaultManager.currentDirectoryPath stringByAppendingPathComponent:directory];
 NSError *err=nil;if(![NSFileManager.defaultManager createDirectoryAtPath:directory withIntermediateDirectories:YES attributes:nil error:&err]){[self error:err.localizedDescription];[NSApp terminate:nil];return;}
 NSString *channel=[@"DeskTiles.activate." stringByAppendingString:directory];lockFD=open([[directory stringByAppendingPathComponent:@"session.lock"] fileSystemRepresentation],O_CREAT|O_RDWR,0600);
 if(lockFD<0){[self error:@"資料目錄無法寫入。請使用 --data-dir 指定其他位置。"];[NSApp terminate:nil];return;}if(flock(lockFD,LOCK_EX|LOCK_NB)!=0){[NSDistributedNotificationCenter.defaultCenter postNotificationName:channel object:nil userInfo:nil deliverImmediately:YES];close(lockFD);lockFD=-1;[NSApp terminate:nil];return;}
 [NSDistributedNotificationCenter.defaultCenter addObserver:self selector:@selector(activateExisting:) name:channel object:nil];
 NSString *path=[directory stringByAppendingPathComponent:@"layout.hexdesk"];try{if([NSFileManager.defaultManager fileExistsAtPath:path]){std::ifstream in(U(path));tiles=Load(in);}else{for(int n=0;n<3;++n){Tile t;t.id=n+1;t.position={100.+n*120,130.+(n%2)*Height/2};t.fontFamily="PingFang TC";t.text=n==0?"今天的重點\n專心完成一件事":n==1?"桌面拼格":"00:05:00";if(n==2){t.kind=Kind::Timer;t.fontSize=22;t.textColor=0x71EDC4;}tiles.push_back(t);}}}catch(const std::exception& e){[self error:[S(e.what()) stringByAppendingString:@"\n原檔案已保留；程式將結束，避免覆寫。"]];[NSApp terminate:nil];return;}
 [self createControl];ready=YES;[self rebuild];[self save];tray=[NSStatusBar.systemStatusBar statusItemWithLength:NSVariableStatusItemLength];tray.button.title=@"▦";tray.button.toolTip=@"桌面拼格 DeskTiles";tray.menu=[self menu];
 NSMenu *main=[NSMenu new];NSMenuItem *root=[NSMenuItem new];root.submenu=[self menu];[main addItem:root];NSMenuItem *editRoot=[[NSMenuItem alloc] initWithTitle:@"編輯" action:nil keyEquivalent:@""];NSMenu *edit=[[NSMenu alloc] initWithTitle:@"編輯"];NSArray *editTitles=@[@"復原",@"剪下",@"拷貝",@"貼上",@"全選"];NSArray *editKeys=@[@"z",@"x",@"c",@"v",@"a"];SEL selectors[]={@selector(undo:),@selector(cut:),@selector(copy:),@selector(paste:),@selector(selectAll:)};for(int n=0;n<5;++n)[edit addItem:[[NSMenuItem alloc] initWithTitle:editTitles[n] action:selectors[n] keyEquivalent:editKeys[n]]];editRoot.submenu=edit;[main addItem:editRoot];NSApp.mainMenu=main;
 lastTick=NSProcessInfo.processInfo.systemUptime;clock=[NSTimer scheduledTimerWithTimeInterval:1./12 target:self selector:@selector(tick:) userInfo:nil repeats:YES];[NSRunLoop.mainRunLoop addTimer:clock forMode:NSRunLoopCommonModes];[self show:nil];
 if([args containsObject:@"--smoke-test"])[self performSelector:@selector(smoke) withObject:nil afterDelay:.4];
}
-(BOOL)applicationShouldHandleReopen:(NSApplication*)sender hasVisibleWindows:(BOOL)flag{[self show:nil];return YES;}
-(NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)sender{[clock invalidate];[self save];if(lockFD>=0){flock(lockFD,LOCK_UN);close(lockFD);lockFD=-1;}return NSTerminateNow;}
-(void)smoke{
 try{
 if(windows.count!=tiles.size()||!control.isVisible)throw std::runtime_error("視窗初始化失敗");
 if(![NSApp.applicationIconImage isValid] || ![NSFileManager.defaultManager fileExistsAtPath:[NSBundle.mainBundle pathForResource:@"DeskTiles" ofType:@"icns"]])throw std::runtime_error("App icon failed to load");
 auto original=tiles;selected={tiles.front().id};primary=tiles.front().id;
 for(int n=0;n<3;++n)[self add:static_cast<Shape>(n)];
 if(windows.count!=original.size()+3)throw std::runtime_error("新增形狀失敗");
 for(int n=2;n<=4;++n){[self openSettings:n];[self applySettings:nil];}
 Tile *t=[self tile:primary];t->kind=Kind::Timer;t->duration=1;t->running=true;if(!Advance(*t,2)||Clock(*t)!="00:00:00")throw std::runtime_error("計時器失敗");
 t->borderStyle=BorderStyle::Rgb;t->backgroundTransparency=100;[self rebuild];
 for(auto& tile:tiles){TileView *view=(TileView*)windows[@(tile.id)].contentView;NSBitmapImageRep *rep=[view bitmapImageRepForCachingDisplayInRect:view.bounds];[view cacheDisplayInRect:view.bounds toBitmapImageRep:rep];[[rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}] writeToFile:[directory stringByAppendingPathComponent:[NSString stringWithFormat:@"tile-%d.png",tile.id]] atomically:YES];}
 NSBitmapImageRep *rep=[control.contentView bitmapImageRepForCachingDisplayInRect:control.contentView.bounds];[control.contentView cacheDisplayInRect:control.contentView.bounds toBitmapImageRep:rep];[[rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}] writeToFile:[directory stringByAppendingPathComponent:@"control.png"] atomically:YES];
 // Exercise the original Windows sample with its embedded image on macOS.
 std::ifstream fixture(U([NSBundle.mainBundle pathForResource:@"sample" ofType:@"json"]));Bundle sample=ParseBundle(fixture);for(auto& entry:sample.images){NSData *bytes=[NSData dataWithBytes:entry.second.bytes.data() length:entry.second.bytes.size()];[self validateImage:bytes];NSImage *im=[[NSImage alloc] initWithData:bytes];if(!im)throw std::runtime_error("sample image decode failed");}
 auto merged=Merge(tiles,sample.tiles);if(merged.size()!=tiles.size()+sample.tiles.size())throw std::runtime_error("merge failed");
 Tile *first=[self tile:tiles.front().id];first->topmost=false;[self rebuild];if(windows[@(first->id)].level!=CGWindowLevelForKey(kCGDesktopIconWindowLevelKey)-1)throw std::runtime_error("desktop level failed");first->topmost=true;[self rebuild];
 auto json=BundleJson(Pack(tiles,[](const std::string&){return Asset{};}));auto restored=ReadBundle(json);if(restored.tiles.size()!=tiles.size())throw std::runtime_error("JSON roundtrip failed");
 [self save];std::ifstream in(U([directory stringByAppendingPathComponent:@"layout.hexdesk"]));auto loaded=Load(in);if(loaded.size()!=tiles.size())throw std::runtime_error("保存失敗");
 [@"PASS: AppKit windows, three shapes, settings, timer, transparent drawing, JSON and layout round trips\n" writeToFile:[directory stringByAppendingPathComponent:@"smoke-result.txt"] atomically:YES encoding:NSUTF8StringEncoding error:nil];
 tiles=original;[self save];[NSApp terminate:nil];
 }catch(const std::exception& e){[S(e.what()) writeToFile:[directory stringByAppendingPathComponent:@"smoke-result.txt"] atomically:YES encoding:NSUTF8StringEncoding error:nil];exit(2);}
}
@end
int main(int argc,char **argv){@autoreleasepool{[NSApplication sharedApplication];[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];app=[Controller new];NSApp.delegate=app;[NSApp run];}return 0;}
