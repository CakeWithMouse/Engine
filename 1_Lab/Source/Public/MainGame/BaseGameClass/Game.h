#pragma once
#include <Windows.h>
#include <cstddef>
#include <d3d11.h>
#include <dxgi.h>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <wrl/client.h>
#include "../../../includes/GLM-master/glm/vec3.hpp"

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

constexpr int MaxShadowCascades = 3;

/**
 * Класс отвечающий за само приложение
 */
class GameComponent;
class PointLightComponent;
class FBXComponent;
struct CubeMapResource;

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

struct ShadowPassBufferData
{
    DirectX::XMFLOAT4X4 worldMatrix;
    DirectX::XMFLOAT4X4 lightViewProjection;
};

struct DeferredLightingBufferData
{
    DirectX::XMFLOAT4X4 worldMatrix;
    DirectX::XMFLOAT4X4 viewMatrix;
    DirectX::XMFLOAT4X4 projectionMatrix;
    DirectX::XMFLOAT4X4 invViewMatrix;
    DirectX::XMFLOAT4X4 invProjectionMatrix;
    DirectX::XMFLOAT4 ObjectColor;
    DirectX::XMFLOAT2 UVOffset;
    float HasTexture = 0.0f;
    float padding = 0.0f;
    DirectX::XMFLOAT4 CameraPosition;
    DirectX::XMFLOAT4 LightPositions[8];
    DirectX::XMFLOAT4 LightColors[8];
    DirectX::XMFLOAT4 LightParams[8];
    DirectX::XMFLOAT4 LightMeta;
    DirectX::XMFLOAT4 ReflectionData;
    DirectX::XMFLOAT4X4 LightViewProjection[MaxShadowCascades];
    DirectX::XMFLOAT4 CascadeSplits;
    DirectX::XMFLOAT4 ShadowParams;
    DirectX::XMFLOAT4 LightDirection;
    DirectX::XMFLOAT4 DirectionalLightColorIntensity;
};

class Game
{
public:
    Game() = default;
    ~Game();
    
    void Initialize();
    void StartGame();
    void SetGameType(GameType gameType) { GameType = gameType; };
    void SetRenderingType(RenderType NewType){RenderingType = NewType;}
    void SetShadowSettings(bool enabled, float shadowDistance);
    void SetDirectionalLight(const glm::vec3& direction, const glm::vec3& color, float intensity);
    void SetDirectionalLightDirection(const glm::vec3& direction);
    void SetDirectionalLightColor(const glm::vec3& color);
    void SetDirectionalLightIntensity(float intensity);
    virtual void AfterInitialize(){};
    
    bool IsShadowEnabled() const { return bShadowsEnabled; }
    bool RegisterComponent(std::string Name, GameComponent* GameComponent,std::string PShaderName = {}, std::string VShaderName= {});
    bool CreateProceduralCubeMap(const std::string& cubeMapName, CubeMapPreset preset, int faceSize = 256);
    bool CreateCubeMapFromFiles(const std::string& cubeMapName, const std::vector<std::string>& facePaths);
    
    Player* GetPlayer(){return FirstPlayer;}
    
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
    
    InputDevice* GetInputDevice() const { return InputDevicePtr; }
    ID3D11DeviceContext* GetContext() const { return Context; }
    
    CubeMapResource* GetCubeMap(const std::string& cubeMapName) const;
    std::vector<PointLightInfo> GetPointLights(size_t maxLights = 8) const;

protected:
    //Init phase
    void InitSwapChainDesc(const RECT& WindowRect);
    void CreateBlendStates();
    void CreateDepthBuffer(Microsoft::WRL::ComPtr<ID3D11Device> device, int width, int height);
    void UpdateShadowCascades();
    void RenderShadowMaps();
    bool InitShadowResources();
    HRESULT InitRenderTarget();
    HRESULT CreateDeviceAndSwapChain();
    HRESULT InitShaderBuffers();
    static HRESULT CreateShader(HWND hWnd,CONST D3D_SHADER_MACRO* pDefines, LPCWSTR FileName, LPCSTR pEntrypoint,
                                LPCSTR pTarget,
                                ID3DBlob** Buffer);
    ID3D11DepthStencilView* GetDepthStencilView() { return depthStencilView.Get(); }
    
    
    
