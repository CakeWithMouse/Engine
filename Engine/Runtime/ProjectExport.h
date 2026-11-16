#pragma once
#include "Project.h"
#include "ProjectUpgrade.h"
#include <Engine/UI/ProjectHud.h>
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include "../ThirdParty/nlohmann/json.hpp"

namespace ProjectExport {
namespace fs = std::filesystem;
using Json = nlohmann::json;
inline void Write(const fs::path& path, const Json& value) {
    std::ofstream out(path, std::ios::binary); out.exceptions(std::ios::failbit | std::ios::badbit); out << value.dump(2) << '\n';
}
inline void CopyTree(const fs::path& source, const fs::path& target) {
    if (GetFileAttributesW(source.c_str()) & FILE_ATTRIBUTE_REPARSE_POINT) throw std::runtime_error("Export does not follow linked directories: " + source.u8string());
    fs::create_directories(target);
    for (const auto& entry : fs::recursive_directory_iterator(source)) {
        if (GetFileAttributesW(entry.path().c_str()) & FILE_ATTRIBUTE_REPARSE_POINT) throw std::runtime_error("Export does not follow linked assets: " + entry.path().u8string());
        const auto dest = target / entry.path().lexically_relative(source);
        if (entry.is_directory()) fs::create_directories(dest);
        else if (entry.is_regular_file()) fs::copy_file(entry.path(), dest);
    }
}
inline void CheckProcess(const fs::path& exe, const std::wstring& args, const fs::path& log) {
    SECURITY_ATTRIBUTES security{sizeof(security),nullptr,TRUE};
    HANDLE output = CreateFileW(log.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &security, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (output == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot create export validation log");
    STARTUPINFOW startup{sizeof(startup)}; startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = startup.hStdError = output; startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION process{}; auto command = L"\"" + exe.wstring() + L"\" " + args;
    const auto started = CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,exe.parent_path().c_str(),&startup,&process);
    CloseHandle(output);
    if (!started) throw std::runtime_error("Cannot start export validation");
    CloseHandle(process.hThread);
    const auto wait = WaitForSingleObject(process.hProcess,120000);
    if (wait != WAIT_OBJECT_0) { TerminateProcess(process.hProcess,124); WaitForSingleObject(process.hProcess,5000); }
    DWORD code=1; GetExitCodeProcess(process.hProcess,&code); CloseHandle(process.hProcess);
    if (code || wait != WAIT_OBJECT_0) throw std::runtime_error("Export validation failed; see " + log.u8string());
}
inline fs::path FindRuntime(const EngineRelease& current, const std::string& version, const std::string& configuration) {
    if (current.version == version && current.configuration == configuration) return current.runtime;
    const auto besideLauncher = current.runtime.parent_path();
    for (const auto& candidate : {
        besideLauncher.parent_path() / configuration / "Runtime",
        besideLauncher / "Engines" / version / configuration,
        EngineStore() / version / configuration
    }) {
        if (!fs::is_regular_file(candidate / "EngineRuntime.exe")) continue;
        const auto release = ReadEngineRelease(candidate);
        if (release.version == version && release.configuration == configuration) return release.runtime;
    }
    return {};
}
inline void Export(const fs::path& file, const fs::path& destination, const fs::path& currentRuntime) {
    const auto project = ReadRuntimeProject(file);
    const auto current = ReadEngineRelease(currentRuntime);
    const auto runtime = FindRuntime(current, current.version, "Release");
    if (runtime.empty() || !fs::is_regular_file(runtime / "GameExecutable.exe"))
        throw std::runtime_error("Build or install the matching Release runtime with game export support first.");
    const auto release = ReadEngineRelease(runtime);
    auto module = project.module;
    if (project.engineConfiguration == "Debug") {
        const auto debug = FindRuntime(current, project.engineVersion, "Debug");
        if (debug.empty() || ProjectUpgrade::Hash(module) != ProjectUpgrade::Hash(debug / "Templates/SceneProject/Game.dll"))
            throw std::runtime_error("This project uses a custom or unavailable Debug module. Build the project with the Release SDK before export.");
        module = runtime / "Templates/SceneProject/Game.dll";
    }
    for (const auto* dependency : {"msvcp140.dll", "vcruntime140.dll", "vcruntime140_1.dll"})
        if (!fs::is_regular_file(runtime/dependency)) throw std::runtime_error(std::string("Runtime redistributable is missing; rebuild the Release runtime: ")+dependency);
    const auto target = fs::absolute(destination).lexically_normal();
    const auto inside = [](const fs::path& child, const fs::path& parent) {
        auto rel = fs::weakly_canonical(child).lexically_relative(fs::weakly_canonical(parent));
        return !rel.empty() && !rel.is_absolute() && *rel.begin() != "..";
    };
    if (fs::exists(target)) throw std::runtime_error("Choose a new export folder; existing folders are never overwritten");
    if (inside(target,project.file.parent_path()) || inside(target,runtime) || inside(target,current.runtime))
        throw std::runtime_error("Export folder must be outside project and runtime");
    if (project.startupScene.empty() || !inside(project.startupScene,project.content)) throw std::runtime_error("Save the startup scene inside Content before export");
    const auto hud = ProjectHud::Read(project.content);
    if (!hud.empty() && !fs::is_regular_file(hud)) throw std::runtime_error("Selected HUD file is missing: " + hud.u8string());
    if (!hud.empty() && !fs::is_regular_file(runtime / "EngineContent" / "Shaders" / "UI.hlsl")) throw std::runtime_error("Runtime is missing Shaders/UI.hlsl; rebuild the engine before export");
    // Material paths must be portable. Imported /Game paths and relative paths
    // confined to Content are supported; absolute authoring paths are diagnosed.
    for (const auto& item : fs::recursive_directory_iterator(project.content)) if (item.is_regular_file() && item.path().extension()==".mat") {
        const auto material=Json::parse(std::ifstream(item.path()));
        auto check=[&](const Json& value) {
            if (value.is_null()) return;
            const auto text=value.get<std::string>(); if(text.empty())return;
            if(text.rfind("/Game/",0)==0) {
                const auto resource = project.content/fs::u8path(text.substr(6));
                if(!inside(resource,project.content) || !fs::is_regular_file(resource))
                    throw std::runtime_error("Nonportable or missing material resource: "+text);
                return;
            }
            const auto path=fs::u8path(text);
            if(path.is_absolute() || !inside(item.path().parent_path()/path,project.content) || !fs::is_regular_file(item.path().parent_path()/path))
                throw std::runtime_error("Nonportable or missing material resource: "+item.path().u8string()+": "+text);
        };
        check(material.at("shader"));
        if(material.contains("textures"))for(const auto& texture:material.at("textures"))check(texture);
    }
    fs::create_directories(target.parent_path());
    auto staging=target; staging += L".staging-"+std::to_wstring(GetCurrentProcessId());
    if(!fs::create_directory(staging))throw std::runtime_error("Export staging already exists");
    try {
        const auto bundled=staging/"Runtime", game=staging/"Game";
        fs::create_directories(bundled); fs::create_directories(game/"Binaries");
        fs::copy_file(runtime / "GameExecutable.exe", staging / "Game.exe");
        for(const auto& entry:fs::directory_iterator(runtime)) {
            const auto name=entry.path().filename().u8string();
            if(entry.is_directory()) {
                if(name=="SDK"||name=="Tools"||name=="Projects"||name==".shadow_cache")continue;
                CopyTree(entry.path(),bundled/entry.path().filename());
            } else if(entry.is_regular_file() && (entry.path().extension()==".dll" || name=="EngineRuntime.exe" || name=="EngineVersion.json" || name=="EngineConfiguration.txt")) fs::copy_file(entry.path(),bundled/entry.path().filename());
        }
        CopyTree(project.content,game/"Content");
        for(const auto& entry:fs::directory_iterator(module.parent_path())) if(entry.is_regular_file() && entry.path().extension()==".dll") fs::copy_file(entry.path(),game/"Binaries"/entry.path().filename());
        if(!fs::is_regular_file(game/"Binaries"/module.filename()))throw std::runtime_error("Game DLL is missing");
        const auto scene=(fs::path("Content")/project.startupScene.lexically_relative(project.content)).generic_u8string();
        Write(game/"Project.json",{{"name",project.name},{"module",(fs::path("Binaries")/module.filename()).generic_u8string()},{"content","Content"},{"startupScene",scene}});
        Write(game/"Engine.lock.json",{{"format",1},{"engineVersion",release.version},{"engineConfiguration",release.configuration}});
        const auto exe=bundled/"EngineRuntime.exe";
        CheckProcess(exe,L"--validate-project \""+(game/"Project.json").wstring()+L"\"",staging/"Validation.log");
        CheckProcess(exe,L"--project \""+(game/"Project.json").wstring()+L"\" --mode game --frames 3",staging/"GameSmoke.log");
        std::ofstream start(staging/"Start.cmd",std::ios::binary); start.exceptions(std::ios::failbit|std::ios::badbit);
        start << "@echo off\r\n\"%~dp0Runtime\\EngineRuntime.exe\" --project \"%~dp0Game\\Project.json\" --mode game %*\r\n"; start.close();
        Json files=Json::array(); for(const auto& entry:fs::recursive_directory_iterator(staging))if(entry.is_regular_file())files.push_back(entry.path().lexically_relative(staging).generic_u8string());
        Write(staging/"ExportReport.json",{{"format",1},{"engineVersion",release.version},{"sourceEngineVersion",project.engineVersion},{"configuration",release.configuration},{"files",files},{"validation","scene validation and standalone smoke passed"}});
        fs::rename(staging,target);
    } catch(const std::exception& e) {
        // Keep the new staging directory and diagnostics. Never touch source or
        // an existing destination, and never publish a partially valid package.
        throw std::runtime_error(std::string(e.what())+"; staging retained: "+staging.u8string());
    }
}
}
