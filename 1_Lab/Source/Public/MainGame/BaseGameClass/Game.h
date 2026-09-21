#pragma once
#include <Windows.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <d3d11.h>
#include <dxgi.h>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <wrl/client.h>
#include "../../../includes/GLM-master/glm/vec3.hpp"

#include "../../Render/ShaderConstants.h"
#include "../Player.h"
#include "../../Display/Display.h"
#include "../../InputDevice/InputDevice.h"

enum GameType
{
    FirstLabTriangles,
    FirstLabCubes,
    SecondLab,
    ThirdLab,
    ForthLab,
};

enum RenderType
{
    Forward,
    Deffered,
    Custom
};

enum class ShaderCompileVariant
{
    Default,
    DeferredGBuffer,
    DeferredLighting
};

enum class VertexFormat
{
    Primitive, // POSITION float4 + COLOR float4, stride 32
    Mesh       // POSITION float4 + NORMAL float4 + TEXCOORD float2 (padded to float4), stride 48
};

enum class RegisterResult
{
    Ok,
    NullComponent,
    EmptyName,
    DuplicateName,
    DeviceNotReady
};

const char* ToString(RegisterResult result);

class GameComponent;
class PointLightComponent;
class FBXComponent;
struct ComponentShaderVariant;

enum class CubeMapPreset
{
    Space,
    Studio
};

struct CubeMapResource
{
    std::string Name;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> Texture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ShaderResourceView;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> SamplerState;
};

struct PointLightInfo
{
    glm::vec3 Position{0.0f, 0.0f, 0.0f};
    float Intensity{1.0f};
    glm::vec3 Color{1.0f, 1.0f, 1.0f};
    float Radius{50.0f};
    bool bEnabled{true};
};

/** Everything a component needs to draw itself into one shadow cascade. */
struct ShadowPassContext
{
    ID3D11VertexShader* VertexShader = nullptr;
    ID3D11VertexShader* InstancedVertexShader = nullptr;
    ID3D11Buffer* ConstantBuffer = nullptr;
    ID3D11InputLayout* PrimitiveLayout = nullptr;
    ID3D11InputLayout* MeshLayout = nullptr;
    DirectX::XMFLOAT4X4 LightViewProjection{};
};

/** Per-frame counters shown in the window title. */
struct FrameStats
{
    uint32_t DrawCalls = 0;
    uint32_t InstancesDrawn = 0;
    uint32_t Dispatches = 0;
    uint32_t VisibleObjects = 0;
    uint32_t CulledObjects = 0;
    uint64_t IndicesDrawn = 0;

    void Reset() { *this = FrameStats{}; }
};

/**
 * The application: window, D3D11 device, scene registry and the frame loop.
 *
 * Frame contract (Run):
 *   messages -> resize at frame boundary -> UpdateFrame(real dt)
 *     [PreUpdate -> camera -> TickComponents -> shadow cascades -> frame constants -> object constants]
 *   -> RenderFrame (passes only draw, they never tick or update) -> Present -> input end of frame.
 */
class Game
{
public:
    Game();
    virtual ~Game();
    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    /** Creates window, device and base resources. Returns false (and releases everything) on failure. */
    bool Initialize();
    /** Runs the frame loop until quit. Returns the process exit code. */
    int StartGame();

    void SetGameType(GameType gameType) { CurrentGameType = gameType; }
    // Shader variants are resolved per pass, so the render type may be switched at any time.
    void SetRenderingType(RenderType NewType) { RenderingType = NewType; }
    RenderType GetRenderingType() const { return RenderingType; }
    void SetShadowSettings(bool enabled, float shadowDistance);
    void SetDirectionalLight(const glm::vec3& direction, const glm::vec3& color, float intensity);
    void SetDirectionalLightDirection(const glm::vec3& direction);
    void SetDirectionalLightColor(const glm::vec3& color);
    void SetDirectionalLightIntensity(float intensity);
    void SetVSync(bool enabled) { bVSync = enabled; }
    void SetDeferredDebugOverlay(bool enabled) { bShowDeferredDebugOverlay = enabled; }
    /**
     * Scene time = real time * scale. The default reproduces the speeds that the scenes were tuned
     * for (the old code advanced 0.13 time units per frame, i.e. 7.8 units per second at 60 FPS).
     */
    void SetSimulationTimeScale(float scale) { SimulationTimeScale = scale > 0.0f ? scale : 0.0f; }
    virtual void AfterInitialize() {}

