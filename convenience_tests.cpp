#include <iostream>
#include <sstream>
#include "convenience.hpp"
using namespace hexdesk;
using namespace desktile_convenience;
int count=0;
void Check(bool value,const char* message){++count;if(!value)throw std::runtime_error(message);}
int main() {
    try {
        std::cout<<"Input: convenience.hpp, layouts.hpp, in-memory fixtures\nOutput: stdout\n";
        std::cout<<"[1/5] History and clock preservation\n";
        LayoutCatalog initial;Profile p;p.name="Desk";p.tiles=Preset(0);NormalizeProfile(p);initial.profiles.emplace(1,p);
        auto& clock=initial.current().tiles[2];clock.running=true;clock.elapsed=48;
        History history;history.Reset(initial);
        auto tick=initial;tick.current().tiles[2].elapsed=95;
        Check(!history.Record(tick)&&history.undo.empty(),"clock ticks do not create undo steps");
        auto edited=tick;edited.current().tiles[0].text="Edited";
        Check(history.Record(edited)&&history.undo.size()==1,"content changes create undo steps");
        auto restored=history.undo.back();PreserveClocks(restored,edited);
        Check(restored.current().tiles[2].running&&restored.current().tiles[2].elapsed==95,"undo preserves a running timer");
        auto deleted=edited;deleted.current().tiles.erase(deleted.current().tiles.begin()+2);
        auto removed=Removed(edited,deleted);
        Check(removed.size()==1&&removed[0].tile.kind==Kind::Timer,"removed timers enter recycle records");
        restored=edited;PreserveClocks(restored,deleted);
        Check(!restored.current().tiles[2].running,"restored deleted timer starts paused");
        for(int i=0;i<40;++i){edited.current().tiles[0].text=std::to_string(i);history.Record(edited);}
        Check(history.undo.size()==30,"history is bounded");

        std::cout<<"[2/5] Sharing redaction and embedded images\n";
        auto shareTiles=Preset(0);shareTiles[0].text="private@example.com";
        shareTiles[1].kind=Kind::Image;shareTiles[1].image="private.png";
        shareTiles[2].running=true;
        auto redacted=ShareBundle(shareTiles,false,false,[](const std::string&)->Asset{throw std::runtime_error("Private photo must not be read");});
        Check(redacted.tiles[0].text=="Your note"&&redacted.images.empty()&&redacted.tiles[1].image.empty(),"default share excludes private content");
        Check(!redacted.tiles[2].running&&!redacted.tiles[2].alarm.enabled,"shared schedules are paused");
        const std::string png("\x89PNG\r\n\x1a\n",8);
        auto included=ShareBundle(shareTiles,true,true,[&](const std::string&){return Asset{"image/png",png};});
        Check(included.tiles[0].text=="private@example.com"&&included.images.size()==1,"explicitly included text and pictures are retained");
        auto roundtrip=ReadBundle(BundleJson(included));
        Check(roundtrip.tiles.size()==3&&roundtrip.images.size()==1,"share package survives normal JSON import");

        std::cout<<"[3/5] Sync conflict decisions\n";
        Check(DecideSync("a","a","old")==SyncDecision::NoChange,"equal versions need no transfer");
        Check(DecideSync("a","","")==SyncDecision::Upload,"empty remote accepts first upload");
        Check(DecideSync("new","old","old")==SyncDecision::Upload,"local-only edit uploads");
        Check(DecideSync("old","new","old")==SyncDecision::Download,"remote-only edit downloads");
        Check(DecideSync("a","b","old")==SyncDecision::Conflict,"concurrent changes require a choice");
        Check(DecideSync("a","b","")==SyncDecision::Conflict,"existing remote is protected on first use");

        std::cout<<"[4/5] Presets and unique layout names\n";
        for(int i=0;i<4;++i){
            auto tiles=Preset(i);ValidateTiles(tiles);
            Check(tiles.size()==3&&!tiles[2].running,"presets contain paused schedules");
        }
        auto catalog=initial;NewProfile(catalog,"Study",Preset(2));NewProfile(catalog,"Study",Preset(2));
        Check(catalog.current().name=="Study 2","reusing a preset creates a unique layout");
        Check(catalog.profiles.at(1).tiles[2].running,"creating a preset preserves old timers");

        std::cout<<"[5/5] Fullscreen detection geometry\n";
        Check(FullscreenRect({-1920,0},{1920,1080},{-1920,0},{1920,1080}),"fullscreen on a negative-coordinate monitor");
        Check(!FullscreenRect({-1910,20},{1200,900},{-1920,0},{1920,1080}),"windowed applications do not trigger fullscreen");
        std::cout<<"PASS: "<<count<<" checks\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
