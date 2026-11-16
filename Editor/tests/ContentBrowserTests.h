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
#include <QPainter>
#include <QStyleOptionViewItem>
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

inline void CheckBranchArrowHighDpi()
{
    for (qreal dpr : {1.0, 1.5, 2.0})
    {
        const int logicalSize = 20;
        const int physSize = std::max(1, qRound(logicalSize * dpr));

        // 1. Closed branch (chevron-right)
        QImage closedImage(physSize, physSize, QImage::Format_ARGB32_Premultiplied);
        closedImage.setDevicePixelRatio(dpr);
        closedImage.fill(Qt::transparent);

        {
            QPainter p(&closedImage);
            QStyleOptionViewItem opt;
            opt.rect = QRect(0, 0, logicalSize, logicalSize);
            opt.state = QStyle::State_Children | QStyle::State_Enabled;
            QApplication::style()->drawPrimitive(QStyle::PE_IndicatorBranch, &opt, &p);
        }

        int closedInk = 0;
        int minX = physSize, maxX = 0, minY = physSize, maxY = 0;
        for (int y = 0; y < physSize; ++y)
        {
            for (int x = 0; x < physSize; ++x)
            {
                if (closedImage.pixelColor(x, y).alpha() > 20)
                {
                    ++closedInk;
                    minX = std::min(minX, x);
                    maxX = std::max(maxX, x);
                    minY = std::min(minY, y);
                    maxY = std::max(maxY, y);
                }
            }
        }
        Check(closedInk > 0, "closed branch arrow renders visible pixels");
        Check(minX > 0 && maxX < physSize - 1, "closed branch arrow not clipped on horizontal bounds");
        Check(minY > 0 && maxY < physSize - 1, "closed branch arrow not clipped on vertical bounds");

        const int closedW = maxX - minX + 1;
        const int closedH = maxY - minY + 1;
        Check(closedW >= qRound(4 * dpr) && closedH >= qRound(7 * dpr),
              "closed arrow dimensions scale proportionally with DPR");

        // 2. Open branch (chevron-down)
        QImage openImage(physSize, physSize, QImage::Format_ARGB32_Premultiplied);
        openImage.setDevicePixelRatio(dpr);
        openImage.fill(Qt::transparent);

        {
            QPainter p(&openImage);
            QStyleOptionViewItem opt;
            opt.rect = QRect(0, 0, logicalSize, logicalSize);
            opt.state = QStyle::State_Children | QStyle::State_Open | QStyle::State_Enabled;
            QApplication::style()->drawPrimitive(QStyle::PE_IndicatorBranch, &opt, &p);
        }

        int openInk = 0;
        int openMinX = physSize, openMaxX = 0, openMinY = physSize, openMaxY = 0;
        for (int y = 0; y < physSize; ++y)
        {
            for (int x = 0; x < physSize; ++x)
            {
                if (openImage.pixelColor(x, y).alpha() > 20)
                {
                    ++openInk;
                    openMinX = std::min(openMinX, x);
                    openMaxX = std::max(openMaxX, x);
                    openMinY = std::min(openMinY, y);
                    openMaxY = std::max(openMaxY, y);
                }
            }
        }
        Check(openInk > 0, "open branch arrow renders visible pixels");
        Check(openMinX > 0 && openMaxX < physSize - 1, "open branch arrow not clipped on horizontal bounds");
        Check(openMinY > 0 && openMaxY < physSize - 1, "open branch arrow not clipped on vertical bounds");
        Check(openImage != closedImage, "open and closed branch glyphs are visually distinct");

        // 3. Leaf item (no children) draws nothing
        QImage leafImage(physSize, physSize, QImage::Format_ARGB32_Premultiplied);
        leafImage.setDevicePixelRatio(dpr);
        leafImage.fill(Qt::transparent);

        {
            QPainter p(&leafImage);
            QStyleOptionViewItem opt;
            opt.rect = QRect(0, 0, logicalSize, logicalSize);
            opt.state = QStyle::State_Enabled;
            QApplication::style()->drawPrimitive(QStyle::PE_IndicatorBranch, &opt, &p);
        }

        int leafInk = 0;
        for (int y = 0; y < physSize; ++y)
            for (int x = 0; x < physSize; ++x)
                if (leafImage.pixelColor(x, y).alpha() > 20)
                    ++leafInk;
        Check(leafInk == 0, "leaf item without children has no branch indicator ink");
    }
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
    CheckBranchArrowHighDpi();
    std::printf("ContentBrowser icon tests: %d checks passed\n", checks);
}
}
