#include <Engine/Render/MaterialAsset.h>

#include <Engine/Render/ShaderCompiler.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>

using Json = nlohmann::ordered_json; // keeps the key order of the author's file

namespace
{
    struct BlendName
    {
        MaterialBlendMode Mode;
        const char* Name;
    };
    const BlendName BlendNames[] = {
        {MaterialBlendMode::Opaque, "opaque"},
        {MaterialBlendMode::Masked, "masked"},
        {MaterialBlendMode::Translucent, "translucent"},
        {MaterialBlendMode::Additive, "additive"},
    };

    const char* ToName(MaterialBlendMode mode)
    {
        for (const BlendName& blend : BlendNames)
        {
            if (blend.Mode == mode) return blend.Name;
        }
        return "opaque";
    }

    std::filesystem::path FromUtf8(const std::string& path) { return std::filesystem::u8path(path); }

    bool ReadText(const std::string& path, std::string& text)
    {
        std::ifstream input(FromUtf8(path), std::ios::binary);
        if (!input)
        {
            return false;
        }
        text.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
        if (text.compare(0, 3, "\xef\xbb\xbf") == 0)
        {
            text.erase(0, 3);
        }
        return true;
    }

    /** The double that prints as the shortest decimal reading back as this float: 0.8f -> 0.8. */
    double ShortestDouble(float value)
    {
        char buffer[32];
        for (int precision = 6; precision <= 9; ++precision)
        {
            std::snprintf(buffer, sizeof(buffer), "%.*g", precision, value);
            if (std::strtof(buffer, nullptr) == value)
            {
                break;
            }
        }
        return std::strtod(buffer, nullptr);
    }

    Json ToJson(const std::vector<float>& values)
    {
        if (values.size() == 1)
        {
            return ShortestDouble(values[0]);
        }
        Json array = Json::array();
        for (float value : values)
        {
            array.push_back(ShortestDouble(value));
        }
        return array;
    }

    template <class T>
    void SetEntry(std::vector<std::pair<std::string, T>>& entries, const std::string& name, T value)
    {
        const auto found = std::find_if(entries.begin(), entries.end(), [&](const auto& entry) { return entry.first == name; });
        if (found != entries.end())
        {
            found->second = std::move(value);
        }
        else
        {
            entries.emplace_back(name, std::move(value));
        }
    }

    template <class T>
    const T* FindEntry(const std::vector<std::pair<std::string, T>>& entries, const std::string& name)
    {
        const auto found = std::find_if(entries.begin(), entries.end(), [&](const auto& entry) { return entry.first == name; });
        return found != entries.end() ? &found->second : nullptr;
    }

    bool SamePath(const std::string& a, const std::string& b)
    {
        return ShaderCompiler::NormalizePath(a) == ShaderCompiler::NormalizePath(b);
    }
}

