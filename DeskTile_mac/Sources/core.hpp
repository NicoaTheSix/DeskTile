#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <istream>
#include <ostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace hexdesk {
constexpr double Width = 160.0;
constexpr double Height = 138.56406460551018;
constexpr double SnapDistance = 22.0;
struct Point { double x = 0, y = 0; };
inline Point operator+(Point a, Point b) { return {a.x + b.x, a.y + b.y}; }
inline Point operator-(Point a, Point b) { return {a.x - b.x, a.y - b.y}; }
enum class Kind { Text, Image, Timer };
enum class Shape { Hexagon, Triangle, Rectangle };
enum class BorderStyle { Solid, Pixel, Rgb, Breathing };
struct Tile {
    int id = 0;
    Point position;
    Kind kind = Kind::Text;
    Shape shape = Shape::Hexagon;
    std::string text = "新文字";
    std::string image;
    bool topmost = true;
    bool countdown = true;
    double duration = 300, elapsed = 0;
    bool running = false, notified = false;
    BorderStyle borderStyle = BorderStyle::Solid;
    std::uint32_t borderColor = 0x485F69;
    double borderWidth = 2, rgbCycleSeconds = 6;
    bool rgbAnimated = true;
    double size = Width;
    std::string fontFamily = "Microsoft JhengHei UI";
    double fontSize = 14;
    std::uint32_t textColor = 0xEBF4F5;
    std::uint32_t backgroundColor = 0x172730;
    double backgroundTransparency = 0;
};
inline double TileHeight(double size,Shape shape=Shape::Hexagon) {
    return shape==Shape::Rectangle?size*2/3:size*std::sqrt(3.0)/2;
}
inline bool SameTypography(const Tile& a,const Tile& b) {
    return a.fontFamily==b.fontFamily && a.fontSize==b.fontSize && a.textColor==b.textColor;
}
inline bool AnimatedBorder(const Tile& t) {
    return (t.borderStyle==BorderStyle::Rgb || t.borderStyle==BorderStyle::Breathing) && t.rgbAnimated;
}
inline double BreathLevel(double seconds,double cycle,bool playing=true) {
    if(!playing)return 1;
    double phase=seconds/cycle-std::floor(seconds/cycle);
    return .2+.8*(.5-.5*std::cos(phase*6.283185307179586));
}
inline bool SameAppearance(const Tile& a,const Tile& b) {
    return a.borderStyle==b.borderStyle && a.borderColor==b.borderColor &&
        a.borderWidth==b.borderWidth && a.rgbCycleSeconds==b.rgbCycleSeconds && a.rgbAnimated==b.rgbAnimated;
}
inline bool ParseColor(const std::string& text,std::uint32_t& color) {
    size_t start=!text.empty() && text[0]=='#'?1:0;
    if(text.size()-start!=6)return false;
    std::uint32_t value=0;
    for(size_t i=start;i<text.size();++i) {
        char c=text[i];int digit=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;
        if(digit<0)return false;
        value=(value<<4)|digit;
    }
    color=value;return true;
}
inline std::string ColorText(std::uint32_t color) {
    std::ostringstream out;out << '#' << std::hex << std::uppercase << std::setw(6) << std::setfill('0') << color;
    return out.str();
}
inline std::uint32_t Rainbow(double hue) {
    hue-=std::floor(hue);double sector=hue*6;int part=static_cast<int>(sector);
    int rising=static_cast<int>(std::round((sector-part)*255)),falling=255-rising;
    int r=0,g=0,b=0;
    switch(part) {
    case 0:r=255;g=rising;break;case 1:r=falling;g=255;break;
    case 2:g=255;b=rising;break;case 3:g=falling;b=255;break;
    case 4:r=rising;b=255;break;default:r=255;b=falling;break;
    }
    return (r<<16)|(g<<8)|b;
}
inline double ColorHue(std::uint32_t color) {
    double r=double((color>>16)&255)/255,g=double((color>>8)&255)/255,b=double(color&255)/255;
    double hi=std::max({r,g,b}),lo=std::min({r,g,b}),range=hi-lo;
    if(range==0)return 0;
    double hue=hi==r?(g-b)/range:hi==g?2+(b-r)/range:4+(r-g)/range;
    return hue/6-std::floor(hue/6);
}
inline std::vector<Point> BorderOutline(BorderStyle style,double inset,double size=Width,Shape shape=Shape::Hexagon) {
    double height=TileHeight(size,shape);
    std::vector<Point> corners;
    if(shape==Shape::Rectangle)corners={{inset,inset},{size-inset,inset},{size-inset,height-inset},{inset,height-inset}};
    else {
        corners=shape==Shape::Triangle?std::vector<Point>{{size/2,0},{size,height},{0,height}}:
            std::vector<Point>{{size/4,0},{size*3/4,0},{size,height/2},{size*3/4,height},{size/4,height},{0,height/2}};
        double centerY=shape==Shape::Triangle?height*2/3:height/2;
        double radius=shape==Shape::Triangle?height/3:height/2,scale=1-inset/radius;
        for(auto& p:corners)p={size/2+(p.x-size/2)*scale,centerY+(p.y-centerY)*scale};
    }
    if(style!=BorderStyle::Pixel)return corners;
    auto quantize=[&](Point p) {return Point{std::clamp(std::round(p.x/4)*4,0.0,size),std::clamp(std::round(p.y/4)*4,0.0,height)};};
    std::vector<Point> steps;
    auto add=[&](Point p){if(steps.empty() || steps.back().x!=p.x || steps.back().y!=p.y)steps.push_back(p);};
    add(quantize(corners[0]));
    for(size_t edge=0;edge<corners.size();++edge) {
        Point a=corners[edge],b=corners[(edge+1)%corners.size()];
        int count=static_cast<int>(std::ceil(std::max(std::abs(a.x-b.x),std::abs(a.y-b.y))/4));
        for(int i=1;i<=count;++i) {
            double t=double(i)/count;Point p=quantize({a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t});
            add({p.x,steps.back().y});add(p);
        }
    }
    return steps;
}
inline double Cross(Point a,Point b,Point c){return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);}
inline std::vector<Point> ContactBoundary(double fixedSize,double movingSize,Shape fixedShape,Shape movingShape) {
    auto fixed=BorderOutline(BorderStyle::Solid,0,fixedSize,fixedShape),moving=BorderOutline(BorderStyle::Solid,0,movingSize,movingShape);
    std::vector<Point> points;for(auto a:fixed)for(auto b:moving)points.push_back(a-b);
    std::sort(points.begin(),points.end(),[](Point a,Point b){return a.x==b.x?a.y<b.y:a.x<b.x;});
    points.erase(std::unique(points.begin(),points.end(),[](Point a,Point b){return a.x==b.x&&a.y==b.y;}),points.end());
    std::vector<Point> hull;
    for(auto p:points){while(hull.size()>=2 && Cross(hull[hull.size()-2],hull.back(),p)<=1e-8)hull.pop_back();hull.push_back(p);}
    auto lower=hull.size();
    for(size_t i=points.size()-1;i-->0;){auto p=points[i];while(hull.size()>lower && Cross(hull[hull.size()-2],hull.back(),p)<=1e-8)hull.pop_back();hull.push_back(p);}
    hull.pop_back();return hull;
}
inline std::vector<Point> Neighbors(double fixedSize=Width,double movingSize=Width,
        Shape fixedShape=Shape::Hexagon,Shape movingShape=Shape::Hexagon,const Point* preferred=nullptr) {
    if(fixedShape!=Shape::Hexagon || movingShape!=Shape::Hexagon) {
        auto hull=ContactBoundary(fixedSize,movingSize,fixedShape,movingShape);std::vector<Point> contacts;
        for(size_t i=0;i<hull.size();++i) {
            auto a=hull[i],b=hull[(i+1)%hull.size()];contacts.push_back({(a.x+b.x)/2,(a.y+b.y)/2});
        }
        contacts.insert(contacts.end(),hull.begin(),hull.end());
        if(preferred)for(size_t i=0;i<hull.size();++i) {
            auto a=hull[i],edge=hull[(i+1)%hull.size()]-a,difference=*preferred-a;
            double fraction=std::clamp((difference.x*edge.x+difference.y*edge.y)/(edge.x*edge.x+edge.y*edge.y),0.0,1.0);
            contacts.push_back({a.x+fraction*edge.x,a.y+fraction*edge.y});
        }
        return contacts;
    }
    double fixedH=TileHeight(fixedSize),movingH=TileHeight(movingSize);
    double distance=(fixedH+movingH)/2;
    Point shift{(fixedSize-movingSize)/2,(fixedH-movingH)/2};
    std::vector<Point> directions={{0,-distance},{distance*std::sqrt(3.0)/2,-distance/2},
        {distance*std::sqrt(3.0)/2,distance/2},{0,distance},
        {-distance*std::sqrt(3.0)/2,distance/2},{-distance*std::sqrt(3.0)/2,-distance/2}};
    for(auto& p:directions)p=p+shift;
    return directions;
}
inline bool Overlaps(Point a, Point b,double aSize=Width,double bSize=Width,
        Shape aShape=Shape::Hexagon,Shape bShape=Shape::Hexagon) {
    double aH=TileHeight(aSize,aShape),bH=TileHeight(bSize,bShape);
    if(aShape!=Shape::Hexagon || bShape!=Shape::Hexagon) {
        if(a.x+aSize<=b.x+.6 || b.x+bSize<=a.x+.6 || a.y+aH<=b.y+.6 || b.y+bH<=a.y+.6)return false;
        auto first=BorderOutline(BorderStyle::Solid,0,aSize,aShape),second=BorderOutline(BorderStyle::Solid,0,bSize,bShape);
        for(auto& p:first)p=p+a;for(auto& p:second)p=p+b;
        for(const auto* polygon:{&first,&second})for(size_t i=0;i<polygon->size();++i) {
            auto edge=(*polygon)[(i+1)%polygon->size()]-(*polygon)[i];double length=std::hypot(edge.x,edge.y);
            Point axis{-edge.y/length,edge.x/length};
            auto project=[&](const std::vector<Point>& vertices) {
                double lo=vertices[0].x*axis.x+vertices[0].y*axis.y,hi=lo;
                for(auto p:vertices){double value=p.x*axis.x+p.y*axis.y;lo=std::min(lo,value);hi=std::max(hi,value);}return std::pair<double,double>{lo,hi};
            };
            auto p=project(first),q=project(second);if(std::min(p.second,q.second)-std::max(p.first,q.first)<=.6)return false;
        }
        return true;
    }
    const double dx=std::abs(a.x+aSize/2-b.x-bSize/2),dy=std::abs(a.y+aH/2-b.y-bH/2);
    // Minkowski sum of aligned regular hexagons, including unequal sizes.
    return dy<(aH+bH)/2-0.6 && std::sqrt(3.0)*dx+dy<aH+bH-0.6;
}
inline bool Select(std::set<int>& selected, int id, bool shift) {
    if (shift) {
        if (selected.count(id)) selected.erase(id); else selected.insert(id);
    } else if (!(selected.size() == 1 && selected.count(id))) {
        selected.clear(); selected.insert(id);
    }
    return selected.count(id) != 0;
}
inline bool ValidDelta(const std::vector<Tile>& tiles, const std::set<int>& selected, Point delta) {
    for (const auto& moving : tiles) if (selected.count(moving.id))
        for (const auto& fixed : tiles) if (!selected.count(fixed.id))
            if (Overlaps(moving.position+delta, fixed.position,moving.size,fixed.size,moving.shape,fixed.shape)) return false;
    return true;
}
inline Point Snap(const std::vector<Tile>& tiles, const std::set<int>& selected, Point desired,
                  bool* snapped = nullptr) {
    std::vector<std::pair<double,Point>> candidates;
    std::set<std::pair<long long,long long>> seen;
    for (const auto& moving : tiles) if (selected.count(moving.id))
        for (const auto& fixed : tiles) if (!selected.count(fixed.id)) {
            auto preferred=moving.position+desired-fixed.position;
            auto directions=Neighbors(fixed.size,moving.size,fixed.shape,moving.shape,&preferred);
            size_t aligned=(fixed.shape==Shape::Hexagon&&moving.shape==Shape::Hexagon)?directions.size():directions.size()*2/3;
            for (size_t i=0;i<directions.size();++i) {
                auto direction=directions[i];
                Point candidate = fixed.position+direction-moving.position;
                Point difference = candidate-desired;
                double distance = difference.x*difference.x+difference.y*difference.y;
                if (distance <= SnapDistance*SnapDistance &&
                    seen.emplace(std::llround(candidate.x*1000),std::llround(candidate.y*1000)).second)
                    candidates.push_back({distance+(i>=aligned?SnapDistance*SnapDistance:0),candidate});
            }
        }
    std::sort(candidates.begin(),candidates.end(),[](const auto& a,const auto& b){return a.first<b.first;});
    for(const auto& item:candidates) if(ValidDelta(tiles,selected,item.second)) {
        if(snapped)*snapped=true;
        return item.second;
    }
    if(snapped)*snapped=false;
    return desired;
}
inline void Move(std::vector<Tile>& tiles, const std::set<int>& selected, Point delta) {
    for (auto& tile:tiles) if (selected.count(tile.id)) tile.position=tile.position+delta;
}
inline bool Resize(std::vector<Tile>& tiles,const std::set<int>& selected,int anchor,double size) {
    if(!std::isfinite(size) || size<80 || size>480 || selected.empty() || !selected.count(anchor))return false;
    auto at=std::find_if(tiles.begin(),tiles.end(),[&](const Tile& t){return t.id==anchor;});
    if(at==tiles.end())return false;
    const Point origin=at->position;double ratio=size/at->size;
    auto candidate=tiles;
    for(auto& t:candidate)if(selected.count(t.id)) {
        t.position=origin+Point{(t.position.x-origin.x)*ratio,(t.position.y-origin.y)*ratio};t.size=size;
    }
    for(size_t i=0;i<candidate.size();++i)for(size_t j=i+1;j<candidate.size();++j) {
        const auto& a=candidate[i];const auto& b=candidate[j];
        if((selected.count(a.id)||selected.count(b.id)) && Overlaps(a.position,b.position,a.size,b.size,a.shape,b.shape))return false;
    }
    tiles=std::move(candidate);return true;
}
inline bool Advance(Tile& tile, double seconds) {
    if (tile.kind!=Kind::Timer || !tile.running) return false;
    tile.elapsed += std::max(0.0,seconds);
    if (tile.countdown && tile.elapsed>=tile.duration) {
        tile.elapsed=tile.duration; tile.running=false;
        if (!tile.notified) { tile.notified=true; return true; }
    }
    return false;
}
inline std::string Clock(const Tile& tile) {
    const double value=tile.countdown ? std::max(0.0,tile.duration-tile.elapsed) : tile.elapsed;
    const auto seconds=static_cast<long long>(tile.countdown ? std::ceil(value) : std::floor(value));
    std::ostringstream out;
    out << std::setfill('0') << std::setw(2) << seconds/3600 << ':'
        << std::setw(2) << seconds/60%60 << ':' << std::setw(2) << seconds%60;
    return out.str();
}
inline void Save(std::ostream& out, const std::vector<Tile>& tiles) {
    out << "HEXDESK 5\n" << tiles.size() << '\n' << std::setprecision(17);
    for (const auto& t:tiles)
        out << t.id << ' ' << t.position.x << ' ' << t.position.y << ' '
            << static_cast<int>(t.kind) << ' ' << t.topmost << ' ' << t.countdown << ' '
            << t.duration << ' ' << t.elapsed << ' ' << std::quoted(t.text) << ' '
            << std::quoted(t.image) << ' ' << static_cast<int>(t.borderStyle) << ' ' << t.borderColor
            << ' ' << t.borderWidth << ' ' << t.rgbCycleSeconds << ' ' << t.rgbAnimated
            << ' ' << t.size << ' ' << std::quoted(t.fontFamily) << ' ' << t.fontSize << ' ' << t.textColor
            << ' ' << t.backgroundColor << ' ' << t.backgroundTransparency << ' ' << static_cast<int>(t.shape) << '\n';
    if (!out) throw std::runtime_error("寫入版面失敗");
}
inline std::vector<Tile> Load(std::istream& in) {
    std::string magic; int version=0; size_t count=0;
    if (!(in>>magic>>version>>count) || magic!="HEXDESK" || version<1 || version>5 || count>500)
        throw std::runtime_error("不支援的版面格式，或格子超過 500 個");
    std::vector<Tile> result; std::set<int> ids;
    for (size_t i=0;i<count;++i) {
        Tile t; int kind=0, top=0, down=0;
        if (!(in>>t.id>>t.position.x>>t.position.y>>kind>>top>>down
              >>t.duration>>t.elapsed>>std::quoted(t.text)>>std::quoted(t.image)) ||
            t.id<=0 || t.id>1000000 || !ids.insert(t.id).second || kind<0 || kind>2 ||
            (top!=0 && top!=1) || (down!=0 && down!=1) ||
            !std::isfinite(t.position.x) || !std::isfinite(t.position.y) ||
            std::abs(t.position.x)>1000000 || std::abs(t.position.y)>1000000 ||
            !std::isfinite(t.duration) || t.duration<1 || t.duration>8640000 ||
            !std::isfinite(t.elapsed) || t.elapsed<0 || t.elapsed>1e12 ||
            t.text.size()>65536 || t.image.size()>32768)
            throw std::runtime_error("版面資料損壞，已保留原始檔案");
        t.kind=static_cast<Kind>(kind); t.topmost=top!=0; t.countdown=down!=0;
        if(version>=2) {
            int style=0,animated=0;
            if(!(in>>style>>t.borderColor>>t.borderWidth>>t.rgbCycleSeconds>>animated) ||
                style<0 || style>(version>=4?3:2) || t.borderColor>0xFFFFFF || !std::isfinite(t.borderWidth) ||
                t.borderWidth<1 || t.borderWidth>12 || !std::isfinite(t.rgbCycleSeconds) ||
                t.rgbCycleSeconds<2 || t.rgbCycleSeconds>60 || (animated!=0 && animated!=1))
                throw std::runtime_error("六角形外觀資料損壞，已保留原始檔案");
            t.borderStyle=static_cast<BorderStyle>(style);t.rgbAnimated=animated!=0;
        }
        if(t.kind==Kind::Timer){t.fontSize=22;t.textColor=0x71EDC4;}
        if(version>=3) {
            if(!(in>>t.size>>std::quoted(t.fontFamily)>>t.fontSize>>t.textColor) ||
                !std::isfinite(t.size) || t.size<80 || t.size>480 || t.fontFamily.empty() ||
                t.fontFamily.size()>256 || t.fontFamily.find_first_of("\r\n\0",0,3)!=std::string::npos ||
                !std::isfinite(t.fontSize) || t.fontSize<8 || t.fontSize>96 || t.textColor>0xFFFFFF)
                throw std::runtime_error("大小或文字樣式資料損壞，已保留原始檔案");
        }
        if(version>=4 && (!(in>>t.backgroundColor>>t.backgroundTransparency) || t.backgroundColor>0xFFFFFF ||
            !std::isfinite(t.backgroundTransparency) || t.backgroundTransparency<0 || t.backgroundTransparency>100))
            throw std::runtime_error("背景資料損壞，已保留原始檔案");
        if(version>=5) {
            int shape=0;if(!(in>>shape) || shape<0 || shape>2)throw std::runtime_error("圖形資料損壞，已保留原始檔案");
            t.shape=static_cast<Shape>(shape);
        }
        if (t.countdown) t.elapsed=std::min(t.elapsed,t.duration);
        t.notified=t.countdown && t.elapsed>=t.duration;
        result.push_back(t);
    }
    in>>std::ws;
    if (!in.eof()) throw std::runtime_error("版面包含無法辨識的附加資料");
    return result;
}
} // namespace hexdesk
