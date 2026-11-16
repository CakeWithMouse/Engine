#pragma once

#include <QMargins>
#include <QLabel>
#include <QWidget>
#include <QLayout>

namespace InspectorCardStyle
{
    // Unified sizing constants across all inspector cards
    constexpr int kLabelWidth = 80;
    constexpr int kFieldHeight = 22;
    constexpr int kButtonHeight = 24;
    constexpr int kHeaderHeight = 22;
    constexpr int kCardSpacing = 4;
    constexpr int kRowSpacing = 4;

    // Card content margins (6, 4, 6, 4)
    constexpr int kMarginLeft = 6;
    constexpr int kMarginTop = 4;
    constexpr int kMarginRight = 6;
    constexpr int kMarginBottom = 4;

    inline QMargins CardMargins()
    {
        return QMargins(kMarginLeft, kMarginTop, kMarginRight, kMarginBottom);
    }

    inline QMargins RowMargins()
    {
        return QMargins(0, 0, 0, 0);
    }

    inline void ApplyLabelStyle(QLabel* label, int width = kLabelWidth)
    {
        if (label)
        {
            label->setObjectName(QStringLiteral("CardFieldLabel"));
            label->setFixedWidth(width);
        }
    }

    inline void ApplyRowLayout(QLayout* layout, int spacing = kRowSpacing)
    {
        if (layout)
        {
            layout->setContentsMargins(RowMargins());
            layout->setSpacing(spacing);
        }
    }
}
