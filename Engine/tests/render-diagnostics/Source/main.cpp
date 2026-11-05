#define NOMINMAX
#include <Engine/MainGame/BaseGameClass/Game.h>
#include <Engine/Components/SpecificComponents/InstancedMeshComponent.h>
#include <Engine/Components/SpecificComponents/CubeComponent.h>
#include <Engine/Components/SpecificComponents/MeshComponent.h>
#include <Engine/Render/MeshPrimitives.h>
#include <Engine/Resources/Paths.h>
#include <iostream>
#include <filesystem>
#include <numeric>
#include <fstream>
static int checks=0;
void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);++checks;}
// Compare projected geometry, not CPU instance submission counts. Render an owner
// first, then create copies so cached non-instanced shaders are also exercised.
void CheckComponentCopies(HWND window,const std::filesystem::path& output){
    bool allPlaced=true;
    for(int kind=0;kind<3;++kind){
        Game game;Check(game.InitializeViewport(window,640,480),"copy probe viewport");
        game.SetSimulationEnabled(false);game.GetSettings().SetVSync(false);
        auto* camera=game.GetPlayer();camera->SetCameraMode(CameraMode::Free);
        camera->SetPosition({0,24,0});camera->SetViewDirection({0,-1,0},{0,0,1});camera->SetProjection(true,60,.1f,200,12);
        std::unique_ptr<GameComponent> owner;
        if(kind==2){auto mesh=std::make_unique<MeshComponent>();mesh->SetMesh(MeshPrimitives::Cube());owner=std::move(mesh);}
        else owner=std::make_unique<CubeComponent>();
        auto* ptr=owner.get();
        if(kind==1)Check(ptr->SetMaterial(game.GetDefaultMaterial()),"copy material");
        game.GetScene().AddComponent("Copy template",owner);
        Check(game.RenderViewportFrame(0),"owner frame before copies");
        for(float x:{-3.f,0.f,3.f})ptr->CreateInstance({x,0,0},{0,0,0},{1,1,1},{1,1,1,1});
        const auto path=output/("component-copies-"+std::to_string(kind)+".bmp");
        game.RequestFrameDump(path.u8string());Check(game.RenderViewportFrame(0),"copy frame");
        std::ifstream image(path,std::ios::binary);BITMAPFILEHEADER header{};BITMAPINFOHEADER info{};
        image.read(reinterpret_cast<char*>(&header),sizeof(header));image.read(reinterpret_cast<char*>(&info),sizeof(info));
        Check(image.good()&&(info.biBitCount==24||info.biBitCount==32)&&info.biWidth==640&&std::abs(info.biHeight)==480,"copy screenshot format");
        const int bytes=info.biBitCount/8,stride=(640*bytes+3)&~3,row=info.biHeight>0?479-240:240;
        auto brightness=[&](int x){unsigned char rgba[4]{};image.seekg(header.bfOffBits+row*stride+x*bytes);image.read(reinterpret_cast<char*>(rgba),bytes);return int(rgba[0])+rgba[1]+rgba[2];};
        bool placed=true;for(int x:{200,320,440})placed&=brightness(x)>150;
        for(int x:{260,380})placed&=brightness(x)<100;
        std::cout<<"ComponentCopies kind="<<kind<<" placed="<<placed<<'\n';allPlaced&=placed;
    }
    Check(allPlaced,"CreateInstance projects three copies with and without a material");
}
class ProbeGame:public Game {
public:
    uint64_t observed=0;bool observedValid=false;uint32_t observedDraws=0,currentDraws=0;
    void Draw()override{} // A custom pass deliberately does not submit scene meshes.
    void PreUpdate(float)override{
        // A hidden standalone window still receives Raw Input from the user's mouse.
        if(GetInputDevice())GetInputDevice()->MouseOffset={0,0};
        const auto& f=GetCompletedFrameStats();observed=f.frameIndex;observedValid=f.valid;observedDraws=f.counters.DrawCalls;currentDraws=GetFrameStats().DrawCalls;
    }
};
int wmain(int argc,wchar_t** argv){try{
    Check(argc==3,"content and output paths");EnginePaths::SetContentRoot(argv[1]);std::filesystem::create_directories(argv[2]);
    HWND window=CreateWindowExW(0,L"STATIC",L"LOD diagnostics",WS_OVERLAPPEDWINDOW,0,0,640,480,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    Check(window!=nullptr,"window");
    for(bool embedded:{true,false}){
        ProbeGame game;Check(!game.GetCompletedFrameStats().valid,"no fabricated first frame");
        Check(embedded?game.InitializeViewport(window,640,480):game.InitializeRuntimeWindow(false),"initialize D3D");
        game.GetSettings().SetVSync(false);game.SetSimulationTimeScale(1);
        auto* player=game.GetPlayer();player->SetCameraMode(CameraMode::Free);player->SetPosition({0,10,0});
        player->SetRotate(-1.5707963f,-1.5607963f); // Keep free-camera Euler state consistent in standalone.
        player->SetViewDirection({0,-1,0},{0,0,1});player->SetProjection(true,60,.1f,200,20);
        auto mesh=std::make_unique<InstancedMeshComponent>();auto* ptr=mesh.get();
        Check(ptr->SetLod(0,MeshPrimitives::Cube())&&ptr->SetLod(1,MeshPrimitives::Cube()),"LOD meshes");
        ptr->SetLodDistances({15});ptr->SetDrawDistance(100);ptr->SetLodFadeFraction(0);
        MeshInstance instance;ptr->AddInstance(instance);instance.bVisible=false;ptr->AddInstance(instance);
        instance.bVisible=true;instance.Position={1000,0,0};ptr->AddInstance(instance);instance.Position={40,0,0};ptr->AddInstance(instance);
        std::unique_ptr<GameComponent> component=std::move(mesh);auto result=game.GetScene().AddComponent("Units",component);
        auto frame=[&]{Check(embedded?game.RenderViewportFrame(0):game.RunRuntimeFrame(0)==Game::RuntimeFrameResult::Running,"render frame");};
        frame();const auto first=game.GetCompletedFrameStats();
        Check(first.valid&&!game.observedValid&&first.counters.DrawCalls>0,"first frame published after render");
        Check(first.lodComponents.size()==1,"one component");const auto& d=first.lodComponents[0];
        Check(d.prepared&&d.frameIndex==first.frameIndex&&d.total==4,"current frame LOD data");
        Check(d.drawn==1&&d.hidden==1&&d.distanceCulled==1&&d.frustumCulled==1,"exclusive rejection reasons");
        Check(d.perLod==std::vector<uint32_t>({1,0}),"near LOD0");
        Check(d.hasDistance&&d.minDistance==10&&d.thresholds==std::vector<float>{15},"distance and thresholds");
        frame();Check(game.observed==first.frameIndex&&game.observedDraws==first.counters.DrawCalls&&game.currentDraws==0,"PreUpdate reads previous completed counters despite current reset");
        const auto withoutPanel=game.GetCompletedFrameStats().counters;
        game.SetRenderDiagnosticsVisible(true);game.SetLodDiagnosticsTarget(result.id);
        const auto output=std::filesystem::path(argv[2])/(embedded?"embedded-near.bmp":"runtime-near.bmp");game.RequestFrameDump(output.u8string());
        frame();Check(std::filesystem::file_size(output)>54,"actual panel frame dump");
        Check(game.GetCompletedFrameStats().counters.DrawCalls==withoutPanel.DrawCalls&&game.GetFrameStats().DrawCalls==withoutPanel.DrawCalls,"panel excluded from counters");
        Check(game.GetRenderDiagnosticsLines().size()>10,"panel has LOD and rejection details");
        game.GetInput().SetKey(Keys::F7,true);frame();
        Check(!game.IsRenderDiagnosticsVisible(),"F7 toggles diagnostics through shared input in either host");
        frame();Check(!game.IsRenderDiagnosticsVisible(),"held F7 does not toggle repeatedly");
        game.GetInput().SetKey(Keys::F7,false);frame();game.GetInput().SetKey(Keys::F7,true);frame();
        Check(game.IsRenderDiagnosticsVisible(),"released and pressed F7 shows diagnostics again");
        game.GetInput().SetKey(Keys::F7,false);frame();
        game.InvalidateAllShaders();frame();
        Check(game.GetCompletedFrameStats().counters.DrawCalls==withoutPanel.DrawCalls,"visible panel survives shader cache invalidation");
        player->SetPosition({0,30,0});frame();
        for(const auto& line:game.GetRenderDiagnosticsLines())std::cout<<line<<'\n';
        Check(game.GetCompletedFrameStats().lodComponents[0].perLod==std::vector<uint32_t>({0,1}),"far LOD1");
        ptr->SetLodScreenHeights({50});frame();
        const auto orthoBuckets=game.GetCompletedFrameStats().lodComponents[0].perLod;
        player->SetPosition({0,10,0});frame();
        Check(game.GetCompletedFrameStats().lodComponents[0].perLod==orthoBuckets,"orthographic screen LOD is independent of distance");
        player->SetPosition({0,30,0});frame();
        const float threshold=ptr->GetLocalBoundingRadius()*game.GetDisplay()->GetHeight()*player->GetProjectionMatrix()._22;
        ptr->SetLodScreenHeights({threshold});ptr->SetLodFadeFraction(.2f);frame();
        const auto fade=game.GetCompletedFrameStats().lodComponents[0];
        Check(fade.screenSize&&fade.drawn==1&&fade.perLod==std::vector<uint32_t>({1,1}),"cross-fade buckets overlap without double-counting unique visibility");
        ptr->SetLodScreenHeights({});ptr->SetLodFadeFraction(0);
        game.GetSettings().SetRenderingType(Deffered);frame();
        Check(game.GetCompletedFrameStats().valid&&game.GetCompletedFrameStats().lodComponents[0].prepared,"Deferred publishes frame and LOD");
        game.GetSettings().SetRenderingType(Custom);frame();
        Check(!game.GetCompletedFrameStats().lodComponents[0].prepared&&game.GetCompletedFrameStats().lodComponents[0].perLod.empty(),"Custom pass does not expose stale scene buckets");
        game.GetSettings().SetRenderingType(Forward);
        std::unique_ptr<GameComponent> empty=std::make_unique<InstancedMeshComponent>();
        const auto emptyId=game.GetScene().AddComponent("Empty mesh",empty).id;game.SetLodDiagnosticsTarget(emptyId);frame();
        auto lines=game.GetRenderDiagnosticsLines();
        Check(lines.back().find("Not prepared")!=std::string::npos,"panel target with no geometry reports unavailable data");
        game.SetLodDiagnosticsTarget(result.id);
        game.GetInput().SetKey(Keys::F8,true);frame();
        Check(game.GetRenderDiagnosticsLines().back().find("Not prepared")!=std::string::npos,"F8 selects next component through shared input");
        game.GetInput().SetKey(Keys::F8,false);frame();game.GetInput().SetKey(Keys::F8,true);frame();
        Check(game.GetRenderDiagnosticsLines().back().find("Not prepared")==std::string::npos,"F8 cycles back to prepared component");
        game.GetInput().SetKey(Keys::F8,false);frame();
        game.GetScene().RemoveComponent(emptyId);game.SetLodDiagnosticsTarget(result.id);
        const auto previous=game.GetCompletedFrameStats().frameIndex;game.SetSimulationEnabled(false);frame();
        Check(game.GetCompletedFrameStats().frameIndex>previous,"paused simulation publishes rendered frames");
        const auto paused=game.GetCompletedFrameStats().frameIndex;game.OnWindowResized(0,0,true);frame();
        Check(game.GetCompletedFrameStats().frameIndex==paused,"minimized frame does not fabricate snapshot");
        game.OnWindowResized(320,240,false);
        game.RequestFrameDump((std::filesystem::path(argv[2])/(embedded?"embedded-small.bmp":"runtime-small.bmp")).u8string());frame();
        Check(game.GetCompletedFrameStats().frameIndex>paused,"resize resumes publication");
        game.GetScene().RemoveComponent(result.id);frame();Check(game.GetCompletedFrameStats().lodComponents.empty(),"removed components leave no stale panel entries");
    }
    CheckComponentCopies(window,argv[2]);
    DestroyWindow(window);std::cout<<"RenderDiagnosticsTests: "<<checks<<" checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"RenderDiagnosticsTests: "<<e.what()<<"\n";return 1;}}
