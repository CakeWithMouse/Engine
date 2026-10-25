#pragma once

#include <array>
#include <cstdint>
#include <d3d11.h>
#include <directxmath.h>
#include <string>
#include <vector>
#include <wrl/client.h>

enum class MaterialBlendMode
{
    Opaque,      // writes depth
    Masked,      // opaque, pixels with alpha below AlphaClip are discarded
    Translucent, // alpha blended, sorted back to front, no depth write
    Additive     // colour * alpha added to the scene, no depth write
};

/** Everything that selects the compiled shaders and the pipeline state of a material. */
struct MaterialRenderState
{
    MaterialBlendMode Blend = MaterialBlendMode::Opaque;
    float AlphaClip = 0.5f; // Masked only
    bool bUnlit = false;
    bool bTwoSided = false;
    bool bCastShadows = true;

    bool operator==(const MaterialRenderState& other) const;
    bool operator!=(const MaterialRenderState& other) const { return !(*this == other); }
};

enum class MaterialParameterType
{
    Float,
    Float2,
    Float3,
    Float4,
    Unsupported // listed so the editor can show it; keeps its default bytes and cannot be set
};

/** One variable of `cbuffer MaterialParams : register(b1)`, found by reflection. */
struct MaterialParameter
{
    std::string Name;
    MaterialParameterType Type = MaterialParameterType::Unsupported;
    uint32_t Offset = 0;                // bytes into the constant buffer
    uint32_t Size = 0;                  // bytes
    std::array<float, 4> Default{};     // from the HLSL initializer, zero without one
};

/** One `Texture2D` of the surface at t6 or above, found by reflection. */
struct MaterialTexture
{
    std::string Name;
    uint32_t Slot = 0;
    std::string Path; // empty: a 1x1 white texture is bound
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> View;
};

/**
 * A surface shader (see Shaders/Material/MaterialCommon.hlsli) compiled through the material
 * template for one render state, with its parameter values and textures.
 *
 * A material that does not compile keeps its error text and draws the error surface instead, so
 * a broken material stays visible. Recompiling keeps parameter values and textures whose name
 * (and type) still exist.
 */
class Material
{
public:
    /** surfacePath: UTF-8 path of the surface .hlsl file. */
    explicit Material(std::string surfacePath, MaterialRenderState state = {});

    const std::string& GetSurfacePath() const { return SurfacePath; }
    const MaterialRenderState& GetRenderState() const { return State; }
    /** Takes effect on the next Compile: blend, clip, lighting and sides are compile-time options. */
    void SetRenderState(const MaterialRenderState& state);
    bool NeedsCompile() const { return bNeedsCompile; }
    /** Drawn by the transparent pass (sorted, no depth write). */
    bool IsTransparent() const { return State.Blend == MaterialBlendMode::Translucent || State.Blend == MaterialBlendMode::Additive; }

    /**
     * Compiles the shaders from disk: VS and both pixel variants, the shadow pair of a masked
     * material, and the instanced set once RequireInstancing was called. False if the surface
     * failed (fallback active).
     */
    bool Compile(ID3D11Device* device, ID3D11DeviceContext* context);
    bool IsCompiled() const { return Standard.VertexShader != nullptr; }
    /** Also compile the instanced set (copies read from the instance stream); on the next Compile. */
    void RequireInstancing();
    bool IsInstancingRequired() const { return bInstancingRequired; }
    /** Changes whenever the look may have changed: compile, parameter, texture. */
    uint64_t GetRevision() const { return Revision; }
    /** True while the error surface is drawn instead of this material. */
    bool HasErrors() const { return !Errors.empty(); }
    const std::string& GetErrors() const { return Errors; }
    /** True if the last compilation read this file (the surface or anything it includes). */
    bool DependsOn(const std::string& file) const;

    const std::vector<MaterialParameter>& GetParameters() const { return Parameters; }
    const MaterialParameter* FindParameter(const std::string& name) const;
    /** Components beyond the parameter type are ignored. False for unknown or unsupported names. */
    bool SetParameter(const std::string& name, const std::array<float, 4>& value);
    bool GetParameter(const std::string& name, std::array<float, 4>& value) const;

