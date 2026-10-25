#include <Engine/MainGame/Player.h>
#include "../../Editor/Include/CameraKeys.h"
#define NOMINMAX
#include "EditorPreview.h"
#include "PreviewScenes.h"
#include <Engine/Runtime/ViewportSession.h>
#include <Engine/Components/SpecificComponents/CubeComponent.h>
#include <Engine/Components/Light/PointLightComponent.h>
#include <Engine/Resources/Paths.h>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

#define CHECK(value) do { if (!(value)) throw std::runtime_error(#value); } while (false)

// Separate hardware/window integration test; the ordinary WARP suite stays headless.
class PreviewTest final : public BasicExamplesPreview
{
public:
    uint64_t checksum = 0;
    size_t coloredPixels = 0;
    std::vector<uint32_t> pixels;
    int simulationTicks = 0;
    void PreUpdate(float dt) override
    {
        ++simulationTicks;
        BasicExamplesPreview::PreUpdate(dt);
    }
    void Draw() override
    {
        OrderedSceneGame::Draw();
        D3D11_TEXTURE2D_DESC desc{};
        BackTexture->GetDesc(&desc);
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        desc.MiscFlags = 0;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> readback;
        CHECK(SUCCEEDED(Device->CreateTexture2D(&desc, nullptr, &readback)));
        Context->CopyResource(readback.Get(), BackTexture.Get());
        D3D11_MAPPED_SUBRESOURCE mapped{};
        CHECK(SUCCEEDED(Context->Map(readback.Get(), 0, D3D11_MAP_READ, 0, &mapped)));
        checksum = 0;
        coloredPixels = 0;
        pixels.resize(static_cast<size_t>(desc.Width) * desc.Height);
        for (UINT y = 0; y < desc.Height; ++y)
        {
            const auto* row = reinterpret_cast<const uint32_t*>(static_cast<const char*>(mapped.pData) + y * mapped.RowPitch);
            for (UINT x = 0; x < desc.Width; ++x)
            {
                const uint32_t rgb = row[x] & 0x00ffffff;
                coloredPixels += rgb != 0;
                checksum = checksum * 31 + rgb;
                // D3D RGBA -> BMP BGRA.
                pixels[y * desc.Width + x] = (row[x] & 0xff00ff00) | ((row[x] & 255) << 16) | ((row[x] >> 16) & 255);
            }
        }
        Context->Unmap(readback.Get(), 0);
    }
    void Save(const char* path)
    {
        BITMAPFILEHEADER file{};
        BITMAPINFOHEADER info{};
        info.biSize = sizeof(info);
        info.biWidth = GetDisplay()->GetWidth();
        info.biHeight = -GetDisplay()->GetHeight();
        info.biPlanes = 1;
        info.biBitCount = 32;
        info.biSizeImage = static_cast<DWORD>(pixels.size() * 4);
        file.bfType = 0x4d42;
        file.bfOffBits = sizeof(file) + sizeof(info);
        file.bfSize = file.bfOffBits + info.biSizeImage;
        std::ofstream output(path, std::ios::binary);
        output.write(reinterpret_cast<const char*>(&file), sizeof(file));
        output.write(reinterpret_cast<const char*>(&info), sizeof(info));
        output.write(reinterpret_cast<const char*>(pixels.data()), info.biSizeImage);
        CHECK(output.good());
    }
};

