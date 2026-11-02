#define NOMINMAX
#include "AssetUpdate.h"
#include <Engine/Scene/SceneAssets.h>
#include "../../ThirdParty/nlohmann/json.hpp"
#include <Windows.h>
#include <fstream>
#include <vector>
#include <algorithm>
#include <cctype>
namespace fs=std::filesystem;
using Json=nlohmann::json;
namespace {
std::string Extension(const fs::path& path) {auto value=path.extension().u8string();std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c){return char(std::tolower(c));});return value;}
bool Model(const fs::path& path) {const auto e=Extension(path);return e==".fbx"||e==".obj"||e==".gltf"||e==".glb";}
bool Inside(const fs::path& child,const fs::path& root) {
    auto relative=fs::weakly_canonical(child).lexically_relative(fs::canonical(root));
    return !relative.empty()&&!relative.is_absolute()&&*relative.begin()!="..";
}
void Copy(const fs::path& source,const fs::path& target) {
    if(GetFileAttributesW(source.c_str())&FILE_ATTRIBUTE_REPARSE_POINT)throw std::runtime_error("Linked assets cannot be updated");
    if(fs::is_directory(source)) {
        fs::create_directories(target);
        for(const auto& entry:fs::directory_iterator(source))Copy(entry.path(),target/entry.path().filename());
    } else {fs::create_directories(target.parent_path());fs::copy_file(source,target,fs::copy_options::overwrite_existing);}
}
void Write(const fs::path& path,const Json& json){std::ofstream f(path,std::ios::binary);f.exceptions(std::ios::failbit|std::ios::badbit);f<<json.dump(2)<<'\n';f.close();}
void RemapJson(Json& json,const std::string& before,const std::string& after) {
    if(json.is_string())json=AssetUpdate::Remap(json.get<std::string>(),before,after);
    else if(json.is_structured())for(auto& item:json)RemapJson(item,before,after);
}
}
std::string AssetUpdate::Remap(const std::string& value,const std::string& before,const std::string& after) {
    if(before.empty()||before==after)return value;
    if(value==before || (value.size()>before.size() && value.compare(0,before.size(),before)==0 && value[before.size()]=='/'))return after+value.substr(before.size());
    return value;
}
void AssetUpdate::Run(const fs::path& content,unsigned operation,const std::string& path,const std::string& argument,const Commit& commit) {
    if(operation>1 || path.empty() || !SceneAssets::ValidPath(path))throw std::runtime_error("Invalid asset operation/path");
    const auto file=content/fs::u8path(path.substr(6));
    if(!Inside(file,content)||!fs::is_regular_file(file))throw std::runtime_error("Asset must be an existing file inside Content");
    const auto ext=Extension(file);
    const bool model=Model(file);
    const bool texture=ext==".png"||ext==".jpg"||ext==".jpeg"||ext==".bmp"||ext==".tga";
    if(!model&&!texture&&!(operation==1&&ext==".mat"))throw std::runtime_error("Select a model, texture or material");
    const auto sourceRoot=model?file.parent_path():file;
    if(operation==1&&!model)for(auto folder=file.parent_path();folder!=content&&Inside(folder,content);folder=folder.parent_path())
        for(const auto& entry:fs::directory_iterator(folder))if(entry.is_regular_file()&&Model(entry.path()))throw std::runtime_error("This file belongs to a model bundle; move the model folder to keep its internal references");
    if(model && (sourceRoot==content || sourceRoot==content/"Models"))throw std::runtime_error("Model updates require their own imported folder");
    const auto work=content.parent_path()/".painengine";fs::create_directories(work);
    HANDLE lock=CreateFileW((work/"AssetUpdate.lock").c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(lock==INVALID_HANDLE_VALUE)throw std::runtime_error("Another asset update is in progress");
    struct Guard{HANDLE h;~Guard(){CloseHandle(h);}} guard{lock};
    const auto backup=work/"AssetBackups"/(std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
    fs::create_directories(backup);
    auto target=file,targetRoot=sourceRoot;std::string before,after;fs::path imported,importRoot;
    std::vector<std::pair<fs::path,fs::path>> changed;
    bool moved=false,installed=false;
    try {
        if(operation==0) {
            std::string source=argument;
            if(source.empty()) {
                std::ifstream metadata(fs::u8path(file.u8string()+".source.json"));
                if(!metadata)throw std::runtime_error("Source is unknown; choose the original file for reimport");
                source=Json::parse(metadata).at("source").get<std::string>();
            }
            const auto original=fs::u8path(source);
            if(!original.is_absolute() || !fs::is_regular_file(original))throw std::runtime_error("Original source is missing; choose its new location");
            if(Inside(original,sourceRoot.parent_path()) && (original==file || (model&&Inside(original,sourceRoot))))throw std::runtime_error("Choose the external original, not a file inside this import");
            const auto virtualImport=SceneAssets::Import(content,original,model?SceneAssets::Model:SceneAssets::Texture);
            imported=content/fs::u8path(virtualImport.substr(6));importRoot=model?imported.parent_path():imported;
            if(Extension(imported)!=ext)throw std::runtime_error("Reimport must keep the asset format (BLEND is converted to GLB)");
            const auto candidate=backup/"candidate";
            Copy(sourceRoot,candidate);
            if(model) {
                for(const auto& entry:fs::recursive_directory_iterator(importRoot))if(entry.is_regular_file()) {
                    auto relative=entry.path().lexically_relative(importRoot);
                    if(entry.path()==imported)relative=file.filename();
                    if(entry.path()==fs::u8path(imported.u8string()+".source.json"))relative=fs::u8path(file.filename().u8string()+".source.json");
                    if(entry.path()==fs::u8path(imported.u8string()+".import.json"))relative=fs::u8path(file.filename().u8string()+".import.json");
                    const auto dest=candidate/relative;
                    if(fs::exists(dest)&&(entry.path().extension()==".mat"||entry.path().extension()==".hlsl"||entry.path().u8string().find(".import.json")!=std::string::npos))continue;
                    Copy(entry.path(),dest);
                }
            } else Copy(imported,candidate);
            fs::rename(sourceRoot,backup/"original");moved=true;
            fs::rename(candidate,sourceRoot);installed=true;
            if(!model) {
                const auto sidecar=fs::u8path(file.u8string()+".source.json");
                if(fs::exists(sidecar)){Copy(sidecar,backup/"source.json");changed.push_back({sidecar,backup/"source.json"});}
                else changed.push_back({sidecar,{}});
                Copy(fs::u8path(imported.u8string()+".source.json"),sidecar);
            }
        } else {
            if(argument.empty()||!SceneAssets::ValidPath(argument))throw std::runtime_error("Destination must be a /Game path");
            target=content/fs::u8path(argument.substr(6));targetRoot=model?target.parent_path():target;
            if(!Inside(target,content)||Extension(target)!=ext||fs::exists(targetRoot))throw std::runtime_error("Choose a new destination inside Content with the same extension");
            if(model && target.filename()!=file.filename())throw std::runtime_error("Rename/move the model's folder; keep the model filename");
            if(!model)for(const auto* suffix:{".source.json",".import.json"})if(fs::exists(fs::u8path(target.u8string()+suffix)))throw std::runtime_error("Destination metadata already exists");
            if(model && Inside(targetRoot,sourceRoot))throw std::runtime_error("Destination cannot be inside the moved folder");
            before=model?path.substr(0,path.find_last_of('/')):path;
            after=model?argument.substr(0,argument.find_last_of('/')):argument;
            Copy(sourceRoot,backup/"original");
            fs::create_directories(targetRoot.parent_path());fs::rename(sourceRoot,targetRoot);moved=true;installed=true;
            if(!model) for(const auto* suffix:{".source.json",".import.json"}) {
                const auto old=fs::u8path(file.u8string()+suffix),next=fs::u8path(target.u8string()+suffix);
                if(fs::exists(old)){Copy(old,backup/suffix);changed.push_back({old,backup/suffix});changed.push_back({next,{}});Copy(old,next);fs::remove(old);}
            }
            // Rewrite portable scene references (including explicit projectData
            // asset paths) and resolve material-relative links against old locations.
            std::vector<fs::path> documents;
            for(const auto& e:fs::recursive_directory_iterator(content))if(e.is_regular_file()&&(e.path().extension()==".mat" || e.path().filename().u8string().find(".scene.json")!=std::string::npos))documents.push_back(e.path());
            for(const auto& doc:documents) {
                auto json=Json::parse(std::ifstream(doc));const auto oldJson=json;
                if(doc.extension()==".mat") {
                    const auto virtualDoc="/Game/"+doc.lexically_relative(content).generic_u8string();
                    const auto oldDoc=content/fs::u8path(Remap(virtualDoc,after,before).substr(6));
                    auto link=[&](Json& v){if(!v.is_string()||v.get<std::string>().empty())return;
                        auto value=v.get<std::string>();if(value.rfind("/Game/",0)==0){v=Remap(value,before,after);return;}
                        auto absolute=(oldDoc.parent_path()/fs::u8path(value)).lexically_normal();if(!Inside(absolute,content))return;
                        const auto resource=Remap("/Game/"+absolute.lexically_relative(content).generic_u8string(),before,after);
                        v=(content/fs::u8path(resource.substr(6))).lexically_relative(doc.parent_path()).generic_u8string();};
                    if(json.contains("shader"))link(json["shader"]);if(json.contains("textures"))for(auto& v:json["textures"])link(v);
                } else RemapJson(json,before,after);
                if(json!=oldJson){auto save=backup/("document-"+std::to_string(changed.size())+".json");Copy(doc,save);changed.push_back({doc,save});Write(doc,json);}
            }
        }
        Write(backup/"Operation.json",{{"operation",operation},{"asset",path},{"argument",argument},{"before",before},{"after",after}});
        commit(before,after); // Last fallible step. The live scene swaps only after full validation.
    } catch(...) {
        for(auto i=changed.rbegin();i!=changed.rend();++i){if(i->second.empty()){std::error_code ec;fs::remove(i->first,ec);}else Copy(i->second,i->first);}
        if(moved) {
            if(operation==0){if(installed)fs::remove_all(sourceRoot);fs::rename(backup/"original",sourceRoot);}
            else if(installed)fs::rename(targetRoot,sourceRoot);
        }
        if(!importRoot.empty()){std::error_code ec;fs::remove_all(importRoot,ec);fs::remove(fs::u8path(imported.u8string()+".source.json"),ec);}
        throw;
    }
    if(!importRoot.empty()){std::error_code ec;fs::remove_all(importRoot,ec);fs::remove(fs::u8path(imported.u8string()+".source.json"),ec);}
}
