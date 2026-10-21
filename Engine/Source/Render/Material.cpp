#include <Engine/Render/Material.h>

#include <Engine/Render/ShaderCompiler.h>
#include <Engine/Render/TextureLoader.h>
#include <Engine/Resources/Paths.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <d3dcompiler.h>
#include <iostream>

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")

namespace
{
    constexpr uint32_t ParameterBufferSlot = 1;
    constexpr uint32_t FirstTextureSlot = 6;
    constexpr uint32_t SamplerSlot = 2;
    const char* const ParameterBufferName = "MaterialParams";
    const char* const SurfaceIncludeName = "MaterialSurface.hlsli";

    std::string TemplatePath() { return EnginePaths::Shader("Material/MaterialTemplate.hlsl").generic_u8string(); }
    std::string ErrorSurfacePath() { return EnginePaths::Shader("Material/ErrorSurface.hlsl").generic_u8string(); }

    std::vector<std::pair<std::string, std::string>> MakeDefines(const MaterialRenderState& state)
    {
        std::vector<std::pair<std::string, std::string>> defines;
        defines.emplace_back("MATERIAL_BLEND", std::to_string(static_cast<int>(state.Blend)));
        if (state.Blend == MaterialBlendMode::Masked)
        {
            char clip[32];
            std::snprintf(clip, sizeof(clip), "%.6g", state.AlphaClip);
            defines.emplace_back("MATERIAL_ALPHA_CLIP", clip);
        }
        if (state.bUnlit)
        {
            defines.emplace_back("MATERIAL_UNLIT", "1");
        }
        if (state.bTwoSided)
        {
            defines.emplace_back("MATERIAL_TWO_SIDED", "1");
        }
        return defines;
    }

    UINT CountRenderTargets(ID3D11ShaderReflection* reflection)
    {
        D3D11_SHADER_DESC desc = {};
        reflection->GetDesc(&desc);
        UINT targets = 0;
        for (UINT i = 0; i < desc.OutputParameters; ++i)
        {
            D3D11_SIGNATURE_PARAMETER_DESC parameter = {};
            reflection->GetOutputParameterDesc(i, &parameter);
            targets += parameter.SystemValueType == D3D_NAME_TARGET ? 1 : 0;
        }
        return targets;
    }

    Microsoft::WRL::ComPtr<ID3D11ShaderReflection> ReflectBlob(ID3DBlob* blob)
    {
        Microsoft::WRL::ComPtr<ID3D11ShaderReflection> reflection;
        if (blob)
        {
            D3DReflect(blob->GetBufferPointer(), blob->GetBufferSize(), IID_ID3D11ShaderReflection,
                       reinterpret_cast<void**>(reflection.GetAddressOf()));
        }
        return reflection;
    }

    MaterialParameterType ToParameterType(const D3D11_SHADER_TYPE_DESC& type)
    {
        const bool bFloatVector = type.Type == D3D_SVT_FLOAT && type.Rows == 1 && type.Elements == 0 &&
                                  (type.Class == D3D_SVC_SCALAR || type.Class == D3D_SVC_VECTOR);
        if (!bFloatVector)
        {
            return MaterialParameterType::Unsupported;
        }
        switch (type.Columns)
        {
        case 1: return MaterialParameterType::Float;
        case 2: return MaterialParameterType::Float2;
        case 3: return MaterialParameterType::Float3;
        case 4: return MaterialParameterType::Float4;
        default: return MaterialParameterType::Unsupported;
        }
    }

    uint32_t ComponentCount(MaterialParameterType type)
    {
        return type == MaterialParameterType::Unsupported ? 0u : static_cast<uint32_t>(type) + 1u;
    }
}

bool MaterialRenderState::operator==(const MaterialRenderState& other) const
{
    return Blend == other.Blend && AlphaClip == other.AlphaClip && bUnlit == other.bUnlit &&
           bTwoSided == other.bTwoSided && bCastShadows == other.bCastShadows;
}

Material::Material(std::string surfacePath, MaterialRenderState state)
    : SurfacePath(std::move(surfacePath)), State(state)
{
}

void Material::SetRenderState(const MaterialRenderState& state)
{
    // Shadow casting is a draw decision only; everything else changes the shaders.
    MaterialRenderState compiledPart = state;
    compiledPart.bCastShadows = State.bCastShadows;
    if (compiledPart != State)
    {
        bNeedsCompile = true;
    }
    State = state;
}

