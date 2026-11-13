#pragma once

#include "ModelReimportScaleTests.h"
#include "TestSkipped.h"
#include "../Source/Resources/VfsRead.h"
#include <Engine/Runtime/SceneEditSession.h>
#include <Engine/Scene/SceneAssets.h>

#include <Windows.h>
#include <atomic>
#include <ctime>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace ModelStagingTests
{

namespace fs = std::filesystem;
using ModelGeometryScaleTests::Check;
using ModelGeometryScaleTests::WriteText;

// Records every file/directory name added under a root while a synchronous
// import runs, so transient <stem>_1 staging artifacts cannot hide from the check.
class ContentWatcher
{
public:
    explicit ContentWatcher(const fs::path& root)
    {
        dir_ = CreateFileW(root.c_str(), FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
            OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);
        if (dir_ == INVALID_HANDLE_VALUE) throw std::runtime_error("ContentWatcher: cannot open directory");
        stop_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        overlapped_.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        worker_ = std::thread([this] { Loop(); });
    }
    ~ContentWatcher() { Stop(); }

    std::vector<std::string> Stop()
    {
        if (!stopped_.exchange(true))
        {
            SetEvent(stop_);
            if (worker_.joinable()) worker_.join();
            CloseHandle(overlapped_.hEvent);
            CloseHandle(stop_);
            CloseHandle(dir_);
        }
        std::lock_guard lock(mutex_);
        return added_;
    }

private:
    void Loop()
    {
        for (;;)
        {
            ResetEvent(overlapped_.hEvent);
            if (!ReadDirectoryChangesW(dir_, buffer_.data(), static_cast<DWORD>(buffer_.size() * sizeof(DWORD)),
                TRUE, FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_FILE_NAME, nullptr, &overlapped_, nullptr))
                return;
            HANDLE handles[2] = {stop_, overlapped_.hEvent};
            if (WaitForMultipleObjects(2, handles, FALSE, INFINITE) != WAIT_OBJECT_0 + 1) return;
            DWORD bytes = 0;
            if (!GetOverlappedResult(dir_, &overlapped_, &bytes, FALSE)) return;
            DWORD offset = 0;
            while (offset < bytes)
            {
                const auto* info = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(
                    reinterpret_cast<const char*>(buffer_.data()) + offset);
                if (info->Action == FILE_ACTION_ADDED || info->Action == FILE_ACTION_RENAMED_NEW_NAME)
                {
                    const std::wstring name(info->FileName, info->FileNameLength / sizeof(WCHAR));
                    std::lock_guard lock(mutex_);
                    added_.push_back(fs::path(name).u8string());
                }
                if (!info->NextEntryOffset) break;
                offset += info->NextEntryOffset;
            }
        }
    }

    HANDLE dir_ = INVALID_HANDLE_VALUE;
    HANDLE stop_ = nullptr;
    OVERLAPPED overlapped_{};
    std::thread worker_;
    std::mutex mutex_;
    std::vector<std::string> added_;
    std::vector<DWORD> buffer_ = std::vector<DWORD>(16384);
    std::atomic<bool> stopped_{false};
};

inline void Run()
{
#if !defined(ENGINE_ENABLE_ASSIMP)
    throw TestSkipped("staging checks require -EnableAssimp");
#else
    const auto stamp = std::to_string(static_cast<unsigned long long>(std::time(nullptr))) + "_" + std::to_string(std::clock());
    const auto root = fs::temp_directory_path() / ("ModelStaging_" + stamp);
    const auto content = root / "Content";
    fs::create_directories(content / "Scenes");
    try
    {
        {
            SceneEditSession session({nullptr, 0});
            auto api = session.GetApi();
            auto author = session.GetAuthoringApi();
            char error[2048]{};
            Check(api.setContentRoot(api.context, content.u8string().c_str(), error, sizeof(error)) == EngineSceneOk,
                  "setContentRoot mounts /Game");

            WriteText(root / "mesh.obj", "v 0 0 0\nv 10 0 0\nv 10 10 0\nv 0 10 0\nf 1 2 3\nf 1 3 4\n");
            std::vector<std::string> added;
            {
                ContentWatcher watch(content);
                const auto model = SceneAssets::Import(content, root / "mesh.obj", SceneAssets::Model);
                Check(model == "/Game/Models/mesh/mesh.obj", "fresh import keeps the plain stem");
                uint64_t mesh = 0;
                Check(api.createModel(api.context, model.c_str(), "Mesh", nullptr, &mesh, error, sizeof(error)) == EngineSceneOk,
                      "createModel succeeds");
                const auto saved = content / "Scenes/Main.scene.json";
                Check(api.saveScene(api.context, saved.u8string().c_str(), error, sizeof(error)) == EngineSceneOk,
                      "saveScene succeeds");
                Check(author.asset(author.context, 0, model.c_str(), "", error, sizeof(error)) == EngineSceneOk,
                      "first reimport succeeds");
                Check(author.asset(author.context, 0, model.c_str(), "", error, sizeof(error)) == EngineSceneOk,
                      "second reimport succeeds");
                added = watch.Stop();
            }
            for (const auto& name : added)
                Check(name.find("_1") == std::string::npos,
                      ("no <stem>_1 appears in Content during import/reimport, got: " + name).c_str());
            Check(!fs::exists(content / "Models/mesh_1"), "no mesh_1 directory after reimports");
            const auto stagingRoot = root / ".painengine" / "Staging";
            Check(!fs::exists(stagingRoot) || fs::is_empty(stagingRoot), "staging directory cleaned after reimport");

            // Direct staged import: assets land under the staging root, Content stays untouched.
            WriteText(root / "staged.obj", "v 0 0 0\nv 1 0 0\nv 1 1 0\nf 1 2 3\n");
            const auto staging = root / ".painengine" / "Staging" / "direct";
            fs::create_directories(staging);
            std::string virtualImport;
            {
                VfsRead::GetVFS().Mount(SceneAssets::StagingRoot, staging);
                virtualImport = SceneAssets::Import(content, root / "staged.obj", SceneAssets::Model, {},
                                                    staging, SceneAssets::StagingRoot);
                VfsRead::GetVFS().Unmount(SceneAssets::StagingRoot);
            }
            Check(virtualImport.rfind(std::string(SceneAssets::StagingRoot) + "/", 0) == 0,
                  "staged import returns a /_staging path");
            Check(fs::is_regular_file(staging / "Models/staged/staged.obj"), "staged asset exists under the staging root");
            Check(!fs::exists(content / "Models/staged"), "staged import creates nothing in Content");
            Check(!fs::exists(content / "Models/staged_1"), "staged import creates no <stem>_1 in Content");
        }
        fs::remove_all(root);
    }
    catch (...)
    {
        fs::remove_all(root);
        throw;
    }
#endif
}

} // namespace ModelStagingTests