    bool IsShadowEnabled() const { return bShadowsEnabled; }
    /** Shadows requested and shadow resources are ready. */
    bool AreShadowsActive() const { return bShadowsEnabled && bShadowResourcesReady; }

    /**
     * Adds a component to the scene. On Ok the scene takes ownership of the pointer.
     * On any other result nothing is changed and ownership stays with the caller.
     */
    RegisterResult RegisterComponent(const std::string& Name, GameComponent* Component,
                                     const std::string& PShaderName = {}, const std::string& VShaderName = {});
    /** Removes and destroys a component. Children lose their parent, lights leave the light list. */
    bool UnregisterComponent(const std::string& Name);
    GameComponent* FindComponent(const std::string& Name) const;

    bool CreateProceduralCubeMap(const std::string& cubeMapName, CubeMapPreset preset, int faceSize = 256);
    bool CreateCubeMapFromFiles(const std::string& cubeMapName, const std::vector<std::string>& facePaths);
    CubeMapResource* GetCubeMap(const std::string& cubeMapName) const;

    Player* GetPlayer() const { return FirstPlayer.get(); }
    Display* GetDisplay() const { return DisplayPtr.get(); }
    InputDevice* GetInputDevice() const { return InputDevicePtr.get(); }
    ID3D11Device* GetDevice() const { return Device.Get(); }
    ID3D11DeviceContext* GetContext() const { return Context.Get(); }
    ID3D11InputLayout* GetInputLayout(VertexFormat format) const;

    ID3D11ShaderResourceView* GetShadowMapSRV() const { return shadowSRV.Get(); }
    ID3D11SamplerState* GetShadowSampler() const { return shadowSampler.Get(); }
    ID3D11ShaderResourceView* GetDepthStencilSRV() const { return depthStencilSRV.Get(); }

    const DirectX::XMFLOAT4X4& GetViewMatrix() const { return FirstPlayer->GetViewMatrix(); }
    const DirectX::XMFLOAT4X4& GetProjectionMatrix() const { return FirstPlayer->GetProjectionMatrix(); }
    const DirectX::XMFLOAT4X4* GetShadowMatrices() const { return CascadeLightViewProjection; }
    const DirectX::XMFLOAT4& GetCascadeSplits() const { return CascadeSplits; }
    const DirectX::XMFLOAT4& GetShadowParams() const { return ShadowParams; }
    const DirectX::XMFLOAT4& GetDirectionalLightDirection() const { return DirectionalLightDirection; }
    const DirectX::XMFLOAT4& GetDirectionalLightColorIntensity() const { return DirectionalLightColorIntensity; }

    /** Snapshot of camera/light/shadow data for the current frame. */
    const FrameConstants& GetFrameConstants() const { return CurrentFrame; }
    uint64_t GetFrameIndex() const { return FrameIndex; }
    FrameStats& GetFrameStats() { return Stats; }
    void CountDraw(uint32_t indexCount, uint32_t instanceCount = 1);
    void CountDispatch(uint32_t dispatchCount = 1) { Stats.Dispatches += dispatchCount; }

    std::vector<PointLightInfo> GetPointLights(size_t maxLights = MaxPointLights) const;

    // Called by the window procedure.
    void OnWindowResized(int clientWidth, int clientHeight, bool minimized);
    void OnFocusLost();

    ID3D11VertexShader* GetVertexShader() { return BaseVertexShader.Get(); }
    ID3D11VertexShader* GetVertexShader(const std::string& VertexShaderName,
                                        ShaderCompileVariant variant = ShaderCompileVariant::Default);
    ID3D11PixelShader* GetPixelShader() { return BasePixelShader.Get(); }
    ID3D11PixelShader* GetPixelShader(const std::string& PixelShaderName,
                                      ShaderCompileVariant variant = ShaderCompileVariant::Default);

