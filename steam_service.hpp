#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <stdexcept>

#ifdef DESKTILES_STEAM_SDK
#include <steam/steam_api.h>
#endif
namespace desktile_steam {
class Service {
#ifdef DESKTILES_STEAM_SDK
    CCallResult<Service,CreateItemResult_t> created;
    CCallResult<Service,SubmitItemUpdateResult_t> submitted;
    std::string contentPath,previewPath,title;
    bool active=false,publishing=false;
    PublishedFileId_t publishingId=0;
    void OnCreated(CreateItemResult_t* result,bool io) {
        if(io||result->m_eResult!=k_EResultOK){publishing=false;status="Workshop creation failed.";return;}
        publishingId=result->m_nPublishedFileId;
        const auto handle=SteamUGC()->StartItemUpdate(SteamUtils()->GetAppID(),publishingId);
        const bool configured=SteamUGC()->SetItemTitle(handle,title.c_str())&&
            SteamUGC()->SetItemContent(handle,contentPath.c_str())&&
            SteamUGC()->SetItemPreview(handle,previewPath.c_str())&&
            SteamUGC()->SetItemVisibility(handle,k_ERemoteStoragePublishedFileVisibilityPrivate);
        if(!configured){publishing=false;status="Workshop metadata could not be set.";return;}
        const auto call=SteamUGC()->SubmitItemUpdate(handle,"DeskTiles layout preset");
        if(call==k_uAPICallInvalid){publishing=false;status="Workshop upload could not start.";return;}
        submitted.Set(call,this,&Service::OnSubmitted);
        status=result->m_bUserNeedsToAcceptWorkshopLegalAgreement?
            "Accept the Steam Workshop legal agreement on the item's Steam page.":"Uploading Workshop item.";
    }
    void OnSubmitted(SubmitItemUpdateResult_t* result,bool io) {
        publishing=false;
        status=io||result->m_eResult!=k_EResultOK?"Workshop upload failed.":
            "Workshop upload complete (private). Review the item on Steam before making it public.";
    }
#endif
public:
    std::string status="Steamworks App ID / SDK are not configured. Local sharing and sync-folder tests are available.";
    void Start() {
#ifdef DESKTILES_STEAM_SDK
        active=SteamAPI_Init();
        status=active?"Steam connected.":"Steam is unavailable. Start this build with your registered App ID.";
#endif
    }
    bool Available() const {
#ifdef DESKTILES_STEAM_SDK
        return active;
#else
        return false;
#endif
    }
    void Poll() {
#ifdef DESKTILES_STEAM_SDK
        if(active)SteamAPI_RunCallbacks();
#endif
    }
    void Stop() {
#ifdef DESKTILES_STEAM_SDK
        created.Cancel();submitted.Cancel();if(active)SteamAPI_Shutdown();active=false;
#endif
    }
    std::string ReadCloud() {
#ifdef DESKTILES_STEAM_SDK
        if(!active||!SteamRemoteStorage()->IsCloudEnabledForAccount()||!SteamRemoteStorage()->IsCloudEnabledForApp())
            throw std::runtime_error("Steam Cloud is unavailable or disabled.");
        constexpr auto name="desktiles-sync.json";
        if(!SteamRemoteStorage()->FileExists(name))return {};
        const int size=SteamRemoteStorage()->GetFileSize(name);
        if(size<0||size>64*1024*1024)throw std::runtime_error("Cloud package exceeds 64 MiB.");
        std::string bytes(size,'\0');
        if(SteamRemoteStorage()->FileRead(name,bytes.data(),size)!=size)throw std::runtime_error("Cloud read failed.");
        return bytes;
#else
        throw std::runtime_error(status);
#endif
    }
    void WriteCloud(const std::string& bytes) {
#ifdef DESKTILES_STEAM_SDK
        if(!active||!SteamRemoteStorage()->IsCloudEnabledForAccount()||!SteamRemoteStorage()->IsCloudEnabledForApp())
            throw std::runtime_error("Steam Cloud is unavailable or disabled.");
        if(bytes.size()>64*1024*1024)throw std::runtime_error("Cloud package exceeds 64 MiB.");
        if(!SteamRemoteStorage()->FileWrite("desktiles-sync.json",bytes.data(),static_cast<int32>(bytes.size())))
            throw std::runtime_error("Cloud upload failed.");
#else
        (void)bytes;throw std::runtime_error(status);
#endif
    }
    void Publish(const std::filesystem::path& directory,const std::filesystem::path& preview,const std::string& name) {
#ifdef DESKTILES_STEAM_SDK
        if(!active)throw std::runtime_error(status);
        if(publishing)throw std::runtime_error("A Workshop upload is already in progress.");
        contentPath=directory.u8string();previewPath=preview.u8string();title=name;publishing=true;
        const auto call=SteamUGC()->CreateItem(SteamUtils()->GetAppID(),k_EWorkshopFileTypeCommunity);
        if(call==k_uAPICallInvalid){publishing=false;throw std::runtime_error("Could not create a Workshop item.");}
        created.Set(call,this,&Service::OnCreated);status="Creating private Workshop item.";
#else
        (void)directory;(void)preview;(void)name;throw std::runtime_error(status);
#endif
    }
    std::vector<std::filesystem::path> InstalledItems() {
        std::vector<std::filesystem::path> paths;
#ifdef DESKTILES_STEAM_SDK
        if(!active)throw std::runtime_error(status);
        std::vector<PublishedFileId_t> ids(SteamUGC()->GetNumSubscribedItems());
        const auto count=SteamUGC()->GetSubscribedItems(ids.data(),static_cast<uint32>(ids.size()));
        for(uint32 i=0;i<count;++i){
            if(!(SteamUGC()->GetItemState(ids[i])&k_EItemStateInstalled)){SteamUGC()->DownloadItem(ids[i],false);continue;}
            char folder[32768]{};uint64 bytes=0;uint32 stamp=0;
            if(SteamUGC()->GetItemInstallInfo(ids[i],&bytes,folder,sizeof(folder),&stamp))
                paths.push_back(std::filesystem::u8path(folder)/"layout.json");
        }
#endif
        return paths;
    }
    unsigned AppId() const {
#ifdef DESKTILES_STEAM_SDK
        return active?SteamUtils()->GetAppID():0;
#else
        return 0;
#endif
    }
};
} // namespace desktile_steam
