#pragma once
#include "core.hpp"
#include <functional>
#include <map>
#pragma warning(push,0)
#include "third_party/nlohmann/json.hpp"
#pragma warning(pop)

namespace hexdesk {
using Json=nlohmann::json;
constexpr size_t ImageLimit=16*1024*1024,AssetTotalLimit=64*1024*1024,JsonLimit=128*1024*1024;
struct Asset {std::string mime,bytes;};
struct Bundle {
    std::vector<Tile> tiles;
    std::map<std::string,Asset> images;
    std::map<int,std::string> tileImages;
};
inline std::string Base64(const std::string& bytes) {
    static const char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;out.reserve((bytes.size()+2)/3*4);
    for(size_t i=0;i<bytes.size();i+=3) {
        unsigned a=static_cast<unsigned char>(bytes[i]),b=i+1<bytes.size()?static_cast<unsigned char>(bytes[i+1]):0,
            c=i+2<bytes.size()?static_cast<unsigned char>(bytes[i+2]):0;
        out+=alphabet[a>>2];out+=alphabet[((a&3)<<4)|(b>>4)];
        out+=i+1<bytes.size()?alphabet[((b&15)<<2)|(c>>6)]:'=';
        out+=i+2<bytes.size()?alphabet[c&63]:'=';
    }
    return out;
}
inline std::string Unbase64(const std::string& encoded) {
    if(encoded.size()%4 || encoded.size()>(ImageLimit+2)/3*4)throw std::runtime_error("圖片 Base64 長度錯誤或超過 16 MiB");
    auto digit=[](char c)->int {
        if(c>='A'&&c<='Z')return c-'A';if(c>='a'&&c<='z')return c-'a'+26;
        if(c>='0'&&c<='9')return c-'0'+52;if(c=='+')return 62;if(c=='/')return 63;return -1;
    };
    std::string out;out.reserve(encoded.size()/4*3);
    for(size_t i=0;i<encoded.size();i+=4) {
        int a=digit(encoded[i]),b=digit(encoded[i+1]),c=digit(encoded[i+2]),d=digit(encoded[i+3]);
        bool padC=encoded[i+2]=='=',padD=encoded[i+3]=='=';
        if(a<0 || b<0 || (!padC&&c<0) || (!padD&&d<0) || (padC&&!padD) ||
            ((padC||padD)&&i+4!=encoded.size()) || (padC&&(b&15)) || (!padC&&padD&&(c&3)))
            throw std::runtime_error("圖片 Base64 編碼無效");
        out+=static_cast<char>((a<<2)|(b>>4));
        if(!padC)out+=static_cast<char>((b<<4)|(c>>2));
        if(!padD)out+=static_cast<char>((c<<6)|d);
    }
    if(out.size()>ImageLimit)throw std::runtime_error("圖片超過 16 MiB");return out;
}
inline std::string ImageMime(const std::string& bytes) {
    if(bytes.size()>=8 && bytes.compare(0,8,"\x89PNG\r\n\x1a\n",8)==0)return "image/png";
    if(bytes.size()>=3 && static_cast<unsigned char>(bytes[0])==255 && static_cast<unsigned char>(bytes[1])==216 && static_cast<unsigned char>(bytes[2])==255)return "image/jpeg";
    if(bytes.size()>=2 && bytes[0]=='B' && bytes[1]=='M')return "image/bmp";
    throw std::runtime_error("配置圖片僅支援 PNG、JPEG 或 BMP");
}
inline void ValidateTiles(const std::vector<Tile>& tiles) {
    std::stringstream data;Save(data,tiles);Load(data);
}
inline Bundle Pack(const std::vector<Tile>& tiles,const std::function<Asset(const std::string&)>& read) {
    ValidateTiles(tiles);Bundle bundle;bundle.tiles=tiles;
    std::map<std::string,std::string> paths;size_t total=0;
    for(auto& t:bundle.tiles) {
        if(!t.image.empty()) {
            auto found=paths.find(t.image);
            if(found==paths.end()) {
                Asset asset=read(t.image);
                if(asset.bytes.empty() || asset.bytes.size()>ImageLimit)throw std::runtime_error("圖片不存在、為空或超過 16 MiB");
                asset.mime=ImageMime(asset.bytes);total+=asset.bytes.size();
                if(total>AssetTotalLimit)throw std::runtime_error("配置圖片總量超過 64 MiB");
                auto key="image-"+std::to_string(bundle.images.size()+1);
                found=paths.emplace(t.image,key).first;bundle.images.emplace(key,std::move(asset));
            }
            bundle.tileImages[t.id]=found->second;
        }
        t.image.clear();
    }
    return bundle;
}
inline Json BundleJson(const Bundle& bundle) {
    Json root={{"format","HexDesk"},{"version",2},{"tiles",Json::array()},{"images",Json::object()}};
    for(const auto& t:bundle.tiles) {
        Json image=nullptr;auto it=bundle.tileImages.find(t.id);if(it!=bundle.tileImages.end())image=it->second;
        root["tiles"].push_back({{"id",t.id},{"position",{{"x",t.position.x},{"y",t.position.y}}},{"size",t.size},
            {"shape",static_cast<int>(t.shape)},{"kind",static_cast<int>(t.kind)},{"text",t.text},{"imageAsset",image},{"topmost",t.topmost},
            {"timer",{{"countdown",t.countdown},{"duration",t.duration},{"elapsed",t.elapsed}}},
            {"border",{{"style",static_cast<int>(t.borderStyle)},{"color",ColorText(t.borderColor)},
                {"width",t.borderWidth},{"cycleSeconds",t.rgbCycleSeconds},{"animated",t.rgbAnimated}}},
            {"typography",{{"family",t.fontFamily},{"size",t.fontSize},{"color",ColorText(t.textColor)}}},
            {"background",{{"color",ColorText(t.backgroundColor)},{"transparency",t.backgroundTransparency}}}});
    }
    for(const auto& item:bundle.images)root["images"][item.first]={{"mime",item.second.mime},{"data",Base64(item.second.bytes)}};
    return root;
}
inline std::string JsonString(const Json& v,size_t max) {
    if(!v.is_string())throw std::runtime_error("JSON 欄位必須是文字");
    auto text=v.get<std::string>();if(text.size()>max || text.find('\0')!=std::string::npos)throw std::runtime_error("JSON 文字過長或包含空字元");return text;
}
inline int JsonInt(const Json& v,int lo,int hi) {
    if(!v.is_number_integer() || v<lo || v>hi)throw std::runtime_error("JSON 整數欄位超出範圍");return v.get<int>();
}
inline double JsonNumber(const Json& v) {
    if(!v.is_number())throw std::runtime_error("JSON 欄位必須是數字");
    double value=v.get<double>();if(!std::isfinite(value))throw std::runtime_error("JSON 數字無效");return value;
}
inline bool JsonBool(const Json& v) {if(!v.is_boolean())throw std::runtime_error("JSON 欄位必須是布林值");return v.get<bool>();}
inline std::uint32_t JsonColor(const Json& v) {
    std::uint32_t color=0;if(!ParseColor(JsonString(v,7),color))throw std::runtime_error("JSON 色碼無效");return color;
}
inline Bundle ReadBundle(const Json& root) {
    if(!root.is_object() || root.at("format")!="HexDesk" || JsonInt(root.at("version"),1,2)<1 ||
        !root.at("tiles").is_array() || root.at("tiles").size()>500 || !root.at("images").is_object() || root.at("images").size()>500)
        throw std::runtime_error("不支援的桌面拼格 JSON 配置");
    Bundle bundle;size_t total=0;
    for(auto it=root.at("images").begin();it!=root.at("images").end();++it) {
        if(it.key().empty() || it.key().size()>80)throw std::runtime_error("圖片識別碼無效");
        Asset asset;asset.mime=JsonString(it.value().at("mime"),32);
        const auto& encoded=it.value().at("data");if(!encoded.is_string())throw std::runtime_error("圖片資料必須是 Base64 文字");
        asset.bytes=Unbase64(encoded.get_ref<const std::string&>());
        if(asset.bytes.empty() || asset.mime!=ImageMime(asset.bytes))throw std::runtime_error("圖片格式與內容不一致");
        total+=asset.bytes.size();if(total>AssetTotalLimit)throw std::runtime_error("配置圖片總量超過 64 MiB");
        bundle.images.emplace(it.key(),std::move(asset));
    }
    for(const auto& j:root.at("tiles")) {
        Tile t;t.id=JsonInt(j.at("id"),1,1000000);t.position={JsonNumber(j.at("position").at("x")),JsonNumber(j.at("position").at("y"))};
        if(root.at("version")==2)t.shape=static_cast<Shape>(JsonInt(j.at("shape"),0,2));
        t.size=JsonNumber(j.at("size"));t.kind=static_cast<Kind>(JsonInt(j.at("kind"),0,2));t.text=JsonString(j.at("text"),65536);
        t.topmost=JsonBool(j.at("topmost"));const auto& timer=j.at("timer");
        t.countdown=JsonBool(timer.at("countdown"));t.duration=JsonNumber(timer.at("duration"));t.elapsed=JsonNumber(timer.at("elapsed"));
        const auto& b=j.at("border");t.borderStyle=static_cast<BorderStyle>(JsonInt(b.at("style"),0,3));
        t.borderColor=JsonColor(b.at("color"));t.borderWidth=JsonNumber(b.at("width"));
        t.rgbCycleSeconds=JsonNumber(b.at("cycleSeconds"));t.rgbAnimated=JsonBool(b.at("animated"));
        const auto& f=j.at("typography");t.fontFamily=JsonString(f.at("family"),256);t.fontSize=JsonNumber(f.at("size"));t.textColor=JsonColor(f.at("color"));
        t.backgroundColor=JsonColor(j.at("background").at("color"));t.backgroundTransparency=JsonNumber(j.at("background").at("transparency"));
        if(!j.at("imageAsset").is_null()) {
            auto key=JsonString(j.at("imageAsset"),80);if(!bundle.images.count(key))throw std::runtime_error("配置缺少圖片資料");
            bundle.tileImages[t.id]=key;
        }
        t.notified=t.countdown && t.elapsed>=t.duration;bundle.tiles.push_back(std::move(t));
    }
    ValidateTiles(bundle.tiles);
    for(auto& t:bundle.tiles){if(t.countdown)t.elapsed=std::min(t.elapsed,t.duration);t.notified=t.countdown&&t.elapsed>=t.duration;}
    for(size_t i=0;i<bundle.tiles.size();++i)for(size_t j=i+1;j<bundle.tiles.size();++j) {
        const auto& a=bundle.tiles[i];const auto& b=bundle.tiles[j];
        if(Overlaps(a.position,b.position,a.size,b.size,a.shape,b.shape))throw std::runtime_error("JSON 配置內的圖形彼此重疊");
    }
    return bundle;
}
inline Bundle ParseBundle(std::istream& in) {
    std::vector<std::set<std::string>> keys;
    auto callback=[&](int depth,Json::parse_event_t event,Json& parsed) {
        if(depth>32)throw std::runtime_error("JSON 結構過深");
        if(event==Json::parse_event_t::object_start)keys.emplace_back();
        else if(event==Json::parse_event_t::key && !keys.back().insert(parsed.get<std::string>()).second)throw std::runtime_error("JSON 欄位重複");
        else if(event==Json::parse_event_t::object_end)keys.pop_back();return true;
    };
    return ReadBundle(Json::parse(in,callback));
}
inline std::vector<Tile> Merge(const std::vector<Tile>& existing,const std::vector<Tile>& incoming) {
    if(existing.size()+incoming.size()>500)throw std::runtime_error("合併後超過 500 格");
    ValidateTiles(existing);ValidateTiles(incoming);auto added=incoming;std::set<int> ids;
    for(const auto& t:existing)ids.insert(t.id);int next=1;
    for(auto& t:added) {while(ids.count(next))++next;t.id=next;ids.insert(next++);}
    auto valid=[&](Point delta) {
        for(const auto& a:added) {
            if(std::abs(a.position.x+delta.x)>1000000 || std::abs(a.position.y+delta.y)>1000000)return false;
            for(const auto& b:existing)if(Overlaps(a.position+delta,b.position,a.size,b.size,a.shape,b.shape))return false;
        }
        return true;
    };
    Point delta{};
    if(!added.empty() && !existing.empty() && !valid(delta)) {
        double right=existing[0].position.x+existing[0].size,left=added[0].position.x;
        double bottom=existing[0].position.y+TileHeight(existing[0].size,existing[0].shape),top=added[0].position.y;
        for(const auto& t:existing){right=std::max(right,t.position.x+t.size);bottom=std::max(bottom,t.position.y+TileHeight(t.size,t.shape));}
        for(const auto& t:added){left=std::min(left,t.position.x);top=std::min(top,t.position.y);}
        delta={right-left+24,0};if(!valid(delta)){delta={0,bottom-top+24};if(!valid(delta))throw std::runtime_error("無法找到合併後的有效位置");}
    }
    std::set<int> addedIds;for(const auto& t:added)addedIds.insert(t.id);Move(added,addedIds,delta);
    auto result=existing;result.insert(result.end(),added.begin(),added.end());ValidateTiles(result);return result;
}
} // namespace hexdesk
