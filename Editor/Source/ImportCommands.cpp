#include "MainWindow.h"
#include "EditorLocale.h"
#include "QtHost.h"
#include "CommandRegistry.h"
#include "ImportSettingsDialog.h"
#include "ModelImportSidecar.h"
#include "RecoveryStore.h"
#include "AssetOps.h"
#include "Logger.h"

#include <QAction>
#include <QFileDialog>
#include <QMenu>
#include <QMessageBox>
#include <chrono>

void EditorMainWindow::RegisterAssetCommands()
{
    CommandRegistry::Instance().RegisterCommand({
        QStringLiteral("Assets.Import"),
        QStringLiteral("Import Assets"),
        QString(),
        QStringLiteral("Import models, textures, or audio into the project"),
        [this](const CommandContext& ctx) {
            if (!host_->HasScene() ||
                sceneBusy_ ||
                host_->PlayState() != EditorQtHost::EditorState::Edit)
            {
                return false;
            }

            const int kind = ctx.parameters.value(QStringLiteral("kind"), 0).toInt();
            QString source = ctx.parameters.value(QStringLiteral("source")).toString();
            if (source.isEmpty())
            {
                source = ctx.parameters.value(QStringLiteral("path")).toString();
            }

            const QString title = kind == 1
                ? Tr("Импорт текстур в проект", "Import Textures to Project")
                : kind == 2
                    ? Tr("Импорт аудио в проект", "Import Audio to Project")
                    : Tr("Импорт моделей в проект", "Import Models to Project");

            const QString filter = kind == 1
                ? Tr("Текстуры (*.png *.jpg *.jpeg *.bmp *.tga)", "Textures (*.png *.jpg *.jpeg *.bmp *.tga)")
                : kind == 2
                    ? Tr("Аудио (*.wav *.ogg *.mp3 *.flac)", "Audio (*.wav *.ogg *.mp3 *.flac)")
                    : Tr("Модели (*.glb *.gltf *.obj *.fbx *.blend)", "Models (*.glb *.gltf *.obj *.fbx *.blend)");

            const QStringList paths = source.isEmpty()
                                          ? QFileDialog::getOpenFileNames(this, title, {}, filter)
                                          : QStringList{source};

            if (paths.isEmpty())
            {
                return false;
            }

            float scale = 1.0f;
            QString optionsJson;
            if (kind == 0)
            {
                const auto decision = ResolveModelImportDecision(ctx.parameters, !source.isEmpty(), this);
                if (!decision.proceed)
                {
                    return false;
                }
                scale = decision.scale;
                if (ctx.parameters.contains(QStringLiteral("options")))
                {
                    optionsJson = ctx.parameters.value(QStringLiteral("options")).toString();
                }
                else
                {
                    optionsJson = QString::fromUtf8(EditorCore::ImportSettingsFile::Serialize(decision.settings));
                }
            }

            for (const auto& path : paths)
            {
                EditorCommand c;
                c.type = kind == 1
                             ? EditorCommand::ImportTexture
                             : kind == 2
                                 ? EditorCommand::ImportAudio
                                 : EditorCommand::ImportModel;
                c.path = path;
                c.importScale = scale;
                c.importOptionsJson = optionsJson;
                host_->PostCommand(c);
            }
            return true;
        },
        [this]() {
            return host_ && host_->HasScene() && !sceneBusy_ && host_->PlayState() == EditorQtHost::EditorState::Edit;
        }
    });

    CommandRegistry::Instance().RegisterCommand({
        QStringLiteral("Assets.EditImportSettings"),
        QStringLiteral("Edit Model Import Settings"),
        QString(),
        QStringLiteral("Edit import settings for an existing model and update its geometry"),
        [this](const CommandContext& ctx) {
            if (!host_->HasScene() ||
                sceneBusy_ ||
                host_->PlayState() != EditorQtHost::EditorState::Edit)
            {
                return false;
            }

            const QString path = ctx.parameters.value(QStringLiteral("path")).toString();
            if (path.isEmpty())
            {
                return false;
            }

            const bool showDialog = ctx.parameters.value(QStringLiteral("showDialog"), true).toBool();
            return EditModelImportSettings(path, showDialog, ctx.parameters);
        },
        [this]() {
            return host_ && host_->HasScene() && !sceneBusy_ && host_->PlayState() == EditorQtHost::EditorState::Edit;
        }
    });

    CommandRegistry::Instance().RegisterCommand({
        QStringLiteral("Assets.Delete"),
        QStringLiteral("Delete Asset"),
        QString(),
        QStringLiteral("Delete an asset and its companion files from the project"),
        [this](const CommandContext& ctx) {
            if (!host_->HasScene() ||
                sceneBusy_ ||
                host_->PlayState() != EditorQtHost::EditorState::Edit)
            {
                return false;
            }

            const QString path = ctx.parameters.value(QStringLiteral("path")).toString();
            if (path.isEmpty())
            {
                return false;
            }

            const bool skipPrompt = ctx.parameters.value(QStringLiteral("skipPrompt"), false).toBool();
            return DeleteAsset(path, skipPrompt);
        },
        [this]() {
            return host_ && host_->HasScene() && !sceneBusy_ && host_->PlayState() == EditorQtHost::EditorState::Edit;
        }
    });
}