    /**
     * Makes the component's shader pair for the given pass current.
     * Returns false if the component cannot be drawn in that pass (e.g. its pixel shader has no
     * G-buffer output, so it must be drawn by the forward pass instead).
     */
    bool BindComponentShaders(GameComponent* Component, ShaderCompileVariant variant);
    /** True if the component's material writes the full G-buffer (3 render targets). */
    bool SupportsDeferredGeometry(GameComponent* Component);

protected:
    // Initialization
    void InitSwapChainDesc(int width, int height);
    bool CreateDeviceAndSwapChain();
    bool CreateSwapChainResources(int width, int height);
    void ReleaseSwapChainResources();
    bool CreateDepthBuffer(int width, int height);
    bool CreateRenderStates();
    bool CreateInputLayouts();
    bool InitShaderBuffers();
    bool InitShadowResources();
    bool InitDeferredResources();
    void DestroyResources();

    // Frame phases
    void Run();
    bool PumpMessages();
    void ApplyPendingResize();
    void UpdateFrame(float realDeltaTime);
    /** Game logic that must run before the camera and the component ticks (scaled dt). */
    virtual void PreUpdate(float deltaTime) {}
    /** Advances components. Default: tick everything, Space pauses everything except the skybox. */
    virtual void TickComponents(float deltaTime);
    void ProcessHotkeys();
    void UpdateShadowCascades();
    void BuildFrameConstants();
    void RenderFrame();
    void EndFrame();
    void UpdateWindowTitle(float realDeltaTime);

    // Render passes (they only submit work, never tick or update the scene)
    virtual void Draw();          // RenderType::Custom
    virtual void DrawForward();   // RenderType::Forward
    virtual void DrawDeffered();  // RenderType::Deffered
    virtual std::array<float, 4> GetClearColor() const { return {0.0f, 0.0f, 0.2f, 1.0f}; }

    void BeginMainPass(bool bUseDepth);
    void SetFullViewport();
    void RenderShadowMaps();
    enum class OpaqueFilter { All, DeferredCapable, ForwardOnly };
    void RenderOpaque(OpaqueFilter filter, ShaderCompileVariant variant);
    void RenderSkybox();
    void DispatchComputePhase();
    void RenderTransparent();
    void RenderDeferredLightingPass();
    void RenderDeferredDebugOverlay();
    void FillFrameConstantBuffer(ConstantBufferData& data) const;
    bool IsVisible(GameComponent* Component);
    ComponentShaderVariant& ResolveComponentShaders(GameComponent* Component, ShaderCompileVariant variant);

    static HRESULT CreateShader(HWND hWnd, const D3D_SHADER_MACRO* pDefines, LPCWSTR FileName, LPCSTR pEntrypoint,
                                LPCSTR pTarget, ID3DBlob** Buffer);
    void RegisterShaders(const std::string& ShaderName, const std::string& AdditionalAttributeToName,
                         ShaderCompileVariant variant = ShaderCompileVariant::Default);

    // GPU frame timer (timestamp queries, read back without stalling)
    void CreateGpuTimer();
    void BeginGpuTimer();
    void EndGpuTimer();

protected:
    GameType CurrentGameType = FirstLabTriangles;
    RenderType RenderingType = Forward;

    float TotalTime = 0.0f;
    float SimulationTimeScale = 0.13f * 60.0f;
    uint64_t FrameIndex = 0;
    bool bInitialized = false;
    bool bRunning = false;
    int ExitCode = 0;

    // Window/resize state
    bool bResizePending = false;
    bool bMinimized = false;
    int PendingWidth = 0;
    int PendingHeight = 0;

    DXGI_SWAP_CHAIN_DESC SwapChainDescription{};
    Microsoft::WRL::ComPtr<ID3D11Device> Device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> Context;
    Microsoft::WRL::ComPtr<IDXGISwapChain> SwapChain;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> BackTexture;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> RenderTargetView;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> BaseVertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> BasePixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> layout;      // VertexFormat::Primitive
    Microsoft::WRL::ComPtr<ID3D11InputLayout> meshLayout;  // VertexFormat::Mesh
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> DefaultRasterState;

    std::unique_ptr<Display> DisplayPtr;
    std::unique_ptr<InputDevice> InputDevicePtr;
    std::unique_ptr<Player> FirstPlayer;

    // Scene: the registry owns components, PointLights only observes them.
    std::map<std::string, std::unique_ptr<GameComponent>> Components;
    std::vector<PointLightComponent*> PointLights;
    std::map<std::string, std::unique_ptr<CubeMapResource>> cubeMapCache;

