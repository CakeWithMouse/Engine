#include <Engine/Runtime/SceneEditSession.h>
#include <Engine/MainGame/Player.h>
#include <Engine/Scene/SceneDocument.h>
#include <cstring>
#include <cmath>
#include <climits>
#include <limits>
#include <map>
#include <stdexcept>

namespace
{
template<class F> uint32_t DocumentCall(const char* path, char* error, uint32_t capacity, F&& action)
{
    if (capacity && !error) return EngineSceneInvalidArgument;
    if (capacity) error[0] = 0;
    auto message = [&](const char* text) {
        if (capacity) { const auto size = (std::min)(strlen(text), size_t(capacity - 1)); memcpy(error, text, size); error[size] = 0; }
    };
    try
    {
        if (!path || !*path || !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, nullptr, 0))
            throw SceneDocumentError(EngineSceneInvalidArgument, "Expected absolute UTF-8 path");
        const auto file = std::filesystem::u8path(path);
        if (!file.is_absolute()) throw SceneDocumentError(EngineSceneInvalidArgument, "Expected absolute path");
        action(file);
        return EngineSceneOk;
    }
    catch (const SceneDocumentError& e) { message(e.what()); return e.status; }
    catch (const std::exception& e) { message(e.what()); return EngineSceneFailure; }
    catch (...) { message("Scene operation failed"); return EngineSceneFailure; }
}
std::unique_ptr<Game> EmptyGame()
{
    auto game = std::make_unique<Game>();
    game->SetSimulationEnabled(false); // frame() is the sole simulation owner
    game->SetVSync(false);
    return game;
}
}
SceneEditSession::SceneEditSession(ComponentTypeCatalog catalog)
    : catalog_(catalog), edit_(EmptyGame()), commands_(edit_->GetScene(), catalog, &busy_) {}

