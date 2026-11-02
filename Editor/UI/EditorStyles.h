#pragma once

#include <QString>
#include <QColor>

enum class ThemeMode;

namespace EditorStyles
{
    QString BuildStyleSheet(ThemeMode mode, const QColor& accentColor,
                           const QString& arrowUp = {},
                           const QString& arrowDown = {},
                           const QString& arrowRight = {});
}
