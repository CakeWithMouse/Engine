#pragma once
#include "PointShadowDiagnostics.h"
#include <Engine/SaveGame/SaveGame.h>
#include <Engine/Scene/Scene.h>
#include <Engine/Render/MeshSimplifier.h>
#include <Engine/Render/MeshValidation.h>
#include <QImage>

namespace MeshDiagnostics {
inline QJsonObject Simplify(const QJsonObject& in) {
    using namespace PointShadowDiagnostics;
    const int slices=Integer(in,"slices",24,3,128),stacks=Integer(in,"stacks",12,2,128);
    const float radius=Number(in,"radius",1);
    Check(radius>0 && radius<=100,"invalid source sphere");
    auto source=MeshPrimitives::Sphere(radius,slices,stacks);
    const auto offset=Vec(in["offset"],{});
    for(auto& vertex:source.Vertices) {
        vertex.Position.x+=offset.x;vertex.Position.y+=offset.y;vertex.Position.z+=offset.z;
    }
    MeshSimplifySettings settings;
    settings.TargetRatio=Number(in,"ratio",.4f);settings.TargetError=Number(in,"error",.1f);
    Check(settings.TargetRatio>0 && settings.TargetRatio<=1 && settings.TargetError>=0 && settings.TargetError<=1,"invalid simplification settings");
    settings.bAllowSloppy=in["allowSloppy"].toBool(true);
    MeshSimplifyResult changed;
    const auto reduced=SimplifyMesh(source,settings,&changed);
    auto report=[](const MeshLodData& mesh) {
        const auto validation=ValidateMesh(mesh);
        QJsonArray problems,warnings;
        for(const auto& value:validation.Problems)problems.append(QString::fromStdString(value));
        for(const auto& value:validation.Warnings)warnings.append(QString::fromStdString(value));
        return QJsonObject{{"usable",validation.IsUsable()},{"problems",problems},{"warnings",warnings},
            {"vertices",validation.Vertices},{"triangles",validation.Triangles},
            {"degenerateTriangles",validation.DegenerateTriangles},{"missingNormals",validation.MissingNormals},
            {"hasTexCoords",validation.bHasTexCoords}};
    };
    return {{"source",report(source)},{"reduced",report(reduced)},
        {"targetTriangles",changed.TargetTriangles},{"error",changed.Error},
        {"silhouetteDrift",changed.SilhouetteDrift},{"usedSloppy",changed.bUsedSloppy}};
}
inline QJsonArray Basis(const MeshLodData& mesh) {
    QJsonArray result;
    for (const auto& vertex:mesh.Vertices)
        result.append(QJsonArray{vertex.Tangent.x,vertex.Tangent.y,vertex.Tangent.z,vertex.Tangent.w});
    return result;
}
inline QJsonObject Run(const QJsonObject& in) {
    using namespace PointShadowDiagnostics;
    const int width=Integer(in,"width",320,100,1920),height=Integer(in,"height",240,100,1080);
    const auto path=in["material"].toString();
    Check(QFileInfo(path).isAbsolute() && QFileInfo(path).isFile(),"absolute material file required");
    const auto frames=in["frames"].toArray();
    Check(!frames.empty() && frames.size()<=64,"1..64 mesh frames required");
    EnginePaths::SetContentRoot(std::filesystem::path(QCoreApplication::applicationDirPath().toStdWString())/"EngineContent");
    HWND hwnd=CreateWindowExW(0,L"STATIC",L"Mesh diagnostics",WS_OVERLAPPEDWINDOW,0,0,width,height,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    Check(hwnd!=nullptr,"mesh viewport creation failed");
    struct Guard { HWND value; ~Guard(){DestroyWindow(value);} } guard{hwnd};
    Game game; Check(game.InitializeViewport(hwnd,width,height),"mesh D3D initialization failed");
    game.SetSimulationEnabled(false);game.GetSettings().SetVSync(false);
    const int mode=Integer(in,"rendering",0,0,1);
    game.GetSettings().SetRenderingType(static_cast<RenderType>(mode));
    game.SetDeferredDebugOverlay(false);game.SetShadowSettings(false,30);
    const auto eye=Vec(in["camera"],{0,3,0}),target=Vec(in["target"],{}),up=Vec(in["up"],{0,0,1});
    Check(glm::length(target-eye)>.01f && glm::length(glm::cross(target-eye,up))>.001f,"invalid camera basis");
    auto* camera=game.GetPlayer();camera->SetCameraMode(CameraMode::Free);
    camera->SetPosition(eye);camera->SetViewDirection(glm::normalize(target-eye),up);
    camera->SetProjection(in["orthographic"].toBool(true),60,.1f,100,Number(in,"orthoHeight",4));
    camera->StopRotate(false,false);camera->EnableWASD(false);
    MeshLodData geometry;
    const auto shape=in["shape"].toString("plane");
    const float size=Number(in,"size",2);Check(size>0 && size<=100,"invalid mesh size");
    if(shape=="plane")geometry=MeshPrimitives::Plane(size,size);
    else if(shape=="cube")geometry=MeshPrimitives::Cube(size);
    else if(shape=="sphere")geometry=MeshPrimitives::Sphere(size*.5f,24,12);
    else throw std::runtime_error("mesh shape must be plane, cube or sphere");
    if(in["mirrorUv"].toBool()) {
        for(auto& vertex:geometry.Vertices)vertex.TexCoord.x=1-vertex.TexCoord.x;
        GenerateTangents(geometry);
    }
    const ComponentType type{"Diagnostics.Mesh","Mesh",[]()->std::unique_ptr<GameComponent>{return std::make_unique<MeshComponent>();}};
    QTemporaryDir saveRoot;
    Check(saveRoot.isValid(),"mesh SaveGame scratch creation failed");
    game.Saves().Configure("mesh-diagnostics",std::filesystem::path(saveRoot.path().toStdWString()));
    game.Saves().SetCatalog({&type,1});
    std::unique_ptr<GameComponent> object=std::make_unique<MeshComponent>();
    auto* mesh=static_cast<MeshComponent*>(object.get());mesh->SetMesh(std::move(geometry));
    Check(game.GetScene().AddComponent("Observed mesh",object,type.typeName).result==SceneResult::Ok,"mesh registration failed");
    std::string error;
    auto material=game.LoadMaterial(path.toUtf8().constData(),&error);
    if(!material)throw std::runtime_error(error);
    Check(!material->HasErrors(),material->GetErrors().c_str());
    mesh->SetMaterial(material);
    const auto original=Basis(mesh->GetMesh());
    QJsonArray results;
    for(const auto value:frames) {
        const auto frame=value.toObject();
        if(frame.contains("scale"))mesh->SetScale(Vec(frame["scale"],{1,1,1}));
        if(frame.contains("rotation"))mesh->SetRotation(Vec(frame["rotation"],{}));
        if(frame.contains("position"))mesh->SetPosition(Vec(frame["position"],{}));
        bool restored=false;
        if(frame["saveRoundTrip"].toBool()) {
            const auto saved=game.Saves().Capture();
            if(frame.contains("saveFile")) {
                QFile file(frame["saveFile"].toString());
                Check(!file.exists() && file.open(QIODevice::WriteOnly),"saveFile must be new and writable");
                Check(file.write(saved.data(),qint64(saved.size()))==qint64(saved.size()),"saveFile write failed");
            }
            game.Saves().Restore(saved);
            mesh=dynamic_cast<MeshComponent*>(game.GetScene().FindComponentByName("Observed mesh"));
            Check(mesh!=nullptr,"SaveGame did not restore mesh");
            // This probe tests geometry/basis restoration. A directly assigned
            // transient Material is rebound explicitly; asset persistence is a separate test.
            mesh->SetMaterial(material);restored=true;
        }
        const auto image=frame["image"].toString();
        Check(QFileInfo(image).isAbsolute() && !QFileInfo::exists(image) && QFileInfo(image).dir().exists(),"mesh image must be a new absolute file");
        game.RequestFrameDump(image.toUtf8().toStdString());Check(game.RenderViewportFrame(0),"mesh render failed");
        QImage pixels(image);Check(!pixels.isNull(),"mesh image readback failed");
        const auto center=pixels.pixelColor(pixels.width()/2,pixels.height()/2);
        QFile file(image);Check(file.open(QIODevice::ReadOnly),"mesh image cannot be read");
        results.append(QJsonObject{{"image",image},{"center",QJsonArray{center.red(),center.green(),center.blue()}},
            {"sha256",QString::fromLatin1(QCryptographicHash::hash(file.readAll(),QCryptographicHash::Sha256).toHex())},
            {"scale",Array(mesh->GetScale())},{"basis",Basis(mesh->GetMesh())},
            {"restored",restored},{"transientMaterialRebound",restored}});
    }
    return {{"adapter",QString::fromStdString(game.GetAdapterName())},{"originalBasis",original},{"frames",results}};
}
}
