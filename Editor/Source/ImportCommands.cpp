#include "MainWindow.h"
#include "SceneObjectType.h"
#include "EditorLocale.h"
#include "QtHost.h"
#include "CommandRegistry.h"
#include "ImportSettingsDialog.h"
#include "ModelImportEditFlow.h"
#include "ModelImportSidecar.h"
#include "ModelImportSidecarTransaction.h"
#include "RecoveryStore.h"
#include "AssetOps.h"
#include "AssetExtensions.h"
#include "Logger.h"
#include "FolderCommands.h"
#include "AssetPickerDialog.h"
#include <QAction>
#include <QFileDialog>
#include <QMenu>
#include <QMessageBox>
#include <chrono>
namespace {
const IEditorHost::SceneObject* FindObject(const IEditorHost::Snapshot& snap, uint64_t id)
{
    for (const auto& o : snap.objects)
        if (o.id == id) return &o;
    return nullptr;
}
}
void EditorMainWindow::RegisterAssetCommands()
{
    FolderCommands::Register(fileDock_, QFileInfo(ProjectPath()).dir().absolutePath(),
        QString::fromUtf8(host_->host().contentRoot), [this] {
            return host_ && host_->HasScene() && !sceneBusy_ && host_->PlayState() == EditorQtHost::EditorState::Edit;
        });
    CommandRegistry::Instance().RegisterCommand({
        QStringLiteral("Assets.Import"),
        QStringLiteral("Import Assets"),
        QString(),
        QStringLiteral("Import models, textures, or audio into the project"),
        [this](const CommandContext& ctx) {
            if (!host_->HasScene() || sceneBusy_ || host_->PlayState() != EditorQtHost::EditorState::Edit)
                return false;
            const int kind = ctx.parameters.value(QStringLiteral("kind"), 0).toInt();
            QString source = ctx.parameters.value(QStringLiteral("source")).toString();
            if (source.isEmpty())
                source = ctx.parameters.value(QStringLiteral("path")).toString();
            const QString title = kind == 1
                ? Tr("Импорт текстур в проект", "Import Textures to Project")
                : kind == 2
                    ? Tr("Импорт аудио в проект", "Import Audio to Project")
                    : Tr("Импорт моделей в проект", "Import Models to Project");
            const QString filter = EditorCore::DialogFilter(static_cast<EditorCore::AssetKind>(kind));
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
                bool defaultsFallback = false;
                if (!host_->AssetImportApi())
                {
                    const auto support = EnsureModelImportSupport();
                    if (support == ImportSupport::Cancelled)
                    {
                        return false;
                    }
                    defaultsFallback = support == ImportSupport::Defaults;
                }
                if (defaultsFallback)
                {
                    QMessageBox::information(this, Tr("Импорт модели", "Model Import"),
                        Tr("Game.dll проекта не поддерживает настройки импорта. Модель импортируется с настройками по умолчанию.",
                           "The project's Game.dll does not support import settings. The model is imported with default settings."));
                }
                else
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

    CommandRegistry::Instance().RegisterCommand({
        QStringLiteral("Scene.ReplaceModel"),
        QStringLiteral("Replace Model"),
        QString(),
        QStringLiteral("Replace model geometry on selected scene object"),
        [this](const CommandContext& ctx) {
            if (!host_->HasScene() || sceneBusy_ || host_->PlayState() != EditorQtHost::EditorState::Edit)
                return false;
            const uint64_t targetId = ctx.parameters.value(QStringLiteral("objectId")).toULongLong();
            if (targetId == 0) return false;
            const auto snapshot = host_->ReadSnapshot();
            const auto* found = FindObject(snapshot, targetId);
            if (!found) return false;
            if (!EditorCore::IsModelObjectType(found->type)) {
                QMessageBox::warning(this, Tr("Замена недоступна", "Replacement Unavailable"),
                    EditorCore::IsPrimitiveObjectType(found->type) ? Tr("Замена геометрии примитива на модель заблокирована (требуется C-ABI движка).",
                       "Replacing primitive geometry with a model is blocked (requires Engine C-ABI).") :
                    Tr("Замена модели не поддерживается для типа %1.", "Model replacement is unsupported for type %1.")
                        .arg(QString::fromStdString(found->type)));
                return false;
            }
            QString newModel = ctx.parameters.value(QStringLiteral("model")).toString();
            if (newModel.isEmpty()) {
                AssetPickerDialog picker(EditorCore::AssetKind::Model, QString::fromUtf8(host_->host().contentRoot), QString::fromStdString(found->modelAsset), this);
                if (picker.exec() != QDialog::Accepted) return false;
                newModel = picker.SelectedVirtualPath();
            }
            if (newModel.isEmpty()) return false;
            EditorCommand c;
            c.type = EditorCommand::SetModel;
            c.objectId = targetId;
            c.path = newModel;
            host_->PostCommand(c);
            return true;
        },
        [this]() {
            return host_ && host_->HasScene() && !sceneBusy_ && host_->PlayState() == EditorQtHost::EditorState::Edit;
        }
    });

    CommandRegistry::Instance().RegisterCommand({
        QStringLiteral("Scene.AssignMaterial"),
        QStringLiteral("Assign Material"),
        QString(),
        QStringLiteral("Assign project material to selected scene object"),
        [this](const CommandContext& ctx) {
            if (!host_->HasScene() || sceneBusy_ || host_->PlayState() != EditorQtHost::EditorState::Edit)
                return false;
            const uint64_t targetId = ctx.parameters.value(QStringLiteral("objectId"), selectedId_).toULongLong();
            if (targetId == 0) return false;
            const auto snapshot = host_->ReadSnapshot();
            const auto* found = FindObject(snapshot, targetId);
            if (!found) return false;
            QString newMaterial = ctx.parameters.value(QStringLiteral("material")).toString();
            if (newMaterial.isEmpty()) {
                AssetPickerDialog picker(EditorCore::AssetKind::Material, QString::fromUtf8(host_->host().contentRoot), QString::fromStdString(found->materialAsset), this);
                if (picker.exec() != QDialog::Accepted) return false;
                newMaterial = picker.SelectedVirtualPath();
            }
            if (newMaterial.isEmpty()) return false;
            EditorCommand c;
            c.type = EditorCommand::SetMaterial;
            c.objectId = targetId;
            c.path = newMaterial;
            host_->PostCommand(c);
            return true;
        },
        [this]() {
            return host_ && host_->HasScene() && !sceneBusy_ && host_->PlayState() == EditorQtHost::EditorState::Edit && selectedId_ != 0;
        }
    });
}

void EditorMainWindow::UnregisterAssetCommands()
{
    CommandRegistry::Instance().UnregisterCommand(QStringLiteral("Assets.Import"));
    CommandRegistry::Instance().UnregisterCommand(QStringLiteral("Assets.EditImportSettings"));
    CommandRegistry::Instance().UnregisterCommand(QStringLiteral("Assets.Delete"));
    CommandRegistry::Instance().UnregisterCommand(QStringLiteral("Scene.ReplaceModel"));
    CommandRegistry::Instance().UnregisterCommand(QStringLiteral("Scene.AssignMaterial"));
    CommandRegistry::Instance().UnregisterCommand(QStringLiteral("Assets.CreateFolder"));
    CommandRegistry::Instance().UnregisterCommand(QStringLiteral("Assets.RenameFolder"));
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
        params[QStringLiteral("source")] = source;
    CommandRegistry::Instance().Execute(QStringLiteral("Assets.Import"), params);
}

bool EditorMainWindow::EditModelImportSettings(const QString& path, bool showDialog, const QVariantMap& overrideParams)
{
    if (path.isEmpty() || !host_)
        return false;

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
        ModelImportEditFlow::PrepareForEdit(dlg, settings);
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

    sidecarTx_.Begin(fs, path, contentRoot);
    QString saveErr;
    if (!EditorCore::ModelImportSidecar::WriteSidecar(fs, path, settings, contentRoot, &saveErr))
    {
        Logger::Instance().Error(QStringLiteral("ImportSettings: failed to save sidecar for '%1': %2").arg(path, saveErr));
        sidecarTx_.Commit();
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
                    EditorCore::DialogFilter(EditorCore::AssetKind::Model));
                if (sourceFile.isEmpty())
                {
                    sidecarTx_.Commit();
                    return true;
                }
                if (EditorCore::KindForFile(sourceFile) != EditorCore::AssetKind::Model)
                {
                    QMessageBox::warning(this,
                        Tr("Переимпорт модели", "Model Reimport"),
                        Tr("Выбранный файл не является моделью. Настройки сохранены, переимпорт отменён.",
                           "The selected file is not a model. Settings were saved, reimport cancelled."));
                    sidecarTx_.Commit();
                    return true;
                }
            }
            else
            {
                sidecarTx_.Commit();
                return true;
            }
        }
        else
        {
            sidecarTx_.Commit();
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


