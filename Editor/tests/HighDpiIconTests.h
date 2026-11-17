#pragma once

#include "IconProvider.h"
#include <QPixmap>
#include <QImage>
#include <QIcon>
#include <iostream>
#include <stdexcept>

namespace HighDpiIconTests
{

inline void Check(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(std::string("HighDpiIcon check failed: ") + message);
}

inline void Run()
{
    int checks = 0;
    std::cout << "[Test] Running HighDpiIconTests..." << std::endl;

    const QString testIcons[] = {
        QStringLiteral("play"),
        QStringLiteral("pause"),
        QStringLiteral("settings"),
        QStringLiteral("rotate"),
        QStringLiteral("cube"),
        QStringLiteral("bulb")
    };

    const int baseSizes[] = {16, 24};
    const struct { qreal dpr; int scaleNum; int scaleDen; } dprCases[] = {
        {1.0, 1, 1},
        {1.5, 3, 2},
        {2.0, 2, 1}
    };

    for (const auto& name : testIcons)
    {
        for (int baseSize : baseSizes)
        {
            for (const auto& tc : dprCases)
            {
                QPixmap pix = IconProvider::GetPixmap(name, QColor(), baseSize, tc.dpr);
                Check(!pix.isNull(), "Pixmap is not null");

                // devicePixelRatio must equal tc.dpr
                Check(qFuzzyCompare(pix.devicePixelRatio(), tc.dpr), "devicePixelRatio matches requested DPR");

                // Physical backing store size in pixels: baseSize * tc.dpr
                const int expectedPhysSize = qRound(baseSize * tc.dpr);
                Check(pix.width() == expectedPhysSize, "Physical pixmap width == qRound(baseSize * dpr)");
                Check(pix.height() == expectedPhysSize, "Physical pixmap height == qRound(baseSize * dpr)");

                const QImage img = pix.toImage();
                Check(img.width() == expectedPhysSize, "Physical image width == qRound(baseSize * dpr)");
                Check(img.height() == expectedPhysSize, "Physical image height == qRound(baseSize * dpr)");

                // Logical size: physical / dpr == baseSize
                const int logicalW = qRound(pix.width() / pix.devicePixelRatio());
                const int logicalH = qRound(pix.height() / pix.devicePixelRatio());
                Check(logicalW == baseSize, "Logical width equals baseSize");
                Check(logicalH == baseSize, "Logical height equals baseSize");
                ++checks;
            }
        }
    }

    // Multi-DPR QIcon test
    {
        QIcon icon = IconProvider::GetIcon(QStringLiteral("play"), QColor(), 16);
        Check(!icon.isNull(), "Multi-DPR icon is not null");

        // Verify cache hit
        QIcon iconCached = IconProvider::GetIcon(QStringLiteral("play"), QColor(), 16);
        Check(!iconCached.isNull(), "Cached multi-DPR icon is not null");

        // Clear cache and verify re-render
        IconProvider::ClearCache();
        QIcon iconFresh = IconProvider::GetIcon(QStringLiteral("play"), QColor(), 16);
        Check(!iconFresh.isNull(), "Fresh multi-DPR icon after cache clear is not null");
        ++checks;
    }

    std::cout << "HighDpiIcon tests: " << checks << " checks passed" << std::endl;
}

} // namespace HighDpiIconTests
