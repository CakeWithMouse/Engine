#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <Engine/Runtime/GameEditModule.h>
#include <Engine/Runtime/EditExtensions.h>
#include <stdexcept>
#include <memory>
#include <cstdio>
#include <utility>

GameEditModule::GameEditModule(const std::filesystem::path& module, const std::filesystem::path& content,
    const std::filesystem::path& projectContent)
    : engineContent_(content), projectContent_(projectContent), loadedModule_(module)
{
    if (!module.is_absolute() || !content.is_absolute()) throw std::invalid_argument("Edit module/content paths must be absolute");
    library_ = LoadLibraryExW(module.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (!library_) throw std::runtime_error("Cannot load edit game module");
    try
    {
        auto get = reinterpret_cast<EngineGetEditModuleApiFn>(GetProcAddress(static_cast<HMODULE>(library_), "EngineGetEditModuleApi"));
        if (get && get(ENGINE_EDIT_API_VERSION, sizeof(factory_), &factory_))
        {
            if (factory_.version != ENGINE_EDIT_API_VERSION || !factory_.create || !factory_.destroy ||
                !factory_.create(content.u8string().c_str(), &api_) || !IsCompleteEditSessionApi(api_))
                throw std::runtime_error("Cannot create edit game session");

        }
        else if (get && reinterpret_cast<EngineLegacyV5::GetFactory>(get)(5, sizeof(legacy5Factory_), &legacy5Factory_))
        {
            if (legacy5Factory_.version != 5 || !legacy5Factory_.create || !legacy5Factory_.destroy ||
                !legacy5Factory_.create(content.u8string().c_str(), legacy5_.get()) || !EngineLegacyV5::Complete(*legacy5_))
                throw std::runtime_error("Cannot create v5 edit session");
            api_ = EngineLegacyV5::Adapt(*legacy5_);
        }
        else
        {
            const auto oldGet = reinterpret_cast<EngineLegacyV4::GetFactory>(get);
            if (!oldGet || !oldGet(4, sizeof(legacyFactory_), &legacyFactory_) || legacyFactory_.version != 4 ||
                !legacyFactory_.create || !legacyFactory_.destroy)
                throw std::runtime_error("Unsupported game DLL. Open it with its original engine version or rebuild the custom game module.");
            if (!legacyFactory_.create(content.u8string().c_str(), legacy_.get()) || !EngineLegacyV4::Complete(*legacy_))
                throw std::runtime_error("Cannot create legacy edit session");
            api_ = EngineLegacyV4::Adapt(*legacy_);
        }
        if (!legacy_->context)
        {
            void* extensionContext = legacy5_->context ? legacy5_->context : api_.context;
            const auto query = reinterpret_cast<EngineGetEditExtensionFn>(GetProcAddress(static_cast<HMODULE>(library_), "EngineGetEditExtension"));
            if (query && query(extensionContext, EngineViewportInputExtension, 1, sizeof(viewportInput_), &viewportInput_))
                if (viewportInput_.version != 1 || viewportInput_.context != extensionContext || !viewportInput_.focus || !viewportInput_.key || !viewportInput_.pointer || !viewportInput_.objectKind)
                    throw std::runtime_error("Invalid viewport input API");
            if (query && query(extensionContext, EngineEditorCameraExtension, 1, sizeof(camera_), &camera_))
                if (camera_.version != 1 || camera_.context != extensionContext || !camera_.read || !camera_.write)
                    throw std::runtime_error("Invalid editor camera API");
            EnginePointLightApi light{};
            EngineDocumentCapabilities document{};
            if (query && query(extensionContext, EngineViewportOverlayExtension, 1, sizeof(overlay_), &overlay_))
                if (overlay_.version != 1 || overlay_.context != extensionContext || !overlay_.getCamera || !overlay_.setOverlayLines || !overlay_.setOverlayIcon || !overlay_.setOverlayBillboards)
                    throw std::runtime_error("Invalid viewport overlay API");
            if (query && query(extensionContext, EngineSelectionExtension, 1, sizeof(selection_), &selection_))
                if (selection_.version != 1 || selection_.context != extensionContext || !selection_.select)
                    throw std::runtime_error("Invalid selection API");
            if (query && query(extensionContext, EngineProjectDataExtension, 1, sizeof(projectData_), &projectData_))
                if (projectData_.version != 1 || projectData_.context != extensionContext || !projectData_.read || !projectData_.write || !projectData_.remapDuplicate)
                    throw std::runtime_error("Invalid project data API");
            if (query && query(extensionContext, EngineDocumentExtension, 1, sizeof(document), &document))
            {
                if (document.version != 1 || document.minReadVersion > 1 || document.maxReadVersion < document.preferredWriteVersion ||
                    document.preferredWriteVersion < 1 || document.preferredWriteVersion > 3)
                    throw std::runtime_error("Unsupported document extension");
                preferredSceneVersion_ = document.preferredWriteVersion;
            }
            if (query && query(extensionContext, EnginePointLightExtension, 1, sizeof(light), &light))
            {
                if (light.version != 1 || light.context != (legacy5_->context ? legacy5_->scene.context : api_.scene.context) || !light.set)
                    throw std::runtime_error("Invalid point light extension");
                if (legacy5_->context) legacy5_->scene.setPointLight = light.set;
                else api_.scene.setPointLight = light.set;
            }
        }
        if (!projectContent.empty())
        {
            char error[1024]{};
            if (api_.setContentRoot(api_.context, projectContent.u8string().c_str(), error, sizeof(error)) != EngineSceneOk)
                throw std::runtime_error(std::string("Cannot configure project assets: ") + error);
        }
    }
    catch (...)
    {
        if (legacy5_->context && legacy5Factory_.destroy) legacy5Factory_.destroy(legacy5_->context);
        else if (legacy_->context && legacyFactory_.destroy) legacyFactory_.destroy(legacy_->context);
        else if (api_.context && factory_.destroy) factory_.destroy(api_.context);
        api_ = {};
        FreeLibrary(static_cast<HMODULE>(library_)); library_ = nullptr;
        throw;
    }
}
GameEditModule::~GameEditModule()
{
    const auto context = api_.context;
    api_ = {};
    if (legacy5_->context) legacy5Factory_.destroy(legacy5_->context);
    else if (legacy_->context) legacyFactory_.destroy(legacy_->context);
    else if (context) factory_.destroy(context);
    if (library_) FreeLibrary(static_cast<HMODULE>(library_));
}

uint32_t GameEditModule::Reload(const std::filesystem::path& module, const std::filesystem::path& checkpoint,
    void* hwnd, uint32_t width, uint32_t height, char* error, uint32_t capacity) noexcept
{
    if (capacity && !error) return EngineSceneInvalidArgument;
    if (capacity) error[0] = 0;
    try {
        uint32_t state = 0;
        if (api_.getState(api_.context, &state) != EngineSceneOk || state != EngineEditing) return EngineSceneReadOnly;
        // Validated builds use immutable unique paths. An unchanged build must not
        // create two sessions sharing the same DLL-local VFS globals.
        if (std::filesystem::exists(module) && std::filesystem::equivalent(module,loadedModule_)) return EngineSceneOk;
        auto candidate = std::make_unique<GameEditModule>(module, engineContent_, projectContent_);
        uint64_t id = 0;
        auto result = candidate->api_.loadScene(candidate->api_.context, checkpoint.u8string().c_str(), &id, error, capacity);
        if (result != EngineSceneOk) return result;
        EngineEditorCameraState camera{};
        const bool hasCamera = camera_.read && camera_.read(camera_.context, &camera) == EngineSceneOk;
        if (hwnd) {
            result = candidate->api_.attachViewport(candidate->api_.context, hwnd, width, height);
            if (result != EngineSceneOk) throw std::runtime_error("Candidate viewport initialization failed; previous module kept");
            if (hasCamera && candidate->camera_.write && candidate->camera_.write(candidate->camera_.context, &camera) != EngineSceneOk)
                throw std::runtime_error("Cannot restore editor camera");
            if (candidate->api_.frame(candidate->api_.context, 0) != EngineSceneOk)
                throw std::runtime_error("Candidate viewport frame failed; previous module kept");
        }
        if (viewportInput_.focus) viewportInput_.focus(viewportInput_.context, 0);
        // No callable address escapes before commit. Old objects die before the old DLL unloads.
        using std::swap;
        swap(loadedModule_, candidate->loadedModule_);
        swap(library_, candidate->library_); swap(factory_, candidate->factory_); swap(api_, candidate->api_);
        swap(projectData_, candidate->projectData_); swap(selection_, candidate->selection_);
        swap(viewportInput_, candidate->viewportInput_); swap(camera_, candidate->camera_);
        swap(legacyFactory_, candidate->legacyFactory_); swap(legacy_, candidate->legacy_);
        swap(legacy5Factory_, candidate->legacy5Factory_); swap(legacy5_, candidate->legacy5_);
        // Heap-owned adapter tables move with their DLL, preserving session identity.
        swap(preferredSceneVersion_, candidate->preferredSceneVersion_);
        return EngineSceneOk;
    } catch (const std::exception& e) { if (capacity) std::snprintf(error, capacity, "%s", e.what()); }
      catch (...) { if (capacity) std::snprintf(error, capacity, "Reload failed; previous module kept"); }
    return EngineSceneFailure;
}
