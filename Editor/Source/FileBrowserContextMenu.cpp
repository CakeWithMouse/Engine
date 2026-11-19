#include "FileBrowser.h"
#include "QtHost.h"
#include "MainWindow.h"
#include "CommandRegistry.h"
#include "EditorLocale.h"
#include "AssetExtensions.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QGuiApplication>
#include <QMenu>
#include <QProcess>
#include <QSortFilterProxyModel>
#include <QTreeView>
#include <QUrl>

using EditorCore::AssetKind;

void FileBrowserDock::OnContextMenu(const QPoint& pos)
{
    const QModelIndex clicked = view_->indexAt(pos);
    const QModelIndex col0 = clicked.isValid() ? clicked.sibling(clicked.row(), 0) : QModelIndex();
    const QModelIndex index = proxy_->mapToSource(col0);
    const bool hasFile = index.isValid();
    const QString path = hasFile ? model_->filePath(index) : QString();
    const bool directory = hasFile && model_->isDir(index);

    QString folderParent = directory ? path : QString();
    if (!hasFile) folderParent = proxy_->full ? model_->rootPath() : contentDir_;
    const QString folderParentRel = folderParent.isEmpty() ? QString() : QDir(projectDir_).relativeFilePath(folderParent);
    const QString creationDir = directory ? path
        : hasFile ? QFileInfo(path).absolutePath()
        : (proxy_->full ? model_->rootPath() : contentDir_);

    QMenu menu(this);
    menu.setObjectName(QStringLiteral("ContentBrowserMenu"));

    if (directory || !hasFile)
        AddCreateSubmenu(menu, folderParentRel, creationDir);
    if (hasFile && !directory)
    {
        AddAssetActions(menu, path, EditorCore::KindForFile(path));
        AddFileOperations(menu, path);
    }
    else if (directory)
        AddFolderActions(menu, path);

    QAction* refresh = menu.addAction(Tr("Обновить", "Refresh"));
    connect(refresh, &QAction::triggered, this, [this] { model_->setRootPath(model_->rootPath()); });

    menu.exec(view_->viewport()->mapToGlobal(pos));
}

void FileBrowserDock::AddCreateSubmenu(QMenu& menu, const QString& folderParent, const QString& directory)
{
    auto& registry = CommandRegistry::Instance();
    QMenu* create = menu.addMenu(Tr("Создать", "Create"));
    create->setObjectName(QStringLiteral("AssetsCreateMenu"));

    QMenu* basic = create->addMenu(Tr("Основное", "Basic"));
    if (!folderParent.isEmpty())
    {
        QAction* folder = basic->addAction(Tr("Новая папка", "New Folder"));
        folder->setObjectName(QStringLiteral("AssetsCreateFolder"));
        folder->setEnabled(registry.CanExecute(QStringLiteral("Assets.CreateFolder")));
        connect(folder, &QAction::triggered, this, [folderParent] {
            CommandRegistry::Instance().Execute(QStringLiteral("Assets.CreateFolder"),
                {{QStringLiteral("parent"), folderParent}});
        });
    }
    if (host_->HasMaterials())
    {
        QAction* material = basic->addAction(Tr("Новый материал…", "New Material…"));
        connect(material, &QAction::triggered, this, [directory] {
            CommandRegistry::Instance().Execute(QStringLiteral("Assets.CreateMaterial"),
                {{QStringLiteral("directory"), directory}});
        });
    }
    QAction* importModel = basic->addAction(Tr("Импорт модели…", "Import Model…"));
    connect(importModel, &QAction::triggered, this, [] {
        CommandRegistry::Instance().Execute(QStringLiteral("Assets.Import"), {{QStringLiteral("kind"), int(AssetKind::Model)}});
    });
    QAction* importTexture = basic->addAction(Tr("Импорт текстуры…", "Import Texture…"));
    connect(importTexture, &QAction::triggered, this, [] {
        CommandRegistry::Instance().Execute(QStringLiteral("Assets.Import"), {{QStringLiteral("kind"), int(AssetKind::Texture)}});
    });

    QMenu* code = create->addMenu(Tr("Код", "Code"));
    QAction* cppClass = code->addAction(Tr("C++ класс…", "C++ Class…"));
    cppClass->setObjectName(QStringLiteral("AssetsCreateCppClass"));
    connect(cppClass, &QAction::triggered, this, [directory] {
        CommandRegistry::Instance().Execute(QStringLiteral("Assets.CreateCppClass"),
            {{QStringLiteral("directory"), directory}});
    });

    QMenu* logic = create->addMenu(Tr("Логика", "Logic"));
    if (host_->ReadSnapshot().scripts)
    {
        QAction* script = logic->addAction(Tr("Новый скрипт…", "New Script…"));
        connect(script, &QAction::triggered, this, [directory] {
            CommandRegistry::Instance().Execute(QStringLiteral("Assets.CreateScript"),
                {{QStringLiteral("directory"), directory}});
        });
    }
    QAction* machine = logic->addAction(Tr("Новый автомат ИИ…", "New AI State Machine…"));
    connect(machine, &QAction::triggered, this, [directory] {
        CommandRegistry::Instance().Execute(QStringLiteral("Assets.CreateAiMachine"),
            {{QStringLiteral("directory"), directory}});
    });

    QMenu* sound = create->addMenu(Tr("Звук", "Sound"));
    QAction* importAudio = sound->addAction(Tr("Импорт аудио…", "Import Audio…"));
    connect(importAudio, &QAction::triggered, this, [] {
        CommandRegistry::Instance().Execute(QStringLiteral("Assets.Import"), {{QStringLiteral("kind"), int(AssetKind::Audio)}});
    });

    QMenu* ui = create->addMenu(Tr("UI", "UI"));
    QAction* newUi = ui->addAction(Tr("Новый UI…", "New UI…"));
    connect(newUi, &QAction::triggered, this, [directory] {
        CommandRegistry::Instance().Execute(QStringLiteral("UI.Create"),
            {{QStringLiteral("directory"), directory}});
    });
}

