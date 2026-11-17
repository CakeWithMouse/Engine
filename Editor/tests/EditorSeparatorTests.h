#pragma once

#include "EditorSeparator.h"
#include "EditorTheme.h"
#include <QImage>
#include <QPainter>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <cmath>

namespace EditorSeparatorTests
{

inline void Check(bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

inline void Run()
{
    int checks = 0;
    const std::vector<qreal> testDprs = { 1.0, 1.25, 1.5, 2.0 };

    EditorTheme::Instance().SetMode(ThemeMode::Dark);
    const QColor darkBorder = EditorTheme::Instance().Palette().borderLight;

    // Test Vertical separator across DPRs
    for (qreal dpr : testDprs)
    {
        EditorSeparator sep(Qt::Vertical);
        const int logicalW = 6;
        const int logicalH = 16;
        sep.setFixedSize(logicalW, logicalH);

        const int physW = std::max(1, qRound(logicalW * dpr));
        const int physH = std::max(1, qRound(logicalH * dpr));

        QImage img(physW, physH, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        img.setDevicePixelRatio(dpr);

        sep.render(&img);

        int filledRows = 0;
        int maxThicknessInRow = 0;

        for (int y = 0; y < physH; ++y)
        {
            int rowPixels = 0;
            for (int x = 0; x < physW; ++x)
            {
                const QColor c = img.pixelColor(x, y);
                if (c.alpha() > 128)
                {
                    ++rowPixels;
                }
            }
            if (rowPixels >= 1)
            {
                ++filledRows;
                maxThicknessInRow = std::max(maxThicknessInRow, rowPixels);
            }
        }

        Check(filledRows == physH, "vertical separator has solid line covering full physical height");
        Check(maxThicknessInRow >= 1, "vertical separator line thickness is at least 1 physical pixel");
        ++checks;
    }

    // Test Horizontal separator across DPRs
    for (qreal dpr : testDprs)
    {
        EditorSeparator sep(Qt::Horizontal);
        const int logicalW = 16;
        const int logicalH = 6;
        sep.setFixedSize(logicalW, logicalH);

        const int physW = std::max(1, qRound(logicalW * dpr));
        const int physH = std::max(1, qRound(logicalH * dpr));

        QImage img(physW, physH, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        img.setDevicePixelRatio(dpr);

        sep.render(&img);

        int filledCols = 0;
        int maxThicknessInCol = 0;

        for (int x = 0; x < physW; ++x)
        {
            int colPixels = 0;
            for (int y = 0; y < physH; ++y)
            {
                const QColor c = img.pixelColor(x, y);
                if (c.alpha() > 128)
                {
                    ++colPixels;
                }
            }
            if (colPixels >= 1)
            {
                ++filledCols;
                maxThicknessInCol = std::max(maxThicknessInCol, colPixels);
            }
        }

        Check(filledCols == physW, "horizontal separator has solid line covering full physical width");
        Check(maxThicknessInCol >= 1, "horizontal separator line thickness is at least 1 physical pixel");
        ++checks;
    }

    // Test theme change updates color
    {
        EditorSeparator sep(Qt::Vertical);
        sep.setFixedSize(6, 16);

        QImage imgDark(6, 16, QImage::Format_ARGB32_Premultiplied);
        imgDark.fill(Qt::transparent);
        imgDark.setDevicePixelRatio(1.0);
        sep.render(&imgDark);

        QColor centerDark = imgDark.pixelColor(3, 8);
        if (centerDark.alpha() == 0) centerDark = imgDark.pixelColor(2, 8);
        Check(centerDark.alpha() > 0, "dark theme separator pixel drawn");
        Check(centerDark.rgb() == darkBorder.rgb(), "dark theme uses borderLight");
        ++checks;

        EditorTheme::Instance().SetMode(ThemeMode::Light);
        const QColor lightBorder = EditorTheme::Instance().Palette().borderLight;

        QImage imgLight(6, 16, QImage::Format_ARGB32_Premultiplied);
        imgLight.fill(Qt::transparent);
        imgLight.setDevicePixelRatio(1.0);
        sep.render(&imgLight);

        QColor centerLight = imgLight.pixelColor(3, 8);
        if (centerLight.alpha() == 0) centerLight = imgLight.pixelColor(2, 8);
        Check(centerLight.alpha() > 0, "light theme separator pixel drawn");
        Check(centerLight.rgb() == lightBorder.rgb(), "light theme uses borderLight");
        Check(centerLight.rgb() != darkBorder.rgb(), "separator color changes with theme");
        ++checks;

        EditorTheme::Instance().SetMode(ThemeMode::Dark);
    }

    std::printf("EditorSeparator tests: %d checks passed\n", checks);
}

} // namespace EditorSeparatorTests