bool MaterialAsset::Load(const std::string& matPath, MaterialAsset& asset, std::string& error)
{
    auto fail = [&](const std::string& message)
    {
        error = matPath + ": " + message;
        return false;
    };

    std::string text;
    if (!ReadText(matPath, text))
    {
        return fail("cannot read the file");
    }

    Json document;
    try
    {
        document = Json::parse(text);
    }
    catch (const Json::parse_error& parseError)
    {
        return fail(std::string("invalid JSON: ") + parseError.what());
    }
    if (!document.is_object())
    {
        return fail("the root must be a JSON object");
    }

    MaterialAsset result;
    result.OriginalText = text;

    if (document.contains("version"))
    {
        const Json& version = document["version"];
        if (!version.is_number_integer())
        {
            return fail("\"version\" must be an integer");
        }
        if (version.get<int>() < 1 || version.get<int>() > CurrentVersion)
        {
            return fail("unsupported version " + version.dump() + " (this engine reads version " + std::to_string(CurrentVersion) + ")");
        }
    }

    if (!document.contains("shader") || !document["shader"].is_string() || document["shader"].get<std::string>().empty())
    {
        return fail("\"shader\" (path of the surface .hlsl file) is required");
    }
    result.Shader = document["shader"].get<std::string>();

    if (document.contains("render"))
    {
        const Json& render = document["render"];
        if (!render.is_object())
        {
            return fail("\"render\" must be an object");
        }
        if (render.contains("blend"))
        {
            const Json& blend = render["blend"];
            const auto known = std::find_if(std::begin(BlendNames), std::end(BlendNames),
                                            [&](const BlendName& b) { return blend.is_string() && blend.get<std::string>() == b.Name; });
            if (known == std::end(BlendNames))
            {
                return fail("render.blend must be one of: opaque, masked, translucent, additive");
            }
            result.Render.Blend = known->Mode;
        }
        if (render.contains("alphaClip"))
        {
            const Json& clip = render["alphaClip"];
            if (!clip.is_number() || clip.get<float>() < 0.0f || clip.get<float>() > 1.0f)
            {
                return fail("render.alphaClip must be a number from 0 to 1");
            }
            result.Render.AlphaClip = clip.get<float>();
        }
        if (render.contains("lighting"))
        {
            const Json& lighting = render["lighting"];
            if (!lighting.is_string() || (lighting != "lit" && lighting != "unlit"))
            {
                return fail("render.lighting must be \"lit\" or \"unlit\"");
            }
            result.Render.bUnlit = lighting == "unlit";
        }
        const std::pair<const char*, bool*> flags[] = {
            {"twoSided", &result.Render.bTwoSided},
            {"castShadows", &result.Render.bCastShadows},
        };
        for (const auto& flag : flags)
        {
            if (render.contains(flag.first))
            {
                if (!render[flag.first].is_boolean())
                {
                    return fail(std::string("render.") + flag.first + " must be true or false");
                }
                *flag.second = render[flag.first].get<bool>();
            }
        }
    }

    if (document.contains("params"))
    {
        const Json& params = document["params"];
        if (!params.is_object())
        {
            return fail("\"params\" must be an object");
        }
        for (const auto& item : params.items())
        {
            std::vector<float> values;
            if (item.value().is_number())
            {
                values.push_back(item.value().get<float>());
            }
            else if (item.value().is_array() && !item.value().empty() && item.value().size() <= 4)
            {
                for (const Json& component : item.value())
                {
                    if (!component.is_number())
                    {
                        return fail("params." + item.key() + " must contain only numbers");
                    }
                    values.push_back(component.get<float>());
                }
            }
            else
            {
                return fail("params." + item.key() + " must be a number or an array of 1-4 numbers");
            }
            result.Params.emplace_back(item.key(), std::move(values));
        }
    }

    if (document.contains("textures"))
    {
        const Json& textures = document["textures"];
        if (!textures.is_object())
        {
            return fail("\"textures\" must be an object");
        }
        for (const auto& item : textures.items())
        {
            if (!item.value().is_null() && !item.value().is_string())
            {
                return fail("textures." + item.key() + " must be a path or null");
            }
            result.Textures.emplace_back(item.key(), item.value().is_null() ? std::string() : item.value().get<std::string>());
        }
    }

    asset = std::move(result);
    return true;
}

bool MaterialAsset::Save(const std::string& matPath, std::string& error) const
{
    Json document = Json::object();
    if (!OriginalText.empty())
    {
        Json original = Json::parse(OriginalText, nullptr, false);
        if (original.is_object())
        {
            document = std::move(original);
        }
    }

    document["version"] = CurrentVersion;
    document["shader"] = Shader;

    Json& render = document["render"];
    if (!render.is_object())
    {
        render = Json::object();
    }
    render["blend"] = ToName(Render.Blend);
    render["alphaClip"] = ShortestDouble(Render.AlphaClip);
    render["lighting"] = Render.bUnlit ? "unlit" : "lit";
    render["twoSided"] = Render.bTwoSided;
    render["castShadows"] = Render.bCastShadows;

    Json params = Json::object();
    for (const auto& param : Params)
    {
        params[param.first] = ToJson(param.second);
    }
    document["params"] = std::move(params);

    Json textures = Json::object();
    for (const auto& texture : Textures)
    {
        textures[texture.first] = texture.second.empty() ? Json(nullptr) : Json(texture.second);
    }
    document["textures"] = std::move(textures);

    // Write next to the target and swap, so a failed write never leaves half a file.
    const std::filesystem::path target = FromUtf8(matPath);
    std::filesystem::path temporary = target;
    temporary += ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output << document.dump(2) << '\n';
        if (!output)
        {
            error = matPath + ": cannot write the file";
            return false;
        }
    }
    std::error_code renameError;
    std::filesystem::rename(temporary, target, renameError);
    if (renameError)
    {
        std::filesystem::remove(temporary, renameError);
        error = matPath + ": cannot replace the file";
        return false;
    }
    return true;
}

