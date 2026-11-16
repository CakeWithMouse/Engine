#pragma once

#include "ContentBrowserTests.h"
#include "CommandRegistry.h"
#include "EditorLocale.h"
#include <QComboBox>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QPersistentModelIndex>
#include <QTimer>

namespace FolderCreationTests
{
using ContentBrowserTests::Check;

inline QLineEdit* WaitEditor(QTreeView* view)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 3000) {
        QApplication::processEvents();
        if (auto* editor = view->findChild<QLineEdit*>(QStringLiteral("FolderNameEditor")); editor && editor->isVisible()) return editor;
        QThread::msleep(1);
    }
    const QModelIndex current = view->currentIndex();
    const Qt::ItemFlags flags = current.isValid() ? view->model()->flags(current) : Qt::NoItemFlags;
    const QRect rect = current.isValid() ? view->visualRect(current) : QRect();
    const QString details = QStringLiteral(
        "created folder must immediately enter inline rename "
        "(visible=%1 viewport=%2 current=%3 editable=%4 enabled=%5 rect=%6x%7 name='%8' triggers=%9)")
        .arg(view->isVisible()).arg(view->viewport()->isVisible()).arg(current.isValid())
        .arg(flags.testFlag(Qt::ItemIsEditable)).arg(flags.testFlag(Qt::ItemIsEnabled))
        .arg(rect.width()).arg(rect.height()).arg(current.data().toString()).arg(int(view->editTriggers()));
    throw std::runtime_error(details.toStdString());
}

inline void Finish(QLineEdit* editor, const QString& name, bool cancel = false)
{
    editor->setText(name);
    QKeyEvent enter(QEvent::KeyPress, cancel ? Qt::Key_Escape : Qt::Key_Return, Qt::NoModifier);
    QApplication::sendEvent(editor, &enter);
    QApplication::processEvents();
}

inline void MenuCreate(QWidget* window, QTreeView* view, const QPoint& position)
{
    bool found = false;
    QTimer::singleShot(0, view, [&] {
        auto* menu = window->findChild<QMenu*>(QStringLiteral("ContentBrowserMenu"));
        if (!menu) return;
        auto* action = menu->findChild<QAction*>(QStringLiteral("AssetsCreateFolder"));
        found = action && action->isEnabled();
        if (found) action->trigger();
        menu->close();
    });
    Check(QMetaObject::invokeMethod(view, "customContextMenuRequested", Qt::DirectConnection, Q_ARG(QPoint, position)), "native content context menu invoked");
    Check(found, "named create-folder action exists and is enabled");
}

inline void RunNative(QWidget* window)
{
    const int before = ContentBrowserTests::checks;
    auto* view = window->findChild<QTreeView*>(QStringLiteral("ProjectFiles"));
    Check(view, "named file tree exists for folder commands");
    auto* proxy = static_cast<ProjectFilesFilter*>(view->model());
    auto* source = static_cast<QFileSystemModel*>(proxy->sourceModel());
    FileService files(&Logger::Instance());
    files.SetProjectRoot(source->rootPath());
    QString error;
    auto& registry = CommandRegistry::Instance();
    Check(registry.HasCommand(QStringLiteral("Assets.CreateFolder")), "real create-folder command registered");
    Check(source->isReadOnly(), "filesystem model remains read-only during folder editing");
    const auto originalLanguage = EditorLocale::Instance().Language();
    EditorLocale::Instance().SetLanguage(EditorLanguage::En);
    const QVariantMap parent{{QStringLiteral("parent"), QStringLiteral("Content")}};
    Check(registry.Execute(QStringLiteral("Assets.CreateFolder"), parent), "folder command succeeds");
    auto* editor = WaitEditor(view);
    Check(editor->text() == QStringLiteral("New Folder"), "new folder uses localized default name");
    Finish(editor, QStringLiteral("Renamed Folder"));
    Check(files.DirectoryExists(QStringLiteral("Content/Renamed Folder")), "Enter renames folder through sandbox operations");
    Check(!files.DirectoryExists(QStringLiteral("Content/New Folder")), "generated name removed after rename");
    Check(files.WriteFileAtomic(QStringLiteral("Content/New Folder"), QByteArray("collision"), &error), "file collision fixture created");
    Check(registry.Execute(QStringLiteral("Assets.CreateFolder"), parent), "creation skips file name collision");
    editor = WaitEditor(view);
    Check(editor->text() == QStringLiteral("New Folder (2)"), "file collision gets unique name");
    Finish(editor, QString(), true);
    Check(files.DirectoryExists(QStringLiteral("Content/New Folder (2)")), "Escape keeps the newly created folder");
    const QPersistentModelIndex renamed = proxy->mapFromSource(source->index(source->rootPath() + QStringLiteral("/Content/Renamed Folder")));
    view->expand(renamed.parent());
    view->scrollTo(renamed);
    QApplication::processEvents();
    Check(renamed.isValid() && !view->visualRect(renamed).isEmpty(), "folder context target visible");
    MenuCreate(window, view, view->visualRect(renamed).center());
    Finish(WaitEditor(view), QStringLiteral("Nested"));
    Check(files.DirectoryExists(QStringLiteral("Content/Renamed Folder/Nested")), "folder menu creates within clicked folder");
    view->collapseAll();
    QApplication::processEvents();
    const QPoint empty = view->viewport()->rect().bottomRight() - QPoint(5, 5);
    Check(!view->indexAt(empty).isValid(), "empty-region menu target has no item");
    MenuCreate(window, view, empty);
    editor = WaitEditor(view);
    Check(editor->text() == QStringLiteral("New Folder (3)"), "empty-region creation resolves directory and file collisions");
    Finish(editor, QStringLiteral("../Rejected"));
    Check(files.DirectoryExists(QStringLiteral("Content/New Folder (3)")), "invalid rename preserves original folder");
    Check(!files.DirectoryExists(QStringLiteral("Rejected")), "invalid rename does not escape parent");
    EditorLocale::Instance().SetLanguage(EditorLanguage::Ru);
    Check(registry.Execute(QStringLiteral("Assets.CreateFolder"), parent), "Russian folder creation succeeds");
    editor = WaitEditor(view);
    Check(editor->text() == QString::fromUtf8("Новая папка"), "Russian default name localized");
    Finish(editor, QString(), true);
    for (const auto& path : QStringList{QString(), "../Outside", "Content/../Content", "Content\\..\\Content", source->rootPath(), "C:Outside"})
        Check(!registry.Execute(QStringLiteral("Assets.CreateFolder"), {{QStringLiteral("parent"), path}}), "command rejects non-relative or traversal parent");
    Check(!registry.Execute(QStringLiteral("Assets.RenameFolder"), {{QStringLiteral("path"), QStringLiteral("Content")}, {QStringLiteral("name"), QStringLiteral("MovedContent")}}), "rename command cannot rename protected content root");
    Check(source->isReadOnly(), "source model never becomes writable");
    Check(files.DeleteFile(QStringLiteral("Content/New Folder"), &error), "collision file removed through service");
    for (const auto& name : QStringList{"Renamed Folder", "New Folder (2)", "New Folder (3)", QString::fromUtf8("Новая папка")})
        Check(files.DeleteDirectory(QStringLiteral("Content/") + name, &error), "folder fixture cleanup logged through service");
    EditorLocale::Instance().SetLanguage(originalLanguage);
    std::printf("Folder creation native tests: %d checks passed\n", ContentBrowserTests::checks - before);
}
}
