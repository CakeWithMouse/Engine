#include <Engine/Runtime/SceneEditSession.h>
#include <Engine/Components/EditorProperties.h>
#include <Engine/Scene/SceneDocument.h>
#include <Engine/Scene/SceneAssets.h>
#include "../Scene/AssetUpdate.h"
#include "../../ThirdParty/nlohmann/json.hpp"
#include <fstream>
#include <cstdio>
#include <cmath>
using Json=nlohmann::json;
namespace {
Json Schema(GameComponent& object) {
    auto* provider=dynamic_cast<EditorPropertyProvider*>(&object);
    if(!provider)return Json::object();
    const auto text=provider->DescribeEditorProperties();if(text.empty())return Json::object();
    const auto schema=Json::parse(text);
    if(schema.at("version")!=1 || !schema.at("fields").is_array() || schema.at("fields").size()>256)throw std::runtime_error("Invalid editor property schema");
    return schema;
}
void RemapJson(Json& value,const std::string& before,const std::string& after) {
    if(value.is_string())value=AssetUpdate::Remap(value.get<std::string>(),before,after);
    else if(value.is_structured())for(auto& child:value)RemapJson(child,before,after);
}
template<class F> uint32_t Report(char* error,uint32_t size,F&& f) {
    if(size&&!error)return EngineSceneInvalidArgument;if(size)error[0]=0;
    try {f();return EngineSceneOk;}catch(const std::exception& e){if(size)std::snprintf(error,size,"%s",e.what());return EngineSceneFailure;}
}
}
EngineAuthoringApi SceneEditSession::GetAuthoringApi() noexcept {
    return {1,this,
        [](void* p,uint64_t id,EngineProjectDataVisitor visitor,void* user)->uint32_t {
            if(!visitor)return EngineSceneInvalidArgument;
            return Invoke(p,[&](SceneEditSession& s)->uint32_t {auto* object=s.Active().GetScene().FindComponent(id);if(!object)return EngineSceneNotFound;
                const auto schema=Schema(*object).dump();visitor(user,schema.c_str());return EngineSceneOk;});
        },
        [](void* p,uint64_t id,const char* pointer,const char* text,char* error,uint32_t size)->uint32_t {
            if(!pointer||!text)return EngineSceneInvalidArgument;
            return Invoke(p,[&](SceneEditSession& s)->uint32_t {
                if(s.state_!=EngineEditing)return EngineSceneReadOnly;
                return Report(error,size,[&] {
                    auto* object=s.edit_->GetScene().FindComponent(id);if(!object)throw std::runtime_error("Object not found");
                    auto schema=Schema(*object);Json field;
                    for(const auto& item:schema.at("fields"))if(item.at("path")==pointer){field=item;break;}
                    if(field.is_null()||field.value("readOnly",false)||std::string(pointer)=="/version")throw std::runtime_error("Property is not editable");
                    const auto value=Json::parse(text);const auto type=field.at("type").get<std::string>();
                    if(type=="number"||type=="integer") {
                        if(!value.is_number() || (type=="integer"&&!value.is_number_integer()))throw std::runtime_error("Expected numeric property");
                        const auto number=value.get<double>();if(!std::isfinite(number)||number<field.value("min",-1e12)||number>field.value("max",1e12))throw std::runtime_error("Property outside allowed range");
                    } else if(type=="color") {
                        if(!value.is_array()||value.size()!=3)throw std::runtime_error("Expected RGB color");
                        for(const auto& channel:value)if(!channel.is_number()||!std::isfinite(channel.get<double>())||channel.get<double>()<0||channel.get<double>()>1)
                            throw std::runtime_error("Color channel outside [0,1]");
                    } else if(type=="bool"){if(!value.is_boolean())throw std::runtime_error("Expected boolean property");}
                    else if(type=="enum") {bool found=false;for(const auto& option:field.at("options"))if(option.at("value")==value)found=true;if(!found)throw std::runtime_error("Unknown enum value");}
                    else if(type=="object") {
                        if(!value.is_number_unsigned())throw std::runtime_error("Expected object ID");
                        const auto target=value.get<uint64_t>();auto* other=s.edit_->GetScene().FindComponent(target);
                        if(target&&!other)throw std::runtime_error("Referenced object no longer exists");
                        const auto required=field.value("objectType",std::string{});
                        if(target&&!required.empty()) {bool match=false;for(const auto& row:s.edit_->GetScene().GetSnapshot().components)if(row.id==target&&row.typeName==required)match=true;if(!match)throw std::runtime_error("Incorrect referenced object type");}
                    } else if(type=="asset"||type=="string") {
                        if(!value.is_string()||value.get_ref<const std::string&>().size()>4096)throw std::runtime_error("Invalid string property");
                        if(type=="asset"&&!value.get<std::string>().empty()) {
                            const auto path=value.get<std::string>();if(!SceneAssets::ValidPath(path))throw std::runtime_error("Resource must be a /Game path");
                            const auto file=s.contentRoot_/std::filesystem::u8path(path.substr(6));
                            const auto relative=std::filesystem::weakly_canonical(file).lexically_relative(std::filesystem::canonical(s.contentRoot_));
                            if(relative.empty()||relative.is_absolute()||*relative.begin()=="..")throw std::runtime_error("Resource resolves outside Content");
                            if(!std::filesystem::is_regular_file(file))throw std::runtime_error("Resource does not exist");
                            if(field.contains("extensions")){bool match=false;for(const auto& extension:field["extensions"])if(extension==file.extension().u8string())match=true;if(!match)throw std::runtime_error("Incorrect resource extension");}
                        }
                    } else throw std::runtime_error("Unsupported property type");
                    const auto before=object->SaveProjectData();auto data=Json::parse(before);
                    const auto key=Json::json_pointer(pointer);if(!data.contains(key))throw std::runtime_error("Property is absent from saved data");
                    data[key]=value;
                    try {object->LoadProjectData(data.dump());}catch(...){object->LoadProjectData(before);throw;}
                });
            });
        },
        [](void* p,uint32_t operation,const char* path,const char* argument,char* error,uint32_t size)->uint32_t {
            if(!path||!argument)return EngineSceneInvalidArgument;
            return Invoke(p,[&](SceneEditSession& s)->uint32_t {
                if(s.state_!=EngineEditing)return EngineSceneReadOnly;
                if(!s.previews_.empty()||!s.modifiedMaterials_.empty())return EngineSceneBusy;
                return Report(error,size,[&] {
                    const auto snapshot=s.edit_->GetScene().GetSnapshot();
                    AssetUpdate::Run(s.contentRoot_,operation,path,argument,[&](const auto& before,const auto& after) {
                        auto document=Json::parse(SceneDocument::Serialize(snapshot));RemapJson(document,before,after);
                        for(const auto& entry:std::filesystem::recursive_directory_iterator(s.contentRoot_))if(entry.is_regular_file()) {
                            const auto name=entry.path().filename().u8string();
                            if(name.size()>=11&&name.substr(name.size()-11)==".scene.json") {auto test=std::make_unique<Game>();SceneDocument::Restore(SceneDocument::Read(entry.path()),s.catalog_,test->GetScene());}
                        }
                        s.ReplaceEdit(SceneDocument::Parse(document.dump()));
                    });
                });
            });
        }};
}
