#pragma once

#include <array>
#include <cstdint>
#include <d3d11.h>
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

    /** Compiles VS and both pixel variants from disk. False if the surface failed (fallback active). */
    bool Compile(ID3D11Device* device, ID3D11DeviceContext* context);
    bool IsCompiled() const { return Standard.VertexShader != nullptr; }
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

    ID3D11VertexShader* GetVertexShader() const { return Standard.VertexShader.Get(); }
    /** deferredGBuffer: the shader of the G-buffer pass, null if the material cannot write it. */
    ID3D11PixelShader* GetPixelShader(bool deferredGBuffer) const;
    bool WritesGBuffer() const { return Standard.GBufferPixelShader != nullptr; }

    /** Parameters (b1, VS and PS), textures (t6+) and the material sampler (s2). */
    void Bind(ID3D11DeviceContext* context);
    void Unbind(ID3D11DeviceContext* context) const;

private:
    struct ShaderSet
    {
        Microsoft::WRL::ComPtr<ID3D11VertexShader> VertexShader;
        Microsoft::WRL::ComPtr<ID3D11PixelShader> ForwardPixelShader;
        Microsoft::WRL::ComPtr<ID3D11PixelShader> GBufferPixelShader; // null: forward-only
    };
    /** forwardBlob/dependencies are filled for the reflection and the reload tracking. */
    bool CompileSet(const std::string& surface, ShaderSet& output, std::string& errors,
                    Microsoft::WRL::ComPtr<ID3DBlob>* forwardBlob = nullptr, std::vector<std::string>* dependencies = nullptr);
    bool Reflect(ID3DBlob* pixelShader, std::string& errors);
    bool CreateParameterBuffer();
    ID3D11ShaderResourceView* GetWhiteTexture();

    std::string SurfacePath;
    MaterialRenderState State;
    bool bNeedsCompile = true;
    std::string Errors;
    std::vector<std::string> Dependencies;

    Microsoft::WRL::ComPtr<ID3D11Device> Device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> Context;
    ShaderSet Standard;

    std::vector<MaterialParameter> Parameters;
    std::vector<uint8_t> ParameterData;
    bool bParametersDirty = true;
    Microsoft::WRL::ComPtr<ID3D11Buffer> ParameterBuffer;

    std::vector<MaterialTexture> Textures;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> WhiteTexture;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> Sampler;
};
