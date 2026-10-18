#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <Engine/Runtime/GameEditModule.h>
#include <stdexcept>

GameEditModule::GameEditModule(const std::filesystem::path& module, const std::filesystem::path& content)
{
    if (!module.is_absolute() || !content.is_absolute()) throw std::invalid_argument("Edit module/content paths must be absolute");
    library_ = LoadLibraryExW(module.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (!library_) throw std::runtime_error("Cannot load edit game module");
    try
    {
        auto get = reinterpret_cast<EngineGetEditModuleApiFn>(GetProcAddress(static_cast<HMODULE>(library_), "EngineGetEditModuleApi"));
        if (!get || !get(ENGINE_EDIT_API_VERSION, sizeof(factory_), &factory_) ||
            factory_.version != ENGINE_EDIT_API_VERSION || !factory_.create || !factory_.destroy)
            throw std::runtime_error("Missing/incompatible edit-safe module API");
        if (!factory_.create(content.u8string().c_str(), &api_) || !IsCompleteEditSessionApi(api_))
            throw std::runtime_error("Cannot create edit game session");
    }
    catch (...)
    {
        if (api_.context && factory_.destroy) factory_.destroy(api_.context);
        api_ = {};
        FreeLibrary(static_cast<HMODULE>(library_)); library_ = nullptr;
        throw;
    }
}
GameEditModule::~GameEditModule()
{
    const auto context = api_.context;
    api_ = {};
    if (context) factory_.destroy(context);
    if (library_) FreeLibrary(static_cast<HMODULE>(library_));
}