void EditorMainWindow::UnregisterAssetCommands()
{
    CommandRegistry::Instance().UnregisterCommand(QStringLiteral("Assets.Import"));
    CommandRegistry::Instance().UnregisterCommand(QStringLiteral("Assets.EditImportSettings"));
    CommandRegistry::Instance().UnregisterCommand(QStringLiteral("Assets.Delete"));
}

void EditorMainWindow::BuildResourcesMenu(QMenu* sceneMenu)
{
    RegisterAssetCommands();
    if (!sceneMenu)
    {
        return;
    }

    resourcesSubMenu_ = sceneMenu->addMenu(Tr("Ресурсы", "Assets"));
    actAddModel_ = resourcesSubMenu_->addAction(Tr("Добавить модель…", "Add Model…"), this, [this] { AddModel(); });
    actImportModel_ = resourcesSubMenu_->addAction(Tr("Импорт модели…", "Import Model…"), this, [this] {
        CommandRegistry::Instance().Execute(QStringLiteral("Assets.Import"), {{QStringLiteral("kind"), 0}});
    });
    actImportTexture_ = resourcesSubMenu_->addAction(Tr("Импорт текстуры…", "Import Texture…"), this, [this] {
        CommandRegistry::Instance().Execute(QStringLiteral("Assets.Import"), {{QStringLiteral("kind"), 1}});
    });
    actImportAudio_ = resourcesSubMenu_->addAction(Tr("Импорт аудио…", "Import Audio…"), this, [this] {
        CommandRegistry::Instance().Execute(QStringLiteral("Assets.Import"), {{QStringLiteral("kind"), 2}});
    });
}

void EditorMainWindow::ImportAsset(int kind, const QString& source)
{
    QVariantMap params;
    params[QStringLiteral("kind")] = kind;
    if (!source.isEmpty())
    {
        params[QStringLiteral("source")] = source;
    }
    CommandRegistry::Instance().Execute(QStringLiteral("Assets.Import"), params);
}