std::string MaterialAsset::ResolvePath(const std::string& matPath, const std::string& relativePath)
{
    const std::filesystem::path path = FromUtf8(relativePath);
    if (path.is_absolute())
    {
        return path.lexically_normal().generic_u8string();
    }
    return (std::filesystem::absolute(FromUtf8(matPath)).parent_path() / path).lexically_normal().generic_u8string();
}

std::string MaterialAsset::MakeRelativePath(const std::string& matPath, const std::string& absolutePath)
{
    const std::filesystem::path directory = std::filesystem::absolute(FromUtf8(matPath)).parent_path().lexically_normal();
    const std::filesystem::path path = std::filesystem::absolute(FromUtf8(absolutePath)).lexically_normal();
    const std::filesystem::path relative = path.lexically_relative(directory);
    // Another drive has no relative form.
    return relative.empty() ? path.generic_u8string() : relative.generic_u8string();
}

void MaterialAsset::ApplyTo(Material& material, const std::string& matPath, std::vector<std::string>& warnings) const
{
    for (const auto& param : Params)
    {
        const MaterialParameter* parameter = material.FindParameter(param.first);
        if (parameter == nullptr || parameter->Type == MaterialParameterType::Unsupported)
        {
            warnings.push_back("params." + param.first + (parameter ? " has a type the editor cannot set" : " is not declared by the shader"));
            continue;
        }
        const size_t components = static_cast<size_t>(parameter->Type) + 1;
        if (param.second.size() != components)
        {
            warnings.push_back("params." + param.first + " expects " + std::to_string(components) + " value(s), the file has " +
                               std::to_string(param.second.size()));
        }
        std::array<float, 4> value{};
        std::copy_n(param.second.begin(), (std::min)(components, param.second.size()), value.begin());
        material.SetParameter(param.first, value);
    }

    for (const auto& texture : Textures)
    {
        const auto& slots = material.GetTextures();
        if (std::none_of(slots.begin(), slots.end(), [&](const MaterialTexture& slot) { return slot.Name == texture.first; }))
        {
            warnings.push_back("textures." + texture.first + " is not declared by the shader");
            continue;
        }
        const std::string path = texture.second.empty() ? std::string() : ResolvePath(matPath, texture.second);
        if (!material.SetTexture(texture.first, path))
        {
            warnings.push_back("textures." + texture.first + ": cannot load " + path);
        }
    }
}

MaterialAsset MaterialAsset::FromMaterial(const Material& material, const std::string& matPath, const MaterialAsset& base)
{
    MaterialAsset asset = base;
    const bool bSameShader = !base.Shader.empty() && SamePath(ResolvePath(matPath, base.Shader), material.GetSurfacePath());
    asset.Shader = bSameShader ? base.Shader : MakeRelativePath(matPath, material.GetSurfacePath());
    asset.Render = material.GetRenderState();

    for (const MaterialParameter& parameter : material.GetParameters())
    {
        std::array<float, 4> value{};
        if (material.GetParameter(parameter.Name, value))
        {
            SetEntry(asset.Params, parameter.Name, std::vector<float>(value.begin(), value.begin() + static_cast<size_t>(parameter.Type) + 1));
        }
    }

    for (const MaterialTexture& texture : material.GetTextures())
    {
        std::string path;
        if (!texture.Path.empty())
        {
            // Keep the author's spelling while it still names the same file.
            const std::string* written = FindEntry(base.Textures, texture.Name);
            path = written && !written->empty() && SamePath(ResolvePath(matPath, *written), texture.Path)
                       ? *written
                       : MakeRelativePath(matPath, texture.Path);
        }
        SetEntry(asset.Textures, texture.Name, path);
    }
    return asset;
}