    const std::vector<MaterialTexture>& GetTextures() const { return Textures; }
    /** Loads the image (empty path: back to white). False for an unknown name or an unreadable file. */
    bool SetTexture(const std::string& name, const std::string& path);

    /** instanced: the set for components drawn with an instance stream (null until compiled). */
    ID3D11VertexShader* GetVertexShader(bool instanced = false) const { return Set(instanced).VertexShader.Get(); }
    /** deferredGBuffer: the shader of the G-buffer pass, null if the material cannot write it. */
    ID3D11PixelShader* GetPixelShader(bool deferredGBuffer, bool instanced = false) const;
    bool WritesGBuffer() const { return Standard.GBufferPixelShader != nullptr; }

    /**
     * Masked materials draw their own shadow (the clip leaves holes); every other material uses
     * the engine's depth shader. Valid while HasShadowShaders().
     */
    bool HasShadowShaders() const { return Standard.ShadowPixelShader != nullptr; }
    ID3D11VertexShader* GetShadowVertexShader(bool instanced = false) const { return Set(instanced).ShadowVertexShader.Get(); }
    ID3D11PixelShader* GetShadowPixelShader(bool instanced = false) const { return Set(instanced).ShadowPixelShader.Get(); }

    /**
     * Shaders that render the surface into an impostor atlas tile (albedo with coverage, object
     * normal). Compiled on first use; false while the material has errors.
     */
    bool GetImpostorBakeShaders(ID3D11VertexShader*& vertexShader, ID3D11PixelShader*& pixelShader);
    /**
     * Camera of a pass that is not the main view (b2, HLSL layout): the light of a shadow map or
     * the view of an atlas tile. Also binds everything Bind sets.
     */
    void BindPassTransform(ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& viewProjection);

    /** Parameters (b1, VS and PS), textures (t6+) and the material sampler (s2). */
    void Bind(ID3D11DeviceContext* context);
    void Unbind(ID3D11DeviceContext* context) const;

private:
    /** Shaders of one way of feeding the copies: one object (b0) or an instance stream (t0). */
    struct ShaderSet
    {
        Microsoft::WRL::ComPtr<ID3D11VertexShader> VertexShader;
        Microsoft::WRL::ComPtr<ID3D11PixelShader> ForwardPixelShader;
        Microsoft::WRL::ComPtr<ID3D11PixelShader> GBufferPixelShader; // null: forward-only
        Microsoft::WRL::ComPtr<ID3D11VertexShader> ShadowVertexShader; // masked only
        Microsoft::WRL::ComPtr<ID3D11PixelShader> ShadowPixelShader;
    };
    const ShaderSet& Set(bool instanced) const { return instanced ? Instanced : Standard; }
    /** forwardBlob/dependencies are filled for the reflection and the reload tracking. */
    bool CompileSet(const std::string& surface, bool instanced, ShaderSet& output, std::string& errors,
                    Microsoft::WRL::ComPtr<ID3DBlob>* forwardBlob = nullptr, std::vector<std::string>* dependencies = nullptr);
    bool Reflect(ID3DBlob* pixelShader, std::string& errors);
    bool CreateParameterBuffer();
    ID3D11ShaderResourceView* GetWhiteTexture();

    std::string SurfacePath;
    MaterialRenderState State;
    bool bNeedsCompile = true;
    bool bInstancingRequired = false;
    uint64_t Revision = 0;
    std::string Errors;
    std::vector<std::string> Dependencies;

    Microsoft::WRL::ComPtr<ID3D11Device> Device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> Context;
    ShaderSet Standard;
    ShaderSet Instanced;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> BakeVertexShader; // compiled on first use
    Microsoft::WRL::ComPtr<ID3D11PixelShader> BakePixelShader;
    bool bBakeCompiled = false;
    Microsoft::WRL::ComPtr<ID3D11Buffer> PassTransformBuffer;

    std::vector<MaterialParameter> Parameters;
    std::vector<uint8_t> ParameterData;
    bool bParametersDirty = true;
    Microsoft::WRL::ComPtr<ID3D11Buffer> ParameterBuffer;

    std::vector<MaterialTexture> Textures;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> WhiteTexture;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> Sampler;
};
