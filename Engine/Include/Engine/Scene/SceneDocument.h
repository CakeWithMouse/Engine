#pragma once
#include <Engine/Scene/Scene.h>
#include <Engine/Components/ComponentType.h>
#include <Engine/Runtime/SceneModuleApi.h>
#include <filesystem>
#include <stdexcept>

class SceneDocumentError : public std::runtime_error
{
public:
    uint32_t status;
    SceneDocumentError(uint32_t code, const std::string& message) : std::runtime_error(message), status(code) {}
};
// Module-local C++ API. Snapshot IDs are document-local, not retained on restore.
namespace SceneDocument
{
    SceneSnapshot Parse(const std::string& json);
    std::string Serialize(const SceneSnapshot& data);
    // Pure migration; callers choose a new project copy before writing.
    SceneSnapshot Migrate(const SceneSnapshot& data, uint32_t targetVersion);
    void Validate(const SceneSnapshot& data, ComponentTypeCatalog catalog);
    // Target must be empty and disposable: failures can leave a partially populated target.
    void Restore(const SceneSnapshot& data, ComponentTypeCatalog catalog, Scene& target);
    SceneSnapshot Read(const std::filesystem::path& path);
    void Write(const std::filesystem::path& path, const SceneSnapshot& data);
}
