#include <Engine/Runtime/SceneEditSession.h>
#include <Engine/Components/SpecificComponents/MeshComponent.h>
#include <Engine/Render/Material.h>
#include <Engine/Render/MaterialAsset.h>
#include <Engine/Render/MeshPrimitives.h>
#include <Engine/Render/ShaderCompiler.h>
#include <Engine/Scene/SceneAssets.h>
#include "../Resources/VfsRead.h"
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace
{
template<class F> uint32_t MaterialCall(char* error, uint32_t capacity, F&& action)
{
    if (capacity && !error) return EngineSceneInvalidArgument;
    if (capacity) error[0] = 0;
    try { return action(); }
    catch (const std::exception& e) { if (capacity) std::snprintf(error, capacity, "%s", e.what()); }
    catch (...) { if (capacity) std::snprintf(error, capacity, "Material operation failed"); }
    return EngineSceneFailure;
}

/** Physical path of a /Game .mat file. */
std::string MaterialFile(const char* material)
{
    const std::string path = material ? material : "";
    if (path.empty() || !SceneAssets::ValidPath(path) || path.size() < 4 || _stricmp(path.c_str() + path.size() - 4, ".mat") != 0)
        throw std::invalid_argument("Expected a .mat path inside /Game");
    const std::string physical = VfsRead::Active().Resolve(path);
    if (physical.empty()) throw std::invalid_argument("The material is outside the mounted project content: " + path);
    return physical;
}

std::string VirtualPath(const std::string& physical)
{
    return physical.empty() ? std::string() : VfsRead::Active().ToVirtualPath(physical);
}

/** One material on one shape, lit by a sun, with an orbit camera. */
class MaterialPreview final : public Game
{
public:
    void Setup()
    {
        SetSimulationEnabled(false); // time still runs for animated materials
        SetVSync(false);
        SetDirectionalLight({-0.45f, -1.0f, 0.6f}, {1.0f, 0.97f, 0.92f}, 1.0f);
        SetShadowSettings(false, 10.0f);
        GetPlayer()->SetCameraMode(CameraMode::Orbital);
        GetPlayer()->SetOrbitTarget({0.0f, 0.0f, 0.0f});
        GetPlayer()->SetOrbitRadius(2.4f);
        GetPlayer()->SetRotate(-2.3f, 0.35f);
    }

    void Show(std::shared_ptr<Material> material, uint32_t shape)
    {
        if (shownId_) GetScene().RemoveComponent(shownId_);
        shownId_ = 0;
        std::unique_ptr<GameComponent> object = std::make_unique<MeshComponent>();
        auto& mesh = static_cast<MeshComponent&>(*object);
        mesh.SetMesh(shape == EnginePreviewCube ? MeshPrimitives::Cube(1.0f)
                   : shape == EnginePreviewPlane ? MeshPrimitives::Plane(1.6f, 1.6f)
                   : MeshPrimitives::Sphere(0.6f, 64, 32));
        mesh.SetColor({1.0f, 1.0f, 1.0f, 1.0f});
        mesh.SetMaterial(std::move(material));
        const auto added = GetScene().AddComponent("Preview", object, "");
        if (added.result != SceneResult::Ok) throw std::runtime_error("Cannot create the preview object");
        shownId_ = added.id;
    }

protected:
    std::array<float, 4> GetClearColor() const override { return {0.15f, 0.16f, 0.19f, 1.0f}; }

private:
    ComponentId shownId_ = 0;
};
}

std::vector<Game*> SceneEditSession::Games()
{
    std::vector<Game*> games{edit_.get()};
    if (play_) games.push_back(play_.get());
    for (auto& preview : previews_) games.push_back(preview.second.get());
    return games;
}

void SceneEditSession::RenderPreviews(double dt)
{
    for (auto& preview : previews_) preview.second->RenderViewportFrame(static_cast<float>(dt));
}

template<class F> uint32_t SceneEditSession::EditMaterial(const char* material, F&& change)
{
    const std::string file = MaterialFile(material);
    bool found = false;
    for (Game* game : Games())
    {
        if (const auto loaded = game->FindMaterial(file))
        {
            change(*loaded);
            found = true;
        }
    }
    if (!found) return EngineSceneNotFound;
    modifiedMaterials_.insert(ShaderCompiler::NormalizePath(file));
    return EngineSceneOk;
}

