#define NOMINMAX
#include <Windows.h>
#include <Engine/Runtime/GameEditModule.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <cmath>

namespace {
int checks = 0;
void Check(bool ok, const char* text) { if (!ok) throw std::runtime_error(text); ++checks; }
struct Object { uint64_t id; EngineSceneTransform transform; std::string type; };
std::map<std::string,Object> Read(const EngineEditSessionApi& api) {
    std::map<std::string,Object> objects; EngineSceneInfo info{};
    Check(api.readActiveScene(api.context,&info,[](void* p,const EngineSceneObjectInfo* o) {
        (*static_cast<std::map<std::string,Object>*>(p))[o->name]={o->id,o->transform,o->typeName};
    },&objects)==EngineSceneOk,"read scene"); return objects;
}
}
int wmain(int argc,wchar_t** argv) {
    try {
        Check(argc==3,"usage: EditorReloadTests Runtime scratch");
        const auto runtime=std::filesystem::absolute(argv[1]), scratch=std::filesystem::absolute(argv[2]);
        std::filesystem::create_directories(scratch);
        SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS); AddDllDirectory(runtime.c_str());
        const auto original=runtime/"Templates/SceneProject/Game.dll", candidate=scratch/"Candidate.dll";
        std::filesystem::copy_file(original,candidate,std::filesystem::copy_options::overwrite_existing);
        HWND window=CreateWindowExW(0,L"STATIC",L"Reload test",WS_OVERLAPPEDWINDOW,0,0,320,240,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        Check(window!=nullptr,"test viewport");
        struct Window { HWND value; ~Window(){DestroyWindow(value);} } guard{window};
        GameEditModule module(original,runtime/"EngineContent",scratch);
        auto api=module.Api(); char error[2048]{}; uint64_t hero=0,settings=0;
        EngineSceneTransform transform{}; transform.rotation[3]=1; for(auto& v:transform.scale)v=1;
        transform.position[0]=2.5f;
        Check(api.scene.createObject(api.scene.context,"Gameplay.Character","Hero",&transform,&hero)==EngineSceneOk,"create hero");
        Check(api.scene.createObject(api.scene.context,"Gameplay.PlaySettings","Play",&transform,&settings)==EngineSceneOk,"create play settings");
        const auto data=module.ProjectDataApi();
        Check(data.write(data.context,hero,"{\"version\":1,\"moveSpeed\":9}")==EngineSceneOk,"custom speed");
        const auto play="{\"version\":1,\"character\":"+std::to_string(hero)+",\"camera\":0,\"standardControl\":true,\"cameraDistance\":5,\"cameraHeight\":2,\"lookSensitivity\":0.005}";
        Check(data.write(data.context,settings,play.c_str())==EngineSceneOk,"assign Play");
        Check(api.attachViewport(api.context,window,320,240)==EngineSceneOk,"attach old viewport");
        Check(api.camera(api.context,1,2,3,.2f,.1f,0)==EngineSceneOk,"move editor camera");
        EngineEditorCameraState before{}; Check(module.EditorCameraApi().read(module.EditorCameraApi().context,&before)==EngineSceneOk,"read camera before reload");
        const auto checkpoint=scratch/"Unsaved.scene.json";
        Check(api.saveScene(api.context,checkpoint.u8string().c_str(),error,sizeof(error))==EngineSceneOk,"checkpoint unsaved edits");
        const auto previousContext=api.context;
        Check(module.Reload(scratch/"Missing.dll",checkpoint,window,320,240,error,sizeof(error))!=EngineSceneOk,"failed candidate rejected");
        Check(module.Api().context==previousContext && module.Api().frame(previousContext,0)==EngineSceneOk,"failure keeps live scene renderable");
        const auto invalid=scratch/"Invalid.scene.json"; std::ofstream(invalid)<<"{}";
        Check(module.Reload(candidate,invalid,window,320,240,error,sizeof(error))!=EngineSceneOk,"invalid live scene rejected");
        Check(module.Api().context==previousContext,"invalid checkpoint does not commit");
        auto result=module.Reload(candidate,checkpoint,window,320,240,error,sizeof(error));
        if(result!=EngineSceneOk) std::cerr<<error<<'\n';
        Check(result==EngineSceneOk,"reload commits candidate with same HWND"); api=module.Api();
        Check(api.context!=previousContext,"session replaced without closing window");
        Check(module.BatchApi().context==api.context && module.BatchApi().select,
            "group extension follows the new session after reload");
        Check(module.BatchApi().select(module.BatchApi().context,nullptr,0)==EngineSceneOk,
            "new group extension remains callable after unloading the old DLL");
        EngineEditorCameraState after{}; Check(module.EditorCameraApi().read(module.EditorCameraApi().context,&after)==EngineSceneOk,"read camera after reload");
        Check(std::abs(before.x-after.x)<.0001f && std::abs(before.y-after.y)<.0001f && std::abs(before.z-after.z)<.0001f && std::abs(before.yaw-after.yaw)<.0001f && std::abs(before.pitch-after.pitch)<.0001f,"editor camera restored across DLL replacement");
        Check(module.Reload(candidate,checkpoint,window,320,240,error,sizeof(error))==EngineSceneOk && module.Api().context==api.context,"unchanged build keeps session and mounts");
        auto objects=Read(api);
        Check(module.AuthoringApi().context==api.context && module.AuthoringApi().describe,
            "authoring extension follows reloaded session");
        std::string schema;
        Check(module.AuthoringApi().describe(api.context,objects.at("Hero").id,[](void* p,const char* value){*static_cast<std::string*>(p)=value;},&schema)==EngineSceneOk,
            "authoring callable after unloading old DLL");
        Check(objects.size()==2 && objects.at("Hero").type=="Gameplay.Character" && objects.at("Hero").transform.position[0]==2.5f,"live transform and types restored");
        std::string json;const auto newData=module.ProjectDataApi();
        Check(newData.read(newData.context,objects.at("Hero").id,[](void* p,const char* value){*static_cast<std::string*>(p)=value;},&json)==EngineSceneOk && json.find("9")!=std::string::npos,"character settings restored");
        Check(api.control(api.context,EnginePlay)==EngineSceneOk,"Play after reload");
        Check(module.Reload(original,checkpoint,window,320,240,error,sizeof(error))==EngineSceneReadOnly,"reload rejected in Play");
        const auto input=module.ViewportInputApi();input.focus(input.context,1);input.key(input.context,87,1);
        Check(api.frame(api.context,.1)==EngineSceneOk,"input after reload");
        Check(Read(api).at("Hero").transform.position[2]!=0,"Play assignment remapped");
        Check(api.control(api.context,EngineStop)==EngineSceneOk,"stop after reload");
        Check(Read(api).at("Hero").transform.position[2]==0,"edit scene preserved");
        Check(api.control(api.context,EnginePlay)==EngineSceneOk && api.frame(api.context,.1)==EngineSceneOk,"repeat Play after reload");
        Check(Read(api).at("Hero").transform.position[2]==0,"held keys do not survive Stop");
        Check(api.control(api.context,EngineStop)==EngineSceneOk,"final Stop");
        Check(module.Reload(original,checkpoint,window,320,240,error,sizeof(error))==EngineSceneOk,"second reload");
        std::cout<<"Editor reload: "<<checks<<" checks passed\n";return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