bool EditorMainWindow::EditModelImportSettings(const QString& path, bool showDialog, const QVariantMap& overrideParams)
{
    if (path.isEmpty() || !host_)
    {
        return false;
    }

    const QString projectDir = QFileInfo(ProjectPath()).dir().absolutePath();
    const QString contentRoot = QString::fromUtf8(host_->host().contentRoot);
    FileService fs(&Logger::Instance(), this);
    fs.SetProjectRoot(projectDir);

    EditorCore::ImportSettingsFile::LoadResult loadRes;
    QString loadErr;
    EditorCore::ImportSettings settings = EditorCore::ModelImportSidecar::ReadSettings(fs, path, contentRoot, &loadRes, &loadErr);

    if (loadRes == EditorCore::ImportSettingsFile::LoadResult::Error)
    {
        Logger::Instance().Error(QStringLiteral("ImportSettings: failed to load sidecar for '%1': %2").arg(path, loadErr));
        settings = ImportSettingsDialog::DefaultSettings();
    }
    else if (loadRes == EditorCore::ImportSettingsFile::LoadResult::NotFound)
    {
        Logger::Instance().Info(QStringLiteral("ImportSettings: sidecar not found for '%1', using default settings").arg(path));
        settings = ImportSettingsDialog::DefaultSettings();
    }

    if (showDialog)
    {
        ImportSettingsDialog dlg(this);
        dlg.SetEditMode(true);
        dlg.SetSettings(settings);
        if (dlg.exec() != QDialog::Accepted)
        {
            return false;
        }
        settings = dlg.Settings();
    }
    else
    {
        if (overrideParams.contains(QStringLiteral("options")))
        {
            const QString optJson = overrideParams.value(QStringLiteral("options")).toString();
            EditorCore::ImportSettings parsed;
            QString parseErr;
            if (EditorCore::ImportSettingsFile::Parse(optJson.toUtf8(), parsed, &parseErr))
            {
                settings = parsed;
            }
        }
        if (overrideParams.contains(QStringLiteral("scale")))
        {
            settings.model.scale = static_cast<float>(overrideParams.value(QStringLiteral("scale")).toDouble());
        }
        if (overrideParams.contains(QStringLiteral("mergeMeshes")))
        {
            settings.model.mergeMeshes = overrideParams.value(QStringLiteral("mergeMeshes")).toBool();
        }
        if (overrideParams.contains(QStringLiteral("useFileUnitScale")))
        {
            settings.model.useFileUnitScale = overrideParams.value(QStringLiteral("useFileUnitScale")).toBool();
        }
        if (overrideParams.contains(QStringLiteral("normals")))
        {
            EditorCore::ModelNormalsMode mode{};
            if (EditorCore::ImportSettingsFile::NormalsModeFromString(overrideParams.value(QStringLiteral("normals")).toString(), mode))
            {
                settings.model.normals = mode;
            }
        }
    }

    QString saveErr;
    if (!EditorCore::ModelImportSidecar::WriteSidecar(fs, path, settings, contentRoot, &saveErr))
    {
        Logger::Instance().Error(QStringLiteral("ImportSettings: failed to save sidecar for '%1': %2").arg(path, saveErr));
        return false;
    }

    const QString sourceMetaPath = path + QStringLiteral(".source.json");
    QString sourceFile;
    if (QFileInfo::exists(sourceMetaPath))
    {
        const auto metadata = RecoveryStore::Read(sourceMetaPath);
        sourceFile = metadata.value(QStringLiteral("source")).toString();
    }

    if (sourceFile.isEmpty() || !QFileInfo::exists(sourceFile))
    {
        if (showDialog)
        {
            const QString missingPath = sourceFile.isEmpty() ? Tr("(путь не указан)", "(path not specified)") : sourceFile;
            const auto choice = QMessageBox::question(this,
                Tr("Исходный файл недоступен", "Source File Unavailable"),
                Tr("Исходный файл модели не найден по прежнему пути:\n%1\n\nНастройки сохранены в .importsettings.json.\nУказать расположение исходного файла для переимпорта геометрии?",
                   "Model source file was not found at:\n%1\n\nSettings have been saved to .importsettings.json.\nWould you like to select the source file to reimport geometry?").arg(missingPath),
                QMessageBox::Yes | QMessageBox::No);

            if (choice == QMessageBox::Yes)
            {
                sourceFile = QFileDialog::getOpenFileName(this,
                    Tr("Исходный файл для переимпорта", "Source File for Reimport"),
                    sourceFile,
                    Tr("Ресурсы (*.fbx *.blend *.glb *.gltf *.obj)", "Models (*.fbx *.blend *.glb *.gltf *.obj)"));
                if (sourceFile.isEmpty())
                {
                    return true;
                }
            }
            else
            {
                return true;
            }
        }
        else
        {
            return true;
        }
    }

    UpdateAsset(path, false, sourceFile);
    return true;
}

void EditorMainWindow::HandleModelImported(const EditorCommand& command)
{
    if (command.type != EditorCommand::ImportModel || !command.path.startsWith(QStringLiteral("/Game/")))
    {
        return;
    }

    const QString projectDir = QFileInfo(ProjectPath()).dir().absolutePath();
    const QString contentRoot = QString::fromUtf8(host_->host().contentRoot);
    FileService fs(&Logger::Instance(), this);
    fs.SetProjectRoot(projectDir);

    // If sidecar already exists on disk (written by modern Engine before CreateModel), preserve all its settings
    EditorCore::ImportSettingsFile::LoadResult res;
    QString loadErr;
    EditorCore::ModelImportSidecar::ReadSettings(fs, command.path, contentRoot, &res, &loadErr);
    if (res != EditorCore::ImportSettingsFile::LoadResult::Success)
    {
        QString sidecarError;
        EditorCore::ImportSettings sidecarSettings;
        if (!command.importOptionsJson.isEmpty())
        {
            EditorCore::ImportSettingsFile::Parse(command.importOptionsJson.toUtf8(), sidecarSettings);
        }
        else
        {
            sidecarSettings.model.scale = command.importScale;
        }

        if (!EditorCore::ModelImportSidecar::WriteSidecar(fs, command.path, sidecarSettings, contentRoot, &sidecarError))
        {
            AppendLine(Tr("[Error] Не удалось сохранить настройки импорта для %1: %2",
                          "[Error] Failed to save import settings for %1: %2").arg(command.path, sidecarError));
            static auto s_lastImportWarningTime = std::chrono::steady_clock::time_point{};
            const auto now = std::chrono::steady_clock::now();
            if (now - s_lastImportWarningTime > std::chrono::seconds(3))
            {
                s_lastImportWarningTime = now;
                QMessageBox::warning(this, Tr("Настройки импорта", "Import Settings"),
                                     Tr("Не удалось сохранить настройки импорта (.importsettings.json): %1",
                                        "Failed to save import settings (.importsettings.json): %1").arg(sidecarError));
            }
        }
    }
}