int main(int argc, char** argv)
{
    // Russian layout must move the same camera as English physical WASD/QE.
    CHECK(CameraNavigationKey(0x11, 0x426) == 'W');
    CHECK(CameraNavigationKey(0x1e, 0x424) == 'A');
    CHECK(CameraNavigationKey(0x1f, 0x42b) == 'S');
    CHECK(CameraNavigationKey(0x20, 0x412) == 'D');
    CHECK(CameraNavigationKey(0x10, 0x419) == 'Q');
    CHECK(CameraNavigationKey(0x12, 0x423) == 'E');
    CHECK(CameraNavigationKey(0, 'W') == 'W');
    CHECK(CameraNavigationKey(0x19, 0x417) == 'P');
    HWND window = CreateWindowExW(0, L"STATIC", L"Viewport test", WS_OVERLAPPEDWINDOW,
        0, 0, 640, 480, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    try
    {
        CHECK(window);
        SetWindowLongPtrW(window, GWLP_USERDATA, 12345);
        const auto procedure = GetWindowLongPtrW(window, GWLP_WNDPROC);
        {
            bool failPlay = false;
            class FailingPreview : public BasicExamplesPreview
            {
                void AfterInitialize() override
                {
                    BasicExamplesPreview::AfterInitialize();
                    throw std::runtime_error("Injected Play initialization failure");
                }
            };
            ViewportSession session([&]() -> std::unique_ptr<Game> {
                if (failPlay) return std::make_unique<FailingPreview>();
                return std::make_unique<PreviewTest>();
            });
            CHECK(session.Attach(window, 640, 480));
            auto* edit = static_cast<PreviewTest*>(session.Active());
            CHECK(edit->RenderViewportFrame(1.0f / 60));
            const auto originalPixels = edit->checksum;
            for (int i = 0; i < 4; ++i) CHECK(edit->RenderViewportFrame(1.0f / 60));
            CHECK(edit->simulationTicks == 0 && edit->checksum == originalPixels);
            session.TogglePause();
            CHECK(session.GetState() == ViewportSession::State::Edit);
            for (int pass = 0; pass < 2; ++pass)
            {
                const auto cameraPosition = edit->GetPlayer()->GetPosition();
                CHECK(session.TogglePlay());
                auto* play = static_cast<PreviewTest*>(session.Active());
                CHECK(play != edit && play->GetDevice() == edit->GetDevice());
                CHECK(play->GetScene().GetId() != edit->GetScene().GetId());
                for (int i = 0; i < 8; ++i) CHECK(play->RenderViewportFrame(1.0f / 60));
                CHECK(play->simulationTicks == 8 && play->checksum != originalPixels);
                session.TogglePause();
                const auto pausedPixels = play->checksum;
                const auto pausedRotation = play->FindComponent("1")->GetRotation();
                for (int i = 0; i < 4; ++i) CHECK(play->RenderViewportFrame(1.0f / 60));
                CHECK(session.Active() == play && play->simulationTicks == 8);
                CHECK(play->checksum == pausedPixels);
                session.Resize(801, 451);
                CHECK(play->RenderViewportFrame(1.0f / 60));
                CHECK(play->GetDisplay()->GetWidth() == 801);
                CHECK(play->FindComponent("1")->GetRotation() == pausedRotation);
                session.TogglePause();
                CHECK(play->RenderViewportFrame(1.0f / 60));
                CHECK(play->simulationTicks == 9);
                play->GetPlayer()->MoveEditorCamera(0.2f, 0, 0, 0, 0, 0);
                if (pass == 1) session.TogglePause(); // Stop must work from Pause too.
                CHECK(session.TogglePlay());
                CHECK(session.Active() == edit && edit->simulationTicks == 0);
                CHECK(edit->GetPlayer()->GetPosition() == cameraPosition);
                CHECK(edit->RenderViewportFrame(1.0f / 60));
                CHECK(edit->GetDisplay()->GetWidth() == 801);
                session.Resize(640, 480);
                CHECK(edit->RenderViewportFrame(1.0f / 60));
                CHECK(edit->checksum == originalPixels);
            }
            failPlay = true;
            bool rejected = false;
            try { session.TogglePlay(); } catch (const std::exception&) { rejected = true; }
            CHECK(rejected && session.Active() == edit);
            CHECK(session.GetState() == ViewportSession::State::Edit);
            CHECK(edit->RenderViewportFrame(1.0f / 60));
            CHECK(edit->checksum == originalPixels);
            std::cout << "PASS: static Edit, isolated Play, Pause/Resume, Stop, resize, failed Play rollback\n";
        }
        {
            PreviewTest game;
            game.SetSimulationEnabled(false);
            CHECK(game.InitializeViewport(window, 640, 480));
            game.SetVSync(false);
            CHECK(game.RenderViewportFrame(0.016f));
            const auto originalPixels = game.checksum;
            auto& scene = game.GetScene();
            const auto original = scene.GetSnapshot().components.front();
            auto moved = original.transform;
            moved.position.y += 0.5f;
            CHECK(scene.SetTransform(original.id, moved) == SceneResult::Ok);
            CHECK(game.RenderViewportFrame(0.016f) && game.checksum != originalPixels);
            CHECK(game.simulationTicks == 0);
            CHECK(scene.SetTransform(original.id, original.transform) == SceneResult::Ok);
            CHECK(game.RenderViewportFrame(0.016f) && game.checksum == originalPixels);

            auto cube = std::make_unique<CubeComponent>();
            cube->InitCube();
            std::unique_ptr<GameComponent> component = std::move(cube);
            auto* raw = component.get();
            const auto added = scene.AddComponent("3", component);
            CHECK(added.result == SceneResult::Ok && !raw->GetVertexBuffer());
            CHECK(game.RenderViewportFrame(0.016f));
            CHECK(raw->GetGame() == &game && raw->GetVertexBuffer());
            CHECK(game.GetFrameStats().DrawCalls == 3);
            CHECK(scene.RemoveComponent(added.id) == SceneResult::Ok);
            CHECK(game.RenderViewportFrame(0.016f) && game.checksum == originalPixels);
            std::unique_ptr<GameComponent> light = std::make_unique<PointLightComponent>(glm::vec3(0), glm::vec3(1));
            const auto lightId = scene.AddComponent("Light", light).id;
            CHECK(game.GetPointLights().size() == 1);
            CHECK(scene.RemoveComponent(lightId) == SceneResult::Ok);
            CHECK(game.GetPointLights().empty());
            std::cout << "PASS: Scene transforms render without Tick, lazy GPU registration, light removal\n";
        }
        for (int pass = 0; pass < 2; ++pass)
        {
            PreviewTest scene;
            CHECK(!scene.InitializeViewport(nullptr, 640, 480));
            CHECK(scene.InitializeViewport(window, 640, 480));
            CHECK(scene.GetInputDevice() == nullptr);
            scene.SetVSync(false);
            CHECK(scene.RenderViewportFrame(1.0f / 60));
            CHECK(scene.GetFrameStats().DrawCalls == 2);
            CHECK(scene.coloredPixels > 1000);
            const auto first = scene.checksum;
            for (int i = 0; i < 15; ++i) CHECK(scene.RenderViewportFrame(1.0f / 60));
            CHECK(scene.checksum != first);
            scene.OnWindowResized(0, 0, true);
            const auto frame = scene.GetFrameIndex();
            CHECK(scene.RenderViewportFrame(1.0f / 60));
            CHECK(scene.GetFrameIndex() == frame);
            scene.OnWindowResized(801, 451, false);
            CHECK(scene.RenderViewportFrame(1.0f / 60));
            CHECK(scene.GetDisplay()->GetWidth() == 801 && scene.GetDisplay()->GetHeight() == 451);
            CHECK(scene.pixels.size() == 801 * 451 && scene.coloredPixels > 1000);
            if (argc > 1) scene.Save(argv[1]);
        }
        CHECK(IsWindow(window));
        CHECK(GetWindowLongPtrW(window, GWLP_USERDATA) == 12345);
        CHECK(GetWindowLongPtrW(window, GWLP_WNDPROC) == procedure);
        for (const char* name : {"BasicExamples", "Pong", "SunGame", "Battle"})
        {
            auto factory = FindPreviewScene(name, EnginePaths::ExecutableDirectory());
            ViewportSession session(factory);
            CHECK(session.Attach(window, 640, 480));
            auto* scene = session.Active();
            for (int frame = 0; frame < 3; ++frame) CHECK(scene->RenderViewportFrame(1.0f / 60));
            CHECK(scene->GetFrameStats().DrawCalls > 0);
            CHECK(scene->GetFrameIndex() == 3);
            auto* camera = scene->GetPlayer();
            const auto initialPosition = camera->GetPosition();
            const float initialYaw = camera->GetYaw();
            camera->MoveEditorCamera(0.1f, 0.05f, 0.1f, 0.15f, 0.1f, 1.0f);
            CHECK(scene->RenderViewportFrame(1.0f / 60));
            CHECK(glm::length(camera->GetPosition() - initialPosition) > 0.01f);
            CHECK(std::abs(camera->GetYaw() - initialYaw) > 0.01f);
            const auto stoppedPosition = camera->GetPosition();
            CHECK(scene->RenderViewportFrame(1.0f / 60));
            CHECK(glm::length(camera->GetPosition() - stoppedPosition) < 0.001f);

            scene->OnWindowResized(800, 600, false);
            CHECK(scene->RenderViewportFrame(1.0f / 60));
            CHECK(scene->GetDisplay()->GetWidth() == 800);
            CHECK(session.TogglePlay());
            for (int frame = 0; frame < 3; ++frame) CHECK(session.Active()->RenderViewportFrame(1.0f / 60));
            session.TogglePause();
            CHECK(session.Active()->RenderViewportFrame(1.0f / 60));
            CHECK(session.TogglePlay());
            CHECK(session.Active() == scene);
            CHECK(scene->RenderViewportFrame(1.0f / 60));
            std::cout << "PASS: preview factory/render/resize " << name << '\n';
        }
        CHECK(!FindPreviewScene("", {}));
        bool unknownRejected = false;
        try { FindPreviewScene("UnknownScene", {}); }
        catch (const std::exception&) { unknownRejected = true; }
        CHECK(unknownRejected);
        std::cout << "PASS: animated BasicExamples pixels, resize, minimize, reattach, borrowed HWND ownership\n";
        DestroyWindow(window);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        if (window) DestroyWindow(window);
        return 1;
    }
}
