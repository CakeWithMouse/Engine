#pragma once
#include <Engine/Components/ComponentType.h>
#include <stdexcept>

// Specialize for an independent hierarchy. The actual T never crosses the DLL
// boundary. A non-rendering scene-owned adapter gives it explicit lifetime.
template<class T> struct ProjectObjectAdapter
{
    static std::unique_ptr<T> Create() { return std::make_unique<T>(); }
    static void Tick(T&, float) {}
    static std::string Save(const T&) { return {}; }
    static void Load(T&, const std::string& data)
    {
        if (!data.empty()) throw std::runtime_error("Object has no data codec");
    }
};
template<class T> class ProjectObjectComponent final : public GameComponent
{
    std::unique_ptr<T> value_ = ProjectObjectAdapter<T>::Create();
public:
    ProjectObjectComponent() { if (!value_) throw std::runtime_error("Object factory returned null"); }
    T& Value() { return *value_; }
    bool IsRenderable() const override { return false; }
    bool CastsShadow() const override { return false; }
    void Tick(float dt) override { GameComponent::Tick(dt); ProjectObjectAdapter<T>::Tick(*value_, dt); }
    std::string SaveProjectData() const override { return ProjectObjectAdapter<T>::Save(*value_); }
    void LoadProjectData(const std::string& data) override { ProjectObjectAdapter<T>::Load(*value_, data); }
    template<class A, class = void> struct HasRemap : std::false_type {};
    template<class A> struct HasRemap<A, std::void_t<decltype(A::Remap(std::declval<T&>(), std::declval<const std::map<ComponentId,ComponentId>&>()))>> : std::true_type {};
    void RemapProjectReferences(const std::map<ComponentId,ComponentId>& ids) override {
        if constexpr (HasRemap<ProjectObjectAdapter<T>>::value) ProjectObjectAdapter<T>::Remap(*value_, ids);
    }
};
namespace EngineComponentDetail
{
    template<class T> std::unique_ptr<GameComponent> CreateObject()
    { return std::make_unique<ProjectObjectComponent<T>>(); }
}
struct GameType
{
    const char* name;
    const char* label;
    std::unique_ptr<Game> (*create)();
};
struct GameTypeCatalog { const GameType* data = nullptr; size_t size = 0; };
template<class T> std::unique_ptr<Game> CreateProjectGame()
{
    static_assert(std::is_convertible_v<T*, Game*> && !std::is_abstract_v<T>);
    return std::make_unique<T>();
}
