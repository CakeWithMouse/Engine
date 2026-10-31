#pragma once

#include "../../Core/FileService.h"
#include "../../Core/Logger.h"
#include "../../Core/SceneFile.h"

#include <QTemporaryDir>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace
{

void CheckScene(bool cond, const char* msg)
{
    if (!cond)
    {
        std::fprintf(stderr, "FAIL (SceneFile): %s\n", msg);
        std::exit(1);
    }
}

inline void RunSceneFileTests()
{
    using namespace EditorCore;

    QTemporaryDir tempDir;
    CheckScene(tempDir.isValid(), "Temporary directory created for SceneFile tests");

    Logger logger;
    FileService fs(&logger);
    fs.SetProjectRoot(tempDir.path());

    // 1. Scene with tricky object name `a"b}` and various transforms
    SceneData scene;
    scene.version = 1;
    scene.name = QStringLiteral("TestSceneWithQuotes");

    SceneObject trickyObj;
    trickyObj.id = 101;
    trickyObj.name = QStringLiteral("a\"b}");
    trickyObj.type = QStringLiteral("RuntimeExample::MovingCube");
    trickyObj.parent = 0;
    trickyObj.inheritParentScale = true;
    trickyObj.position[0] = 1.5f;
    trickyObj.position[1] = -2.5f;
    trickyObj.position[2] = 3.0f;
    trickyObj.rotation[0] = 0.0f;
    trickyObj.rotation[1] = 0.7071f;
    trickyObj.rotation[2] = 0.0f;
    trickyObj.rotation[3] = 0.7071f;
    trickyObj.scale[0] = 2.0f;
    trickyObj.scale[1] = 2.0f;
    trickyObj.scale[2] = 2.0f;
    trickyObj.materialAsset = QStringLiteral("/Game/Materials/Test.mat");

    SceneObject childObj;
    childObj.id = 102;
    childObj.name = QStringLiteral("Child {Nested \"Quote\"}");
    childObj.type = QStringLiteral("Engine.PointLight");
    childObj.parent = 101;
    childObj.inheritParentScale = false;
    childObj.position[0] = 0.0f;
    childObj.position[1] = 1.0f;
    childObj.position[2] = 0.0f;

    scene.objects.append(trickyObj);
    scene.objects.append(childObj);

    // 2. Save via FileService
    QString saveError;
    const QString scenePath = QStringLiteral("Content/Scenes/Main.scene.json");
    bool saveOk = SceneFile::Save(fs, scenePath, scene, &saveError);
    CheckScene(saveOk, "SceneFile::Save succeeded");
    CheckScene(saveError.isEmpty(), "No error on scene save");

    // 3. Load via FileService
    SceneData loadedScene;
    QString loadError;
    bool loadOk = SceneFile::Load(fs, scenePath, loadedScene, &loadError);
    CheckScene(loadOk, "SceneFile::Load succeeded");
    CheckScene(loadError.isEmpty(), "No error on scene load");

    // 4. Validate exact match including tricky name `a"b}`
    CheckScene(loadedScene.version == 1, "Loaded scene version matches");
    CheckScene(loadedScene.name == QStringLiteral("TestSceneWithQuotes"), "Loaded scene name matches");
    CheckScene(loadedScene.objects.size() == 2, "Loaded object count matches");

    const auto& loadedTricky = loadedScene.objects[0];
    CheckScene(loadedTricky.id == 101, "Object id matches");
    CheckScene(loadedTricky.name == QStringLiteral("a\"b}"), "Tricky object name a\"b} preserved exactly");
    CheckScene(loadedTricky.type == QStringLiteral("RuntimeExample::MovingCube"), "Object type matches");
    CheckScene(loadedTricky.parent == 0, "Object parent matches");
    CheckScene(loadedTricky.inheritParentScale == true, "inheritParentScale matches");
    CheckScene(std::fabs(loadedTricky.position[0] - 1.5f) < 1e-4f, "Position X matches");
    CheckScene(std::fabs(loadedTricky.position[1] - (-2.5f)) < 1e-4f, "Position Y matches");
    CheckScene(std::fabs(loadedTricky.position[2] - 3.0f) < 1e-4f, "Position Z matches");
    CheckScene(std::fabs(loadedTricky.scale[0] - 2.0f) < 1e-4f, "Scale X matches");
    CheckScene(loadedTricky.materialAsset == QStringLiteral("/Game/Materials/Test.mat"), "Material asset path matches");

    const auto& loadedChild = loadedScene.objects[1];
    CheckScene(loadedChild.id == 102, "Child id matches");
    CheckScene(loadedChild.name == QStringLiteral("Child {Nested \"Quote\"}"), "Child name with quotes and braces preserved");
    CheckScene(loadedChild.parent == 101, "Child parent matches");
    CheckScene(loadedChild.inheritParentScale == false, "Child inheritParentScale false matches");

    // 5. Error handling on non-existent file
    SceneData dummy;
    QString missingErr;
    CheckScene(!SceneFile::Load(fs, QStringLiteral("Content/Missing.scene.json"), dummy, &missingErr), "Loading non-existent file fails");
    CheckScene(!missingErr.isEmpty(), "Error populated on missing file");

    // 6. Error handling on corrupted JSON
    fs.WriteFileAtomic(QStringLiteral("Content/Corrupt.scene.json"), "This is { not valid json");
    QString corruptErr;
    CheckScene(!SceneFile::Load(fs, QStringLiteral("Content/Corrupt.scene.json"), dummy, &corruptErr), "Loading corrupt json fails");
    CheckScene(!corruptErr.isEmpty(), "Error populated on corrupt json");
}

} // namespace
