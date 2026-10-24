#include <Engine/MainGame/Player.h>
#include "../../Editor/Include/CameraKeys.h"
#define NOMINMAX
#include "EditorPreview.h"
#include "PreviewScenes.h"
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
    HWND window = CreateWindowExW(0, L"STATIC", L"Viewport test", WS_OVERLAPPEDWINDOW,
        0, 0, 640, 480, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    try
    {
        CHECK(window);
        SetWindowLongPtrW(window, GWLP_USERDATA, 12345);
        const auto procedure = GetWindowLongPtrW(window, GWLP_WNDPROC);
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
            auto scene = factory();
            CHECK(scene->InitializeViewport(window, 640, 480));
            scene->SetVSync(false);
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
