#include <Engine/MainGame/BaseGameClass/Game.h>
#include <Engine/Components/SpecificComponents/InstancedMeshComponent.h>
#include <Engine/Components/SpecificComponents/UIComponent.h>
#include <sstream>
#include <iomanip>

void Game::PublishFrameStats()
{
    CompletedFrameStats next;
    next.valid=true;next.frameIndex=FrameIndex;next.counters=Stats;
    for(const auto& entry:scene.GetComponents())if(auto* mesh=dynamic_cast<InstancedMeshComponent*>(entry.second.get())){
        auto value=mesh->GetLodDiagnostics();
        // Do not present stale buckets as current when a component did not render.
        if(!value.prepared||value.frameIndex!=FrameIndex){value={};value.total=static_cast<uint32_t>(mesh->GetMeshInstanceCount());}
        value.componentId=mesh->GetComponentId();value.name=entry.first;
        next.lodComponents.push_back(std::move(value));
    }
    CompletedStats=std::move(next);LastFrameStats=Stats;
}

std::vector<std::string> Game::GetRenderDiagnosticsLines() const
{
    std::vector<std::string> lines={"RENDER / LOD    F7 hide | F8 next"};
    if(!CompletedStats.valid){lines.push_back("Waiting for a completed render frame");return lines;}
    const auto& s=CompletedStats.counters;
    lines.push_back("Frame "+std::to_string(CompletedStats.frameIndex)+" | CPU submission counters");
    lines.push_back("Draws "+std::to_string(s.DrawCalls)+"  instances "+std::to_string(s.InstancesDrawn)+"  dispatch "+std::to_string(s.Dispatches));
    lines.push_back("Indices "+std::to_string(s.IndicesDrawn)+" (all passes)");
    const auto& list=CompletedStats.lodComponents;
    if(list.empty()){lines.push_back("No InstancedMeshComponent in this scene");return lines;}
    size_t index=0;for(size_t i=0;i<list.size();++i)if(list[i].componentId==DiagnosticsTarget){index=i;break;}
    const auto& d=list[index];
    lines.push_back("Component "+std::to_string(index+1)+"/"+std::to_string(list.size())+"  "+d.name);
    if(!d.prepared){lines.push_back("Not prepared this frame (no current LOD data)");return lines;}
    lines.push_back("Color visible "+std::to_string(d.drawn)+" / "+std::to_string(d.total));
    lines.push_back("Hidden "+std::to_string(d.hidden)+"  distance "+std::to_string(d.distanceCulled)+"  frustum "+std::to_string(d.frustumCulled));
    auto number=[](float value){std::ostringstream out;out<<std::fixed<<std::setprecision(1)<<value;return out.str();};
    lines.push_back(d.hasDistance?"Camera distance "+number(d.minDistance)+".."+number(d.maxDistance):"Camera distance: no enabled instances");
    lines.push_back("Draw limit "+(d.drawDistance>0?number(d.drawDistance):"off")+" | LOD by "+(d.screenSize?"screen pixels":"distance"));
    for(size_t i=0;i<d.perLod.size();++i){
        std::string row="LOD"+std::to_string(i)+(static_cast<int>(i)==d.impostorLevel?" sprite":" mesh")+": "+std::to_string(d.perLod[i]);
        if(i<d.thresholds.size())row+=" | next "+std::string(d.screenSize?"< ":">= ")+number(d.thresholds[i])+(d.screenSize?" px":" units");
        lines.push_back(std::move(row));
    }
    lines.push_back("Buckets may overlap during LOD fade");
    lines.push_back("Color selection only; shadows counted separately");
    return lines;
}

void Game::RenderDiagnosticsPanel()
{
    if(!bRenderDiagnostics||!DisplayPtr||!Device||!Context)return;
    const float width=std::min(430.f,static_cast<float>(DisplayPtr->GetWidth())-16.f);
    const float availableHeight=static_cast<float>(DisplayPtr->GetHeight())-16.f;
    if(width<100||availableHeight<40)return;
    if(!DiagnosticsHud){
        DiagnosticsHud=std::make_unique<UIComponent>();DiagnosticsHud->SetGame(this);
        FontAtlasSettings font;font.PixelHeight=14;DiagnosticsHud->SetFont(font);
    }
    DiagnosticsHud->CreateBuffers(Device.Get());
    DiagnosticsHud->Clear();
    auto lines=GetRenderDiagnosticsLines();
    const size_t capacity=static_cast<size_t>((availableHeight-16.f)/18.f);
    if(lines.size()>capacity){lines.resize(capacity);lines.back()="More details: enlarge viewport / use snapshot API";}
    UIRect rect;rect.Anchor=UIAnchor::TopRight;rect.X=8;rect.Y=8;rect.Width=width;rect.Height=16.f+18.f*lines.size();
    DiagnosticsHud->AddPanel(rect,{.025f,.035f,.055f,.94f});
    for(size_t i=0;i<lines.size();++i){
        rect.X=16;rect.Y=16.f+18.f*i;rect.Width=width-16;rect.Height=18;
        auto* label=DiagnosticsHud->AddLabel(rect,lines[i],{.90f,.94f,1,1});
        auto& text=label->Text;const auto& font=DiagnosticsHud->GetFont();
        if(font.MeasureWidth(text)>rect.Width){
            while(!text.empty()&&font.MeasureWidth(text+L"...")>rect.Width){text.pop_back();if(!text.empty()&&text.back()>=0xD800&&text.back()<=0xDBFF)text.pop_back();}
            text+=L"...";
        }
    }
    const auto before=Stats;
    if(BindComponentShaders(DiagnosticsHud.get(),ShaderCompileVariant::Default))DiagnosticsHud->Render(Context.Get());
    Stats=before; // The diagnostic overlay must not perturb the reported counters.
}