    //PrepareData
    void CreateBackBuffer();
    void PrepareFrame();
    void PrepareResources();
    void RegisterShaders(std::string ShaderName, std::string AdditionalAttributeToName,
                         ShaderCompileVariant variant = ShaderCompileVariant::Default);
    bool InitDeferredResources();
    void UpdateDeferredLightingBuffer();
    void RenderDeferredLightingPass(ID3D11RasterizerState* RasterState);
    void RenderDeferredDebugOverlay(ID3D11RasterizerState* RasterState);
    bool ShouldUseDeferredGeometryVariant(GameComponent* Component) const;
    
    //FreeData
    void DestroyResources(){};

    //Loop
    virtual void Draw(ID3D11RasterizerState* RasterState){};
    virtual void DrawForward(ID3D11RasterizerState* RasterState){};
    virtual void DrawDeffered(ID3D11RasterizerState* RasterState){};
    
    void Update(float deltaTime);
    void UpdateInternal();
    void EndFrame();
    void RestoreTargets();
    void Run();

    // Обработки всякого
    void ResizeScreen();
    void ProcessInput(float deltaTime);
    void MessageHandler();
    void Exit();
    
public:
    ID3D11VertexShader* GetVertexShader() { return BaseVertexShader; }
    ID3D11VertexShader* GetVertexShader(const std::string& VertexShaderName,
                                        ShaderCompileVariant variant = ShaderCompileVariant::Default);
    ID3D11PixelShader* GetPixelShader() { return BasePixelShader; }
    ID3D11PixelShader* GetPixelShader(const std::string& PixelShaderName,
                                      ShaderCompileVariant variant = ShaderCompileVariant::Default);
    
    Display* GetDisplay() const { return DisplayPtr; };

protected:
    GameType GameType;
    
    float TotalTime = 0;
    unsigned long long FrameCount = 0;
    int Signed = 0;

    DXGI_SWAP_CHAIN_DESC SwapChainDescription{};
    Microsoft::WRL::ComPtr<ID3D11Device> Device{nullptr};

    ID3D11DeviceContext* Context{nullptr};
    IDXGISwapChain* SwapChain{nullptr};
    ID3D11Texture2D* BackTexture{nullptr};
    ID3D11RenderTargetView* RenderTargetView{nullptr};
    ID3D11VertexShader* BaseVertexShader{nullptr};
    ID3D11PixelShader* BasePixelShader{nullptr};
    ID3D11InputLayout* layout{nullptr};

    Player* FirstPlayer;
    Display* DisplayPtr{nullptr};
    InputDevice* InputDevicePtr{nullptr};

    //Special buffers
    ID3DBlob* pixelBC{nullptr};
    ID3DBlob* vertexBC{nullptr};

    //ToDo: Add public methods to create it
    std::map<std::string, GameComponent*> Components;
    std::vector<PointLightComponent*> PointLights;
    std::map<std::string, std::unique_ptr<CubeMapResource>> cubeMapCache;

    //ToDO: add check when create in another place
    std::map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> shaderCache;
    std::map<std::string, ID3D11VertexShader*> vertexShaderCache;
    std::map<std::string, ID3D11PixelShader*> pixelShaderCache;
    
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthStencilState;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthStencilView;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> depthStencilSRV;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> depthStencilBuffer;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> gBufferAlbedoTexture;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> gBufferAlbedoRTV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> gBufferAlbedoSRV;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> gBufferNormalTexture;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> gBufferNormalRTV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> gBufferNormalSRV;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> gBufferMaterialTexture;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> gBufferMaterialRTV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> gBufferMaterialSRV;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> gBufferSamplerState;
    Microsoft::WRL::ComPtr<ID3D11Buffer> deferredLightingCB;
    int deferredBufferWidth = 0;
    int deferredBufferHeight = 0;

    bool bShadowsEnabled = false;
    RenderType RenderingType = Forward;
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
    Microsoft::WRL::ComPtr<ID3DBlob> shadowVertexShaderBlob;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> shadowInputLayoutPrimitive;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> shadowInputLayoutMesh;
    Microsoft::WRL::ComPtr<ID3D11Buffer> shadowPassCB;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> shadowRasterState;
    
    Microsoft::WRL::ComPtr<ID3D11BlendState> transparentBlendState;
    Microsoft::WRL::ComPtr<ID3D11BlendState> opaqueBlendState;

private:
};
