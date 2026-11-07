#include "ModelImportSettings.h"
#include "../Resources/VfsRead.h"
#include "../../ThirdParty/nlohmann/json.hpp"

#include <cstdio>
#include <vector>

bool ModelImportSettings::IsValidScale(float scale)
{
    return std::isfinite(scale) && scale >= MinScale && scale <= MaxScale;
}

bool ModelImportSettings::IsValidSmoothingAngle(float angle)
{
    return std::isfinite(angle) && angle >= MinAngle && angle <= MaxAngle;
}

bool ModelImportSettings::IsValidNormals(std::string_view normals)
{
    return normals == "file" || normals == "flat" || normals == "smooth";
}

std::string ModelImportSettings::ResolveSidecarPath(std::string_view assetPath)
{
    if (assetPath.empty())
    {
        return {};
    }
    constexpr std::string_view suffix = ".importsettings.json";
    if (assetPath.size() >= suffix.size() &&
        assetPath.substr(assetPath.size() - suffix.size()) == suffix)
    {
        return std::string(assetPath);
    }
    return std::string(assetPath) + ".importsettings.json";
}

bool ModelImportSettings::Parse(std::string_view jsonText, ModelImportSettings& outSettings, std::string* outError)
{
    outSettings = ModelImportSettings{};
    auto reportError = [&](const std::string& err) {
        std::fprintf(stderr, "[Error] %s\n", err.c_str());
        if (outError)
        {
            *outError = err;
        }
    };

    try
    {
        nlohmann::json root = nlohmann::json::parse(jsonText.begin(), jsonText.end(), nullptr, false);
        if (root.is_discarded() || !root.is_object())
        {
            reportError("ModelImportSettings: JSON parse error: malformed or not an object");
            return false;
        }

        // Legacy S16 format: {"scale": 1.0} or {"scale": 1.0, ...} without "kind" or "model"
        if (!root.contains("kind") && !root.contains("model") && root.contains("scale"))
        {
            const auto& scaleVal = root["scale"];
            if (!scaleVal.is_number())
            {
                reportError("ModelImportSettings: legacy 'scale' is not a number, using default 1.0");
                return false;
            }

            const float fScale = scaleVal.get<float>();
            if (!IsValidScale(fScale))
            {
                char buf[160];
                std::snprintf(buf, sizeof(buf), "ModelImportSettings: legacy scale %g is out of allowed range [%g, %g], using default 1.0",
                    static_cast<double>(fScale), static_cast<double>(MinScale), static_cast<double>(MaxScale));
                reportError(buf);
                return false;
            }

            std::printf("[Info] ModelImportSettings: migrating legacy sidecar with 'scale' to v1 model format\n");
            outSettings.version = 1;
            outSettings.kind = "model";
            outSettings.scale = fScale;
            return true;
        }

        // Modern v1 format
        // 1. version
        if (root.contains("version"))
        {
            const auto& vVal = root["version"];
            if (!vVal.is_number_integer())
            {
                reportError("ModelImportSettings: 'version' must be an integer");
                return false;
            }
            outSettings.version = vVal.get<int>();
        }
        else
        {
            outSettings.version = 1;
        }

        // 2. kind
        if (!root.contains("kind"))
        {
            reportError("ModelImportSettings: missing 'kind' field in JSON, using default settings");
            return false;
        }
        const auto& kindVal = root["kind"];
        if (!kindVal.is_string())
        {
            reportError("ModelImportSettings: 'kind' must be a string");
            return false;
        }
        outSettings.kind = kindVal.get<std::string>();
        if (outSettings.kind != "model")
        {
            reportError("ModelImportSettings: unsupported kind '" + outSettings.kind + "'");
            return false;
        }

        // 3. model section
        if (!root.contains("model") || !root["model"].is_object())
        {
            reportError("ModelImportSettings: missing or invalid 'model' object");
            return false;
        }

        const auto& modelObj = root["model"];

        // scale (required in model)
        if (!modelObj.contains("scale"))
        {
            reportError("ModelImportSettings: missing 'scale' in model settings");
            return false;
        }
        const auto& mScaleVal = modelObj["scale"];
        if (!mScaleVal.is_number())
        {
            reportError("ModelImportSettings: 'scale' is not a number, using default 1.0");
            return false;
        }
        const float fScale = mScaleVal.get<float>();
        if (!IsValidScale(fScale))
        {
            char buf[160];
            std::snprintf(buf, sizeof(buf), "ModelImportSettings: scale %g is out of allowed range [%g, %g], using default 1.0",
                static_cast<double>(fScale), static_cast<double>(MinScale), static_cast<double>(MaxScale));
            reportError(buf);
            return false;
        }
        outSettings.scale = fScale;

        // useFileUnitScale (optional, bool, default false)
        if (modelObj.contains("useFileUnitScale"))
        {
            const auto& val = modelObj["useFileUnitScale"];
            if (!val.is_boolean())
            {
                reportError("ModelImportSettings: 'useFileUnitScale' must be a boolean");
                return false;
            }
            outSettings.useFileUnitScale = val.get<bool>();
        }

        // normals (optional, enum: "file", "flat", "smooth", default "file")
        if (modelObj.contains("normals"))
        {
            const auto& val = modelObj["normals"];
            if (!val.is_string())
            {
                reportError("ModelImportSettings: 'normals' must be a string");
                return false;
            }
            const std::string nStr = val.get<std::string>();
            if (!IsValidNormals(nStr))
            {
                reportError("ModelImportSettings: invalid normals mode '" + nStr + "' (must be 'file', 'flat', or 'smooth')");
                return false;
            }
            outSettings.normals = nStr;
        }

        // smoothingAngle (optional, float [0, 175], default 60.0)
        if (modelObj.contains("smoothingAngle"))
        {
            const auto& val = modelObj["smoothingAngle"];
            if (!val.is_number())
            {
                reportError("ModelImportSettings: 'smoothingAngle' must be a number");
                return false;
            }
            const float fAngle = val.get<float>();
            if (!IsValidSmoothingAngle(fAngle))
            {
                char buf[160];
                std::snprintf(buf, sizeof(buf), "ModelImportSettings: smoothingAngle %g is out of allowed range [%g, %g]",
                    static_cast<double>(fAngle), static_cast<double>(MinAngle), static_cast<double>(MaxAngle));
                reportError(buf);
                return false;
            }
            outSettings.smoothingAngle = fAngle;
        }

        // importMaterials (optional, bool, default true)
        if (modelObj.contains("importMaterials"))
        {
            const auto& val = modelObj["importMaterials"];
            if (!val.is_boolean())
            {
                reportError("ModelImportSettings: 'importMaterials' must be a boolean");
                return false;
            }
            outSettings.importMaterials = val.get<bool>();
        }

        // importTextures (optional, bool, default true)
        if (modelObj.contains("importTextures"))
        {
            const auto& val = modelObj["importTextures"];
            if (!val.is_boolean())
            {
                reportError("ModelImportSettings: 'importTextures' must be a boolean");
                return false;
            }
            outSettings.importTextures = val.get<bool>();
        }

        // flipUv (optional, bool, default false)
        if (modelObj.contains("flipUv"))
        {
            const auto& val = modelObj["flipUv"];
            if (!val.is_boolean())
            {
                reportError("ModelImportSettings: 'flipUv' must be a boolean");
                return false;
            }
            outSettings.flipUv = val.get<bool>();
        }

        // mergeMeshes (optional, bool, default true)
        if (modelObj.contains("mergeMeshes"))
        {
            const auto& val = modelObj["mergeMeshes"];
            if (!val.is_boolean())
            {
                reportError("ModelImportSettings: 'mergeMeshes' must be a boolean");
                return false;
            }
            outSettings.mergeMeshes = val.get<bool>();
        }

        return true;
    }
    catch (const std::exception& ex)
    {
        reportError(std::string("ModelImportSettings: JSON exception: ") + ex.what());
        outSettings = ModelImportSettings{};
        return false;
    }
    catch (...)
    {
        reportError("ModelImportSettings: unknown exception during parse");
        outSettings = ModelImportSettings{};
        return false;
    }
}

