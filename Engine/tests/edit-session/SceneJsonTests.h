// Included after the shared test helpers; uses the public API, no Editor.
#include <Engine/Scene/SceneDocument.h>
#include "../../ThirdParty/nlohmann/json.hpp"
#include "../../Source/Resources/VfsRead.h"
#include <chrono>
#include <clocale>
#include <fstream>
#include <iostream>

static void ExerciseJson(const EngineEditSessionApi& api, const char* type, bool local = false,
    const std::filesystem::path& viewportContent = {})
{
    for (const auto& row : Read(api).rows)
        Check(api.scene.removeObject(api.scene.context, row.id) == EngineSceneOk, "reset JSON fixture");
    const auto file = std::filesystem::temp_directory_path() / (L"engine-\u0441\u0446\u0435\u043d\u0430-" + std::to_wstring(GetCurrentProcessId()) + L".json");
    struct Cleanup { std::filesystem::path file; ~Cleanup() { std::error_code ec; std::filesystem::remove(file, ec); } } cleanup{file};
    const auto path = file.u8string(); char error[512]{}; uint64_t loaded = 0;
    uint64_t parent, child;
    Check(api.scene.createObject(api.scene.context, type, "json parent", nullptr, &parent) == EngineSceneOk, "JSON parent");
    Check(api.scene.createObject(api.scene.context, type, "json child", nullptr, &child) == EngineSceneOk, "JSON child");
    Check(api.setParent(api.context, child, parent, 0) == EngineSceneOk, "JSON hierarchy");
    EngineSceneTransform transform{{3.25f,-2,7}, {0,0,0.6f,0.8f}, {-2,3,4}};
    Check(api.scene.setTransform(api.scene.context, child, &transform) == EngineSceneOk, "JSON nonidentity transform");
    Check(api.saveScene(api.context, path.c_str(), error, sizeof(error)) == EngineSceneOk, "save JSON");
    const auto original = SceneDocument::Read(file);
    const auto text = SceneDocument::Serialize(original);
    Check(SceneDocument::Serialize(SceneDocument::Parse(text)) == text, "stable document round trip");
    auto originalId = Read(api).info.id;
    const auto oldSnapshot = Read(api);
    auto write = [&](const std::string& data) { std::ofstream output(file, std::ios::binary | std::ios::trunc); output << data; output.close(); Check(bool(output), "fixture written"); };
    auto reject = [&](const std::string& data, uint32_t expected) {
        write(data); loaded = 123;
        Check(api.loadScene(api.context, path.c_str(), &loaded, error, sizeof(error)) == expected, "invalid load status");
        Check(!loaded && error[0] && Read(api).info.id == originalId, "failed load keeps scene and reports error");
        const auto unchanged = Read(api);
        Check(unchanged.info.revision == oldSnapshot.info.revision && unchanged.rows.size() == oldSnapshot.rows.size() &&
            unchanged.rows.back().transform.position[0] == oldSnapshot.rows.back().transform.position[0], "failed load preserves data and revision");
    };
    auto json = nlohmann::json::parse(text);
    auto bad = json; bad["objects"][0]["type"] = "Missing::Class";
    auto* before = last;
    reject(bad.dump(), EngineSceneUnknownType);
    if (local) Check(last == before, "unknown type checked before constructors");
    reject("{", EngineSceneInvalidDocument);
    reject("{\"version\":1,\"name\":\"\xff\",\"objects\":[]}", EngineSceneInvalidDocument);
    reject("{\"version\":1,\"version\":1,\"name\":\"x\",\"objects\":[]}", EngineSceneInvalidDocument);
    bad = json; bad["version"] = 2; reject(bad.dump(), EngineSceneUnsupportedVersion);
    bad = json; bad["extra"] = 1; reject(bad.dump(), EngineSceneInvalidDocument);
    bad = json; bad["objects"][1]["id"] = bad["objects"][0]["id"]; reject(bad.dump(), EngineSceneInvalidDocument);
    bad = json; bad["objects"][0]["parent"] = 999999; reject(bad.dump(), EngineSceneInvalidDocument);
    bad = json; bad["objects"][0]["parent"] = bad["objects"][1]["id"]; reject(bad.dump(), EngineSceneInvalidDocument);
    bad = json; bad["objects"][0]["transform"]["scale"] = {1,0,1}; reject(bad.dump(), EngineSceneInvalidDocument);
    bad = json; bad["objects"][0]["transform"]["rotation"] = {0,0,0,0}; reject(bad.dump(), EngineSceneInvalidDocument);
    bad = json; bad["objects"][0]["name"] = std::string("a\0b",3); reject(bad.dump(), EngineSceneInvalidDocument);
    if (local)
    {
        failConstruction = true;
        reject(text, EngineSceneFailure);
        failConstruction = false;
    }
    write(text);
    if (!viewportContent.empty())
    {
        EnginePaths::SetContentRoot(viewportContent / "missing-json-shaders");
        reject(text, EngineSceneFailure);
        EnginePaths::SetContentRoot(viewportContent);
        Check(api.frame(api.context, 0.01) == EngineSceneOk, "old viewport restored after failed load");
        Check(api.resizeViewport(api.context, 0, 0) == EngineSceneOk, "hide viewport");
        Check(api.loadScene(api.context, path.c_str(), &loaded, error, sizeof(error)) == EngineSceneBusy, "hidden viewport load deferred");
        Check(api.resizeViewport(api.context, 320, 240) == EngineSceneOk, "show viewport");
    }
    Check(api.loadScene(api.context, path.c_str(), &loaded, error, sizeof(error)) == EngineSceneOk && loaded != originalId && !error[0], "transactional load");
    auto restored = Read(api);
    Check(restored.info.id == loaded && restored.rows.size() == original.components.size(), "existing API table rebound");
    Check(restored.rows.back().parent == restored.rows[restored.rows.size()-2].id, "parent restored");
    Check(api.saveScene(api.context, path.c_str(), error, sizeof(error)) == EngineSceneOk, "save restored scene");
    Check(!SceneDocument::Read(file).components.back().inheritParentScale, "inherit scale restored");
    Check(api.scene.renameObject(api.scene.context, restored.rows.back().id, "renamed after load") == EngineSceneOk, "commands target loaded scene");
    Check(api.control(api.context, EnginePlay) == EngineSceneOk && api.frame(api.context, 0.05) == EngineSceneOk, "loaded scene plays");
    Check(api.loadScene(api.context, path.c_str(), &loaded, error, sizeof(error)) == EngineSceneReadOnly && !loaded, "no load during Play");
    Check(api.saveScene(api.context, path.c_str(), error, sizeof(error)) == EngineSceneOk, "save during Play");
    const auto saved = SceneDocument::Read(file);
    Check(saved.components.back().transform.position.x == restored.rows.back().transform.position[0], "save uses edit not simulation");
    Check(api.control(api.context, EnginePause) == EngineSceneOk, "pause loaded scene");
    Check(api.saveScene(api.context, path.c_str(), error, sizeof(error)) == EngineSceneOk, "save during Pause");
    Check(api.control(api.context, EngineStop) == EngineSceneOk && Read(api).info.id == restored.info.id, "Stop retains loaded edit");
    HANDLE lock = CreateFileW(file.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    Check(lock != INVALID_HANDLE_VALUE, "lock destination");
    auto status = api.saveScene(api.context, path.c_str(), error, sizeof(error));
    CloseHandle(lock);
    Check(status == EngineSceneIoError && SceneDocument::Serialize(SceneDocument::Read(file)) == SceneDocument::Serialize(saved), "failed replacement preserves file");
    Check(api.saveScene(api.context, "relative.json", error, sizeof(error)) == EngineSceneInvalidArgument, "absolute path required");
    uint32_t threadStatus = 0;
    std::thread worker([&] { threadStatus = api.saveScene(api.context, path.c_str(), nullptr, 0); }); worker.join();
    Check(threadStatus == EngineSceneWrongThread, "JSON thread guard");
    // Restore file with Unicode, escaped control characters and non-sequential IDs.
    bad = json; bad["name"] = "\xD0\xA1\xD1\x86\xD0\xB5\xD0\xBD\xD0\xB0";
    bad["objects"][0]["name"] = "escaped\n\"name";
    bad["objects"][0]["id"] = uint64_t(9007199254740993ULL);
    for (auto& object : bad["objects"]) if (object["parent"] == json["objects"][0]["id"]) object["parent"] = bad["objects"][0]["id"];
    write(bad.dump());
    Check(api.loadScene(api.context, path.c_str(), &loaded, error, sizeof(error)) == EngineSceneOk, "Unicode and exact uint64 IDs");
    Check(api.frame(api.context, 0.01) == EngineSceneOk, "loaded edit renders without simulation");
    write("{\"version\":1,\"name\":\"Empty\",\"objects\":[]}");
    Check(api.loadScene(api.context, path.c_str(), &loaded, error, sizeof(error)) == EngineSceneOk && Read(api).rows.empty(), "load empty scene");
    std::filesystem::remove(file);
    Check(api.loadScene(api.context, path.c_str(), &loaded, error, sizeof(error)) == EngineSceneIoError && !loaded, "missing file");
}

struct LocaleGuard
{
    std::string saved;
    LocaleGuard()
    {
        const char* cur = std::setlocale(LC_ALL, nullptr);
        saved = cur ? cur : "C";
    }
    ~LocaleGuard()
    {
        std::setlocale(LC_ALL, saved.c_str());
    }
};

static void ExerciseJsonLocale()
{
    LocaleGuard guard;

    SceneSnapshot snapshot;
    snapshot.name = "LocaleTestScene";
    SceneComponentSnapshot comp;
    comp.id = 1;
    comp.name = "LocaleObject";
    comp.typeName = "Probe";
    comp.parentId = 0;
    comp.inheritParentScale = true;
    comp.transform.position = {0.5f, -1.25f, 1e-7f};
    comp.transform.rotation = {1.0f, 0.0f, 0.0f, 0.0f};
    comp.transform.scale = {123456.75f, 1.0f, 1.0f};
    snapshot.components.push_back(comp);

    // 1. Under "C" locale
    std::setlocale(LC_ALL, "C");
    const std::string cSerialized = SceneDocument::Serialize(snapshot);
    const SceneSnapshot cParsed = SceneDocument::Parse(cSerialized);
    Check(cParsed.components.size() == 1, "c-locale components size");
    Check(cParsed.components[0].transform.position.x == 0.5f, "c-locale position.x");
    Check(cParsed.components[0].transform.position.y == -1.25f, "c-locale position.y");
    Check(cParsed.components[0].transform.position.z == 1e-7f, "c-locale position.z");
    Check(cParsed.components[0].transform.scale.x == 123456.75f, "c-locale scale.x");

    // 2. Under system default locale: setlocale(LC_ALL, "")
    std::setlocale(LC_ALL, "");
    const std::string sysSerialized = SceneDocument::Serialize(snapshot);
    Check(sysSerialized == cSerialized, "system locale Serialize matches C locale byte-for-byte");
    const SceneSnapshot sysParsed = SceneDocument::Parse(sysSerialized);
    Check(sysParsed.components[0].transform.position.x == 0.5f, "sys-locale position.x");
    Check(sysParsed.components[0].transform.position.y == -1.25f, "sys-locale position.y");
    Check(sysParsed.components[0].transform.position.z == 1e-7f, "sys-locale position.z");
    Check(sysParsed.components[0].transform.scale.x == 123456.75f, "sys-locale scale.x");

    // 3. Under comma-decimal locale
    const char* commaCandidates[] = {"ru-RU", "de-DE", "Russian_Russia.1251"};
    const char* selectedLocale = nullptr;
    for (const char* loc : commaCandidates)
    {
        if (std::setlocale(LC_ALL, loc) != nullptr)
        {
            selectedLocale = loc;
            break;
        }
    }

    if (!selectedLocale)
    {
        std::cout << "SKIP: no comma-decimal locale\n";
    }
    else
    {
        const std::string commaSerialized = SceneDocument::Serialize(snapshot);
        Check(commaSerialized == cSerialized, "comma locale Serialize matches C locale byte-for-byte");
        Check(commaSerialized.find("0.5") != std::string::npos, "serialized string has decimal point");
        Check(commaSerialized.find("0,5") == std::string::npos, "serialized string has no comma in numbers");
        const SceneSnapshot commaParsed = SceneDocument::Parse(commaSerialized);
        Check(commaParsed.components[0].transform.position.x == 0.5f, "comma-locale position.x");
        Check(commaParsed.components[0].transform.position.y == -1.25f, "comma-locale position.y");
        Check(commaParsed.components[0].transform.position.z == 1e-7f, "comma-locale position.z");
        Check(commaParsed.components[0].transform.scale.x == 123456.75f, "comma-locale scale.x");
    }
}

static void ExerciseJsonVfsWrite()
{
    const auto mountsBefore = VfsRead::GetVFS().GetMounts();

    const auto tempDir = (std::filesystem::temp_directory_path() /
        (L"engine-test-vfs-scenes-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
         std::to_wstring(std::chrono::steady_clock::now().time_since_epoch().count()))).lexically_normal();
    std::error_code ec;
    std::filesystem::create_directories(tempDir, ec);
    Check(!ec, "create test scenes temp dir");

    struct VfsScenesCleanup
    {
        std::filesystem::path dir;
        ~VfsScenesCleanup()
        {
            VfsRead::GetVFS().Unmount("/TestScenes");
            std::error_code ec;
            std::filesystem::remove_all(dir, ec);
        }
    } cleanup{tempDir};

    VfsRead::GetVFS().Mount("/TestScenes", tempDir);

    const auto resolvedOpt = VfsRead::GetVFS().Resolve("/TestScenes/new.scene.json");
    Check(resolvedOpt.has_value(), "Resolve non-existent file returns path");
    const auto physicalPath = *resolvedOpt;
    Check(!std::filesystem::exists(physicalPath, ec), "target file does not exist before Write");

    SceneSnapshot snapshot;
    snapshot.name = "VfsWriteTestScene";
    SceneComponentSnapshot comp;
    comp.id = 1;
    comp.name = "TestObject";
    comp.typeName = "Probe";
    comp.parentId = 0;
    comp.inheritParentScale = true;
    comp.transform.position = {1.0f, 2.0f, 3.0f};
    comp.transform.rotation = {1.0f, 0.0f, 0.0f, 0.0f};
    comp.transform.scale = {1.0f, 1.0f, 1.0f};
    snapshot.components.push_back(comp);

    // 1. Initial write
    SceneDocument::Write(physicalPath, snapshot);
    Check(std::filesystem::exists(physicalPath, ec), "file exists after Write");
    Check(VfsRead::GetVFS().Exists("/TestScenes/new.scene.json"), "VFS Exists returns true after Write");

    const auto readSnapshot = SceneDocument::Read(physicalPath);
    Check(SceneDocument::Serialize(readSnapshot) == SceneDocument::Serialize(snapshot), "Write and Read preserve Serialize");

    // Check no *.tmp.* leftovers
    for (const auto& entry : std::filesystem::directory_iterator(tempDir, ec))
    {
        const auto filename = entry.path().filename().wstring();
        Check(filename.find(L".tmp.") == std::wstring::npos, "no leftover tmp file after initial write");
    }

    // 2. Repeat write replaces file
    snapshot.name = "ReplacedVfsScene";
    SceneDocument::Write(physicalPath, snapshot);
    const auto replacedSnapshot = SceneDocument::Read(physicalPath);
    Check(replacedSnapshot.name == "ReplacedVfsScene", "repeat write replaces file content");
    Check(SceneDocument::Serialize(replacedSnapshot) == SceneDocument::Serialize(snapshot), "repeat write preserves Serialize");

    for (const auto& entry : std::filesystem::directory_iterator(tempDir, ec))
    {
        const auto filename = entry.path().filename().wstring();
        Check(filename.find(L".tmp.") == std::wstring::npos, "no leftover tmp file after repeat write");
    }

    // 3. Write into non-existent parent directory throws SceneDocumentError with EngineSceneIoError
    const auto badResolvedOpt = VfsRead::GetVFS().Resolve("/TestScenes/missing_subdir/scene.json");
    Check(badResolvedOpt.has_value(), "Resolve in missing subdir returns path");
    const auto badPhysicalPath = *badResolvedOpt;
    bool caughtIoError = false;
    try
    {
        SceneDocument::Write(badPhysicalPath, snapshot);
    }
    catch (const SceneDocumentError& err)
    {
        if (err.status == EngineSceneIoError)
        {
            caughtIoError = true;
        }
    }
    Check(caughtIoError, "write to non-existent parent throws SceneDocumentError with EngineSceneIoError");
    Check(!std::filesystem::exists(badPhysicalPath, ec), "no bad file created");
    Check(!std::filesystem::exists(tempDir / "missing_subdir", ec), "no trash directory created");

    // 4. Verify unmount and cleanup, check GetMounts() matches before
    VfsRead::GetVFS().Unmount("/TestScenes");
    std::filesystem::remove_all(tempDir, ec);

    const auto mountsAfter = VfsRead::GetVFS().GetMounts();
    Check(mountsAfter.size() == mountsBefore.size(), "VFS mount count preserved after test");
    for (size_t i = 0; i < mountsBefore.size(); ++i)
    {
        Check(mountsBefore[i].virtualRoot == mountsAfter[i].virtualRoot, "mount virtual root matches");
        Check(mountsBefore[i].physicalPath == mountsAfter[i].physicalPath, "mount physical path matches");
    }
}

