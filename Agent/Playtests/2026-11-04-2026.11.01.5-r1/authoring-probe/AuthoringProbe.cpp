// Supplementary authoring experience on a COPY of each persona project.
// Uses released public C APIs; never modifies the engine or the original project.
#define NOMINMAX
#include <Windows.h>
#include <Engine/Runtime/EditSessionApi.h>
#include <Engine/Runtime/EditExtensions.h>
#include <Engine/Runtime/SceneBatchApi.h>
#include <Engine/Runtime/AuthoringApi.h>
#include <Engine/Runtime/ProjectDataApi.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include <string>
namespace fs=std::filesystem;
using Json=nlohmann::json;
static int failures=0;
static char errorText[4096]{};
static void Record(const char* feature,const char* action,uint32_t result,bool expected=true){
    std::cout << Json{{"feature",feature},{"action",action},{"status",expected?"passed":"failed"},{"returnCode",result},{"detail",errorText}}.dump()<<std::endl;
    if(!expected) ++failures; errorText[0]=0;
}
static void Put(const fs::path& path,const std::string& value){std::ofstream f(path,std::ios::binary);f<<value;if(!f)throw std::runtime_error("write failed");}
static Json ReadJson(const fs::path& p){std::ifstream f(p);return Json::parse(f);}
int wmain(int argc,wchar_t** argv){
    if(argc!=4){std::cerr<<"AuthoringProbe <project copy> <runtime> <repo>\n";return 2;}
    try{
        const fs::path root=fs::absolute(argv[1]), runtime=fs::absolute(argv[2]), repo=fs::absolute(argv[3]);
        const auto project=ReadJson(root/"Project.json");
        const auto content=root/"Content";
        SetDllDirectoryW(runtime.c_str());
        auto library=LoadLibraryW((root/fs::u8path(project.at("module").get<std::string>())).c_str());
        if(!library) throw std::runtime_error("LoadLibrary failed: "+std::to_string(GetLastError()));
        auto get=reinterpret_cast<EngineGetEditModuleApiFn>(GetProcAddress(library,"EngineGetEditModuleApi"));
        auto ext=reinterpret_cast<EngineGetEditExtensionFn>(GetProcAddress(library,"EngineGetEditExtension"));
        EngineEditModuleApi factory{};EngineEditSessionApi api{};
        if(!get||!get(6,sizeof(factory),&factory)||!factory.create((runtime/"EngineContent").u8string().c_str(),&api))throw std::runtime_error("edit factory failed");
        auto done=[&]{factory.destroy(api.context);FreeLibrary(library);};
        auto r=api.setContentRoot(api.context,content.u8string().c_str(),errorText,sizeof(errorText));Record("asset-paths","set project Content",r,r==0);
        uint64_t sceneId=0;
        r=api.loadScene(api.context,(content/"Scenes/Main.scene.json").u8string().c_str(),&sceneId,errorText,sizeof(errorText));Record("scene-save-load","reopen persona scene",r,r==0);
        HWND viewport=CreateWindowExW(0,L"STATIC",L"Persona authoring probe",WS_POPUP,0,0,800,600,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        r=api.attachViewport(api.context,viewport,800,600);Record("render-host","attach hidden D3D viewport",r,r==0);
        auto frame=[&]{return api.frame(api.context,0.016f);};
        r=frame();Record("render-host","render loaded project",r,r==0);
        r=api.resizeViewport(api.context,1024,512);Record("render-resize","resize to 1024x512",r,r==0);frame();
        EngineSceneBatchApi batch{};EngineAuthoringApi author{};EngineProjectDataApi data{};
        if(ext){ext(api.context,EngineSceneBatchExtension,1,sizeof(batch),&batch);ext(api.context,EngineAuthoringExtension,1,sizeof(author),&author);ext(api.context,EngineProjectDataExtension,1,sizeof(data),&data);}
        std::vector<uint64_t> ids;
        const char* types[]={"Primitive.Cube","Primitive.Sphere","Primitive.Plane","Primitive.Cylinder","Primitive.Cone","Primitive.Pyramid","Light.Point","Light.Directional","Particle.Sparks","Particle.Smoke","Environment.Sky"};
        for(unsigned i=0;i<11;++i){
            EngineSceneTransform t{{float(i%6)*2-5,1,float(i/6)*3+6},{0,0,0,1},{1,1,1}};uint64_t id=0;
            r=api.scene.createObject(api.scene.context,types[i],(std::string("Lab.")+types[i]).c_str(),&t,&id);
            const char* feature=i<6?"scene-primitives":i==6?"light-point":i==7?"light-sun":i<10?"particles":"sky";
            Record(feature,types[i],r,r==0);if(id)ids.push_back(id);frame();
        }
        if(ids.size()>=6){
            r=api.setParent(api.context,ids[1],ids[0],1);Record("scene-hierarchy","parent sphere to cube with scale inheritance",r,r==0);
            EngineSceneTransform t{{3,2,5},{0,0.3826834f,0,0.9238795f},{2,1,1}};
            r=api.scene.setTransform(api.scene.context,ids[0],&t);Record("scene-transform","translate rotate scale parent",r,r==0);
            r=api.focusObject(api.context,ids[0]);Record("scene-select","focus selected parent",r,r==0);frame();
            uint64_t picked=0;r=api.pickObject(api.context,0,0,&picked);Record("scene-select","pick centre of focused viewport",r,r==0&&picked!=0);
            r=api.setParent(api.context,ids[0],ids[1],1);Record("scene-hierarchy","reject hierarchy cycle",r,r==EngineSceneInvalidArgument);
            if(batch.apply){
                uint64_t input[]={ids[0],ids[1]}, output[2]{};
                r=batch.select(batch.context,input,2);Record("scene-groups","select parent and child",r,r==0);
                t.position[0]=4;
                r=batch.apply(batch.context,EngineBatchTransform,input,2,ids[0],&t,output,errorText,sizeof(errorText));Record("scene-groups","group transform",r,r==0);
                r=batch.apply(batch.context,EngineBatchDuplicate,input,2,ids[0],nullptr,output,errorText,sizeof(errorText));Record("scene-clone","duplicate parent subtree",r,r==0);
                if(r==0){uint64_t deleted[2]{};r=batch.apply(batch.context,EngineBatchDelete,output,2,output[0],nullptr,deleted,errorText,sizeof(errorText));Record("scene-create-delete","delete duplicated group",r,r==0);}
            }
        }
        fs::create_directories(content/"Materials");fs::create_directories(root/"ProbeSources");
        const std::string shader="cbuffer MaterialParams : register(b1) { float4 Tint=float4(0.25,0.75,0.35,1); };\nTexture2D BaseMap : register(t6);\nvoid Surface(MaterialInput i,inout MaterialOutput o) { o.Albedo=Tint.rgb*BaseMap.Sample(MaterialSampler,i.UV).rgb; o.Alpha=Tint.a; }\n";
        const auto shaderFile=content/"Materials/PersonaLab.hlsl";
        Put(shaderFile,shader);Put(content/"Materials/PersonaLab.mat",R"({"version":1,"shader":"PersonaLab.hlsl","render":{"lighting":"lit"},"params":{"Tint":[0.25,0.75,0.35,1]}})");
        const char* material="/Game/Materials/PersonaLab.mat";
        if(!ids.empty()){
            r=api.setMaterial(api.context,ids[0],material,errorText,sizeof(errorText));Record("material-assign","assign custom material",r,r==0);frame();
            std::string errors;
            r=api.readMaterial(api.context,material,[](void* p,const EngineMaterialInfo* m){*static_cast<std::string*>(p)=m->errors?m->errors:"";},&errors,errorText,sizeof(errorText));Record("material-params","compile/read reflected material",r,r==0&&errors.empty());
            float tint[4]={0.9f,0.2f,0.1f,0.7f};r=api.setMaterialParameter(api.context,material,"Tint",tint);Record("material-params","change Tint",r,r==0);
            for(uint32_t blend=0;blend<4;++blend){EngineMaterialRenderState state{blend,0.5f,0,1,1};r=api.setMaterialRenderState(api.context,material,&state);Record("material-blends",("set blend "+std::to_string(blend)+" twoSided").c_str(),r,r==0);frame();}
            r=api.saveMaterial(api.context,material,errorText,sizeof(errorText));Record("material-editor","save material through public authoring API",r,r==0);
            tint[0]=0.1f;api.setMaterialParameter(api.context,material,"Tint",tint);r=api.revertMaterial(api.context,material,errorText,sizeof(errorText));Record("material-editor","revert unsaved parameter",r,r==0);
            Put(shaderFile,"this is deliberately invalid hlsl\n");api.reloadShaderFile(api.context,shaderFile.u8string().c_str());frame();errors.clear();
            r=api.readMaterial(api.context,material,[](void* p,const EngineMaterialInfo* m){*static_cast<std::string*>(p)=m->errors?m->errors:"";},&errors,errorText,sizeof(errorText));
            Record("material-errors","invalid HLSL reports diagnostic",r,r==0&&!errors.empty());std::cout<<Json{{"expectedShaderDiagnostic",errors}}.dump()<<std::endl;
            Put(shaderFile,shader);api.reloadShaderFile(api.context,shaderFile.u8string().c_str());frame();errors.clear();
            r=api.readMaterial(api.context,material,[](void* p,const EngineMaterialInfo* m){*static_cast<std::string*>(p)=m->errors?m->errors:"";},&errors,errorText,sizeof(errorText));Record("material-errors","repair HLSL and reload",r,r==0&&errors.empty());
        }
        const auto textureSource=root/"ProbeSources/checker.bmp";
        // Standard 2x2 24-bit BMP, two 8-byte rows.
        unsigned char bmp[70]={0x42,0x4d,70,0,0,0,0,0,0,0,54,0,0,0,40,0,0,0,2,0,0,0,2,0,0,0,1,0,24,0};
        bmp[54]=255;bmp[58]=255;bmp[62]=255;bmp[63]=255;bmp[64]=255;
        {std::ofstream f(textureSource,std::ios::binary);f.write(reinterpret_cast<char*>(bmp),70);}
        char asset[4096]{};r=api.importAsset(api.context,textureSource.u8string().c_str(),1,asset,sizeof(asset),errorText,sizeof(errorText));Record("texture-import","import BMP",r,r==0);
        if(r==0&&!ids.empty()){
            const std::string texture=asset;
            r=api.setTexture(api.context,ids[2],texture.c_str(),errorText,sizeof(errorText));Record("texture-import","assign imported texture",r,r==0);
            r=api.setMaterialTexture(api.context,material,"BaseMap",texture.c_str(),errorText,sizeof(errorText));Record("material-params","assign material texture slot",r,r==0);frame();api.saveMaterial(api.context,material,errorText,sizeof(errorText));
            if(author.asset){r=author.asset(author.context,0,texture.c_str(),textureSource.u8string().c_str(),errorText,sizeof(errorText));Record("asset-reimport","reimport BMP from recorded source",r,r==0);}
        }
        Put(root/"ProbeSources/triangle.obj","v -1 0 0\nv 1 0 0\nv 0 2 0\nf 1 2 3\n");
        for(const auto& source:{root/"ProbeSources/triangle.obj",repo/"Game/RuntimeExample/Content/Models/CesiumMan.glb",repo/"Engine/tests/assets/fixtures/auto-color.fbx"}){
            asset[0]=0;r=api.importAsset(api.context,source.u8string().c_str(),0,asset,sizeof(asset),errorText,sizeof(errorText));Record("model-import",("import "+source.filename().u8string()).c_str(),r,r==0);
            if(r==0){uint64_t model=0;r=api.createModel(api.context,asset,("Lab.Model."+source.extension().u8string()).c_str(),nullptr,&model,errorText,sizeof(errorText));Record(source.extension()==".glb"?"skeletal":"model-import","instantiate imported model",r,r==0);frame();}
        }
        if(author.setProperty){
            std::vector<std::pair<uint64_t,std::string>> rows;EngineSceneInfo info{};
            api.scene.readScene(api.scene.context,&info,[](void* p,const EngineSceneObjectInfo* o){static_cast<decltype(rows)*>(p)->push_back({o->id,o->typeName});},&rows);
            for(const auto& row:rows)if(row.second=="CalmCity.CarMover"){
                r=author.setProperty(author.context,row.first,"/speed","3.25",errorText,sizeof(errorText));Record("scene-properties","set saved car speed 3.25",r,r==0);
                std::string saved;data.read(data.context,row.first,[](void* p,const char* s){*static_cast<std::string*>(p)=s;},&saved);Record("scene-codec","read saved speed",0,Json::parse(saved).at("speed")==3.25);
            }
        }
        const auto lab=content/"Scenes/FeatureLab.scene.json";
        r=api.saveScene(api.context,lab.u8string().c_str(),errorText,sizeof(errorText));Record("scene-new-save-as","save authoring lab as new scene",r,r==0);
        r=api.loadScene(api.context,lab.u8string().c_str(),&sceneId,errorText,sizeof(errorText));Record("scene-save-load","reopen saved lab",r,r==0);frame();
        r=api.control(api.context,EnginePlay);Record("play","Play saved lab",r,r==0);for(int i=0;i<10;++i)frame();
        r=api.control(api.context,EnginePause);Record("play","Pause",r,r==0);frame();
        r=api.control(api.context,EngineResume);Record("play","Resume",r,r==0);frame();
        r=api.control(api.context,EngineStop);Record("play","Stop",r,r==0);
        done();DestroyWindow(viewport);
        std::cout<<Json{{"summaryFailures",failures},{"visualVerification",false}}.dump()<<std::endl;
        return failures?1:0;
    }catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 2;}
}
