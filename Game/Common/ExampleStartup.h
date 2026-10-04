#pragma once
#include "Deployment.h"
#include <Engine/Resources/Paths.h>
#include <Engine/MainGame/BaseGameClass/Game.h>
#include <iostream>
#include <memory>

namespace ExampleStartup
{
inline bool ParseSmoke(int argc, char** argv)
{
    if (argc == 1) return false;
    if (argc == 2 && std::string(argv[1]) == "--smoke") return true;
    throw std::runtime_error("Usage: game.exe [--smoke]");
}

inline std::filesystem::path PrepareContent()
{
    const auto directory = ExampleDeployment::ExecutableDirectory();
    ExampleDeployment::ValidateContent(directory);
    EnginePaths::SetContentRoot(ExampleDeployment::ContentRoot(L"ENGINE_CONTENT_ROOT", directory / "EngineContent"));
    return ExampleDeployment::ContentRoot(L"GAME_CONTENT_ROOT", directory / "GameContent");
}

inline int ReportError(const std::exception& error, bool smoke)
{
    std::cerr << error.what() << '\n';
    if (!smoke) MessageBoxW(nullptr, ExampleDeployment::Wide(error.what()).c_str(), L"Game startup failed", MB_OK | MB_ICONERROR);
    return 1;
}

template<class T> class SmokeGame : public T
{
public:
    explicit SmokeGame(bool smoke) : remaining(smoke ? 30 : 0) {}
protected:
    void PreUpdate(float dt) override
    {
        T::PreUpdate(dt);
        if (remaining > 0 && --remaining == 0) PostQuitMessage(0);
    }
private:
    int remaining;
};

// Legacy scenes transfer a newly allocated object only after registration succeeds.
template<class T> inline void Register(Game& game, const std::string& name, T* component,
                                      const std::string& pixel = {}, const std::string& vertex = {})
{
    std::unique_ptr<T> owner(component);
    const auto result = game.RegisterComponent(name, owner.get(), pixel, vertex);
    if (result != RegisterResult::Ok)
        throw std::runtime_error("Cannot register " + name + ": " + ToString(result));
    owner.release();
}
}