std::unique_ptr<Game> SceneEditSession::CopyForPlay()
{
    auto candidate = EmptyGame();
    SceneDocument::Restore(edit_->GetScene().GetSnapshot(), catalog_, candidate->GetScene());
    return candidate;
}
uint32_t SceneEditSession::Control(uint32_t command)
{
    if (command == EnginePlay)
    {
        if (state_ != EngineEditing) return EngineSceneBusy;
        auto candidate = CopyForPlay(); // failure leaves edit state/viewport untouched
        if (attached_)
        {
            if (!width_ || !height_) return EngineSceneBusy;
            edit_->SuspendViewportTargets();
            try
            {
                if (!candidate->InitializePlayViewport(*edit_)) throw std::runtime_error("Play viewport failed");
                *candidate->GetPlayer() = *edit_->GetPlayer();
                candidate->GetPlayer()->SetGamePointer(candidate.get());
            }
            catch (...)
            {
                candidate.reset();
                edit_->RestoreViewportTargets();
                throw;
            }
        }
        play_ = std::move(candidate);
        state_ = EnginePlaying;
        commands_.SetReadOnly(true);
        return EngineSceneOk;
    }
    if (command == EngineStop)
    {
        if (state_ == EngineEditing) return EngineSceneOk;
        play_.reset();
        state_ = EngineEditing;
        commands_.SetReadOnly(false);
        if (attached_)
        {
            if (!edit_->RestoreViewportTargets()) return EngineSceneFailure;
            edit_->OnWindowResized(static_cast<int>(width_), static_cast<int>(height_), !width_ || !height_);
        }
        return EngineSceneOk;
    }
    if (command == EnginePause && state_ == EnginePlaying) { state_ = EnginePaused; return EngineSceneOk; }
    if (command == EngineResume && state_ == EnginePaused) { state_ = EnginePlaying; return EngineSceneOk; }
    return EngineSceneInvalidArgument;
}
EngineEditSessionApi SceneEditSession::GetApi() noexcept
{
    EngineEditSessionApi api{ENGINE_EDIT_API_VERSION, this, commands_.GetApi(),
        [](void* p, uint32_t command) -> uint32_t {
            return Invoke(p, [&](SceneEditSession& s) { return s.Control(command); });
        },
        [](void* p, uint32_t* state) -> uint32_t {
            if (!state) return EngineSceneInvalidArgument;
            return Invoke(p, [&](SceneEditSession& s) -> uint32_t { *state = s.state_; return EngineSceneOk; });
        },
        [](void* p, EngineSceneInfo* info, EngineSceneObjectVisitor visit, void* user) -> uint32_t {
            return Invoke(p, [&](SceneEditSession& s) {
                SceneCommands view(s.Active().GetScene(), s.catalog_);
                auto api = view.GetApi();
                return api.readScene(api.context, info, visit, user);
            });
        },
        [](void* p, uint64_t child, uint64_t parent, uint32_t inherit) -> uint32_t {
            return Invoke(p, [&](SceneEditSession& s) -> uint32_t {
                if (s.state_ != EngineEditing) return EngineSceneReadOnly;
                if (inherit > 1) return EngineSceneInvalidArgument;
                auto& scene = s.edit_->GetScene();
                const auto result = parent ? scene.SetParent(child, parent, inherit != 0) : scene.ClearParent(child);
                if (result == SceneResult::Ok) return EngineSceneOk;
                if (result == SceneResult::NotFound) return EngineSceneNotFound;
                return EngineSceneInvalidArgument;
            });
        },
        [](void* p, void* hwnd, uint32_t w, uint32_t h) -> uint32_t {
            return Invoke(p, [&](SceneEditSession& s) -> uint32_t {
                if (s.attached_ || s.state_ != EngineEditing) return EngineSceneBusy;
                if (!hwnd || !IsWindow(static_cast<HWND>(hwnd)) || !w || !h || w > INT_MAX || h > INT_MAX)
                    return EngineSceneInvalidArgument;
                if (!s.edit_->InitializeViewport(static_cast<HWND>(hwnd), static_cast<int>(w), static_cast<int>(h), true))
                    return EngineSceneFailure;
                s.edit_->GetPlayer()->SetPosition({0.0f, 0.0f, 5.0f}); // default yaw faces -Z, toward scene origin
                s.attached_ = true; s.width_ = w; s.height_ = h;
                return EngineSceneOk;
            });
        },
        [](void* p, uint32_t w, uint32_t h) -> uint32_t {
            return Invoke(p, [&](SceneEditSession& s) -> uint32_t {
                if (!s.attached_ || w > INT_MAX || h > INT_MAX) return EngineSceneInvalidArgument;
                s.width_ = w; s.height_ = h;
                s.Active().OnWindowResized(static_cast<int>(w), static_cast<int>(h), !w || !h);
                return EngineSceneOk;
            });
        },
        [](void* p, float r, float u, float f, float y, float pitch, float z) -> uint32_t {
            return Invoke(p, [&](SceneEditSession& s) -> uint32_t {
                if (!s.attached_ || !std::isfinite(r) || !std::isfinite(u) || !std::isfinite(f) ||
                    !std::isfinite(y) || !std::isfinite(pitch) || !std::isfinite(z)) return EngineSceneInvalidArgument;
                s.Active().GetPlayer()->MoveEditorCamera(r, u, f, y, pitch, z);
                return EngineSceneOk;
            });
        },
        [](void* p, double dt) -> uint32_t {
            return Invoke(p, [&](SceneEditSession& s) -> uint32_t {
                if (!std::isfinite(dt) || dt < 0 || dt > 0.1) return EngineSceneInvalidArgument;
                if (s.state_ == EnginePlaying)
                {
                    for (const auto& item : s.play_->GetScene().GetComponents()) item.second->SetGame(s.play_.get());
                    try { s.play_->GetScene().Tick(static_cast<float>(dt)); }
                    catch (...) { s.state_ = EnginePaused; return EngineSceneSimulationFailed; }
                }
                if (s.attached_ && !s.Active().RenderViewportFrame(0)) return EngineSceneFailure;
                return EngineSceneOk;
            });
        },
        [](void* p, const char* path, char* error, uint32_t capacity) -> uint32_t {
            if (capacity && error) error[0] = 0;
            return Invoke(p, [&](SceneEditSession& s) {
                return DocumentCall(path, error, capacity, [&](const std::filesystem::path& file) {
                    const auto data = s.edit_->GetScene().GetSnapshot();
                    SceneDocument::Validate(data, s.catalog_);
                    SceneDocument::Write(file, data);
                });
            });
        },
        [](void* p, const char* path, uint64_t* newId, char* error, uint32_t capacity) -> uint32_t {
            if (capacity && error) error[0] = 0;
            if (!newId) return EngineSceneInvalidArgument;
            *newId = 0;
            return Invoke(p, [&](SceneEditSession& s) -> uint32_t {
                if (s.state_ != EngineEditing) return EngineSceneReadOnly;
                if (s.attached_ && (!s.width_ || !s.height_)) return EngineSceneBusy;
                return DocumentCall(path, error, capacity, [&](const std::filesystem::path& file) {
                    auto data = SceneDocument::Read(file);
                    auto candidate = EmptyGame();
                    SceneDocument::Restore(data, s.catalog_, candidate->GetScene());
                    if (s.attached_)
                    {
                        s.edit_->SuspendViewportTargets();
                        try
                        {
                            if (!candidate->InitializePlayViewport(*s.edit_)) throw std::runtime_error("Loaded scene viewport failed");
                            *candidate->GetPlayer() = *s.edit_->GetPlayer();
                            candidate->GetPlayer()->SetGamePointer(candidate.get());
                        }
                        catch (...)
                        {
                            candidate.reset();
                            s.edit_->RestoreViewportTargets();
                            throw;
                        }
                    }
                    s.commands_.Rebind(candidate->GetScene());
                    s.edit_.swap(candidate); // noexcept commit; old scene dies inside this call
                    *newId = s.edit_->GetScene().GetId();
                });
            });
        },
        [](void* p, float x, float y, uint64_t* id) -> uint32_t {
            if (!id) return EngineSceneInvalidArgument;
            *id = 0;
            return Invoke(p, [&](SceneEditSession& s) -> uint32_t {
                if (!s.attached_ || !s.width_ || !s.height_ || !std::isfinite(x) || !std::isfinite(y) ||
                    std::abs(x) > 1 || std::abs(y) > 1) return EngineSceneInvalidArgument;
                using namespace DirectX;
                auto* player = s.Active().GetPlayer();
                const auto view = XMMatrixTranspose(XMLoadFloat4x4(&player->GetViewMatrix()));
                const auto projection = XMMatrixTranspose(XMLoadFloat4x4(&player->GetProjectionMatrix()));
                const auto inverse = XMMatrixInverse(nullptr, view * projection);
                XMFLOAT3 nearPoint, farPoint;
                XMStoreFloat3(&nearPoint, XMVector3TransformCoord(XMVectorSet(x,y,0,1), inverse));
                XMStoreFloat3(&farPoint, XMVector3TransformCoord(XMVectorSet(x,y,1,1), inverse));
                const glm::vec3 origin(nearPoint.x, nearPoint.y, nearPoint.z);
                const auto direction = glm::normalize(glm::vec3(farPoint.x, farPoint.y, farPoint.z) - origin);
                float distance = std::numeric_limits<float>::max();
                for (const auto& entry : s.Active().GetScene().GetComponents())
                    if (entry.second->RaycastPrimitive(origin, direction, distance)) *id = entry.second->GetComponentId();
                return EngineSceneOk;
            });
        },
        [](void* p, uint64_t id) -> uint32_t {
            return Invoke(p, [&](SceneEditSession& s) -> uint32_t {
                if (!s.attached_) return EngineSceneInvalidArgument;
                auto* scene = &s.Active().GetScene();
                auto* object = scene->FindComponent(id);
                if (!object) return EngineSceneNotFound;
                auto* player = s.Active().GetPlayer();
                player->SetPosition(object->GetCenter() - player->GetForward() * (std::max)(2.0f, object->GetWorldBoundingRadius() * 3.0f));
                player->MoveEditorCamera(0,0,0,0,0,0);
                return EngineSceneOk;
            });
        }};
    AddAssetApi(api);
    return api;
}