void FileBrowserDock::AddAssetActions(QMenu& menu, const QString& path, AssetKind kind)
{
    const bool authoring = host_->ReadSnapshot().authoring;
    const bool scripts = host_->ReadSnapshot().scripts;

    const bool usable = kind == AssetKind::Model || kind == AssetKind::Texture ||
        kind == AssetKind::Material || (kind == AssetKind::Script && scripts);
    if (usable)
    {
        const QString label = kind == AssetKind::Material ? Tr("Назначить материал", "Assign Material")
            : kind == AssetKind::Script ? Tr("Назначить скрипт выделенному объекту", "Assign Script to Selection")
            : Tr("Добавить модель / назначить текстуру", "Add Model / Assign Texture");
        QAction* use = menu.addAction(label);
        connect(use, &QAction::triggered, this, [this, path] {
            if (auto* editor = qobject_cast<EditorMainWindow*>(parentWidget())) editor->UseAsset(path);
        });
    }
    if (kind == AssetKind::Material && host_->HasMaterials())
    {
        QAction* edit = menu.addAction(Tr("Редактировать материал", "Edit Material"));
        connect(edit, &QAction::triggered, this, [this, path] {
            if (auto* editor = qobject_cast<EditorMainWindow*>(parentWidget())) editor->OpenMaterialEditor(path);
        });
    }
    if (kind == AssetKind::Script)
    {
        QAction* edit = menu.addAction(Tr("Редактировать скрипт", "Edit Script"));
        connect(edit, &QAction::triggered, this, [this, path] {
            if (auto* editor = qobject_cast<EditorMainWindow*>(parentWidget())) editor->OpenScriptEditor(path);
        });
    }
    if (kind == AssetKind::AiMachine)
    {
        QAction* edit = menu.addAction(Tr("Редактировать автомат ИИ", "Edit AI State Machine"));
        connect(edit, &QAction::triggered, this, [this, path] {
            if (auto* editor = qobject_cast<EditorMainWindow*>(parentWidget())) editor->OpenAiMachineEditor(path);
        });
    }
    if (kind == AssetKind::Audio && host_->AudioApi())
    {
        const bool contentAudio = !contentDir_.isEmpty() && QDir::cleanPath(path).startsWith(QDir::cleanPath(contentDir_));
        if (contentAudio)
        {
            QAction* play = menu.addAction(Tr("Прослушать", "Play Audio Preview"));
            connect(play, &QAction::triggered, this, [this, path] {
                EditorCommand c; c.type = EditorCommand::PlayAudio;
                c.path = QStringLiteral("/Game/") + QDir::fromNativeSeparators(QDir(contentDir_).relativeFilePath(path));
                host_->PostCommand(c);
            });
        }
        QAction* stop = menu.addAction(Tr("Остановить звук", "Stop Audio Preview"));
        connect(stop, &QAction::triggered, this, [this] {
            EditorCommand c; c.type = EditorCommand::StopAudio; host_->PostCommand(c);
        });
    }

    const bool importable = kind == AssetKind::Model || kind == AssetKind::Texture;
    if (importable && authoring)
    {
        menu.addSeparator();
        if (kind == AssetKind::Model)
        {
            QAction* settings = menu.addAction(Tr("Настройки импорта…", "Import Settings…"));
            connect(settings, &QAction::triggered, this, [path] {
                CommandRegistry::Instance().Execute(QStringLiteral("Assets.EditImportSettings"), {{QStringLiteral("path"), path}});
            });
        }
        QAction* reimport = menu.addAction(Tr("Переимпортировать…", "Reimport…"));
        connect(reimport, &QAction::triggered, this, [this, path] {
            if (auto* editor = qobject_cast<EditorMainWindow*>(parentWidget())) editor->UpdateAsset(path, false);
        });
    }
    if ((importable || kind == AssetKind::Material || kind == AssetKind::Script) && authoring)
    {
        QAction* move = menu.addAction(Tr("Переместить / переименовать…", "Move / Rename…"));
        connect(move, &QAction::triggered, this, [this, path] {
            if (auto* editor = qobject_cast<EditorMainWindow*>(parentWidget())) editor->UpdateAsset(path, true);
        });
    }
}

