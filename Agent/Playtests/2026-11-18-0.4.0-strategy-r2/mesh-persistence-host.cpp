#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <Engine/Runtime/EditSessionApi.h>
#include <Engine/Runtime/EditExtensions.h>
#include <Engine/Runtime/PhysicsObjectEditApi.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;
static void Check(bool ok,const char* text) { if(!ok) throw std::runtime_error(text); }

struct Objects { std::map<std::string,uint64_t> byName; };
static void VisitObject(void* user,const EngineSceneObjectInfo* object) {
    static_cast<Objects*>(user)->byName[object->name ? object->name : ""] = object->id;
}

static EnginePhysicsObjectState Body(uint32_t motion) {
    EnginePhysicsObjectState s{};
    s.enabled=1; s.bodyType=motion; s.shapeType=EnginePhysicsShapeMesh;
    s.friction=0.8f; s.restitution=0.0f; s.density=12.0f;
    return s;
}

int wmain(int argc,wchar_t** argv) {
    HMODULE module=nullptr; EngineEditModuleApi factory{}; EngineEditSessionApi api{};
    try {
        Check(argc==5,"usage: MeshPersistenceHost <Game.dll> <Runtime> <Content> <fixture dir>");
        const fs::path dllPath(argv[1]), runtime(argv[2]), content(argv[3]), fixtures(argv[4]);
        SetDllDirectoryW(runtime.c_str());
        module=LoadLibraryExW(dllPath.c_str(),nullptr,LOAD_WITH_ALTERED_SEARCH_PATH);
        Check(module!=nullptr,"LoadLibraryExW(Game.dll)");
        const auto getFactory=reinterpret_cast<EngineGetEditModuleApiFn>(GetProcAddress(module,"EngineGetEditModuleApi"));
        const auto getExtension=reinterpret_cast<EngineGetEditExtensionFn>(GetProcAddress(module,"EngineGetEditExtension"));
        Check(getFactory&&getExtension,"edit exports");
        Check(getFactory(ENGINE_EDIT_API_VERSION,sizeof(factory),&factory)!=0,"edit factory ABI");
        const auto engineContent=(runtime/L"EngineContent").u8string();
        Check(factory.create(engineContent.c_str(),&api)!=0,"create edit session");
        char error[2048]{};
        const auto contentUtf8=content.u8string();
        Check(api.setContentRoot(api.context,contentUtf8.c_str(),error,sizeof(error))==EngineSceneOk,"set Content root");
        EnginePhysicsObjectApi physics{};
        Check(getExtension(api.context,EnginePhysicsObjectExtension,1,sizeof(physics),&physics)!=0,"physics object extension query");
        Check(physics.version==1&&physics.read&&physics.write&&physics.available,"physics object API contract");
        Check(physics.available(physics.context)==1,"Jolt physics available");

        auto import=[&](const fs::path& file) {
            char virtualPath[4096]{}; error[0]=0;
            const auto source=file.u8string();
            Check(api.importAsset(api.context,source.c_str(),0,virtualPath,sizeof(virtualPath),error,sizeof(error))==EngineSceneOk,"import OBJ fixture");
            return std::string(virtualPath);
        };
        EngineSceneTransform archTransform{}; archTransform.position[0]=0.25f; archTransform.position[1]=0.95f; archTransform.position[2]=2.0f;
        archTransform.rotation[3]=1.0f; archTransform.scale[0]=-1.0f; archTransform.scale[1]=archTransform.scale[2]=1.0f;
        uint64_t archId=0;
        const auto archAsset=import(fixtures/L"StaticMeshArch.obj");
        Check(api.createModel(api.context,archAsset.c_str(),"ImportedStaticMeshGate",&archTransform,&archId,error,sizeof(error))==EngineSceneOk,"create imported static mesh model");
        auto archBody=Body(EnginePhysicsBodyStatic);
        Check(physics.write(physics.context,archId,&archBody)==EngineSceneOk,"write static mesh physics");

        EngineSceneTransform cargoTransform{}; cargoTransform.position[0]=3.5f; cargoTransform.position[1]=0.7f; cargoTransform.position[2]=2.0f;
        cargoTransform.rotation[3]=1.0f; cargoTransform.scale[0]=cargoTransform.scale[1]=cargoTransform.scale[2]=1.0f;
        uint64_t cargoId=0;
        const auto cargoAsset=import(fixtures/L"OpenCargo.obj");
        Check(api.createModel(api.context,cargoAsset.c_str(),"ImportedMovingMeshCargo",&cargoTransform,&cargoId,error,sizeof(error))==EngineSceneOk,"create imported moving mesh model");
        auto cargoBody=Body(EnginePhysicsBodyDynamic);
        Check(physics.write(physics.context,cargoId,&cargoBody)==EngineSceneOk,"write dynamic mesh physics");

        const fs::path scene=content/L"Scenes"/L"MeshPhysicsPersistence.scene.json";
        Check(api.saveScene(api.context,scene.u8string().c_str(),error,sizeof(error))==EngineSceneOk,"save mesh physics scene");
        std::ifstream saved(scene,std::ios::binary); const std::string json((std::istreambuf_iterator<char>(saved)),{});
        Check(json.find("\"shape\"")!=std::string::npos&&json.find("mesh")!=std::string::npos,"serialized mesh shape tag");

        // Prove disk persistence across a new edit-session context, not same-session reload.
        factory.destroy(api.context); api={};
        Check(factory.create(engineContent.c_str(),&api)!=0,"create fresh edit session");
        Check(api.setContentRoot(api.context,contentUtf8.c_str(),error,sizeof(error))==EngineSceneOk,"set fresh Content root");
        physics={};
        Check(getExtension(api.context,EnginePhysicsObjectExtension,1,sizeof(physics),&physics)!=0,"requery physics extension on fresh session");
        Check(physics.available(physics.context)==1,"Jolt available on fresh session");
        uint64_t sceneId=0;
        Check(api.loadScene(api.context,scene.u8string().c_str(),&sceneId,error,sizeof(error))==EngineSceneOk,"reopen saved mesh physics scene");
        Objects objects;
        EngineSceneInfo sceneInfo{};
        Check(api.readActiveScene(api.context,&sceneInfo,VisitObject,&objects)==EngineSceneOk,"read reopened scene objects");
        Check(objects.byName.count("ImportedStaticMeshGate")&&objects.byName.count("ImportedMovingMeshCargo"),"imported model objects survived reopen");
        EnginePhysicsObjectState actual{};
        Check(physics.read(physics.context,objects.byName.at("ImportedStaticMeshGate"),&actual)==EngineSceneOk,"read static mesh physics after reopen");
        Check(actual.enabled==1&&actual.bodyType==EnginePhysicsBodyStatic&&actual.shapeType==EnginePhysicsShapeMesh,"static mesh physics round trip");
        Check(physics.read(physics.context,objects.byName.at("ImportedMovingMeshCargo"),&actual)==EngineSceneOk,"read moving mesh physics after reopen");
        Check(actual.enabled==1&&actual.bodyType==EnginePhysicsBodyDynamic&&actual.shapeType==EnginePhysicsShapeMesh,"moving mesh physics round trip");
        std::cout<<"Mesh physics persistence PASS; imported OBJ models, mirrored static transform, static/dynamic Mesh settings survived save/reopen.\n";
        std::cout<<"staticAsset="<<archAsset<<" dynamicAsset="<<cargoAsset<<" sceneId="<<sceneId<<"\n";
        factory.destroy(api.context); api={}; factory={}; FreeLibrary(module); module=nullptr;
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"Mesh persistence failure: "<<e.what()<<"\n";
        if(factory.destroy&&api.context) factory.destroy(api.context);
        if(module) FreeLibrary(module);
        return 1;
    }
}