float EditorMainWindow::GetModelSpawnScale(const QString& path)
{
    const QString projectDir = QFileInfo(ProjectPath()).dir().absolutePath();
    const QString contentRoot = QString::fromUtf8(host_->host().contentRoot);
    FileService fs(&Logger::Instance(), this);
    fs.SetProjectRoot(projectDir);

    EditorCore::ImportSettingsFile::LoadResult res;
    QString loadError;
    const float scale = EditorCore::ModelImportSidecar::ReadSpawnScale(fs, path, contentRoot, &res, &loadError);
    if (res == EditorCore::ImportSettingsFile::LoadResult::Error)
    {
        AppendLine(Tr("[Error] Ошибка при чтении настроек модели для %1: %2",
                      "[Error] Error reading model settings for %1: %2").arg(path, loadError));
        static auto s_lastSpawnWarningTime = std::chrono::steady_clock::time_point{};
        const auto now = std::chrono::steady_clock::now();
        if (now - s_lastSpawnWarningTime > std::chrono::seconds(3))
        {
            s_lastSpawnWarningTime = now;
            QMessageBox::warning(this, Tr("Настройки модели", "Model Settings"),
                                 Tr("Ошибка при чтении настроек модели: %1. Использован масштаб 1.0.",
                                    "Error reading model settings: %1. Default scale 1.0 used.").arg(loadError));
        }
    }
    return scale;
}

bool EditorMainWindow::DeleteAsset(const QString& path, bool skipPrompt)
{
    if (path.isEmpty() || !host_)
    {
        return false;
    }

    const QString projectDir = QFileInfo(ProjectPath()).dir().absolutePath();
    const QString contentRoot = QString::fromUtf8(host_->host().contentRoot);
    FileService fs(&Logger::Instance(), this);
    fs.SetProjectRoot(projectDir);

    const QString relPath = QDir(projectDir).relativeFilePath(path);
    if (relPath.startsWith(QStringLiteral("..")) || QDir::isAbsolutePath(relPath))
    {
        Logger::Instance().Error(QStringLiteral("DeleteAsset: path '%1' is outside project sandbox").arg(path));
        return false;
    }

    EditorCore::AssetOps ops(&fs);
    if (!ops.Exists(relPath))
    {
        Logger::Instance().Error(QStringLiteral("DeleteAsset: asset does not exist: %1").arg(path));
        return false;
    }

    const QString fileName = QFileInfo(path).fileName();
    const QString vfsPath = QStringLiteral("/Game/") + QDir::fromNativeSeparators(QDir(contentRoot).relativeFilePath(path));
    int usedCount = 0;
    const auto snapshot = host_->ReadSnapshot();
    for (const auto& obj : snapshot.objects)
    {
        const QString m = QString::fromUtf8(obj.modelAsset.c_str());
        const QString t = QString::fromUtf8(obj.textureAsset.c_str());
        const QString mat = QString::fromUtf8(obj.materialAsset.c_str());
        if ((!m.isEmpty() && (m == vfsPath || m.endsWith(fileName))) ||
            (!t.isEmpty() && (t == vfsPath || t.endsWith(fileName))) ||
            (!mat.isEmpty() && (mat == vfsPath || mat.endsWith(fileName))))
        {
            ++usedCount;
        }
    }

    if (!skipPrompt)
    {
        QMessageBox::StandardButton btn;
        if (usedCount > 0)
        {
            btn = QMessageBox::warning(
                this,
                Tr("Удаление ассета", "Delete Asset"),
                Tr("Ассет используется на сцене: %1 объектов. Удалить файл и сопутствующие настройки?",
                   "Asset is currently used on the scene: %1 objects. Delete the file and companion settings?").arg(usedCount),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No);
        }
        else
        {
            btn = QMessageBox::question(
                this,
                Tr("Удаление ассета", "Delete Asset"),
                Tr("Удалить ассет '%1' и сопутствующие файлы?",
                   "Delete asset '%1' and companion files?").arg(fileName),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::Yes);
        }

        if (btn != QMessageBox::Yes)
        {
            return false;
        }
    }

    QString error;
    if (!ops.Delete(relPath, &error, true))
    {
        Logger::Instance().Error(QStringLiteral("DeleteAsset: failed to delete '%1': %2").arg(path, error));
        return false;
    }

    Logger::Instance().Info(QStringLiteral("DeleteAsset: successfully deleted '%1' and companion files").arg(path));
    return true;
}

