#pragma once

#include "FileBrowser.h"
#include "EditorTheme.h"
#include "FileService.h"
#include "Logger.h"
#include <QApplication>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QThread>
#include <QTreeView>
#include <QPersistentModelIndex>
#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace ContentBrowserTests
{
inline int checks = 0;
inline void Check(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
    ++checks;
}

inline double Luminance(const QColor& color)
{
    auto linear = [](double value) { return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4); };
    return 0.2126 * linear(color.redF()) + 0.7152 * linear(color.greenF()) + 0.0722 * linear(color.blueF());
}

inline double Contrast(const QImage& image, const QColor& background)
{
    double minimum = 100;
    int pixels = 0;
    const double bg = Luminance(background);
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x) {
            const auto color = image.pixelColor(x, y);
            if (color.alpha() < 250) continue;
            const double ink = Luminance(color);
            minimum = std::min(minimum, (std::max(bg, ink) + 0.05) / (std::min(bg, ink) + 0.05));
            ++pixels;
        }
    Check(pixels >= 10, "file icon has opaque visible ink");
    return minimum;
}

inline void CheckTheme(QTreeView* view, const QPersistentModelIndex& folder, const QPersistentModelIndex& file)
{
    QImage darkFolder, darkFile;
    for (auto mode : {ThemeMode::Dark, ThemeMode::Light, ThemeMode::Dark}) {
        EditorTheme::Instance().SetMode(mode);
        QApplication::processEvents();
        Check(folder.isValid() && file.isValid(), "file and folder rows survive theme changes");
        const auto folderImage = folder.data(Qt::DecorationRole).value<QIcon>().pixmap(24, 24).toImage();
        const auto fileImage = file.data(Qt::DecorationRole).value<QIcon>().pixmap(24, 24).toImage();
        const auto background = EditorTheme::Instance().BgPanel();
        const double folderContrast = Contrast(folderImage, background);
        const double fileContrast = Contrast(fileImage, background);
        std::printf("Content icons %s contrast: folder %.2f file %.2f\n",
            mode == ThemeMode::Dark ? "dark" : "light", folderContrast, fileContrast);
        Check(folderContrast >= 3 && fileContrast >= 3, "content icons contrast at least 3:1 after theme switch");
        if (mode == ThemeMode::Light) {
            Check(folderImage != darkFolder && fileImage != darkFile, "cached file and folder glyphs change with theme");
        } else {
            darkFolder = folderImage;
            darkFile = fileImage;
        }
        Check(!view->grab().isNull(), "native content tree paints after theme switch");
    }
}

inline void RunNative(QWidget* window)
{
    auto* view = window->findChild<QTreeView*>(QStringLiteral("ProjectFiles"));
    Check(view, "named Content Browser tree exists");
    auto* proxy = static_cast<ProjectFilesFilter*>(view->model());
    auto* source = static_cast<QFileSystemModel*>(proxy->sourceModel());
    Logger logger;
    FileService files(&logger);
    files.SetProjectRoot(source->rootPath());
    QString error;
    Check(files.WriteFileAtomic(QStringLiteral("Content/icon-test.fbx"), QByteArray("fixture"), &error), "native icon fixture created");
    QElapsedTimer timer;
    timer.start();
    QPersistentModelIndex folder, file;
    while (timer.elapsed() < 3000) {
        QApplication::processEvents();
        folder = proxy->mapFromSource(source->index(source->rootPath() + QStringLiteral("/Content")));
        file = proxy->mapFromSource(source->index(source->rootPath() + QStringLiteral("/Content/icon-test.fbx")));
        if (folder.isValid() && file.isValid()) break;
        QThread::msleep(1);
    }
    Check(folder.isValid() && file.isValid(), "native file model has icon fixture");
    view->expand(folder);
    CheckTheme(view, folder, file);
    Check(files.DeleteFile(QStringLiteral("Content/icon-test.fbx"), &error), "native icon fixture removed");
    std::printf("Native ContentBrowser icon tests: PASS\n");
}

inline void Run()
{
    checks = 0;
    QTemporaryDir fixture;
    Logger logger;
    FileService files(&logger);
    files.SetProjectRoot(fixture.path());
    QString error;
    Check(files.CreateDirectoryPath(QStringLiteral("Content"), &error), "content fixture directory created");
    Check(files.WriteFileAtomic(QStringLiteral("Content/test.fbx"), QByteArray("fixture"), &error), "content fixture file created");
    EditorTheme::Instance().SetMode(ThemeMode::Dark);
    QFileSystemModel source;
    source.setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);
    ProjectFilesFilter proxy;
    proxy.ShowFull(true);
    proxy.setSourceModel(&source);
    QTreeView view;
    view.setObjectName(QStringLiteral("ProjectFiles"));
    view.setModel(&proxy);
    view.setRootIndex(proxy.mapFromSource(source.setRootPath(fixture.path())));
    view.resize(500, 240);
    view.show();
    QElapsedTimer timer;
    timer.start();
    QPersistentModelIndex folder, file;
    while (timer.elapsed() < 3000) {
        QApplication::processEvents();
        folder = proxy.mapFromSource(source.index(fixture.path() + QStringLiteral("/Content")));
        file = proxy.mapFromSource(source.index(fixture.path() + QStringLiteral("/Content/test.fbx")));
        if (folder.isValid() && file.isValid()) break;
        QThread::msleep(1);
    }
    Check(folder.isValid() && file.isValid(), "file model populated without rebuilding on theme switch");
    view.expand(folder);
    CheckTheme(&view, folder, file);
    std::printf("ContentBrowser icon tests: %d checks passed\n", checks);
}
}
