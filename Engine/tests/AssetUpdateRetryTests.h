#pragma once

#include "ModelGeometryScaleTests.h"
#include "../Source/Scene/AssetUpdate.h"

#include <Windows.h>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>

namespace AssetUpdateRetryTests
{

namespace fs = std::filesystem;
using ModelGeometryScaleTests::Check;
using ModelGeometryScaleTests::WriteText;

inline void Run()
{
    const auto stamp = std::to_string(static_cast<unsigned long long>(std::time(nullptr))) + "_" + std::to_string(std::clock());
    const auto root = fs::temp_directory_path() / ("AssetRetry_" + stamp);
    const auto content = root / "Content";
    fs::create_directories(content);
    try
    {
        const auto asset = content / "tone.wav";
        WriteText(asset, "old-bytes");
        const auto source = root / "tone.wav";
        WriteText(source, "new-bytes-0123456789");

        // Hold the asset without sharing so copy attempts 1-2 hit
        // ERROR_SHARING_VIOLATION; release before attempt 3 (~400 ms).
        HANDLE hold = CreateFileW(asset.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        Check(hold != INVALID_HANDLE_VALUE, "test holds the asset exclusively");
        std::thread releaser([hold] { Sleep(300); CloseHandle(hold); });
        try
        {
            AssetUpdate::Run(content, 0, "/Game/tone.wav", source.u8string(),
                             [](const std::string&, const std::string&) {});
        }
        catch (...)
        {
            releaser.join();
            throw;
        }
        releaser.join();

        std::ifstream in(asset, std::ios::binary);
        const std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        in.close();
        Check(bytes == "new-bytes-0123456789", "reimport survives a transient sharing violation");

        // Windows ticks are coarse: separate rapid operations must still retain
        // separate backups instead of overwriting an earlier operation's files.
        const auto backups = root / ".painengine" / "AssetBackups";
        const auto countBackups = [&] {
            return static_cast<size_t>(std::distance(fs::directory_iterator(backups), fs::directory_iterator{}));
        };
        const auto originalBackups = countBackups();
        std::string current = "/Game/tone.wav";
        for (size_t i = 0; i < 32; ++i) {
            const std::string next = i % 2 ? "/Game/tone.wav" : "/Game/tone-next.wav";
            AssetUpdate::Run(content, 1, current, next, [](const std::string&, const std::string&) {});
            current = next;
        }
        Check(countBackups() == originalBackups + 32, "rapid asset updates retain distinct backup directories");
        const auto staging = root / ".painengine" / "Staging";
        Check(!fs::exists(staging) || fs::is_empty(staging), "staging cleaned after retried reimport");
        fs::remove_all(root);
    }
    catch (...)
    {
        fs::remove_all(root);
        throw;
    }
}

} // namespace AssetUpdateRetryTests