bool Material::CompileSet(const std::string& surface, ShaderSet& output, std::string& errors,
                          Microsoft::WRL::ComPtr<ID3DBlob>* forwardBlob, std::vector<std::string>* dependencies)
{
    ShaderCompiler::CompileOptions options;
    // The error surface is drawn with the default state (it declares MATERIAL_UNLIT itself).
    const MaterialRenderState state = surface == SurfacePath ? State : MaterialRenderState{};
    options.Defines = MakeDefines(state);
    options.IncludeOverrides[SurfaceIncludeName] = surface;

    const std::string templatePath = TemplatePath();
    ShaderCompiler::CompileResult forward = ShaderCompiler::Compile(templatePath, "PSMain", "ps_5_0", options);
    if (dependencies)
    {
        *dependencies = forward.Dependencies;
    }
    // Surface errors repeat in every stage; the forward pixel shader reports them first.
    if (!forward.Succeeded())
    {
        errors = forward.Errors;
        return false;
    }
    ShaderCompiler::CompileResult vertex = ShaderCompiler::Compile(templatePath, "VSMain", "vs_5_0", options);
    if (!vertex.Succeeded())
    {
        errors = vertex.Errors;
        return false;
    }
    options.Defines.emplace_back("DEFERRED_GBUFFER", "1");
    ShaderCompiler::CompileResult gbuffer = ShaderCompiler::Compile(templatePath, "PSMain", "ps_5_0", options);
    if (!gbuffer.Succeeded())
    {
        errors = gbuffer.Errors;
        return false;
    }

    if (FAILED(Device->CreateVertexShader(vertex.Blob->GetBufferPointer(), vertex.Blob->GetBufferSize(), nullptr,
                                          output.VertexShader.GetAddressOf())) ||
        FAILED(Device->CreatePixelShader(forward.Blob->GetBufferPointer(), forward.Blob->GetBufferSize(), nullptr,
                                         output.ForwardPixelShader.GetAddressOf())))
    {
        errors = "Failed to create material shaders on the device";
        return false;
    }

    // Only a shader with the full G-buffer signature may be used by the geometry pass.
    Microsoft::WRL::ComPtr<ID3D11ShaderReflection> gbufferReflection = ReflectBlob(gbuffer.Blob.Get());
    if (gbufferReflection && CountRenderTargets(gbufferReflection.Get()) >= 3 &&
        FAILED(Device->CreatePixelShader(gbuffer.Blob->GetBufferPointer(), gbuffer.Blob->GetBufferSize(), nullptr,
                                         output.GBufferPixelShader.GetAddressOf())))
    {
        errors = "Failed to create the G-buffer material shader on the device";
        return false;
    }

    if (forwardBlob)
    {
        *forwardBlob = forward.Blob;
    }
    return true;
}

bool Material::Compile(ID3D11Device* device, ID3D11DeviceContext* context)
{
    if (device == nullptr || context == nullptr)
    {
        return false;
    }
    Device = device;
    Context = context;
    bNeedsCompile = false;
    if (!Sampler)
    {
        TextureLoader::CreateLinearSampler(Device.Get(), D3D11_TEXTURE_ADDRESS_WRAP, Sampler);
    }

    ShaderSet standard;
    Microsoft::WRL::ComPtr<ID3DBlob> forwardBlob;
    std::vector<std::string> dependencies;
    std::string errors;
    const bool bCompiled = CompileSet(SurfacePath, standard, errors, &forwardBlob, &dependencies) &&
                           Reflect(forwardBlob.Get(), errors);
    // Known even after a failure, so a fix of any of these files can trigger a recompilation.
    Dependencies = dependencies;

    if (!bCompiled)
    {
        Errors = errors.empty() ? std::string("Material compilation failed") : errors;
        std::cout << "Material '" << SurfacePath << "' failed to compile:" << std::endl << Errors << std::endl;

        // Parameters and textures stay as they are; only the shaders change to the error surface
        // (unlit, so it has no G-buffer shader).
        ShaderSet fallback;
        std::string fallbackErrors;
        if (!CompileSet(ErrorSurfacePath(), fallback, fallbackErrors))
        {
            std::cout << "Error material failed to compile:" << std::endl << fallbackErrors << std::endl;
            fallback = ShaderSet{};
        }
        Standard = fallback;
        return false;
    }

    Errors.clear();
    Standard = standard;
    return true;
}

