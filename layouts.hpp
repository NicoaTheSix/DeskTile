#pragma once
#include "config.hpp"

namespace hexdesk {
constexpr int LayoutLimit=32, LayoutIdLimit=1000000, TileIdLimit=1000000;
constexpr size_t LayoutFileLimit=128ull*1024*1024;
struct Profile {
    int id=1;
    std::string name;
    int nextTile=1;
    std::vector<Tile> tiles;
    std::set<int> hidden;
};
struct LayoutCatalog {
    int active=1,nextLayout=2;
    std::map<int,Profile> profiles;
    Profile& current(){return profiles.at(active);}
    const Profile& current()const{return profiles.at(active);}
};
inline void CheckProfileName(const std::string& name) {
    if(name.empty() || name.size()>128 || name.front()==' ' || name.back()==' ' ||
       std::any_of(name.begin(),name.end(),[](unsigned char c){return c<32 || c==127;}))
        throw std::runtime_error("Use a layout name of 1–128 UTF-8 bytes without outer spaces or control characters.");
}
inline int ProfileId(const LayoutCatalog& catalog,const std::string& name) {
    for(const auto& entry:catalog.profiles)if(entry.second.name==name)return entry.first;
    throw std::runtime_error("Layout not found: "+name);
}
inline void RequireNewProfileName(const LayoutCatalog& catalog,const std::string& name,int except=0) {
    CheckProfileName(name);
    for(const auto& entry:catalog.profiles)
        if(entry.first!=except && entry.second.name==name)throw std::runtime_error("That layout name already exists.");
}
inline int NextTileId(const Profile& profile) {
    int next=profile.nextTile;
    for(const auto& tile:profile.tiles)next=std::max(next,tile.id+1);
    return next;
}
inline void NormalizeProfile(Profile& profile) {
    std::set<int> ids;
    for(const auto& t:profile.tiles)ids.insert(t.id);
    for(auto at=profile.hidden.begin();at!=profile.hidden.end();)
        if(!ids.count(*at))at=profile.hidden.erase(at);else ++at;
    profile.nextTile=NextTileId(profile);
}
inline int CreateProfile(LayoutCatalog& catalog,const std::string& name,bool empty) {
    RequireNewProfileName(catalog,name);
    if(catalog.profiles.size()>=LayoutLimit || catalog.nextLayout>LayoutIdLimit)
        throw std::runtime_error("Layout limit reached (32 saved layouts).");
    Profile copy=empty?Profile{}:catalog.current();
    copy.id=catalog.nextLayout++;
    copy.name=name;
    if(!empty)for(auto& t:copy.tiles) {
        // A saved copy does not create a second running schedule.
        t.running=false;
        if(t.kind==Kind::Alarm){t.alarm.enabled=false;t.alarm.nextDue=0;t.alarm.lastFired=0;}
    }
    const auto id=copy.id;
    catalog.profiles.emplace(id,std::move(copy));
    return id;
}
inline Json CatalogJson(const LayoutCatalog& catalog) {
    Json profiles=Json::array();
    for(const auto& entry:catalog.profiles) {
        const auto& p=entry.second;
        std::ostringstream body;body.imbue(std::locale::classic());Save(body,p.tiles);
        profiles.push_back({{"id",p.id},{"name",p.name},{"nextTile",p.nextTile},{"hidden",p.hidden},{"tiles",body.str()}});
    }
    return {{"format","DeskTiles layouts"},{"version",1},{"active",catalog.active},
            {"nextLayout",catalog.nextLayout},{"profiles",std::move(profiles)}};
}
inline LayoutCatalog ReadCatalog(std::istream& in) {
    auto root=Json::parse(in,[](int depth,Json::parse_event_t,Json&){
        if(depth>12)throw std::runtime_error("Layout catalog nesting is too deep.");return true;
    });
    if(!root.is_object() || root.at("format")!="DeskTiles layouts" || JsonInt(root.at("version"),1,1)!=1)
        throw std::runtime_error("Unsupported layout catalog.");
    auto& records=root.at("profiles");
    if(!records.is_array() || records.empty() || records.size()>LayoutLimit)
        throw std::runtime_error("Invalid layout count.");
    LayoutCatalog catalog;
    catalog.active=JsonInt(root.at("active"),1,LayoutIdLimit);
    catalog.nextLayout=JsonInt(root.at("nextLayout"),2,LayoutIdLimit+1);
    for(const auto& j:records) {
        Profile p;
        p.id=JsonInt(j.at("id"),1,LayoutIdLimit);p.name=JsonString(j.at("name"),128);
        p.nextTile=JsonInt(j.at("nextTile"),1,TileIdLimit+1);
        RequireNewProfileName(catalog,p.name);
        auto body=JsonString(j.at("tiles"),LayoutFileLimit);
        std::istringstream tiles(body);tiles.imbue(std::locale::classic());p.tiles=Load(tiles);
        const auto& hidden=j.at("hidden");
        if(!hidden.is_array() || hidden.size()>p.tiles.size())throw std::runtime_error("Invalid hidden tile list.");
        for(const auto& value:hidden)
            if(!p.hidden.insert(JsonInt(value,1,TileIdLimit)).second)throw std::runtime_error("Duplicate hidden tile.");
        auto before=p.hidden.size();auto next=p.nextTile;NormalizeProfile(p);
        if(before!=p.hidden.size() || next!=p.nextTile || catalog.nextLayout<=p.id ||
           !catalog.profiles.emplace(p.id,std::move(p)).second)
            throw std::runtime_error("Invalid layout or tile identifier.");
    }
    if(!catalog.profiles.count(catalog.active))throw std::runtime_error("Active layout is missing.");
    return catalog;
}
inline bool HasSchedule(const Tile& t) {
    return (t.kind==Kind::Timer && t.running) || (t.kind==Kind::Alarm && t.alarm.enabled && t.alarm.nextDue);
}
struct ClockChange { bool changed=false,due=false,alarm=false; };
inline ClockChange AdvanceLayoutClock(Tile& t,double seconds,std::int64_t now) {
    if(t.kind==Kind::Timer && t.running)return {true,Advance(t,seconds),false};
    if(t.kind==Kind::Alarm && AdvanceAlarm(t.alarm,now))return {true,true,true};
    return {};
}
} // namespace hexdesk
