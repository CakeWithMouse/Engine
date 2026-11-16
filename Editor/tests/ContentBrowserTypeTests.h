#pragma once

#include "ContentBrowserTests.h"
#include "AssetExtensions.h"
#include "EditorLocale.h"
#include <QPersistentModelIndex>
#include <algorithm>

namespace ContentBrowserTypeTests
{
inline void Run(QTreeView* view, FileService& files)
{
    using ContentBrowserTests::Check;
    const int before = ContentBrowserTests::checks;
    auto* proxy = static_cast<ProjectFilesFilter*>(view->model());
    auto* source = static_cast<QFileSystemModel*>(proxy->sourceModel());
    const QString directory = QStringLiteral("Content/FileTypeRegression");
    QString error;
    Check(files.CreateDirectoryPath(directory, &error), "file type fixture folder created");
    QStringList names;
    QList<int> kinds;
    const QList<QStringList> extensions{EditorCore::ModelExtensions(), EditorCore::TextureExtensions(),
        EditorCore::AudioExtensions(), EditorCore::MaterialExtensions(), {QStringLiteral("xyz")}};
    for (int kind = 0; kind < extensions.size(); ++kind)
        for (const auto& extension : extensions[kind]) {
            const QString name = QStringLiteral("type.") + extension.toUpper();
            Check(files.WriteFileAtomic(directory + "/" + name, QByteArray("fixture"), &error), "file type fixture written through service");
            names << name;
            kinds << kind;
        }
    const QString absolute = QDir(files.GetProjectRoot()).filePath(directory);
    QElapsedTimer timer;
    timer.start();
    QList<QPersistentModelIndex> indexes;
    QPersistentModelIndex folder;
    while (timer.elapsed() < 3000) {
        QApplication::processEvents();
        folder = proxy->mapFromSource(source->index(absolute));
        indexes.clear();
        for (const auto& name : names)
            indexes << proxy->mapFromSource(source->index(absolute + "/" + name).siblingAtColumn(2));
        if (folder.isValid() && std::all_of(indexes.begin(), indexes.end(), [](const auto& index) { return index.isValid(); })) break;
        QThread::msleep(1);
    }
    Check(folder.isValid() && std::all_of(indexes.begin(), indexes.end(), [](const auto& index) { return index.isValid(); }),
        "all supported extension rows populated");
    view->expand(folder.parent());
    view->expand(folder);
    const auto original = EditorLocale::Instance().Language();
    QObject observer;
    int headerChanges = 0;
    QObject::connect(proxy, &QAbstractItemModel::headerDataChanged, &observer,
        [&](Qt::Orientation orientation, int first, int last) { if (orientation == Qt::Horizontal && first <= 2 && last >= 2) ++headerChanges; });
    for (auto language : {EditorLanguage::En, EditorLanguage::Ru, EditorLanguage::En}) {
        EditorLocale::Instance().SetLanguage(language);
        QApplication::processEvents();
        const bool english = language == EditorLanguage::En;
        const QStringList expected = english ? QStringList{"Model", "Texture", "Audio", "Material", "File"}
            : QStringList{QString::fromUtf8("Модель"), QString::fromUtf8("Текстура"), QString::fromUtf8("Аудио"), QString::fromUtf8("Материал"), QString::fromUtf8("Файл")};
        for (int i = 0; i < indexes.size(); ++i)
            Check(indexes[i].data().toString() == expected[kinds[i]], "localized file type matches AssetKind, including uppercase FBX and unknown fallback");
        Check(folder.sibling(folder.row(), 2).data().toString() == (english ? QStringLiteral("Folder") : QString::fromUtf8("Папка")), "folder type localized");
        Check(proxy->headerData(2, Qt::Horizontal).toString() == (english ? QStringLiteral("Type") : QString::fromUtf8("Тип")), "type column header localized");
        Check(!view->grab().isNull(), "native tree paints localized types");
    }
    Check(headerChanges >= 2, "language changes notify the live type header");
    EditorLocale::Instance().SetLanguage(original);
    Check(files.DeleteDirectory(directory, &error), "file type fixture removed through service");
    std::printf("ContentBrowser type tests: %d checks passed\n", ContentBrowserTests::checks - before);
}

inline void RunStandalone()
{
    QTemporaryDir fixture;
    FileService files(&Logger::Instance());
    files.SetProjectRoot(fixture.path());
    QFileSystemModel source;
    ProjectFilesFilter proxy;
    proxy.ShowFull(true);
    proxy.setSourceModel(&source);
    QTreeView view;
    view.setObjectName(QStringLiteral("ProjectFiles"));
    view.setModel(&proxy);
    view.setRootIndex(proxy.mapFromSource(source.setRootPath(fixture.path())));
    view.resize(600, 400);
    view.show();
    Run(&view, files);
}

inline void RunNative(QWidget* window)
{
    auto* view = window->findChild<QTreeView*>(QStringLiteral("ProjectFiles"));
    ContentBrowserTests::Check(view, "named Content Browser tree exists for types");
    auto* proxy = static_cast<ProjectFilesFilter*>(view->model());
    auto* source = static_cast<QFileSystemModel*>(proxy->sourceModel());
    FileService files(&Logger::Instance());
    files.SetProjectRoot(source->rootPath());
    Run(view, files);
}
}