bool Material::Reflect(ID3DBlob* pixelShader, std::string& errors)
{
    Microsoft::WRL::ComPtr<ID3D11ShaderReflection> reflection = ReflectBlob(pixelShader);
    if (!reflection)
    {
        errors = "Shader reflection failed";
        return false;
    }

    std::vector<MaterialParameter> parameters;
    std::vector<uint8_t> data;
    D3D11_SHADER_INPUT_BIND_DESC binding = {};
    // A cbuffer whose variables are all unused is stripped by the compiler and has no parameters.
    if (SUCCEEDED(reflection->GetResourceBindingDescByName(ParameterBufferName, &binding)))
    {
        if (binding.Type != D3D_SIT_CBUFFER || binding.BindPoint != ParameterBufferSlot)
        {
            errors = std::string(ParameterBufferName) + " must be declared as: cbuffer MaterialParams : register(b1)";
            return false;
        }

        ID3D11ShaderReflectionConstantBuffer* buffer = reflection->GetConstantBufferByName(ParameterBufferName);
        D3D11_SHADER_BUFFER_DESC bufferDesc = {};
        buffer->GetDesc(&bufferDesc);
        data.assign(bufferDesc.Size, 0);
        for (UINT i = 0; i < bufferDesc.Variables; ++i)
        {
            ID3D11ShaderReflectionVariable* variable = buffer->GetVariableByIndex(i);
            D3D11_SHADER_VARIABLE_DESC variableDesc = {};
            D3D11_SHADER_TYPE_DESC typeDesc = {};
            variable->GetDesc(&variableDesc);
            variable->GetType()->GetDesc(&typeDesc);

            MaterialParameter parameter;
            parameter.Name = variableDesc.Name;
            parameter.Type = ToParameterType(typeDesc);
            parameter.Offset = variableDesc.StartOffset;
            parameter.Size = variableDesc.Size;
            if (variableDesc.DefaultValue && parameter.Offset + parameter.Size <= data.size())
            {
                std::memcpy(data.data() + parameter.Offset, variableDesc.DefaultValue, parameter.Size);
                std::memcpy(parameter.Default.data(), variableDesc.DefaultValue,
                            std::min<size_t>(ComponentCount(parameter.Type) * sizeof(float), parameter.Size));
            }
            parameters.push_back(std::move(parameter));
        }
    }

    // Values of parameters that survived the edit (same name and type) are kept.
    for (const MaterialParameter& parameter : parameters)
    {
        const MaterialParameter* previous = FindParameter(parameter.Name);
        if (previous && previous->Type == parameter.Type && previous->Size == parameter.Size &&
            previous->Offset + previous->Size <= ParameterData.size())
        {
            std::memcpy(data.data() + parameter.Offset, ParameterData.data() + previous->Offset, parameter.Size);
        }
    }

    std::vector<MaterialTexture> textures;
    D3D11_SHADER_DESC shaderDesc = {};
    reflection->GetDesc(&shaderDesc);
    for (UINT i = 0; i < shaderDesc.BoundResources; ++i)
    {
        reflection->GetResourceBindingDesc(i, &binding);
        if (binding.Type != D3D_SIT_TEXTURE || binding.Dimension != D3D_SRV_DIMENSION_TEXTURE2D ||
            binding.BindPoint < FirstTextureSlot)
        {
            continue;
        }
        MaterialTexture texture;
        texture.Name = binding.Name;
        texture.Slot = binding.BindPoint;
        const auto previous = std::find_if(Textures.begin(), Textures.end(),
                                           [&](const MaterialTexture& t) { return t.Name == texture.Name; });
        if (previous != Textures.end())
        {
            texture.Path = previous->Path;
            texture.View = previous->View;
        }
        textures.push_back(std::move(texture));
    }

    Parameters = std::move(parameters);
    ParameterData = std::move(data);
    Textures = std::move(textures);
    return CreateParameterBuffer();
}

bool Material::CreateParameterBuffer()
{
    ParameterBuffer.Reset();
    bParametersDirty = true;
    if (ParameterData.empty())
    {
        return true;
    }

    D3D11_BUFFER_DESC desc = {};
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.ByteWidth = static_cast<UINT>((ParameterData.size() + 15) / 16 * 16);
    ParameterData.resize(desc.ByteWidth, 0);
    if (FAILED(Device->CreateBuffer(&desc, nullptr, ParameterBuffer.GetAddressOf())))
    {
        std::cout << "Material: failed to create the parameter buffer." << std::endl;
        return false;
    }
    return true;
}

