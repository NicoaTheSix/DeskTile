#pragma once
// The GUI owns all mutations. The CLI sends only a small, versioned request.
using CliArgs=std::vector<std::string>;
using CliOptions=std::map<std::string,std::string>;
namespace ipc=desktiles_ipc;
CliOptions ReadCliOptions(const CliArgs& args,std::initializer_list<const char*> allowed) {
    CliOptions out;
    for(size_t i=2;i<args.size();i+=2){
        if(i+1>=args.size() || std::find(allowed.begin(),allowed.end(),args[i])==allowed.end() ||
           !out.emplace(args[i],args[i+1]).second)
            throw std::runtime_error("Unknown, duplicate or incomplete option: "+args[i]);
    }
    return out;
}
int CliInt(const std::string& value,int low,int high) {
    size_t used=0;long long n=0;
    try{n=std::stoll(value,&used);}catch(...){throw std::runtime_error("Expected an integer: "+value);}
    if(used!=value.size() || n<low || n>high)throw std::runtime_error("Integer out of range: "+value);
    return static_cast<int>(n);
}
double CliNumber(const std::string& value,double low,double high) {
    size_t used=0;double n=0;
    try{n=std::stod(value,&used);}catch(...){throw std::runtime_error("Expected a number: "+value);}
    if(used!=value.size() || !std::isfinite(n) || n<low || n>high)throw std::runtime_error("Number out of range: "+value);
    return n;
}
bool CliBool(const std::string& value) {
    if(value=="true")return true;
    if(value=="false")return false;
    throw std::runtime_error("Expected true or false: "+value);
}
const char* CliShape(Shape s){return s==Shape::Triangle?"triangle":s==Shape::Rectangle?"rectangle":"hexagon";}
const char* CliKind(Kind k){return k==Kind::Image?"image":k==Kind::Timer?"timer":k==Kind::Alarm?"alarm":"text";}
Json CliTile(const std::set<int>& hiddenTiles,const Tile& tile,bool active,bool details) {
    auto hidden=hiddenTiles.count(tile.id)!=0;
    Json item={{"id",tile.id},{"shape",CliShape(tile.shape)},{"kind",CliKind(tile.kind)},
        {"x",tile.position.x},{"y",tile.position.y},{"size",tile.size},{"layer",tile.topmost?"top":"desktop"},
        {"hidden",hidden},{"visible",active&&!hidden}};
    if(details){item["text"]=tile.text;if(tile.kind==Kind::Image)item["image"]=tile.image;}
    if(tile.kind==Kind::Timer)
        item["timer"]={{"mode",tile.countdown?"countdown":"countup"},{"duration",tile.duration},
                       {"elapsed",tile.elapsed},{"running",tile.running},{"clock",Clock(tile)}};
    if(tile.kind==Kind::Alarm)
        item["alarm"]={{"time",AlarmTimeText(tile.alarm.minute)},{"date",tile.alarm.date},
                       {"repeat",tile.alarm.repeat==AlarmRepeat::Weekly?"weekly":tile.alarm.repeat==AlarmRepeat::Daily?"daily":"once"},
                       {"days",tile.alarm.weekdays},{"enabled",tile.alarm.enabled},{"nextDue",tile.alarm.nextDue},{"lastFired",tile.alarm.lastFired}};
    return item;
}
void PlaceCliTile(const std::vector<Tile>& tiles,Tile& tile) {
    auto free=[&](Point p){
        for(const auto& t:tiles)if(TilesOverlap(tile,t,p-tile.position))return false;
        return true;
    };
    RECT work{};SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);
    if(tiles.empty()){tile.position={double(work.left+80),double(work.top+100)};return;}
    for(const auto& reference:tiles)
        for(auto offset:Neighbors(reference.size,tile.size,reference.shape,tile.shape,nullptr,reference.rectangleHeight,tile.rectangleHeight)){
            auto p=reference.position+offset;
            RECT bounds{LONG(p.x),LONG(p.y),LONG(p.x+tile.size),LONG(p.y+TileHeight(tile))};
            if(free(p)&&MonitorFromRect(&bounds,MONITOR_DEFAULTTONULL)){tile.position=p;return;}
        }
    for(int y=work.top+20;y+PixelHeight(tile)<work.bottom;y+=40)
        for(int x=work.left+20;x+PixelWidth(tile)<work.right;x+=40)
            if(free({double(x),double(y)})){tile.position={double(x),double(y)};return;}
    throw std::runtime_error("No free position was found. Move tiles or specify --x and --y.");
}
void ApplyCliTileOptions(Tile& tile,const CliOptions& options) {
    auto find=[&](const char* key)->const std::string*{
        auto it=options.find(key);return it==options.end()?nullptr:&it->second;
    };
    const auto oldKind=tile.kind;
    if(auto value=find("--kind")){
        if(*value=="text")tile.kind=Kind::Text;
        else if(*value=="image")tile.kind=Kind::Image;
        else if(*value=="timer")tile.kind=Kind::Timer;
        else if(*value=="alarm")tile.kind=Kind::Alarm;
        else throw std::runtime_error("Unknown content kind: "+*value);
        if(tile.kind!=oldKind){
            tile.running=false;tile.elapsed=0;tile.notified=false;tile.alarm={};
            if(tile.kind==Kind::Timer){tile.fontSize=22;tile.textColor=0x71EDC4;}
        }
    }
    if(auto value=find("--shape")){
        if(*value=="hexagon")tile.shape=Shape::Hexagon;
        else if(*value=="triangle")tile.shape=Shape::Triangle;
        else if(*value=="rectangle")tile.shape=Shape::Rectangle;
        else throw std::runtime_error("Unknown tile shape: "+*value);
        if(tile.shape!=Shape::Rectangle)tile.rectangleHeight=0;
    }
    if(auto value=find("--text")){
        if(value->size()>65536)throw std::runtime_error("Tile text exceeds 65536 bytes.");
        tile.text=*value;
    }
    if(auto value=find("--image")){
        if(tile.kind!=Kind::Image)throw std::runtime_error("--image requires an image tile.");
        if(value->size()>32768)throw std::runtime_error("Image path is too long.");
        if(!value->empty()){
            auto path=fs::u8path(*value);
            if(!path.is_absolute())throw std::runtime_error("Image path must be absolute.");
            Asset image;image.bytes=ReadBytes(path,ImageLimit);
            image.mime=ImageMime(image.bytes);ValidateAsset(image);
        }
        if(tile.image!=*value){tile.photoFocus={.5,.5};tile.photoZoom=1;}
        tile.image=*value;
    }
    if(auto value=find("--x"))tile.position.x=CliNumber(*value,-1000000,1000000);
    if(auto value=find("--y"))tile.position.y=CliNumber(*value,-1000000,1000000);
    if(auto value=find("--size"))tile.size=CliNumber(*value,80,480);
    if(auto value=find("--layer")){
        if(*value!="top"&&*value!="desktop")throw std::runtime_error("Layer must be top or desktop.");
        tile.topmost=*value=="top";
    }
    auto timer=[&]{if(tile.kind!=Kind::Timer)throw std::runtime_error("Timer options require a timer tile.");};
    if(auto value=find("--seconds")){timer();tile.duration=CliNumber(*value,1,8640000);}
    if(auto value=find("--timer-mode")){
        timer();
        if(*value!="countdown"&&*value!="countup")throw std::runtime_error("Timer mode must be countdown or countup.");
        if(tile.countdown!=(*value=="countdown")){tile.elapsed=0;tile.running=false;tile.notified=false;}
        tile.countdown=*value=="countdown";
    }
    if(tile.kind==Kind::Timer && tile.countdown && tile.elapsed>=tile.duration){
        tile.elapsed=tile.duration;tile.running=false;tile.notified=true;
    }
    if(auto value=find("--running")){
        timer();tile.running=CliBool(*value);
        if(tile.running&&tile.countdown&&tile.elapsed>=tile.duration){tile.elapsed=0;tile.notified=false;}
    }
    bool arm=false;
    auto alarm=[&]{if(tile.kind!=Kind::Alarm)throw std::runtime_error("Alarm options require an alarm tile.");arm=true;};
    if(auto value=find("--alarm-time")){alarm();if(!ParseAlarmTime(*value,tile.alarm.minute))throw std::runtime_error("Use a valid HH:MM alarm time.");}
    if(auto value=find("--alarm-date")){alarm();tile.alarm.date=*value;}
    if(auto value=find("--alarm-days")){alarm();tile.alarm.weekdays=CliInt(*value,1,127);}
    if(auto value=find("--alarm-repeat")){
        alarm();
        if(*value=="once")tile.alarm.repeat=AlarmRepeat::Once;
        else if(*value=="daily")tile.alarm.repeat=AlarmRepeat::Daily;
        else if(*value=="weekly")tile.alarm.repeat=AlarmRepeat::Weekly;
        else throw std::runtime_error("Alarm recurrence must be once, daily or weekly.");
    }
    if(auto value=find("--alarm-enabled")){alarm();tile.alarm.enabled=CliBool(*value);}
    if(arm){
        const auto enabled=tile.alarm.enabled;
        // Preserve the consumed occurrence for a toggle; a changed schedule starts anew.
        if(find("--alarm-time")||find("--alarm-date")||find("--alarm-days")||find("--alarm-repeat"))tile.alarm.lastFired=0;
        if(!ValidAlarm(tile.alarm))throw std::runtime_error("Invalid alarm settings; an enabled one-time alarm requires YYYY-MM-DD.");
        ArmAlarm(tile.alarm,static_cast<std::int64_t>(std::time(nullptr)));
        if(enabled&&!tile.alarm.enabled)throw std::runtime_error("The alarm needs a future occurrence.");
    }
}
Json RunLayoutCommand(const CliArgs& args) {
    if(args.size()<2)throw std::runtime_error("Use layout or tile followed by a command. See --help.");
    const auto& family=args[0];const auto& verb=args[1];
    if(family=="layout"){
        if(verb=="list" && args.size()==2){
            Json entries=Json::array();
            for(const auto& [id,p]:app.catalog.profiles){
                const bool active=id==app.catalog.active;
                const auto& tiles=active?app.tiles:p.tiles;const auto& hidden=active?app.hidden:p.hidden;
                entries.push_back({{"id",id},{"name",p.name},{"active",active},{"tiles",tiles.size()},{"hidden",hidden.size()}});
            }
            return {{"ok",true},{"active",app.catalog.current().name},{"layouts",std::move(entries)}};
        }
        if(!(verb=="create"&&(args.size()==3||(args.size()==4&&args[3]=="--empty"))) &&
           !(verb=="switch"&&args.size()==3) && !(verb=="rename"&&args.size()==4) &&
           !(verb=="delete"&&args.size()==4&&args[3]=="--yes"))
            throw std::runtime_error("Invalid layout command. Deleting a layout requires --yes. See --help.");
        PrepareLayoutEdit();auto next=CurrentCatalog();
        if(verb=="create")CreateProfile(next,args[2],args.size()==4);
        else if(verb=="switch"){
            next.active=ProfileId(next,args[2]);
            if(next.active==app.catalog.active)return {{"ok",true},{"active",next.current().name}};
        } else if(verb=="rename"){
            auto id=ProfileId(next,args[2]);RequireNewProfileName(next,args[3],id);next.profiles.at(id).name=args[3];
        } else {
            auto id=ProfileId(next,args[2]);
            if(id==next.active)throw std::runtime_error("Switch to another layout before deleting the active one.");
            next.profiles.erase(id);
        }
        auto result=Json({{"ok",true},{"active",next.current().name}});
        ApplyCatalog(std::move(next));return result;
    }
    if(family!="tile")throw std::runtime_error("Unknown command group. See --help.");
    const bool edit=verb=="add"||verb=="update";
    const bool read=verb=="list"||verb=="get";
    if(!edit&&!read&&verb!="delete"&&verb!="hide"&&verb!="show")throw std::runtime_error("Unknown tile command. See --help.");
    auto options=edit?ReadCliOptions(args,{"--id","--layout","--shape","--kind","--text","--image","--x","--y","--size","--layer",
        "--timer-mode","--seconds","--running","--alarm-time","--alarm-repeat","--alarm-date","--alarm-days","--alarm-enabled"}):
        verb=="list"?ReadCliOptions(args,{"--layout"}):ReadCliOptions(args,{"--id","--layout"});
    if((verb=="add"&&options.count("--id")) || (verb!="add"&&verb!="list"&&!options.count("--id")))
        throw std::runtime_error("Use --id to target an existing tile. New tiles receive an ID automatically.");
    const int profileId=options.count("--layout")?ProfileId(app.catalog,options.at("--layout")):app.catalog.active;
    const bool active=profileId==app.catalog.active;
    int id=options.count("--id")?CliInt(options.at("--id"),1,TileIdLimit):0;
    if(read){
        const auto& profile=app.catalog.profiles.at(profileId);
        const auto& tiles=active?app.tiles:profile.tiles;
        const auto& hidden=active?app.hidden:profile.hidden;
        if(verb=="list"){
            Json entries=Json::array();for(const auto& t:tiles)entries.push_back(CliTile(hidden,t,active,false));
            return {{"ok",true},{"layout",profile.name},{"tiles",std::move(entries)}};
        }
        auto t=std::find_if(tiles.begin(),tiles.end(),[&](const Tile& candidate){return candidate.id==id;});
        if(t==tiles.end())throw std::runtime_error("Tile ID does not exist in "+profile.name+".");
        return {{"ok",true},{"layout",profile.name},{"tile",CliTile(hidden,*t,active,true)}};
    }
    PrepareLayoutEdit();auto next=CurrentCatalog();auto& p=next.profiles.at(profileId);
    auto tile=std::find_if(p.tiles.begin(),p.tiles.end(),[&](const Tile& t){return t.id==id;});
    if(verb!="add" && tile==p.tiles.end())throw std::runtime_error("Tile ID does not exist in "+p.name+".");
    Json response={{"ok",true},{"layout",p.name}};
    if(verb=="add"){
        if(p.tiles.size()>=500 || p.nextTile>TileIdLimit)throw std::runtime_error("Tile limit reached (500 per layout).");
        Tile t;t.id=id=p.nextTile++;t.text=Utf8(LayoutLabel(L"新文字",L"New text"));
        ApplyCliTileOptions(t,options);
        if(options.count("--x")!=options.count("--y"))throw std::runtime_error("New tiles need both --x and --y, or neither.");
        if(!options.count("--x"))PlaceCliTile(p.tiles,t);
        p.tiles.push_back(std::move(t));tile=std::prev(p.tiles.end());
    } else if(verb=="update"){
        if(options.size()==1+options.count("--layout"))throw std::runtime_error("Specify a tile property to update.");
        ApplyCliTileOptions(*tile,options);
    } else if(verb=="delete"){p.tiles.erase(tile);p.hidden.erase(id);response["deleted"]=id;}
    else if(verb=="hide")p.hidden.insert(id);
    else if(verb=="show")p.hidden.erase(id);
    if(edit){
        ValidateTiles(p.tiles);
        if(verb=="add"||options.count("--x")||options.count("--y")||options.count("--size")||options.count("--shape"))
            for(const auto& other:p.tiles)if(other.id!=id&&TilesOverlap(*tile,other))
                throw std::runtime_error("The requested tile would overlap another tile.");
    }
    if(verb!="delete")response["tile"]=CliTile(p.hidden,*tile,active,true);
    ApplyCatalog(std::move(next),active?id:0);return response;
}
LRESULT ReceiveLayoutCommand(HWND sender,const COPYDATASTRUCT* data) {
    if(!data || data->dwData!=ipc::Request || !data->lpData || !data->cbData ||
       data->cbData>ipc::RequestLimit || !ipc::SameUserWindow(sender,ipc::ClientClass))return FALSE;
    Json result;
    try{
        if(!app.cliReady || app.cliBusy || app.popupActive || !IsWindowEnabled(app.control) || app.exiting)
            throw std::runtime_error("DeskTiles is busy. Close its current dialog or menu and try again.");
        struct Busy {Busy(){app.cliBusy=true;}~Busy(){app.cliBusy=false;}} busy;
        auto input=static_cast<const char*>(data->lpData);
        auto message=Json::parse(input,input+data->cbData,[](int depth,Json::parse_event_t,Json&){
            if(depth>6)throw std::runtime_error("Command nesting is too deep.");return true;
        });
        const auto& args=message.at("args");
        if(message.at("version")!=1 || !args.is_array() || args.size()>48)
            throw std::runtime_error("Unsupported CLI request.");
        CliArgs command;for(const auto& a:args)command.push_back(JsonString(a,65536));
        result=RunLayoutCommand(command);
    }catch(const std::exception& e){result={{"ok",false},{"error",e.what()}};}
    try{
        auto response=result.dump(-1,' ',false,Json::error_handler_t::replace);
        if(response.size()>ipc::ResponseLimit)response=R"({"ok":false,"error":"The response is too large."})";
        return ipc::CopyMessage(sender,app.control,ipc::Response,response,1500,SMTO_BLOCK)?TRUE:FALSE;
    }catch(...){return FALSE;}
}
