#pragma once

#include "../../Core/AssetExtensions.h"

#include <QString>
#include <QStringList>
#include <cstdio>
#include <cstdlib>

namespace AssetExtensionsTests
{

inline void Check(bool cond, const char* msg)
{
    if (!cond)
    {
        std::fprintf(stderr, "FAIL (AssetExtensions): %s\n", msg);
        std::exit(1);
    }
}

inline void Run(int& outChecks)
{
    using namespace EditorCore;

    int checks = 0;
    auto check = [&](bool cond, const char* msg) {
        Check(cond, msg);
        ++checks;
    };

    check(ModelExtensions() == QStringList({"glb", "gltf", "obj", "fbx", "blend"}),
          "Model extensions match Engine import list (glb/gltf/obj/fbx/blend)");
    check(TextureExtensions() == QStringList({"png", "jpg", "jpeg", "bmp", "tga"}),
          "Texture extensions match Engine import list (png/jpg/jpeg/bmp/tga)");
    check(AudioExtensions() == QStringList({"wav", "ogg", "mp3", "flac"}),
          "Audio extensions match Engine import list (wav/ogg/mp3/flac)");
    check(MaterialExtensions() == QStringList({"mat"}), "Material extension is mat");

    for (const QString& extension : ModelExtensions())
        check(KindForFile(QStringLiteral("/Game/Models/Asset.") + extension) == AssetKind::Model,
              "KindForFile classifies model extensions");
    for (const QString& extension : TextureExtensions())
        check(KindForFile(QStringLiteral("/Game/Textures/Image.") + extension) == AssetKind::Texture,
              "KindForFile classifies texture extensions");
    for (const QString& extension : AudioExtensions())
        check(KindForFile(QStringLiteral("/Game/Audio/Sound.") + extension) == AssetKind::Audio,
              "KindForFile classifies audio extensions");

    check(KindForFile(QStringLiteral("/Game/Materials/Tint.mat")) == AssetKind::Material, "KindForFile classifies mat");
    check(KindForFile(QStringLiteral("/Game/Models/Asset.FBX")) == AssetKind::Model, "KindForFile is case-insensitive");
    check(KindForFile(QStringLiteral("C:/Source/model.Obj")) == AssetKind::Model, "KindForFile handles native paths");
    check(KindForFile(QStringLiteral("/Game/Notes.txt")) == AssetKind::Unknown, "KindForFile returns Unknown for txt");
    check(KindForFile(QStringLiteral("/Game/Models/Asset.fbx.importsettings.json")) == AssetKind::Unknown,
          "KindForFile uses the last suffix (sidecar json is not a model)");
    check(KindForFile(QStringLiteral("/Game/Models/README")) == AssetKind::Unknown, "KindForFile returns Unknown without extension");

    const QString modelFilter = DialogFilter(AssetKind::Model);
    for (const QString& extension : ModelExtensions())
        check(modelFilter.contains(QStringLiteral("*.") + extension), "Model filter lists every model extension");
    check(!modelFilter.contains(QStringLiteral("*.png")), "Model filter excludes textures");
    check(!modelFilter.contains(QStringLiteral("*.wav")), "Model filter excludes audio");

    const QString textureFilter = DialogFilter(AssetKind::Texture);
    for (const QString& extension : TextureExtensions())
        check(textureFilter.contains(QStringLiteral("*.") + extension), "Texture filter lists every texture extension");
    check(!textureFilter.contains(QStringLiteral("*.fbx")), "Texture filter excludes models");

    const QString audioFilter = DialogFilter(AssetKind::Audio);
    for (const QString& extension : AudioExtensions())
        check(audioFilter.contains(QStringLiteral("*.") + extension), "Audio filter lists every audio extension");
    check(!audioFilter.contains(QStringLiteral("*.obj")), "Audio filter excludes models");

    check(DialogFilter(AssetKind::Material).contains(QStringLiteral("*.mat")), "Material filter lists mat");

    const QString genericFilter = DialogFilter(AssetKind::Unknown);
    check(genericFilter.contains(QStringLiteral("*.fbx")) && genericFilter.contains(QStringLiteral("*.png")),
          "Unknown kind keeps the legacy generic models+textures filter");
    check(!genericFilter.contains(QStringLiteral("*.wav")), "Generic filter excludes audio");

    check(ReimportSourceMatches(QStringLiteral("/Game/Models/A.fbx"), QStringLiteral("C:/Source/B.obj")),
          "Model reimport accepts any model source (format identity is enforced by the engine)");
    check(ReimportSourceMatches(QStringLiteral("/Game/Models/A.glb"), QStringLiteral("C:/Source/B.blend")),
          "GLB reimport accepts blend source (converted to glb by the engine)");
    check(!ReimportSourceMatches(QStringLiteral("/Game/Models/A.fbx"), QStringLiteral("C:/Source/B.png")),
          "Model reimport rejects texture source");
    check(ReimportSourceMatches(QStringLiteral("/Game/Textures/A.png"), QStringLiteral("C:/Source/B.jpg")),
          "Texture reimport accepts any texture source");
    check(!ReimportSourceMatches(QStringLiteral("/Game/Textures/A.png"), QStringLiteral("C:/Source/B.fbx")),
          "Texture reimport rejects model source");
    check(ReimportSourceMatches(QStringLiteral("/Game/Audio/A.wav"), QStringLiteral("C:/Source/B.ogg")),
          "Audio reimport accepts any audio source");
    check(!ReimportSourceMatches(QStringLiteral("/Game/Audio/A.wav"), QStringLiteral("C:/Source/B.png")),
          "Audio reimport rejects texture source");
    check(ReimportSourceMatches(QStringLiteral("/Game/Materials/A.mat"), QStringLiteral("C:/Source/B.mat")),
          "Material kind matches material source");
    check(!ReimportSourceMatches(QStringLiteral("/Game/Materials/A.mat"), QStringLiteral("C:/Source/B.fbx")),
          "Material kind rejects model source");
    check(!ReimportSourceMatches(QStringLiteral("/Game/Notes.txt"), QStringLiteral("C:/Source/Anything.fbx")),
          "Unknown asset kind is rejected (editor shows a clear error)");
    check(ReimportSourceMatches(QStringLiteral("/Game/Models/A.FBX"), QStringLiteral("C:/Source/B.Obj")),
          "ReimportSourceMatches is case-insensitive");

    outChecks += checks;
    std::printf("AssetExtensionsTests: %d checks passed.\n", checks);
}

} // namespace AssetExtensionsTests