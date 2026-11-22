#pragma once
#include "layouts.hpp"
#include <deque>

namespace desktile_convenience {
using namespace hexdesk;
inline std::string EditKey(LayoutCatalog catalog) {
    for(auto& entry:catalog.profiles)for(auto& tile:entry.second.tiles) {
        tile.elapsed=0;tile.running=false;tile.notified=false;
        tile.alarm.nextDue=0;tile.alarm.lastFired=0;
    }
    return CatalogJson(catalog).dump();
}
inline void PreserveClocks(LayoutCatalog& target,const LayoutCatalog& current) {
    for(auto& entry:target.profiles)for(auto& tile:entry.second.tiles) {
        const auto profile=current.profiles.find(entry.first);
        const Tile* live=nullptr;
        if(profile!=current.profiles.end())for(const auto& candidate:profile->second.tiles)
            if(candidate.id==tile.id){live=&candidate;break;}
        tile.running=false;tile.alarm.enabled=false;tile.alarm.nextDue=0;
        if(!live||live->kind!=tile.kind)continue;
        if(live->duration==tile.duration&&live->countdown==tile.countdown) {
            tile.elapsed=live->elapsed;tile.running=live->running;tile.notified=live->notified;
        }
        if(live->alarm.minute==tile.alarm.minute&&live->alarm.date==tile.alarm.date&&
            live->alarm.repeat==tile.alarm.repeat&&live->alarm.weekdays==tile.alarm.weekdays)tile.alarm=live->alarm;
    }
}
inline size_t HistoryBytes(const LayoutCatalog& catalog) {
    size_t bytes=0;
    for(const auto& p:catalog.profiles){
        bytes+=sizeof(Profile)+p.second.name.size()+p.second.hidden.size()*sizeof(int);
        for(const auto& tile:p.second.tiles)bytes+=sizeof(Tile)+tile.text.size()+tile.image.size()+tile.fontFamily.size();
    }
    return bytes;
}
inline void TrimHistory(std::deque<LayoutCatalog>& items) {
    size_t bytes=0;for(const auto& item:items)bytes+=HistoryBytes(item);
    while(!items.empty()&&(items.size()>30||bytes>8*1024*1024)){
        bytes-=HistoryBytes(items.front());items.pop_front();
    }
}
struct History {
    std::deque<LayoutCatalog> undo,redo;
    LayoutCatalog saved;
    bool ready=false;
    void Reset(const LayoutCatalog& catalog){saved=catalog;undo.clear();redo.clear();ready=true;}
    bool Record(const LayoutCatalog& catalog) {
        if(!ready){Reset(catalog);return false;}
        if(EditKey(saved)==EditKey(catalog)){saved=catalog;return false;}
        undo.push_back(saved);TrimHistory(undo);
        redo.clear();saved=catalog;return true;
    }
};
struct RemovedTile {int layout;std::string name;Tile tile;};
inline std::vector<RemovedTile> Removed(const LayoutCatalog& previous,const LayoutCatalog& next) {
    std::vector<RemovedTile> removed;
    for(const auto& p:previous.profiles)for(const auto& tile:p.second.tiles) {
        const auto at=next.profiles.find(p.first);
        if(at==next.profiles.end()||std::none_of(at->second.tiles.begin(),at->second.tiles.end(),
            [&](const Tile& t){return t.id==tile.id;}))removed.push_back({p.first,p.second.name,tile});
    }
    return removed;
}
inline void PauseSchedules(std::vector<Tile>& tiles) {
    for(auto& tile:tiles){tile.running=false;tile.alarm.enabled=false;tile.alarm.nextDue=0;}
}
inline Bundle ShareBundle(std::vector<Tile> tiles,bool includeText,bool includePhotos,
        const std::function<Asset(const std::string&)>& read) {
    PauseSchedules(tiles);
    for(auto& tile:tiles) {
        if(!includeText)tile.text=tile.kind==Kind::Text?"Your note":"";
        if(!includePhotos){tile.image.clear();if(tile.kind==Kind::Image)tile.text.clear();}
        tile.elapsed=0;tile.notified=false;tile.alarm.lastFired=0;
        tile.alarm.date.clear();tile.alarm.repeat=AlarmRepeat::Daily;
    }
    return Pack(tiles,read);
}
inline int NewProfile(LayoutCatalog& catalog,std::string name,std::vector<Tile> tiles) {
    const auto base=name;
    for(int i=2;std::any_of(catalog.profiles.begin(),catalog.profiles.end(),[&](const auto& p){return p.second.name==name;});++i)
        name=base+" "+std::to_string(i);
    const int id=CreateProfile(catalog,name,true);
    auto& profile=catalog.profiles.at(id);profile.tiles=std::move(tiles);
    PauseSchedules(profile.tiles);NormalizeProfile(profile);catalog.active=id;return id;
}
inline std::vector<Tile> Preset(int index) {
    std::vector<Tile> tiles(3);
    for(int i=0;i<3;++i){auto& t=tiles[i];t.id=i+1;t.position={90.+i*190,160};t.size=180;t.hideLabels=true;}
    tiles[0].text="Today's focus";
    tiles[1].shape=Shape::Rectangle;tiles[1].rectangleHeight=120;tiles[1].text="Next step";
    tiles[2].shape=Shape::Rectangle;tiles[2].rectangleHeight=80;tiles[2].kind=Kind::Timer;
    tiles[2].duration=index==2?1500:300;tiles[2].text.clear();tiles[2].fontFamily="Consolas";tiles[2].fontSize=28;
    if(index==1){tiles[1].shape=Shape::Triangle;tiles[1].rectangleHeight=0;tiles[1].kind=Kind::Image;tiles[1].text.clear();}
    if(index==1)for(auto& t:tiles){t.backgroundColor=0xACC397;t.textColor=0x151D14;t.borderColor=0xC8E5AE;t.borderStyle=BorderStyle::Solid;}
    if(index==2)for(auto& t:tiles){t.backgroundColor=0x202C3B;t.textColor=0xE6EAF0;t.borderColor=0x90ADC6;}
    if(index==3)for(auto& t:tiles){t.backgroundColor=0xF1E4C7;t.textColor=0x332C22;t.borderColor=0xA18256;t.borderStyle=BorderStyle::Solid;}
    return tiles;
}
enum class SyncDecision { NoChange, Upload, Download, Conflict };
inline SyncDecision DecideSync(const std::string& local,const std::string& remote,const std::string& baseline) {
    if(local==remote)return SyncDecision::NoChange;
    if(remote.empty()||remote==baseline)return SyncDecision::Upload;
    if(local==baseline)return SyncDecision::Download;
    return SyncDecision::Conflict;
}
inline bool FullscreenRect(Point origin,Point size,Point monitorOrigin,Point monitorSize) {
    return origin.x<=monitorOrigin.x+1&&origin.y<=monitorOrigin.y+1&&
        origin.x+size.x>=monitorOrigin.x+monitorSize.x-1&&origin.y+size.y>=monitorOrigin.y+monitorSize.y-1;
}
} // namespace desktile_convenience