void FileBrowserDock::AddFileOperations(QMenu& menu, const QString& path)
{
    menu.addSeparator();
    QAction* duplicate = menu.addAction(Tr("Дублировать", "Duplicate"));
    connect(duplicate, &QAction::triggered, this, [path] {
        CommandRegistry::Instance().Execute(QStringLiteral("Assets.Copy"),
            {{QStringLiteral("paths"), QStringList{path}}, {QStringLiteral("destination"), QFileInfo(path).absolutePath()}});
    });
    QAction* open = menu.addAction(Tr("Открыть в приложении", "Open in External App"));
    connect(open, &QAction::triggered, this, [path] { QDesktopServices::openUrl(QUrl::fromLocalFile(path)); });
    QAction* reveal = menu.addAction(Tr("Показать в проводнике", "Reveal in Explorer"));
    connect(reveal, &QAction::triggered, this, [path] {
        QProcess::startDetached(QStringLiteral("explorer.exe"), {QStringLiteral("/select,") + QDir::toNativeSeparators(path)});
    });
    QAction* copy = menu.addAction(Tr("Копировать путь", "Copy Path"));
    connect(copy, &QAction::triggered, this, [path] { QGuiApplication::clipboard()->setText(path); });
    if (!IsProtectedPath(path))
    {
        QAction* remove = menu.addAction(Tr("Удалить", "Delete"));
        connect(remove, &QAction::triggered, this, [path] {
            CommandRegistry::Instance().Execute(QStringLiteral("Assets.Delete"), {{QStringLiteral("path"), path}});
        });
    }
}

void FileBrowserDock::AddFolderActions(QMenu& menu, const QString& path)
{
    if (!IsProtectedPath(path))
    {
        QAction* rename = menu.addAction(Tr("Переименовать", "Rename"));
        connect(rename, &QAction::triggered, this, [this, path] { BeginInlineRename(path); });
    }
    QAction* reveal = menu.addAction(Tr("Показать в проводнике", "Reveal in Explorer"));
    connect(reveal, &QAction::triggered, this, [path] {
        QProcess::startDetached(QStringLiteral("explorer.exe"), {QStringLiteral("/select,") + QDir::toNativeSeparators(path)});
    });
    QAction* copy = menu.addAction(Tr("Копировать путь", "Copy Path"));
    connect(copy, &QAction::triggered, this, [path] { QGuiApplication::clipboard()->setText(path); });
    if (!IsProtectedPath(path))
    {
        QAction* remove = menu.addAction(Tr("Удалить", "Delete"));
        connect(remove, &QAction::triggered, this, [path] {
            CommandRegistry::Instance().Execute(QStringLiteral("Assets.Delete"), {{QStringLiteral("path"), path}});
        });
    }
}