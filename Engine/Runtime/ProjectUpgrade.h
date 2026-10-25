#pragma once
#include "Project.h"
#include <Engine/Runtime/GameEditModule.h>
#include <Engine/Scene/SceneDocument.h>
#include "../ThirdParty/nlohmann/json.hpp"
#include <Windows.h>
#include <bcrypt.h>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <set>
#pragma comment(lib, "bcrypt.lib")

namespace ProjectUpgrade
{
inline std::string Hash(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot hash game DLL");
    BCRYPT_ALG_HANDLE algorithm{}; BCRYPT_HASH_HANDLE hash{};
    struct Cleanup { BCRYPT_ALG_HANDLE& a; BCRYPT_HASH_HANDLE& h; ~Cleanup() { if(h) BCryptDestroyHash(h); if(a) BCryptCloseAlgorithmProvider(a,0); } } cleanup{algorithm,hash};
    if (BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0) < 0 ||
        BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0) < 0) throw std::runtime_error("SHA256 unavailable");
    char buffer[65536];
    while (input.read(buffer,sizeof(buffer)) || input.gcount())
        if (BCryptHashData(hash,reinterpret_cast<PUCHAR>(buffer),static_cast<ULONG>(input.gcount()),0) < 0)
            throw std::runtime_error("SHA256 failed");
    if (!input.eof()) throw std::runtime_error("Cannot read game DLL");
    unsigned char digest[32];
    if (BCryptFinishHash(hash,digest,sizeof(digest),0) < 0) throw std::runtime_error("SHA256 failed");
    std::ostringstream text; text << std::uppercase << std::hex << std::setfill('0');
    for (auto b : digest) text << std::setw(2) << static_cast<unsigned>(b);
    return text.str();
}
inline void WriteJson(const std::filesystem::path& path, const nlohmann::json& data)
{
    std::ofstream out(path, std::ios::binary); out.exceptions(std::ios::badbit | std::ios::failbit);
    out << data.dump(2) << '\n'; out.close();
}
inline std::filesystem::path RelativeInside(const std::filesystem::path& path, const std::filesystem::path& root)
{
    const auto relative = std::filesystem::relative(path,root);
    if (relative.empty() || relative.is_absolute()) throw std::runtime_error("Project paths must stay inside its folder for migration");
    for (const auto& part : relative) if (part == "..") throw std::runtime_error("Project paths must stay inside its folder for migration");
    return relative;
}
inline void Copy(const std::filesystem::path& source, const std::filesystem::path& destination,
    const std::filesystem::path& runtime)
{
    const auto project = ReadRuntimeProject(source);
    const auto release = ReadEngineRelease(runtime);
    const auto root = std::filesystem::canonical(project.file.parent_path());
    const auto target = std::filesystem::weakly_canonical(std::filesystem::absolute(destination));
    auto rootPrefix = root.wstring();
    if (rootPrefix.back() != L'\\') rootPrefix += L'\\';
    // Windows paths are case insensitive; a destination on another drive is valid.
    if (_wcsicmp(target.c_str(),root.c_str()) == 0 ||
        _wcsnicmp(target.c_str(),rootPrefix.c_str(),rootPrefix.size()) == 0)
        throw std::runtime_error("Choose a new folder outside the original project");
    if (std::filesystem::exists(target)) throw std::runtime_error("Destination already exists; nothing was overwritten");
    const auto modulePath = RelativeInside(project.module,root), contentPath = RelativeInside(project.content,root);
    if (!project.startupScene.empty()) RelativeInside(project.startupScene,root);
    auto staging = target; staging += L".upgrading." + std::to_wstring(GetCurrentProcessId());
    if (!std::filesystem::create_directory(staging)) throw std::runtime_error("Upgrade already in progress");
    try
    {
        for (auto it = std::filesystem::recursive_directory_iterator(root); it != std::filesystem::recursive_directory_iterator(); ++it)
        {
            if (GetFileAttributesW(it->path().c_str()) & FILE_ATTRIBUTE_REPARSE_POINT)
                throw std::runtime_error("Project contains a link/junction; migrate it explicitly");
            if (it->is_directory() && it->path().filename() == ".shadow_cache") { it.disable_recursion_pending(); continue; }
            const auto dest = staging / std::filesystem::relative(it->path(),root);
            if (it->is_directory()) std::filesystem::create_directories(dest);
            else if (it->is_regular_file()) std::filesystem::copy_file(it->path(),dest);
        }
        const auto currentTemplate = runtime / "Templates/SceneProject/Game.dll";
        const auto originalHash = Hash(project.module);
        bool standard = originalHash == Hash(currentTemplate);
        std::ifstream knownFile(runtime / "TemplateCompatibility.json");
        if (knownFile)
        {
            const auto known = nlohmann::json::parse(knownFile);
            for (const auto& hash : known.at("SceneProject"))
                standard |= hash.get<std::string>() == originalHash;
        }
        if (standard)
        {
            for (const auto& file : std::filesystem::directory_iterator(currentTemplate.parent_path()))
                if (file.is_regular_file() && file.path().extension() == ".dll")
                    std::filesystem::copy_file(file.path(), file.path().filename() == "Game.dll" ? staging / modulePath : staging / modulePath.parent_path() / file.path().filename(), std::filesystem::copy_options::overwrite_existing);
        }
        {
            GameEditModule module(staging / modulePath,runtime / "EngineContent",staging / contentPath);
            const auto& api = module.Api();
            std::set<std::filesystem::path> processed;
            auto migrate = [&](const std::filesystem::path& path) {
                if (!processed.insert(path).second) return;
                const auto migrated = SceneDocument::Migrate(SceneDocument::Read(path), module.PreferredSceneVersion());
                SceneDocument::Write(path,migrated);
                char error[2048]{}; uint64_t id = 0;
                if (api.loadScene(api.context,path.u8string().c_str(),&id,error,sizeof(error)) != EngineSceneOk)
                    throw std::runtime_error(std::string("Scene migration rejected: ") + error);
            };
            for (const auto& entry : std::filesystem::recursive_directory_iterator(staging / contentPath))
            {
                const auto name = entry.path().filename().u8string();
                if (!entry.is_regular_file() || name.size() < 11 || name.substr(name.size()-11) != ".scene.json") continue;
                migrate(entry.path());
            }
            if (!project.startupScene.empty())
            {
                const auto startup = staging / RelativeInside(project.startupScene,root);
                migrate(startup);
            }
        }
        // Strip optional inline pins in the COPY; the sidecar is readable by old runtimes.
        const auto manifest = staging / project.file.filename();
        std::ifstream file(manifest); auto json = nlohmann::json::parse(file); file.close();
        json.erase("engineVersion"); json.erase("engineConfiguration"); json.erase("templateId");
        json["module"] = modulePath.generic_u8string(); json["content"] = contentPath.generic_u8string();
        if (!project.startupScene.empty()) json["startupScene"] = RelativeInside(project.startupScene,root).generic_u8string();
        WriteJson(manifest,json);
        WriteJson(staging / "Engine.lock.json",{{"format",1},{"engineVersion",release.version},
            {"engineConfiguration",release.configuration},{"templateId",standard ? "SceneProject" : "custom"}});
        WriteJson(staging / "UpgradeReport.json",{{"sourceProject",project.file.u8string()},{"sourceModuleSHA256",originalHash},
            {"engineVersion",release.version},{"standardModuleUpdated",standard},{"originalPreserved",true}});
        ReadRuntimeProject(manifest);
        std::filesystem::rename(staging,target);
    }
    catch (...) { std::error_code ec; std::filesystem::remove_all(staging,ec); throw; }
}
}
