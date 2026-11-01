#pragma once
#include <cstdint>
#include <QIcon>
#include <QPixmap>
#include <QString>
#include <QColor>

class IEditorHost;

class IconProvider
{
public:
    static void UploadIcons(IEditorHost* host, bool force = false);

    static QIcon GetIcon(const QString& name, const QColor& color = QColor(), int size = 16);
    static QPixmap GetPixmap(const QString& name, const QColor& color = QColor(), int size = 16);

    // Helpers for Tabler outline icons
    static QIcon PlayIcon(const QColor& color = QColor());
    static QIcon PauseIcon(const QColor& color = QColor());
    static QIcon StopIcon(const QColor& color = QColor());
    static QIcon ExternalIcon(const QColor& color = QColor());
    static QIcon SettingsIcon(const QColor& color = QColor());
    static QIcon TranslateIcon(const QColor& color = QColor());
    static QIcon RotateIcon(const QColor& color = QColor());
    static QIcon ScaleIcon(const QColor& color = QColor());
    static QIcon WorldIcon(const QColor& color = QColor());
    static QIcon LocalIcon();
    static QIcon SnapIcon(const QColor& color = QColor());
    static QIcon ResetIcon(const QColor& color = QColor());
    static QIcon MoreIcon(const QColor& color = QColor());
    static QIcon EyeIcon(bool visible = true, const QColor& color = QColor());
    static QIcon LockIcon(bool locked = true, const QColor& color = QColor());
    static QIcon ImportIcon(const QColor& color = QColor());
    static QIcon PlusIcon(const QColor& color = QColor());
    static QIcon TrashIcon(const QColor& color = QColor());
    static QIcon FolderIcon(const QColor& color = QColor());
    static QIcon FileIcon(const QColor& color = QColor());
    static QIcon SearchIcon(const QColor& color = QColor());
    static QIcon BulbIcon(const QColor& color = QColor());
    static QIcon CubeIcon(const QColor& color = QColor());
    static QIcon AdjustmentsIcon(const QColor& color = QColor());
};