bool Material::DependsOn(const std::string& file) const
{
    const std::string normalized = ShaderCompiler::NormalizePath(file);
    return std::find(Dependencies.begin(), Dependencies.end(), normalized) != Dependencies.end();
}

const MaterialParameter* Material::FindParameter(const std::string& name) const
{
    const auto found = std::find_if(Parameters.begin(), Parameters.end(),
                                    [&](const MaterialParameter& parameter) { return parameter.Name == name; });
    return found != Parameters.end() ? &*found : nullptr;
}

bool Material::SetParameter(const std::string& name, const std::array<float, 4>& value)
{
    const MaterialParameter* parameter = FindParameter(name);
    if (parameter == nullptr || parameter->Type == MaterialParameterType::Unsupported)
    {
        return false;
    }
    std::memcpy(ParameterData.data() + parameter->Offset, value.data(), ComponentCount(parameter->Type) * sizeof(float));
    bParametersDirty = true;
    return true;
}

bool Material::GetParameter(const std::string& name, std::array<float, 4>& value) const
{
    const MaterialParameter* parameter = FindParameter(name);
    if (parameter == nullptr || parameter->Type == MaterialParameterType::Unsupported)
    {
        return false;
    }
    value = {};
    std::memcpy(value.data(), ParameterData.data() + parameter->Offset, ComponentCount(parameter->Type) * sizeof(float));
    return true;
}

bool Material::SetTexture(const std::string& name, const std::string& path)
{
    const auto texture = std::find_if(Textures.begin(), Textures.end(),
                                      [&](const MaterialTexture& t) { return t.Name == name; });
    if (texture == Textures.end())
    {
        return false;
    }
    if (path.empty())
    {
        texture->Path.clear();
        texture->View.Reset();
        return true;
    }

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
    // Material meshes have v = 0 at the top of the image, as images are stored.
    if (!Device || !TextureLoader::LoadTexture2D(Device.Get(), Context.Get(), path, false, view))
    {
        std::cout << "Material: cannot load texture '" << path << "' for " << name << std::endl;
        return false;
    }
    texture->Path = path;
    texture->View = view;
    return true;
}

ID3D11PixelShader* Material::GetPixelShader(bool deferredGBuffer) const
{
    return deferredGBuffer ? Standard.GBufferPixelShader.Get() : Standard.ForwardPixelShader.Get();
}

ID3D11ShaderResourceView* Material::GetWhiteTexture()
{
    if (!WhiteTexture && Device)
    {
        const uint32_t white = 0xffffffffu;
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = desc.Height = 1;
        desc.MipLevels = desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA data = {&white, sizeof(white), 0};
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
        if (SUCCEEDED(Device->CreateTexture2D(&desc, &data, texture.GetAddressOf())))
        {
            Device->CreateShaderResourceView(texture.Get(), nullptr, WhiteTexture.GetAddressOf());
        }
    }
    return WhiteTexture.Get();
}

void Material::Bind(ID3D11DeviceContext* context)
{
    if (context == nullptr)
    {
        return;
    }
    if (ParameterBuffer)
    {
        if (bParametersDirty)
        {
            context->UpdateSubresource(ParameterBuffer.Get(), 0, nullptr, ParameterData.data(), 0, 0);
            bParametersDirty = false;
        }
        context->VSSetConstantBuffers(ParameterBufferSlot, 1, ParameterBuffer.GetAddressOf());
        context->PSSetConstantBuffers(ParameterBufferSlot, 1, ParameterBuffer.GetAddressOf());
    }
    for (const MaterialTexture& texture : Textures)
    {
        ID3D11ShaderResourceView* view = texture.View ? texture.View.Get() : GetWhiteTexture();
        context->PSSetShaderResources(texture.Slot, 1, &view);
    }
    if (Sampler)
    {
        context->PSSetSamplers(SamplerSlot, 1, Sampler.GetAddressOf());
    }
}

void Material::Unbind(ID3D11DeviceContext* context) const
{
    if (context == nullptr)
    {
        return;
    }
    ID3D11ShaderResourceView* nullView = nullptr;
    for (const MaterialTexture& texture : Textures)
    {
        context->PSSetShaderResources(texture.Slot, 1, &nullView);
    }
}
