#include "MainWindow.h"
#include "EditorLocale.h"
#include "QtHost.h"
#include "CommandRegistry.h"
#include "ImportSettingsDialog.h"
#include "ModelImportSidecar.h"
#include "Logger.h"

#include <QAction>
#include <QFileDialog>
#include <QMenu>
#include <QMessageBox>

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
            if (kind == 0)
            {
                if (ctx.parameters.contains(QStringLiteral("scale")))
                {
                    scale = static_cast<float>(ctx.parameters.value(QStringLiteral("scale")).toDouble());
                }
                else if (source.isEmpty() || ctx.parameters.value(QStringLiteral("showDialog"), false).toBool())
                {
                    ImportSettingsDialog dialog(this);
                    if (dialog.exec() != QDialog::Accepted)
                    {
                        return false;
                    }
                    scale = dialog.Scale();
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
                host_->PostCommand(c);
            }
            return true;
        },
        [this]() {
            return host_ && host_->HasScene() && !sceneBusy_ && host_->PlayState() == EditorQtHost::EditorState::Edit;
        }
    });
}

void EditorMainWindow::UnregisterAssetCommands()
{
    CommandRegistry::Instance().UnregisterCommand(QStringLiteral("Assets.Import"));
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

    QString sidecarError;
    if (!EditorCore::ModelImportSidecar::WriteSidecar(fs, command.path, command.importScale, contentRoot, &sidecarError))
    {
        AppendLine(QStringLiteral("[Error] Не удалось сохранить настройки импорта для %1: %2").arg(command.path, sidecarError));
        QMessageBox::warning(this, Tr("Настройки импорта", "Import Settings"),
                             Tr("Не удалось сохранить настройки импорта (.importsettings.json): %1",
                                "Failed to save import settings (.importsettings.json): %1").arg(sidecarError));
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
        QMessageBox::warning(this, Tr("Настройки модели", "Model Settings"),
                             Tr("Ошибка при чтении настроек модели: %1. Использован масштаб 1.0.",
                                "Error reading model settings: %1. Default scale 1.0 used.").arg(loadError));
    }
    return scale;
}

