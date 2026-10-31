#define NOMINMAX
#include <Windows.h>
#include <Engine/Scene/SceneDocument.h>
#include <Engine/Components/GameComponents.h>
#include <Engine/Scene/SceneAssets.h>
#include "../../ThirdParty/nlohmann/json.hpp"
#include <atomic>
#include <cmath>
#include <fstream>
#include <set>

namespace
{
using Json = nlohmann::json;
constexpr size_t MaxBytes = 16 * 1024 * 1024;
[[noreturn]] void Invalid(const std::string& message) { throw SceneDocumentError(EngineSceneInvalidDocument, message); }
void RequireFields(const Json& value, std::initializer_list<const char*> keys)
{
    if (!value.is_object() || value.size() != keys.size()) Invalid("Unexpected or missing JSON fields");
    for (auto key : keys) if (!value.contains(key)) Invalid(std::string("Missing field: ") + key);
}
std::string Text(const Json& value)
{
    if (!value.is_string()) Invalid("Expected string");
    auto text = value.get<std::string>();
    if (text.empty() || text.find('\0') != std::string::npos) Invalid("Empty string or embedded NUL");
    return text;
}
uint64_t Id(const Json& value)
{
    if (!value.is_number_unsigned()) Invalid("Expected unsigned integer ID");
    return value.get<uint64_t>();
}
void Vector(const Json& value, float* output, size_t count)
{
    if (!value.is_array() || value.size() != count) Invalid("Incorrect transform array length");
    for (size_t i = 0; i < count; ++i)
    {
        if (!value[i].is_number()) Invalid("Expected transform number");
        output[i] = value[i].get<float>();
        if (!std::isfinite(output[i])) Invalid("Non-finite transform");
    }
}
void Structure(const SceneSnapshot& data)
{
    if (data.documentVersion < 1 || data.documentVersion > 3)
        throw SceneDocumentError(EngineSceneUnsupportedVersion, "Unsupported scene version");
    auto validText = [](const std::string& s) {
        return !s.empty() && s.find('\0') == std::string::npos && s.size() <= MaxBytes &&
            MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), nullptr, 0);
    };
    if (!validText(data.name) || data.components.size() > 100000) Invalid("Invalid scene name or too many objects");
    std::map<uint64_t, uint64_t> parents;
    std::set<std::string> names;
    for (const auto& item : data.components)
    {
        if (!item.projectData.empty())
        {
            if (data.documentVersion < 3) Invalid("Custom data requires scene format 3");
            const auto value = Json::parse(item.projectData);
            if (!value.is_object() || !value.contains("version") || !value["version"].is_number_unsigned())
                Invalid("Custom data requires an object with an unsigned version");
        }
        if (item.pointLight && !item.pointLight->IsValid()) Invalid("Invalid point light parameters");
        if (!SceneAssets::ValidPath(item.modelAsset) || !SceneAssets::ValidPath(item.textureAsset) ||
            !SceneAssets::ValidPath(item.materialAsset))
            Invalid("Assets must be canonical /Game paths without traversal");
        if ((item.typeName == SceneAssets::ModelType) != !item.modelAsset.empty())
            Invalid("Engine.Model requires a model asset; other types cannot specify a model");
        if (!item.id || !parents.emplace(item.id, item.parentId).second) Invalid("Duplicate or zero object ID");
        if (!validText(item.name) || !validText(item.typeName) || !names.insert(item.name).second)
            Invalid("Invalid type/name or duplicate object name");
        const auto& t = item.transform;
        for (int i = 0; i < 3; ++i)
            if (!std::isfinite(t.position[i]) || !std::isfinite(t.scale[i]) || std::abs(t.scale[i]) < 1e-6f)
                Invalid("Invalid position or scale");
        const float length = glm::dot(t.rotation, t.rotation);
        if (!std::isfinite(length) || length < 1e-12f) Invalid("Invalid quaternion");
    }
    // Iterative graph validation avoids stack overflow and quadratic parent walks.
    std::set<uint64_t> complete;
    for (const auto& item : parents)
    {
        std::set<uint64_t> path;
        auto id = item.first;
        while (id && !complete.count(id))
        {
            if (!path.insert(id).second) Invalid("Parent cycle");
            auto parent = parents.find(id);
            if (parent == parents.end()) Invalid("Missing parent");
            id = parent->second;
        }
        complete.insert(path.begin(), path.end());
    }
}
}
SceneSnapshot SceneDocument::Parse(const std::string& text)
{
    if (text.size() > MaxBytes) Invalid("Scene exceeds 16 MiB");
    try
    {
        std::map<int, std::set<std::string>> keys;
        auto json = Json::parse(text, [&](int depth, Json::parse_event_t event, Json& value) {
            if (depth > 16) Invalid("JSON nesting limit exceeded");
            if (event == Json::parse_event_t::object_start) keys[depth + 1].clear();
            if (event == Json::parse_event_t::key && !keys[depth].insert(value.get<std::string>()).second)
                Invalid("Duplicate JSON key");
            return true;
        });
        if (!json.is_object() || !json.contains("version")) Invalid("Missing version");
        const auto version = Id(json.at("version"));
        if (version < 1 || version > 3) throw SceneDocumentError(EngineSceneUnsupportedVersion, "Unsupported scene version; open with the original engine");
        RequireFields(json, {"version", "name", "objects"});
        SceneSnapshot data; data.name = Text(json.at("name"));
        data.documentVersion = static_cast<uint32_t>(version);
        if (version >= 2 && json.at("objects").is_array())
            for (auto& object : json["objects"])
            {
                if (object.contains("pointLight")) Invalid("v2 uses versioned properties, not pointLight");
                if (!object.contains("properties")) continue;
                const auto& properties = object.at("properties");
                if (!properties.is_object()) Invalid("Expected properties object");
                for (auto it = properties.begin(); it != properties.end(); ++it)
                {
                    if (it.key() != "Engine.PointLight") Invalid("Unknown component properties; use the original engine. Nothing has been discarded.");
                    RequireFields(it.value(), {"version", "value"});
                    if (Id(it.value().at("version")) != 1)
                        throw SceneDocumentError(EngineSceneUnsupportedVersion, "Unsupported point light property version");
                    object["pointLight"] = it.value().at("value");
                }
                object.erase("properties");
            }
        const auto& objects = json.at("objects");
        if (!objects.is_array() || objects.size() > 100000) Invalid("Invalid objects array");
        for (const auto& o : objects)
        {
            auto core = o;
            core.erase("assets"); core.erase("pointLight");
            if (version >= 3) core.erase("projectData");
            RequireFields(core, {"id", "type", "name", "parent", "inheritParentScale", "transform"});
            SceneComponentSnapshot item;
            if (version >= 3 && o.contains("projectData")) item.projectData = o.at("projectData").dump();
            if (o.contains("pointLight"))
            {
                const auto& light = o.at("pointLight");
                RequireFields(light, {"color", "intensity", "radius", "enabled"});
                SceneComponentSnapshot::PointLight value;
                Vector(light.at("color"), &value.color.x, 3);
                if (!light.at("intensity").is_number() || !light.at("radius").is_number() ||
                    !light.at("enabled").is_boolean()) Invalid("Invalid point light fields");
                value.intensity = light.at("intensity").get<float>();
                value.radius = light.at("radius").get<float>();
                value.enabled = light.at("enabled").get<bool>();
                item.pointLight = value;
            }
            if (o.contains("assets"))
            {
                // "material" is optional: documents written before materials stay readable.
                const auto& a = o.at("assets");
                if (a.is_object() && a.contains("material")) RequireFields(a, {"model", "texture", "material"});
                else RequireFields(a, {"model", "texture"});
                if (!a.at("model").is_string() || !a.at("texture").is_string() ||
                    (a.contains("material") && !a.at("material").is_string())) Invalid("Invalid asset reference");
                item.modelAsset = a.at("model").get<std::string>();
                item.textureAsset = a.at("texture").get<std::string>();
                if (a.contains("material")) item.materialAsset = a.at("material").get<std::string>();
            }
            item.id = Id(o.at("id")); item.parentId = Id(o.at("parent"));
            item.name = Text(o.at("name")); item.typeName = Text(o.at("type"));
            if (!o.at("inheritParentScale").is_boolean()) Invalid("Expected inheritParentScale boolean");
            item.inheritParentScale = o.at("inheritParentScale").get<bool>();
            const auto& t = o.at("transform"); RequireFields(t, {"position", "rotation", "scale"});
            Vector(t.at("position"), &item.transform.position.x, 3);
            Vector(t.at("scale"), &item.transform.scale.x, 3);
            float q[4]; Vector(t.at("rotation"), q, 4);
            item.transform.rotation = {q[3], q[0], q[1], q[2]};
            data.components.push_back(std::move(item));
        }
        Structure(data);
        return data;
    }
    catch (const Json::exception& e) { Invalid(e.what()); }
}
std::string SceneDocument::Serialize(const SceneSnapshot& data)
{
    Structure(data);
    Json json = {{"version", data.documentVersion}, {"name", data.name}, {"objects", Json::array()}};
    for (const auto& o : data.components)
    {
        const auto& t = o.transform;
        json["objects"].push_back({{"id", o.id}, {"type", o.typeName}, {"name", o.name},
            {"parent", o.parentId}, {"inheritParentScale", o.inheritParentScale},
            {"transform", {{"position", {t.position.x,t.position.y,t.position.z}},
                {"rotation", {t.rotation.x,t.rotation.y,t.rotation.z,t.rotation.w}},
                {"scale", {t.scale.x,t.scale.y,t.scale.z}}}}});
        if (!o.modelAsset.empty() || !o.textureAsset.empty() || !o.materialAsset.empty())
        {
            auto& assets = json["objects"].back()["assets"];
            assets = {{"model", o.modelAsset}, {"texture", o.textureAsset}};
            if (!o.materialAsset.empty()) assets["material"] = o.materialAsset;
        }
        if (o.pointLight)
        {
            const auto& light = *o.pointLight;
            json["objects"].back()["pointLight"] = {{"color", {light.color.x, light.color.y, light.color.z}},
                {"intensity", light.intensity}, {"radius", light.radius}, {"enabled", light.enabled}};
            if (data.documentVersion >= 2)
            {
                auto& object = json["objects"].back();
                object["properties"] = {{"Engine.PointLight", {{"version",1},{"value",object["pointLight"]}}}};
                object.erase("pointLight");
            }
        }
        if (!o.projectData.empty()) json["objects"].back()["projectData"] = Json::parse(o.projectData);
    }
    auto text = json.dump(2) + "\n";
    if (text.size() > MaxBytes) Invalid("Scene exceeds 16 MiB");
    return text;
}
SceneSnapshot SceneDocument::Migrate(const SceneSnapshot& data, uint32_t targetVersion)
{
    Structure(data);
    if (targetVersion < data.documentVersion || targetVersion > 3 || targetVersion < 1)
        throw SceneDocumentError(EngineSceneUnsupportedVersion, "Unsupported scene migration (downgrades are not automatic)");
    auto result = data;
    result.documentVersion = targetVersion;
    return result;
}
void SceneDocument::Validate(const SceneSnapshot& data, ComponentTypeCatalog catalog)
{
    Structure(data);
    for (const auto& o : data.components)
        if (o.typeName != SceneAssets::ModelType && !catalog.Find(o.typeName))
            throw SceneDocumentError(EngineSceneUnknownType, "Unknown component type: " + o.typeName);
}
void SceneDocument::Restore(const SceneSnapshot& data, ComponentTypeCatalog catalog, Scene& target)
{
    Validate(data, catalog); // No constructors before all data and keys have been checked.
    if (!target.GetComponents().empty() || target.IsUpdating()) throw std::logic_error("Restore requires an empty scene");
    target.SetName(data.name);
    target.SetDocumentVersion(data.documentVersion);
    std::map<ComponentId, ComponentId> ids;
    for (const auto& o : data.components)
    {
        auto object = o.typeName == SceneAssets::ModelType ? SceneAssets::CreateModel(o.modelAsset, false) : catalog.Find(o.typeName)->create();
        if (!object) throw std::runtime_error("Component factory returned null");
        object->LoadProjectData(o.projectData);
        SceneAssets::Apply(*object, o.modelAsset, o.textureAsset, o.materialAsset);
        auto added = target.AddComponent(o.name, object, o.typeName);
        if (added.result != SceneResult::Ok || target.SetTransform(added.id, o.transform) != SceneResult::Ok)
            throw std::runtime_error("Cannot restore object: " + o.name);
        if (o.pointLight && target.SetPointLight(added.id, *o.pointLight) != SceneResult::Ok)
            Invalid("Point light properties require a point light component: " + o.name);
        ids.emplace(o.id, added.id);
        if (!o.parentId && !o.inheritParentScale) target.FindComponent(added.id)->SetParentWithoutScale(nullptr);
    }
    for (const auto& o : data.components)
        if (o.parentId && target.SetParent(ids.at(o.id), ids.at(o.parentId), o.inheritParentScale) != SceneResult::Ok)
            throw std::runtime_error("Cannot restore hierarchy");
    for (const auto& o : data.components)
        target.FindComponent(ids.at(o.id))->RemapProjectReferences(ids);
}
SceneSnapshot SceneDocument::Read(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) throw SceneDocumentError(EngineSceneIoError, "Cannot open scene file");
    std::string text; char chunk[8192];
    while (input.read(chunk, sizeof(chunk)) || input.gcount())
    {
        text.append(chunk, static_cast<size_t>(input.gcount()));
        if (text.size() > MaxBytes) Invalid("Scene exceeds 16 MiB");
    }
    if (!input.eof()) throw SceneDocumentError(EngineSceneIoError, "Cannot read scene file");
    return Parse(text);
}
void SceneDocument::Write(const std::filesystem::path& path, const SceneSnapshot& data)
{
    const auto text = Serialize(data); // Finish serialization before touching disk.
    static std::atomic<uint64_t> serial{0};
    std::filesystem::path temporary;
    HANDLE file = INVALID_HANDLE_VALUE;
    for (int attempt = 0; attempt < 100 && file == INVALID_HANDLE_VALUE; ++attempt)
    {
        temporary = path; temporary += L".tmp." + std::to_wstring(GetCurrentProcessId()) + L"." + std::to_wstring(++serial);
        file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE && GetLastError() != ERROR_FILE_EXISTS) break;
    }
    if (file == INVALID_HANDLE_VALUE) throw SceneDocumentError(EngineSceneIoError,
        "Cannot create temporary scene file '" + temporary.u8string() + "' (Windows error " + std::to_string(GetLastError()) + ")");
    DWORD written = 0;
    const bool ok = WriteFile(file, text.data(), static_cast<DWORD>(text.size()), &written, nullptr) &&
        written == text.size() && FlushFileBuffers(file);
    const bool closed = CloseHandle(file) != 0;
    if (!ok || !closed || !MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        DeleteFileW(temporary.c_str());
        throw SceneDocumentError(EngineSceneIoError, "Cannot commit scene file");
    }
}