ModelImportSettings ModelImportSettings::ReadFromVfs(std::string_view assetPath)
{
    const std::string sidecarPath = ResolveSidecarPath(assetPath);
    if (sidecarPath.empty())
    {
        return ModelImportSettings{};
    }

    try
    {
        if (!VfsRead::Active().Exists(sidecarPath))
        {
            return ModelImportSettings{};
        }

        const std::vector<uint8_t> bytes = VfsRead::Active().ReadBytes(sidecarPath);
        if (bytes.empty())
        {
            std::fprintf(stderr, "[Error] ModelImportSettings: sidecar '%s' is empty, using default settings\n", sidecarPath.c_str());
            return ModelImportSettings{};
        }

        const std::string_view text(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        ModelImportSettings settings;
        std::string parseErr;
        if (!Parse(text, settings, &parseErr))
        {
            return ModelImportSettings{};
        }
        return settings;
    }
    catch (const std::exception& ex)
    {
        std::fprintf(stderr, "[Error] ModelImportSettings: failed to read sidecar '%s': %s\n", sidecarPath.c_str(), ex.what());
        return ModelImportSettings{};
    }
    catch (...)
    {
        std::fprintf(stderr, "[Error] ModelImportSettings: unknown exception reading sidecar '%s'\n", sidecarPath.c_str());
        return ModelImportSettings{};
    }
}

