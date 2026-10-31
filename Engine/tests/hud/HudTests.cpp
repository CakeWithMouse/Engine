#define NOMINMAX
#include <Engine/Components/SpecificComponents/UIComponent.h>
#include <Engine/UI/UIMarkup.h>
#include <Engine/UI/ProjectHud.h>
#include <Engine/Resources/Paths.h>
#include <Engine/Runtime/SceneEditSession.h>
#include <Engine/Project/StandardTypes.h>
#include <Engine/InputDevice/InputActions.h>
#include "../../Source/Resources/VfsRead.h"
#include <fstream>
#include <iostream>
#include <chrono>
using Microsoft::WRL::ComPtr;
namespace fs=std::filesystem;
static int checks=0;
void Check(bool v,const char* message){if(!v)throw std::runtime_error(message);++checks;}
void Write(const fs::path& path,const std::string& value){std::ofstream(path,std::ios::binary)<<value;static int stamp=0;fs::last_write_time(path,fs::file_time_type::clock::now()+std::chrono::seconds(++stamp));}
class RenderTest : public Game {
    HWND window = CreateWindowExW(0,L"STATIC",L"HUD test",WS_POPUP,0,0,320,240,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
public:
    RenderTest(){Check(window!=nullptr,"hidden borrowed viewport");Check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&Device,nullptr,&Context)),"WARP device");DisplayPtr=std::make_unique<Display>(window,320,240);Check(CreateInputLayouts(),"UI vertex layout");}
    ~RenderTest(){DestroyWindow(window);}
    void EmbeddedClick(){
        auto* hud=GetProjectHud();POINT point{};Check(GetCursorPos(&point)!=0,"desktop cursor available for embedded input test");Check(ScreenToClient(window,&point)!=0,"borrowed window client coordinates");
        auto* button=hud->AddButton({UIAnchor::TopLeft,float(point.x-5000),float(point.y-5000),10000,10000},"click",{});button->Name="embedded";int clicks=0;hud->BindClick("embedded",[&]{++clicks;});
        Check(GetInputDevice()==nullptr,"embedded viewport has no Raw Input device");GetInput().SetKey(Keys::LeftButton,true);hud->PrepareInput();Check(hud->WantsPointer(),"embedded UI captures mouse");GetInput().SetKey(Keys::LeftButton,false);hud->PrepareInput();Check(clicks==1,"Qt forwarded key drives HUD click");
        GetInput().SetKey(Keys::LeftButton,true);hud->PrepareInput();OnFocusLost();hud->PrepareInput();Check(clicks==1,"focus loss cancels captured click");
    }
    uint32_t Pixel(){
        D3D11_TEXTURE2D_DESC d{};d.Width=320;d.Height=240;d.MipLevels=d.ArraySize=1;d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.SampleDesc.Count=1;d.BindFlags=D3D11_BIND_RENDER_TARGET;
        ComPtr<ID3D11Texture2D> target,read;ComPtr<ID3D11RenderTargetView> rtv;
        Check(SUCCEEDED(Device->CreateTexture2D(&d,nullptr,&target)),"target");Device->CreateRenderTargetView(target.Get(),nullptr,&rtv);Context->OMSetRenderTargets(1,rtv.GetAddressOf(),nullptr);
        const float black[]={0,0,0,1};Context->ClearRenderTargetView(rtv.Get(),black);D3D11_VIEWPORT vp{0,0,320,240,0,1};Context->RSSetViewports(1,&vp);
        RenderUI();d.BindFlags=0;d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;Check(SUCCEEDED(Device->CreateTexture2D(&d,nullptr,&read)),"readback");Context->CopyResource(read.Get(),target.Get());D3D11_MAPPED_SUBRESOURCE m{};Check(SUCCEEDED(Context->Map(read.Get(),0,D3D11_MAP_READ,0,&m)),"readback map");const auto pixel=reinterpret_cast<uint32_t*>(static_cast<char*>(m.pData)+20*m.RowPitch)[20];Context->Unmap(read.Get(),0);return pixel;
    }
};
static Game* playGame=nullptr;
std::unique_ptr<Game> Factory(){auto game=std::make_unique<Game>();playGame=game.get();return game;}
int wmain(int argc,wchar_t** argv){try{
    Check(argc==3,"HudTests <fixture> <EngineContent>");const auto root=fs::absolute(argv[1]);fs::create_directories(root/"UI");EnginePaths::SetContentRoot(fs::absolute(argv[2]));VfsRead::GetVFS().Mount("/Game",root);
    Check(ProjectHud::Read(root).empty(),"missing selection means no HUD");
    bool rejected=false;try{ProjectHud::Resolve(root,"../outside.ui");}catch(...){rejected=true;}Check(rejected,"reject escaping HUD path");
    const auto file=root/"UI/Hud.ui";
    const std::string markup="<UI><Panel name=\"panel\" rect=\"10 10 120 100\" color=\"#ff0000ff\" padding=\"1 2 3 4\" border=\"2\"/><Label name=\"text\" rect=\"150 60 120 20\" text=\"Привет &amp; мир\" align=\"right\"/><Button name=\"button\" rect=\"150 10 100 40\" hoverColor=\"#12345678\"/><Bar name=\"bar\" rect=\"150 100 100 12\" fill=\"0.4\"/></UI>";
    Write(file,markup);UIComponent component;int clicks=0;component.BindClick("button",[&]{++clicks;});Check(component.LoadMarkup(file.u8string()),"load initial markup");
    component.Find<UIButton>("button")->OnClick();Check(clicks==1,"initial binding");auto revision=component.GetMarkupRevision();
    Write(file,markup);component.PrepareInput();Check(component.GetMarkupRevision()==revision+1,"automatic hot reload");component.Find<UIButton>("button")->OnClick();Check(clicks==2,"binding survives replacement");
    Check(component.SetLabelText("text","Updated")&&component.SetBarFill("bar",0.7f),"safe name based values");Check(!component.SetLabelText("missing","x"),"missing name safe");
    const auto serialized=UIMarkup::Serialize(component.GetRoot());std::unique_ptr<UIElement> parsed;std::string error;Check(UIMarkup::Parse(serialized,"roundtrip",parsed,error),"serialization parses");Check(UIMarkup::Serialize(*parsed)==serialized,"all supported properties roundtrip");
    auto* old=&component.GetRoot();Write(file,"<UI><wrong/></UI>");component.PrepareInput();Check(&component.GetRoot()==old&&!component.GetMarkupError().empty(),"invalid update keeps working HUD and reports error");
    Write(file,"<UI/>");component.PrepareInput();Check(!component.Find<UIButton>("button"),"removed button safe");Write(file,markup);component.PrepareInput();component.Find<UIButton>("button")->OnClick();Check(clicks==3,"binding survives removal and reappearance");
    component.UnbindClick("button");Check(!component.Find<UIButton>("button")->OnClick,"unbind callback");
    Check(UIMarkup::Parse("\xEF\xBB\xBF<UI/>","BOM",parsed,error),"UTF8 BOM accepted");
    Write(root/"ProjectHud.txt","UI/Hud.ui\n");
    {Game game;Check(!game.GetProjectHud(),"HUD not created before gameplay");game.InitializeGameplay();Check(game.GetProjectHud(),"selection creates HUD without C++ registration");auto* hud=game.GetProjectHud();game.InitializeGameplay();Check(game.GetProjectHud()==hud&&game.GetScene().GetComponents().empty(),"idempotent HUD outside scene serialization");}
    {SceneEditSession session(SceneProject::GetComponentTypes(),Factory);auto api=session.GetApi();for(int i=0;i<3;++i){Check(api.control(api.context,EnginePlay)==EngineSceneOk,"Play");Check(playGame->GetProjectHud()!=nullptr,"HUD recreated on Play");Check(api.control(api.context,EngineStop)==EngineSceneOk,"Stop");}}
    {RenderTest game;game.InitializeGameplay();Check((game.Pixel()&255)>240,"selected HUD actually renders red panel");game.InvalidateAllShaders();Check((game.Pixel()&255)>240,"HUD survives shader reload");Write(file,"<Panel rect=\"10 10 120 100\" color=\"#00ff00ff\"/>");game.GetProjectHud()->PrepareInput();const auto pixel=game.Pixel();Check((pixel&255)<10&&((pixel>>8)&255)>240,"reload renders green root leaf");game.EmbeddedClick();}
    Write(root/"ProjectHud.txt","");{Game game;game.InitializeGameplay();Check(!game.GetProjectHud(),"None disables HUD on next run");}
    Input::System input;Input::Scope scope;int shots=0;float turn=0;input.Bind({"shoot",Input::Context::Game,{Keys::LeftButton}});Input::Binding look;look.action="look";look.source=Input::Source::MouseX;input.Bind(look);
    auto shoot=input.Subscribe(scope,Input::Context::Game,"shoot",Input::Phase::Pressed,[&](auto&){++shots;});auto camera=input.Subscribe(scope,Input::Context::Game,"look",Input::Phase::Axis,[&](auto& e){turn=e.value;});
    input.SetPointerBlocked(true);input.SetKey(Keys::LeftButton,true);input.AddPointer(20,0);input.Tick(.01f);Check(shots==0&&turn==0,"HUD blocks gameplay click and camera");input.SetKey(Keys::LeftButton,false);input.Tick(.01f);input.SetPointerBlocked(false);input.SetKey(Keys::LeftButton,true);input.AddPointer(5,0);input.Tick(.01f);Check(shots==1&&turn==5,"gameplay input restored");
    std::cout<<"HUD: "<<checks<<" checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