void SceneEditSession::AddMaterialApi(EngineEditSessionApi& api) noexcept
{
    api.readMaterial = [](void* p, const char* material, EngineMaterialVisitor visit, void* user, char* error, uint32_t capacity) -> uint32_t {
        if (!visit) return EngineSceneInvalidArgument;
        return Invoke(p, [&](SceneEditSession& s) -> uint32_t {
            return MaterialCall(error, capacity, [&]() -> uint32_t {
                const std::string file = MaterialFile(material);
                // Any game with a device can load it: the edit scene, or a preview before the viewport exists.
                Game* owner = nullptr;
                for (Game* game : s.Games()) if (!owner && game->GetDevice()) owner = game;
                if (!owner) throw std::runtime_error("Attach the viewport or a material preview first");
                std::string loadError;
                const auto loaded = owner->LoadMaterial(file, &loadError);
                if (!loaded) throw std::runtime_error(loadError);
                // Render state edits compile on the next draw; the errors are reported now.
                if (loaded->NeedsCompile()) loaded->Compile(owner->GetDevice(), owner->GetContext());

                std::vector<EngineMaterialParameter> parameters;
                for (const MaterialParameter& parameter : loaded->GetParameters())
                {
                    EngineMaterialParameter value{parameter.Name.c_str(),
                        parameter.Type == MaterialParameterType::Unsupported ? 0u : static_cast<uint32_t>(parameter.Type) + 1u, {}, {}};
                    std::array<float, 4> current{};
                    loaded->GetParameter(parameter.Name, current);
                    std::copy(current.begin(), current.end(), value.value);
                    std::copy(parameter.Default.begin(), parameter.Default.end(), value.defaultValue);
                    parameters.push_back(value);
                }
                std::vector<std::string> texturePaths;
                for (const MaterialTexture& texture : loaded->GetTextures()) texturePaths.push_back(VirtualPath(texture.Path));
                std::vector<EngineMaterialTexture> textures;
                for (size_t i = 0; i < texturePaths.size(); ++i)
                    textures.push_back({loaded->GetTextures()[i].Name.c_str(), texturePaths[i].c_str()});

                const std::string shader = VirtualPath(loaded->GetSurfacePath());
                const MaterialRenderState& state = loaded->GetRenderState();
                EngineMaterialInfo info{shader.c_str(), loaded->GetErrors().c_str(),
                    {static_cast<uint32_t>(state.Blend), state.AlphaClip, state.bUnlit ? 1u : 0u, state.bTwoSided ? 1u : 0u,
                     state.bCastShadows ? 1u : 0u},
                    static_cast<uint32_t>(parameters.size()), parameters.data(),
                    static_cast<uint32_t>(textures.size()), textures.data(),
                    s.modifiedMaterials_.count(ShaderCompiler::NormalizePath(file)) ? 1u : 0u};
                visit(user, &info);
                return EngineSceneOk;
            });
        });
    };
    api.setMaterialRenderState = [](void* p, const char* material, const EngineMaterialRenderState* render) -> uint32_t {
        if (!render || render->blend > EngineMaterialAdditive || !std::isfinite(render->alphaClip) ||
            render->alphaClip < 0.0f || render->alphaClip > 1.0f || render->unlit > 1 || render->twoSided > 1 || render->castShadows > 1)
            return EngineSceneInvalidArgument;
        return Invoke(p, [&](SceneEditSession& s) -> uint32_t {
            return MaterialCall(nullptr, 0, [&]() -> uint32_t {
                MaterialRenderState state;
                state.Blend = static_cast<MaterialBlendMode>(render->blend);
                state.AlphaClip = render->alphaClip;
                state.bUnlit = render->unlit != 0;
                state.bTwoSided = render->twoSided != 0;
                state.bCastShadows = render->castShadows != 0;
                return s.EditMaterial(material, [&](Material& m) { m.SetRenderState(state); });
            });
        });
    };
    api.setMaterialParameter = [](void* p, const char* material, const char* name, const float value[4]) -> uint32_t {
        if (!name || !value) return EngineSceneInvalidArgument;
        for (int i = 0; i < 4; ++i) if (!std::isfinite(value[i])) return EngineSceneInvalidArgument;
        return Invoke(p, [&](SceneEditSession& s) -> uint32_t {
            return MaterialCall(nullptr, 0, [&]() -> uint32_t {
                bool known = true;
                const uint32_t result = s.EditMaterial(material, [&](Material& m) {
                    known = m.SetParameter(name, {value[0], value[1], value[2], value[3]}) && known;
                });
                return result == EngineSceneOk && !known ? EngineSceneNotFound : result;
            });
        });
    };
    api.setMaterialTexture = [](void* p, const char* material, const char* name, const char* texture,
        char* error, uint32_t capacity) -> uint32_t {
        if (!name || !texture) return EngineSceneInvalidArgument;
        return Invoke(p, [&](SceneEditSession& s) -> uint32_t {
            return MaterialCall(error, capacity, [&]() -> uint32_t {
                SceneAssets::ValidateTexture(texture);
                const std::string physical = *texture ? VfsRead::Active().Resolve(texture) : std::string();
                bool loaded = true;
                const uint32_t result = s.EditMaterial(material, [&](Material& m) { loaded = m.SetTexture(name, physical) && loaded; });
                if (result == EngineSceneOk && !loaded) throw std::runtime_error(std::string("Unknown texture slot or unreadable image: ") + name);
                return result;
            });
        });
    };
    api.saveMaterial = [](void* p, const char* material, char* error, uint32_t capacity) -> uint32_t {
        return Invoke(p, [&](SceneEditSession& s) -> uint32_t {
            return MaterialCall(error, capacity, [&]() -> uint32_t {
                const std::string file = MaterialFile(material);
                std::shared_ptr<Material> loaded;
                for (Game* game : s.Games()) if (!loaded) loaded = game->FindMaterial(file);
                if (!loaded) return EngineSceneNotFound;
                // Merged into the file as it is on disk: fields the engine does not know are kept.
                MaterialAsset base;
                std::string message;
                MaterialAsset::Load(file, base, message);
                if (!MaterialAsset::FromMaterial(*loaded, file, base).Save(file, message)) throw std::runtime_error(message);
                s.modifiedMaterials_.erase(ShaderCompiler::NormalizePath(file));
                return EngineSceneOk;
            });
        });
    };
    api.revertMaterial = [](void* p, const char* material, char* error, uint32_t capacity) -> uint32_t {
        return Invoke(p, [&](SceneEditSession& s) -> uint32_t {
            return MaterialCall(error, capacity, [&]() -> uint32_t {
                const std::string file = MaterialFile(material);
                std::string message;
                for (Game* game : s.Games())
                    if (game->FindMaterial(file) && !game->ReloadMaterial(file, &message)) throw std::runtime_error(message);
                s.modifiedMaterials_.erase(ShaderCompiler::NormalizePath(file));
                return EngineSceneOk;
            });
        });
    };
    api.reloadShaderFile = [](void* p, const char* path) -> uint32_t {
        if (!path) return EngineSceneInvalidArgument;
        return Invoke(p, [&](SceneEditSession& s) -> uint32_t {
            for (Game* game : s.Games())
            {
                if (*path) game->InvalidateShaderFile(path);
                else game->InvalidateAllShaders();
            }
            return EngineSceneOk;
        });
    };

    api.attachMaterialPreview = [](void* p, void* hwnd, uint32_t w, uint32_t h, uint64_t* preview) -> uint32_t {
        if (!preview) return EngineSceneInvalidArgument;
        *preview = 0;
        if (!hwnd || !IsWindow(static_cast<HWND>(hwnd)) || !w || !h || w > INT_MAX || h > INT_MAX) return EngineSceneInvalidArgument;
        return Invoke(p, [&](SceneEditSession& s) -> uint32_t {
            auto game = std::make_unique<MaterialPreview>();
            if (!game->InitializeViewport(static_cast<HWND>(hwnd), static_cast<int>(w), static_cast<int>(h))) return EngineSceneFailure;
            game->Setup();
            *preview = s.nextPreview_++;
            s.previews_.emplace(*preview, std::move(game));
            return EngineSceneOk;
        });
    };
    api.resizeMaterialPreview = [](void* p, uint64_t preview, uint32_t w, uint32_t h) -> uint32_t {
        if (w > INT_MAX || h > INT_MAX) return EngineSceneInvalidArgument;
        return Invoke(p, [&](SceneEditSession& s) -> uint32_t {
            const auto found = s.previews_.find(preview);
            if (found == s.previews_.end()) return EngineSceneNotFound;
            found->second->OnWindowResized(static_cast<int>(w), static_cast<int>(h), !w || !h);
            return EngineSceneOk;
        });
    };
    api.detachMaterialPreview = [](void* p, uint64_t preview) -> uint32_t {
        return Invoke(p, [&](SceneEditSession& s) -> uint32_t {
            return s.previews_.erase(preview) ? EngineSceneOk : EngineSceneNotFound;
        });
    };
    api.showMaterialPreview = [](void* p, uint64_t preview, const char* material, uint32_t shape, char* error, uint32_t capacity) -> uint32_t {
        if (shape > EnginePreviewPlane) return EngineSceneInvalidArgument;
        return Invoke(p, [&](SceneEditSession& s) -> uint32_t {
            return MaterialCall(error, capacity, [&]() -> uint32_t {
                const auto found = s.previews_.find(preview);
                if (found == s.previews_.end()) return EngineSceneNotFound;
                auto& game = static_cast<MaterialPreview&>(*found->second);
                std::string message;
                auto loaded = game.LoadMaterial(MaterialFile(material), &message);
                if (!loaded) throw std::runtime_error(message);
                game.Show(std::move(loaded), shape);
                return EngineSceneOk;
            });
        });
    };
    api.materialPreviewCamera = [](void* p, uint64_t preview, float yaw, float pitch, float zoom) -> uint32_t {
        if (!std::isfinite(yaw) || !std::isfinite(pitch) || !std::isfinite(zoom)) return EngineSceneInvalidArgument;
        return Invoke(p, [&](SceneEditSession& s) -> uint32_t {
            const auto found = s.previews_.find(preview);
            if (found == s.previews_.end()) return EngineSceneNotFound;
            found->second->GetPlayer()->MoveEditorCamera(0.0f, 0.0f, 0.0f, yaw, pitch, zoom);
            return EngineSceneOk;
        });
    };
}
