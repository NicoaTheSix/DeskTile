#include <sstream>
#include <iostream>
#include "core.hpp"
#include "config.hpp"
using namespace hexdesk;
int checks=0;
void Check(bool result,const char* message) {
    ++checks;
    if (!result) throw std::runtime_error(message);
}
int main(int argc,char** argv) {
    std::cout << "Input: " << (argc>1?argv[1]:"core.hpp (in-memory fixtures)") << '\n';
    std::cout << "Output: stdout; no data files modified\n";
    try {
        std::cout << "[1/9] Selection\n";
        std::set<int> chosen;
        Select(chosen,1,false); Select(chosen,2,true);
        Check(chosen.size()==2,"shift add");
        Select(chosen,1,true); Check(chosen==std::set<int>{2},"shift toggle");
        Select(chosen,3,false); Check(chosen==std::set<int>{3},"single replaces");
        std::cout << "[2/9] Geometry and group snap\n";
        for (auto p:Neighbors()) Check(!Overlaps({},p),"neighbors must share edges without overlap");
        Check(Overlaps({},{10,10}),"collision");
        Check(!Overlaps({},{400,0}),"separated");
        std::vector<Tile> tiles(3);
        tiles[0].id=1; tiles[0].position={500,400};
        tiles[1].id=2; tiles[1].position={0,0};
        tiles[2].id=3; tiles[2].position={0,Height};
        chosen={2,3}; bool snapped=false;
        auto delta=Snap(tiles,chosen,{379,332},&snapped);
        Check(snapped,"near edge snaps");
        Check(ValidDelta(tiles,chosen,delta),"snapped group collision check");
        auto before=tiles[2].position-tiles[1].position;
        Move(tiles,chosen,delta);
        auto after=tiles[2].position-tiles[1].position;
        Check(std::abs(before.x-after.x)<1e-8 && std::abs(before.y-after.y)<1e-8,"relative positions");
        Check(tiles[0].position.x==500,"stationary tile unchanged");
        Check(!ValidDelta(tiles,{2},{120,Height/2}),"invalid overlapping drop");
        Snap(tiles,{2,3},{2000,2000},&snapped); Check(!snapped,"distant no snap");
        std::cout << "[3/9] Timers\n";
        Tile timer; timer.kind=Kind::Timer; timer.running=true; timer.duration=2;
        Check(!Advance(timer,1.5) && Clock(timer)=="00:00:01","countdown rounding");
        Check(Advance(timer,1) && !timer.running,"complete once");
        Check(!Advance(timer,1),"no repeated alert");
        timer.countdown=false; timer.elapsed=0; timer.running=true;
        Advance(timer,3661.9); Check(Clock(timer)=="01:01:01","count up");
        std::cout << "[4/9] Persistence and Unicode\n";
        tiles[0].text="你好\n\"hex\" \\ test"; tiles[0].image="D:/圖片/測試.png";
        tiles[0].running=true; tiles[0].kind=Kind::Timer;
        tiles[0].borderStyle=BorderStyle::Rgb;tiles[0].borderColor=0xF5B24A;
        tiles[0].borderWidth=8;tiles[0].rgbCycleSeconds=12;tiles[0].rgbAnimated=false;
        tiles[1].borderStyle=BorderStyle::Pixel;
        tiles[0].size=320;tiles[0].fontFamily="微軟正黑體";tiles[0].fontSize=36;tiles[0].textColor=0xFF00CC;
        std::stringstream data; Save(data,tiles); auto restored=Load(data);
        Check(restored.size()==3 && restored[0].text==tiles[0].text,"unicode multiline text");
        Check(restored[0].image==tiles[0].image,"image path");
        Check(!restored[0].running,"restore paused");
        Check(std::abs(restored[1].position.x-tiles[1].position.x)<1e-8,"coordinates roundtrip");
        Check(SameAppearance(restored[0],tiles[0]) && SameAppearance(restored[1],tiles[1]),"appearance roundtrip");
        Check(SameTypography(restored[0],tiles[0]) && restored[0].size==320,"size and Unicode typography roundtrip");
        std::stringstream legacy("HEXDESK 1\n1\n7 100 200 0 0 1 300 0 \"舊版文字\" \"\"\n");
        auto migrated=Load(legacy);
        Check(migrated[0].text=="舊版文字" && migrated[0].borderStyle==BorderStyle::Solid &&
            migrated[0].borderWidth==2 && !migrated[0].topmost,"legacy defaults preserve existing content");
        std::stringstream previous("HEXDESK 2\n1\n7 100 200 2 0 1 300 0 \"舊版\" \"\" 2 123456 4 12 0\n");
        auto previousVersion=Load(previous);
        Check(previousVersion[0].size==160 && previousVersion[0].fontSize==22 &&
            previousVersion[0].borderStyle==BorderStyle::Rgb && previousVersion[0].borderWidth==4 &&
            previousVersion[0].textColor==0x71EDC4,"v2 preserves borders and timer defaults");
        std::cout << "[5/9] Border colors, animation and pixel geometry\n";
        std::uint32_t color=0;
        Check(ParseColor("#aBcD01",color) && color==0xABCD01 && ColorText(color)=="#ABCD01","hex color roundtrip");
        Check(ParseColor("00FF80",color) && color==0x00FF80,"color without prefix");
        for(const auto& invalid:{"", "#FFF", "#1234567", "#GG0000", " 123456", "12345z"})
            Check(!ParseColor(invalid,color),"reject invalid color");
        Check(Rainbow(0)==0xFF0000 && Rainbow(1.0/3)==0x00FF00 && Rainbow(2.0/3)==0x0000FF,"RGB primaries");
        Check(Rainbow(0.25)==Rainbow(1.25) && Rainbow(-0.75)==Rainbow(0.25),"RGB phase wraps");
        Check(std::abs(ColorHue(0x00FF00)-1.0/3)<1e-8,"configured color controls hue");
        Tile appearance;Check(!AnimatedBorder(appearance),"solid does not animate");
        appearance.borderStyle=BorderStyle::Pixel;Check(!AnimatedBorder(appearance),"pixel does not animate");
        appearance.borderStyle=BorderStyle::Rgb;Check(AnimatedBorder(appearance),"RGB animation enabled");
        appearance.rgbAnimated=false;Check(!AnimatedBorder(appearance),"static rainbow does not animate");
        for(double inset:{0.0,3.0,8.0}) {
            auto points=BorderOutline(BorderStyle::Pixel,inset);bool bounded=true,orthogonal=true;
            for(size_t i=0;i<points.size();++i) {
                auto a=points[i],b=points[(i+1)%points.size()];
                bounded&=a.x>=0 && a.x<=Width && a.y>=0 && a.y<=Height;
                orthogonal&=a.x==b.x || a.y==b.y;
            }
            Check(points.size()>6 && bounded && orthogonal,"pixel outline stays bounded with staircase edges");
        }
        std::cout << "[6/9] Variable geometry, resizing and collision rollback\n";
        for(double fixed:{80.0,160.0,480.0})for(double moving:{80.0,240.0,480.0}) {
            bool edges=true,inward=true;
            for(auto p:Neighbors(fixed,moving)) {
                edges&=!Overlaps({},p,fixed,moving);
                Point difference=p+Point{moving/2-fixed/2,(TileHeight(moving)-TileHeight(fixed))/2};
                inward&=Overlaps({},p-Point{difference.x*.01,difference.y*.01},fixed,moving);
            }
            Check(edges && inward,"unequal neighbors touch on all six sides and moving inward collides");
        }
        std::vector<Tile> variable(3);for(int i=0;i<3;++i)variable[i].id=i+1;
        variable[1].position={120,Height/2};variable[2].position={1000,1000};
        auto anchor=variable[0].position;
        Check(Resize(variable,{1,2},1,240),"batch resize succeeds with free space");
        Check(variable[0].position.x==anchor.x && variable[0].position.y==anchor.y &&
            variable[1].position.x==180 && std::abs(variable[1].position.y-TileHeight(240)/2)<1e-8,"resize preserves anchor and adjacency");
        Check(variable[2].size==160 && variable[2].position.x==1000,"resize leaves unselected tile alone");
        Check(!Resize(variable,{1},1,480) && variable[0].size==240 && variable[1].position.x==180,"collision rolls back entire resize");
        Check(!Resize(variable,{1,2},1,79) && !Resize(variable,{1,2},1,481),"resize rejects limits");
        Check(!Resize(variable,{1,2},9,240),"resize rejects missing anchor");
        variable[0].size=240;variable[0].position={500,400};
        variable[1].size=120;variable[1].position={0,0};variable[2].size=120;variable[2].position={0,TileHeight(120)};
        auto target=variable[0].position+Neighbors(240,120)[4];
        auto mixedSnap=Snap(variable,{2,3},target+Point{5,-3},&snapped);
        Check(snapped && std::abs(mixedSnap.x-target.x)<1e-8 && std::abs(mixedSnap.y-target.y)<1e-8,"mixed-size group snaps without overlap");
        auto largePixel=BorderOutline(BorderStyle::Pixel,8,480);
        Check(std::all_of(largePixel.begin(),largePixel.end(),[](Point p){return p.x>=0 && p.x<=480 && p.y>=0 && p.y<=TileHeight(480);}),"resized pixel outline bounds");
        std::cout << "[7/9] Invalid input\n";
        for (const auto& text:{"broken", "HEXDESK 6\n0", "HEXDESK 1\n501",
             "HEXDESK 1\n1\n1 0 0 5 1 1 300 0 \"x\" \"\"",
             "HEXDESK 1\n1\n1 0 0 0 1 1 -1 0 \"x\" \"\"",
             "HEXDESK 1\n0\ngarbage"}) {
            bool rejected=false;
            try { std::stringstream bad(text); Load(bad); } catch (...) { rejected=true; }
            Check(rejected,"invalid file must be rejected");
        }
        for(const auto& invalidAppearance:{"", "3 123 2 6 1", "0 16777216 2 6 1", "0 123 0 6 1",
            "0 123 13 6 1", "0 123 2 0 1", "0 123 2 61 1", "0 123 2 6 2"}) {
            std::stringstream bad(std::string("HEXDESK 2\n1\n1 0 0 0 1 1 300 0 \"x\" \"\" ")+invalidAppearance);
            bool rejected=false;try{Load(bad);}catch(...){rejected=true;}
            Check(rejected,"invalid appearance data rejected");
        }
        for(const auto& invalidLayout:{"", "79 \"Arial\" 14 123", "481 \"Arial\" 14 123", "160 \"\" 14 123",
            "160 \"Arial\" 7 123", "160 \"Arial\" 97 123", "160 \"Arial\" 14 16777216", "160 \"bad\nfont\" 14 123"}) {
            std::stringstream bad(std::string("HEXDESK 3\n1\n1 0 0 0 1 1 300 0 \"x\" \"\" 0 123 2 6 1 ")+invalidLayout);
            bool rejected=false;try{Load(bad);}catch(...){rejected=true;}
            Check(rejected,"invalid layout and typography rejected");
        }
        std::cout << "[8/9] Background, breathing and portable JSON bundles\n";
        Tile fresh;fresh.id=1;fresh.borderStyle=BorderStyle::Breathing;fresh.borderColor=0xCC88FF;
        fresh.backgroundColor=0x102030;fresh.backgroundTransparency=100;fresh.rgbCycleSeconds=4;
        Check(AnimatedBorder(fresh),"breathing uses shared animation clock");
        Check(std::abs(BreathLevel(0,4)-.2)<1e-8 && std::abs(BreathLevel(2,4)-1)<1e-8 &&
            std::abs(BreathLevel(4,4)-.2)<1e-8 && BreathLevel(2,4,false)==1,"breathing brightness cycle and stopped state");
        std::stringstream latest;Save(latest,{fresh});auto latestLoaded=Load(latest);
        Check(latestLoaded[0].backgroundTransparency==100 && latestLoaded[0].backgroundColor==0x102030 && SameAppearance(latestLoaded[0],fresh),"v4 background and breathing roundtrip");
        for(const std::string bytes:{std::string{},std::string("a"),std::string("ab"),std::string("abc"),std::string("\0\1\2\xff",4)})
            Check(Unbase64(Base64(bytes))==bytes,"Base64 binary roundtrip");
        for(const auto& bad:{"x", "====", "AB==", "AAB=", "AA=A", "AA==AAAA", "!!!!"}) {
            bool rejected=false;try{Unbase64(bad);}catch(...){rejected=true;}Check(rejected,"invalid Base64 rejected");
        }
        fresh.kind=Kind::Image;fresh.image="D:/圖片/測試.png";
        std::string png="\x89PNG\r\n\x1a\n";png+="fixture bytes";
        Tile copy=fresh;copy.id=2;copy.position={600,0};
        int reads=0;auto bundle=Pack({fresh,copy},[&](const std::string&){++reads;return Asset{"image/png",png};});
        Check(reads==1 && bundle.images.size()==1 && bundle.tileImages.size()==2,"shared image stored once");
        auto json=BundleJson(bundle);std::stringstream jsonText;jsonText<<json;auto portable=ParseBundle(jsonText);
        Check(portable.images.begin()->second.bytes==png && portable.tiles.size()==2 && portable.tiles[0].image.empty(),"JSON embeds image bytes without original path dependency");
        Check(portable.tiles[0].backgroundTransparency==100 && SameTypography(portable.tiles[0],fresh) &&
            SameAppearance(portable.tiles[0],fresh),"JSON preserves background typography and lighting");
        auto combined=Merge({fresh},portable.tiles);
        Check(combined.size()==3 && combined[0].id==fresh.id && combined[1].id!=fresh.id && combined[2].id!=combined[1].id,"merge preserves existing and allocates IDs");
        Check(combined[0].position.x==fresh.position.x && combined[2].position.x-combined[1].position.x==600 &&
            !Overlaps(combined[0].position,combined[1].position,combined[0].size,combined[1].size),"merge moves imported group without changing relative offsets");
        for(int scenario=0;scenario<12;++scenario) {
            auto bad=json;
            switch(scenario) {
            case 0:bad["version"]=99;break;
            case 1:bad["tiles"][0]["background"]["transparency"]=101;break;
            case 2:bad["tiles"][0]["background"]["color"]="#QQQQQQ";break;
            case 3:bad["tiles"][0]["id"]=1.5;break;
            case 4:bad["tiles"][1]["id"]=1;break;
            case 5:bad["tiles"][0]["topmost"]="true";break;
            case 6:bad["images"]["image-1"]["data"]="!!!!";break;
            case 7:bad["tiles"][0]["imageAsset"]="missing";break;
            case 8:bad["images"]["image-1"]["mime"]="image/jpeg";break;
            case 9:bad["tiles"][0]["border"]["cycleSeconds"]=0;break;
            case 10:bad["tiles"][1]["position"]=bad["tiles"][0]["position"];break;
            case 11:bad["tiles"][0]["typography"]["size"]=999;break;
            }
            bool rejected=false;try{ReadBundle(bad);}catch(...){rejected=true;}Check(rejected,"malformed JSON configuration rejected");
        }
        bool duplicate=false;try{std::stringstream repeated("{\"format\":\"HexDesk\",\"format\":\"Other\"}");ParseBundle(repeated);}catch(...){duplicate=true;}
        Check(duplicate,"duplicate JSON keys rejected");
        auto empty=BundleJson(Bundle{});Check(ReadBundle(empty).tiles.empty(),"empty whole configuration supported");
        std::cout << "[9/9] Multiple shapes, contact geometry and compatibility\n";
        Check(Tile{}.shape==Shape::Hexagon && TileHeight(180,Shape::Rectangle)==120,"default hexagon and rectangular aspect ratio");
        for(auto fixedShape:{Shape::Hexagon,Shape::Triangle,Shape::Rectangle})
            for(auto movingShape:{Shape::Hexagon,Shape::Triangle,Shape::Rectangle})for(double fixedSize:{80.0,160.0,480.0}) {
                double movingSize=120;
                auto contacts=Neighbors(fixedSize,movingSize,fixedShape,movingShape);bool touching=true,symmetric=true;
                for(auto p:contacts)touching&=!Overlaps({},p,fixedSize,movingSize,fixedShape,movingShape);
                for(auto p:{Point{},Point{17,25},Point{-30,4},Point{500,500}})
                    symmetric&=Overlaps({},p,fixedSize,movingSize,fixedShape,movingShape)==Overlaps(p,{},movingSize,fixedSize,movingShape,fixedShape);
                Check(!contacts.empty() && touching && symmetric,"mixed shape contact placements do not overlap and collision is symmetric");
            }
        for(auto shape:{Shape::Hexagon,Shape::Triangle,Shape::Rectangle}) {
            Check(Overlaps({},{},160,160,shape,shape),"matching shapes collide at same position");
            auto outline=BorderOutline(BorderStyle::Solid,0,160,shape);
            Check(outline.size()==(shape==Shape::Hexagon?6:shape==Shape::Triangle?3:4),"shape has expected polygon vertices");
            auto steps=BorderOutline(BorderStyle::Pixel,8,160,shape);bool bounded=true,orthogonal=true;
            for(size_t i=0;i<steps.size();++i){auto a=steps[i],b=steps[(i+1)%steps.size()];bounded&=a.x>=0&&a.x<=160&&a.y>=0&&a.y<=TileHeight(160,shape);orthogonal&=a.x==b.x||a.y==b.y;}
            Check(bounded&&orthogonal,"every shape supports bounded pixel outline");
        }
        Check(!Overlaps({},Point{-30,-20},160,80,Shape::Triangle,Shape::Rectangle),"triangle bounding-box corner does not cause false collision");
        std::vector<Tile> shapes(3);for(int i=0;i<3;++i){shapes[i].id=i+1;shapes[i].shape=static_cast<Shape>(i);shapes[i].position={double(i*500),200};}
        shapes[0].kind=Kind::Timer;shapes[1].kind=Kind::Text;shapes[2].kind=Kind::Image;
        std::stringstream shapeSaved;Save(shapeSaved,shapes);auto shapeLoaded=Load(shapeSaved);
        Check(shapeLoaded[1].shape==Shape::Triangle&&shapeLoaded[2].shape==Shape::Rectangle,"v5 shape persistence");
        auto shapeJson=BundleJson(Pack(shapes,[](const std::string&){return Asset{};}));auto shapeBundle=ReadBundle(shapeJson);
        Check(shapeJson["version"]==2&&shapeBundle.tiles[1].shape==Shape::Triangle&&shapeBundle.tiles[2].shape==Shape::Rectangle,"JSON v2 shape roundtrip");
        auto oldJson=shapeJson;oldJson["version"]=1;for(auto& item:oldJson["tiles"])item.erase("shape");
        auto oldBundle=ReadBundle(oldJson);Check(std::all_of(oldBundle.tiles.begin(),oldBundle.tiles.end(),[](const Tile& t){return t.shape==Shape::Hexagon;}),"legacy JSON defaults to hexagons");
        std::stringstream v4("HEXDESK 4\n1\n1 0 0 0 1 1 300 0 \"legacy\" \"\" 0 123 2 6 1 160 \"Arial\" 14 123 1517360 100\n");
        auto oldShape=Load(v4);Check(oldShape[0].shape==Shape::Hexagon&&oldShape[0].backgroundTransparency==100,"legacy v4 preserves background with default shape");
        for(auto badShape:{-1,3}) {
            auto bad=shapeJson;bad["tiles"][0]["shape"]=badShape;bool rejected=false;
            try{ReadBundle(bad);}catch(...){rejected=true;}Check(rejected,"invalid JSON shape rejected");
            std::string invalid=shapeSaved.str();auto lineEnd=invalid.find('\n',invalid.find('\n')+1);auto firstEnd=invalid.find('\n',lineEnd+1);
            auto shapeAt=invalid.rfind(' ',firstEnd);invalid.replace(shapeAt+1,firstEnd-shapeAt-1,std::to_string(badShape));
            rejected=false;try{std::stringstream badData(invalid);Load(badData);}catch(...){rejected=true;}Check(rejected,"invalid native shape rejected");
        }
        std::vector<Tile> pair(2);pair[0].id=1;pair[0].shape=Shape::Rectangle;pair[0].position={500,400};
        pair[1].id=2;pair[1].shape=Shape::Triangle;pair[1].position={100,100};
        Point contact{pair[0].position.x,pair[0].position.y-TileHeight(pair[1].size,pair[1].shape)};
        auto mixedShapeSnap=Snap(pair,{2},contact-pair[1].position+Point{0,5},&snapped);
        Check(snapped&&std::abs((pair[1].position+mixedShapeSnap).x-contact.x)<1e-8&&
            std::abs((pair[1].position+mixedShapeSnap).y-contact.y)<1e-8&&ValidDelta(pair,{2},mixedShapeSnap),"triangle base snaps and centers against rectangle top without overlap");
        auto edgeOnly=Snap(pair,{2},contact-pair[1].position+Point{50,5},&snapped);
        Check(snapped&&std::abs((pair[1].position+edgeOnly).x-contact.x-50)<1e-8&&
            std::abs((pair[1].position+edgeOnly).y-contact.y)<1e-8,"edge snap works between alignment anchors");
        Move(pair,{2},mixedShapeSnap);auto offset=pair[1].position-pair[0].position;
        Check(Resize(pair,{1,2},1,240)&&pair[1].shape==Shape::Triangle&&pair[0].shape==Shape::Rectangle&&
            std::abs(pair[1].position.y-pair[0].position.y-offset.y*1.5)<1e-8,"mixed-shape group resize preserves shapes and scales offsets");
        auto mergedShapes=Merge(pair,shapeBundle.tiles);bool separated=true;
        for(size_t i=0;i<pair.size();++i)for(size_t j=pair.size();j<mergedShapes.size();++j) {
            const auto& a=mergedShapes[i];const auto& b=mergedShapes[j];separated&=!Overlaps(a.position,b.position,a.size,b.size,a.shape,b.shape);
        }
        Check(separated&&mergedShapes[3].shape==Shape::Triangle&&mergedShapes[4].shape==Shape::Rectangle,"JSON merge preserves mixed shapes and prevents collision");
        std::cout << "PASS: " << checks << " checks\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
