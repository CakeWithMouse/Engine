#pragma once

#include <QFontMetrics>
#include <QLineEdit>
#include <QString>

namespace EditorUI
{

inline QString ElideAssetPath(const QString& path, const QFont& font, int targetWidthPixels = 200)
{
    if (path.isEmpty())
        return path;
    const QFontMetrics fm(font);
    return fm.elidedText(path, Qt::ElideMiddle, targetWidthPixels);
}

inline void FormatAssetPath(
    QLineEdit* edit,
    const QString& fullVirtualPath,
    const QString& displayRelPath,
    int targetWidthPixels = 200
)
{
    if (!edit)
        return;

    edit->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    edit->setMinimumWidth(0);

    if (displayRelPath.isEmpty())
    {
        edit->clear();
        edit->setToolTip(QString());
        return;
    }

    const QString elided = ElideAssetPath(displayRelPath, edit->font(), targetWidthPixels);
    edit->setText(elided);
    if (elided != displayRelPath)
    {
        edit->setToolTip(fullVirtualPath.isEmpty() ? displayRelPath : fullVirtualPath);
    }
    else
    {
        edit->setToolTip(QString());
    }
}

} // namespace EditorUI
