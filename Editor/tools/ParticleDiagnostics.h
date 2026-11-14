#pragma once
#include <Engine/Components/SpecificComponents/ParticleSystemComponent.h>
#include "PointShadowDiagnostics.h"

// A real component/renderer path. Collision evidence comes from controlled A/B
// images, not from claiming that setting a boolean proves a GPU collision.
namespace ParticleDiagnostics {
inline QJsonObject Run(const QJsonObject& in) {
    using namespace PointShadowDiagnostics;
    const int width=Integer(in,"width",480,100,1920), height=Integer(in,"height",360,100,1080);
    const int count=Integer(in,"particleCount",128,64,4096);
    const int rendering=Integer(in,"rendering",0,0,1);
    const auto frames=in["frames"].toArray();
    Check(!frames.empty() && frames.size()<=64,"1..64 particle observations required");
    EnginePaths::SetContentRoot(std::filesystem::path(QCoreApplication::applicationDirPath().toStdWString())/"EngineContent");
    HWND hwnd=CreateWindowExW(0,L"STATIC",L"Particle diagnostics",WS_OVERLAPPEDWINDOW,
        0,0,width,height,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    Check(hwnd!=nullptr,"particle viewport creation failed");
    struct Guard { HWND value; ~Guard(){DestroyWindow(value);} } guard{hwnd};
    Game game;
    Check(game.InitializeViewport(hwnd,width,height),"particle D3D initialization failed");
    game.GetSettings().SetVSync(false);
    game.SetSimulationTimeScale(1);
    game.GetSettings().SetRenderingType(static_cast<RenderType>(rendering));
    game.SetDeferredDebugOverlay(false);
    game.SetShadowSettings(false,30);
    auto* camera=game.GetPlayer();
    const auto eye=Vec(in["camera"],{5,5,-8}), target=Vec(in["target"],{0,1,0});
    Check(glm::length(target-eye)>.01f,"camera and target must differ");
    camera->SetCameraMode(CameraMode::Free);
    camera->SetPosition(eye);
    camera->SetViewDirection(glm::normalize(target-eye),{0,1,0});
    camera->SetProjection(in["orthographic"].toBool(),60,.1f,100,Number(in,"orthoHeight",8));
    camera->StopRotate(false,false); camera->EnableWASD(false);
    auto add=[&](const char* name,std::unique_ptr<GameComponent> component) {
        Check(game.RegisterComponent(name,component.get())==RegisterResult::Ok,"particle scene registration failed");
        component.release();
    };
    if (in["surface"].toBool(true)) {
        auto floor=std::make_unique<MeshComponent>();
        floor->SetMesh(MeshPrimitives::Cube());
        floor->SetPosition(Vec(in["surfacePosition"],{0,-.1f,0}));
        floor->SetScale(Vec(in["surfaceScale"],{8,.2f,8}));
        floor->SetColor({.35f,.4f,.45f,1});
        add("Collision surface",std::move(floor));
    }
    auto particles=std::make_unique<ParticleSystemComponent>();
    particles->SetParticleCount(count);
    particles->SetPosition(Vec(in["position"],{0,2,0}));
    const auto color=Vec(in["color"],{1,.05f,.02f});
    particles->SetColor({color.x,color.y,color.z,1});
    particles->SetBaseLifetime(Number(in,"lifetime",10));
    particles->SetSpreadRadius(Number(in,"spread",.15f));
    particles->SetRadialSpeed(Number(in,"radialSpeed",0));
    particles->SetBaseVelocity(Vec(in["baseVelocity"],{0,-3,0}));
    particles->SetAcceleration(Vec(in["acceleration"],{0,-2,0}));
    particles->SetRandomness(0,0);
    particles->SetSimulationRate(1);
    particles->SetBaseSize(Number(in,"size",.12f));
    particles->SetBrightness(Number(in,"brightness",2));
    particles->SetDepthCollision(in["depthCollision"].toBool(true));
    particles->SetDepthCollisionResponse(Number(in,"bias",.05f),Number(in,"bounce",.8f),Number(in,"friction",0));
    particles->SetSortingEnabled(in["sorting"].toBool(true));
    add("Observed particles",std::move(particles));
    // Establish the previous-frame depth buffer before allowing simulation.
    game.SetSimulationEnabled(false);
    Check(game.RenderViewportFrame(0),"particle warmup failed");
    game.SetSimulationEnabled(true);
    QJsonArray observations;
    int total=0; double elapsed=0;
    for (const auto entry:frames) {
        const auto frame=entry.toObject();
        const int steps=Integer(frame,"steps",1,1,600);
        const float dt=Number(frame,"dt",1.0f/60.0f);
        Check((total+=steps)<=2000 && dt>0 && dt<=.1f,"invalid particle time steps");
        const auto image=frame["image"].toString();
        if (!image.isEmpty()) Check(QFileInfo(image).isAbsolute() && !QFileInfo::exists(image) && QFileInfo(image).dir().exists(),"particle image must be a new absolute file");
        for (int index=0;index<steps;++index) {
            if (index==steps-1 && !image.isEmpty()) game.RequestFrameDump(image.toUtf8().toStdString());
            Check(game.RenderViewportFrame(dt),"particle render failed");
            elapsed+=dt;
        }
        const auto& stats=game.GetCompletedFrameStats().counters;
        QJsonObject state{{"steps",total},{"elapsed",elapsed},{"image",image},
            {"dispatches",double(stats.Dispatches)},{"drawCalls",double(stats.DrawCalls)},
            {"instances",double(stats.InstancesDrawn)}};
        if (!image.isEmpty()) {
            QFile file(image); Check(file.open(QIODevice::ReadOnly) && file.size()>54,"particle frame missing");
            state["sha256"]=QString::fromLatin1(QCryptographicHash::hash(file.readAll(),QCryptographicHash::Sha256).toHex());
        }
        observations.append(state);
    }
    return {{"adapter",QString::fromStdString(game.GetAdapterName())},
        {"depthCollision",in["depthCollision"].toBool(true)}, {"surface",in["surface"].toBool(true)},
        {"frames",observations}, {"measurement","Real GPU simulation and image readback; no particle-state readback or inferred collision counter."}};
}
}
