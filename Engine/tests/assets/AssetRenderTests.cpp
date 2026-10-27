#define NOMINMAX
#include <Windows.h>
#include <Engine/MainGame/BaseGameClass/Game.h>
#include <Engine/Components/SpecificComponents/CubeComponent.h>
#include <Engine/Components/SpecificComponents/MeshComponent.h>
#include <Engine/Components/SpecificComponents/SkeletalMeshComponent.h>
#include <Engine/Render/MeshPrimitives.h>
#include <Engine/Scene/SceneAssets.h>
#include <Engine/Resources/Paths.h>
#include "../../Source/Resources/VfsRead.h"

void Check(bool, const char*);
namespace
{
class CaptureGame : public Game
{
public:
    size_t greenPixels = 0;
    void Draw() override
    {
        Game::Draw();
        D3D11_TEXTURE2D_DESC desc{}; BackTexture->GetDesc(&desc);
        desc.Usage = D3D11_USAGE_STAGING; desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ; desc.MiscFlags = 0;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> staging;
        Check(SUCCEEDED(Device->CreateTexture2D(&desc, nullptr, &staging)), "create pixel readback");
        Context->CopyResource(staging.Get(), BackTexture.Get());
        D3D11_MAPPED_SUBRESOURCE mapped{};
        Check(SUCCEEDED(Context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)), "map pixel readback");
        greenPixels = 0;
        for (unsigned y = 0; y < desc.Height; ++y)
        {
            const auto* row = reinterpret_cast<const uint8_t*>(mapped.pData) + y * mapped.RowPitch;
            for (unsigned x = 0; x < desc.Width; ++x)
            {
                const auto* pixel = row + x * 4;
                if (pixel[1] > 20 && pixel[1] > pixel[0] * 2 && pixel[1] > pixel[2] * 2) ++greenPixels;
            }
        }
        Context->Unmap(staging.Get(),0);
    }
};
}
void RunAssetRenderTests(HWND hwnd, const std::filesystem::path& engineContent,
    const std::filesystem::path& content, const std::string& texture, const std::string& modelPath)
{
    EnginePaths::SetContentRoot(engineContent);
    VfsRead::GetVFS().Mount("/Game",content);
    CaptureGame game;
    game.SetRenderingType(Custom);
    game.SetSimulationEnabled(false); game.SetVSync(false);
    Check(game.InitializeViewport(hwnd,640,480,true), "initialize asset pixel test");
    game.GetPlayer()->SetPosition({0,0,5});
    auto cube = std::make_unique<CubeComponent>(); cube->InitCube(); cube->SetColor({1,1,1,1});
    std::unique_ptr<GameComponent> object = std::move(cube);
    auto added = game.GetScene().AddComponent("cube",object,"Primitive.Cube");
    Check(added.result == SceneResult::Ok, "add pixel-test cube");
    Check(game.RenderViewportFrame(0), "render untextured cube"); const auto before = game.greenPixels;
    Check(game.GetScene().SetTextureAsset(added.id,texture) == SceneResult::Ok, "assign GPU texture");
    Check(game.RenderViewportFrame(0) && game.greenPixels > before + 100, "texture changes actual primitive pixels");
    Check(game.GetScene().SetTextureAsset(added.id,"") == SceneResult::Ok, "clear GPU texture");
    Check(game.RenderViewportFrame(0) && game.greenPixels == before, "clearing restores untextured pixels");

    // Scene primitives are meshes drawn by the default material, which shows the object's texture.
    game.GetScene().RemoveComponent(added.id);
    auto meshCube = std::make_unique<MeshComponent>(); meshCube->SetMesh(MeshPrimitives::Cube()); meshCube->SetColor({1,1,1,1});
    object = std::move(meshCube);
    added = game.GetScene().AddComponent("mesh cube",object,"Primitive.Cube");
    Check(added.result == SceneResult::Ok && game.RenderViewportFrame(0), "render mesh cube");
    const auto meshBefore = game.greenPixels;
    Check(game.GetScene().SetTextureAsset(added.id,texture) == SceneResult::Ok && game.RenderViewportFrame(0) &&
        game.greenPixels > meshBefore + 100, "the default material shows the texture of a mesh");
    Check(game.GetScene().SetTextureAsset(added.id,"") == SceneResult::Ok, "clear mesh texture");
#if defined(ENGINE_ENABLE_ASSIMP)
    game.GetScene().RemoveComponent(added.id);
    object = SceneAssets::CreateModel(modelPath);
    auto* mesh = dynamic_cast<SkeletalMeshComponent*>(object.get());
    Check(mesh && mesh->GetModel()->TotalIndexCount == 6 && mesh->GetLocalBoundingRadius() > 3,
        "static import preserves both mesh nodes and their transforms");
    float distance = 100;
    Check(mesh->RaycastPrimitive({0,0,5},{0,0,-1},distance), "imported mesh supports triangle picking");
    SceneAssets::Apply(*object,modelPath,texture);
    added = game.GetScene().AddComponent("mesh",object,SceneAssets::ModelType);
    Check(added.result == SceneResult::Ok && game.RenderViewportFrame(0), "draw imported mesh");
    Check(game.GetScene().FindComponent(added.id)->HasTexture() && game.greenPixels > 100,
        "static model shader renders real textured pixels without bones");
#endif
}
