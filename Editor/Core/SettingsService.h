#pragma once

#include <QString>
#include <QVariant>
#include <QSettings>

class SettingsService
{
public:
    static SettingsService& Instance();

    void SetValue(const QString& key, const QVariant& value);
    QVariant GetValue(const QString& key, const QVariant& defaultValue = {}) const;
    bool Contains(const QString& key) const;
    void Remove(const QString& key);

    static QString ComputeProjectKey(const QString& projectPath);
    static QString LayoutStateKey(const QString& projectPath);
    static QString LayoutGeometryKey(const QString& projectPath);

    void SaveProjectLayout(const QString& projectPath, const QByteArray& state, const QByteArray& geometry);
    bool RestoreProjectLayout(const QString& projectPath, QByteArray& outState, QByteArray& outGeometry) const;
    void ResetProjectLayout(const QString& projectPath);

private:
    SettingsService();
    mutable QSettings settings_;
};