    // Shader cache, keys: file|stage|variant|policy
    std::map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> shaderCache;
    std::map<std::string, Microsoft::WRL::ComPtr<ID3D11VertexShader>> vertexShaderCache;
    std::map<std::string, Microsoft::WRL::ComPtr<ID3D11PixelShader>> pixelShaderCache;
    std::map<std::string, UINT> pixelShaderTargetCount;

    // Depth
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthStencilState;          // opaque: test + write
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthStencilStateReadOnly;  // transparent: test, no write
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthStencilStateSkybox;    // LESS_EQUAL, no write
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthStencilView;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> depthStencilSRV;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> depthStencilBuffer;

    // G-buffer (created lazily on the first deferred frame)
    Microsoft::WRL::ComPtr<ID3D11Texture2D> gBufferAlbedoTexture;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> gBufferAlbedoRTV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> gBufferAlbedoSRV;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> gBufferNormalTexture;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> gBufferNormalRTV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> gBufferNormalSRV;
    // Third target holds the world position (half float), not material data.
    Microsoft::WRL::ComPtr<ID3D11Texture2D> gBufferWorldPositionTexture;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> gBufferWorldPositionRTV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> gBufferWorldPositionSRV;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> gBufferSamplerState;
    Microsoft::WRL::ComPtr<ID3D11Buffer> deferredLightingCB;
    int deferredBufferWidth = 0;
    int deferredBufferHeight = 0;
    bool bDeferredFailureReported = false;
    bool bShowDeferredDebugOverlay = true;

    // Lighting
    float AmbientIntensity = 0.06f;
    float SpecularShininess = 32.0f;

    // Shadows (created lazily when enabled)
    bool bShadowsEnabled = false;
    bool bShadowResourcesReady = false;
    bool bShadowInitFailed = false;
    float ShadowDistance = 1200.0f;
    int ShadowMapSize = 2048;
    DirectX::XMFLOAT4 CascadeSplits = DirectX::XMFLOAT4(80.0f, 260.0f, 900.0f, 0.0f);
    DirectX::XMFLOAT4 ShadowParams = DirectX::XMFLOAT4(0.0f, static_cast<float>(MaxShadowCascades), 0.0012f, 1.0f / 2048.0f);
    DirectX::XMFLOAT4 DirectionalLightDirection = DirectX::XMFLOAT4(-0.35f, -0.85f, -0.2f, 0.0f);
    DirectX::XMFLOAT4 DirectionalLightColorIntensity = DirectX::XMFLOAT4(0.95f, 0.93f, 0.90f, 0.80f);
    DirectX::XMFLOAT4X4 CascadeLightViewProjection[MaxShadowCascades] = {};
    Microsoft::WRL::ComPtr<ID3D11Texture2D> shadowTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shadowSRV;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> shadowDSVs[MaxShadowCascades];
    Microsoft::WRL::ComPtr<ID3D11SamplerState> shadowSampler;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> shadowVertexShader;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> shadowInstancedVertexShader;
    Microsoft::WRL::ComPtr<ID3DBlob> shadowVertexShaderBlob;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> shadowInputLayoutPrimitive;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> shadowInputLayoutMesh;
    Microsoft::WRL::ComPtr<ID3D11Buffer> shadowPassCB;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> shadowRasterState;

    Microsoft::WRL::ComPtr<ID3D11BlendState> transparentBlendState;
    Microsoft::WRL::ComPtr<ID3D11BlendState> opaqueBlendState;

    // Frame data and diagnostics
    FrameConstants CurrentFrame{};
    FrameStats Stats;
    FrameStats LastFrameStats;
    bool bVSync = true;
    bool bFrustumCulling = true;
    bool bOverlayKeyWasDown = false;
    bool bVSyncKeyWasDown = false;
    bool bRenderTypeKeyWasDown = false;
    float TitleUpdateAccumulator = 0.0f;
    uint32_t TitleFrameCount = 0;
    double CpuFrameMsAccumulator = 0.0;
    float LastGpuFrameMs = 0.0f;

    static constexpr int GpuTimerLatency = 3;
    Microsoft::WRL::ComPtr<ID3D11Query> GpuTimerDisjoint[GpuTimerLatency];
    Microsoft::WRL::ComPtr<ID3D11Query> GpuTimerBegin[GpuTimerLatency];
    Microsoft::WRL::ComPtr<ID3D11Query> GpuTimerEnd[GpuTimerLatency];
    bool GpuTimerIssued[GpuTimerLatency] = {};
    int GpuTimerSlot = 0;
};
