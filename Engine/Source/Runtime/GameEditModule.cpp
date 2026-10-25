#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <Engine/Runtime/GameEditModule.h>
#include <Engine/Runtime/EditExtensions.h>
#include <stdexcept>

GameEditModule::GameEditModule(const std::filesystem::path& module, const std::filesystem::path& content,
    const std::filesystem::path& projectContent)
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
            const auto query = reinterpret_cast<EngineGetEditExtensionFn>(GetProcAddress(static_cast<HMODULE>(library_), "EngineGetEditExtension"));
            EnginePointLightApi light{};
            EngineDocumentCapabilities document{};
            if (query && query(api_.context, EngineDocumentExtension, 1, sizeof(document), &document))
            {
                if (document.version != 1 || document.minReadVersion > 1 || document.maxReadVersion < document.preferredWriteVersion ||
                    document.preferredWriteVersion < 1 || document.preferredWriteVersion > 2)
                    throw std::runtime_error("Unsupported document extension");
                preferredSceneVersion_ = document.preferredWriteVersion;
            }
            if (query && query(api_.context, EnginePointLightExtension, 1, sizeof(light), &light))
            {
                if (light.version != 1 || light.context != api_.scene.context || !light.set)
                    throw std::runtime_error("Invalid point light extension");
                api_.scene.setPointLight = light.set;
            }
        }
        else
        {
            const auto oldGet = reinterpret_cast<EngineLegacyV4::GetFactory>(get);
            if (!oldGet || !oldGet(4, sizeof(legacyFactory_), &legacyFactory_) || legacyFactory_.version != 4 ||
                !legacyFactory_.create || !legacyFactory_.destroy)
                throw std::runtime_error("Unsupported game DLL. Open it with its original engine version or rebuild the custom game module.");
            if (!legacyFactory_.create(content.u8string().c_str(), &legacy_) || !EngineLegacyV4::Complete(legacy_))
                throw std::runtime_error("Cannot create legacy edit session");
            api_ = EngineLegacyV4::Adapt(legacy_);
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
        if (legacy_.context && legacyFactory_.destroy) legacyFactory_.destroy(legacy_.context);
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
    if (legacy_.context) legacyFactory_.destroy(legacy_.context);
    else if (context) factory_.destroy(context);
    if (library_) FreeLibrary(static_cast<HMODULE>(library_));
}
