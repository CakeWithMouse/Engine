#include <Engine/Runtime/SceneEditSession.h>
#include <Engine/Scene/SceneAssets.h>
#include <Engine/Sound/AudioEngine.h>
#include "../Resources/VfsRead.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <stdexcept>

EngineAudioPreviewApi SceneEditSession::GetAudioPreviewApi() noexcept
{
    EngineAudioPreviewApi api{};
    api.version = 1;
    api.context = this;
    api.play = [](void* context, const char* path, char* error, uint32_t capacity) -> uint32_t {
        if (capacity && !error) return EngineSceneInvalidArgument;
        if (capacity) error[0] = 0;
        return Invoke(context, [&](SceneEditSession& session) -> uint32_t {
            if (session.state_ != EngineEditing) return EngineSceneReadOnly;
            if (!path || !*path || !SceneAssets::ValidPath(path)) return EngineSceneInvalidArgument;
            try
            {
                auto extension = std::filesystem::u8path(path).extension().string();
                std::transform(extension.begin(), extension.end(), extension.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (extension != ".wav" && extension != ".ogg" && extension != ".mp3" && extension != ".flac")
                    return EngineSceneInvalidArgument;
                const auto physical = VfsRead::Active().Resolve(path);
                if (physical.empty() || !std::filesystem::is_regular_file(std::filesystem::u8path(physical)))
                    return EngineSceneInvalidArgument;
                auto* audio = session.edit_->GetAudioEngine();
                if (!audio) throw std::runtime_error("Audio device is unavailable");
                const auto sound = audio->LoadSound(std::filesystem::u8path(physical).wstring());
                if (!sound.IsValid()) throw std::runtime_error("Cannot decode audio asset");
                audio->StopAll();
                if (!audio->Play2D(sound)) throw std::runtime_error("Cannot play audio asset");
                return EngineSceneOk;
            }
            catch (const std::exception& e) { if (capacity) std::snprintf(error, capacity, "%s", e.what()); }
            catch (...) { if (capacity) std::snprintf(error, capacity, "Audio preview failed"); }
            return EngineSceneFailure;
        });
    };
    api.stop = [](void* context) -> uint32_t {
        return Invoke(context, [](SceneEditSession& session) -> uint32_t {
            if (session.state_ != EngineEditing) return EngineSceneReadOnly;
            if (auto* audio = session.edit_->GetAudioEngine()) audio->StopAll();
            return EngineSceneOk;
        });
    };
    return api;
}
