#include "SettingsService.h"
#include "Logger.h"
#include <QCryptographicHash>
#include <QDir>

SettingsService& SettingsService::Instance()
{
    static SettingsService s_instance;
    return s_instance;
}

SettingsService::SettingsService()
    : settings_(QStringLiteral("PainEngine"), QStringLiteral("Editor"))
{
}

void SettingsService::SetValue(const QString& key, const QVariant& value)
{
    Logger::Instance().Info(QStringLiteral("SettingsService: Set '%1'").arg(key));
    settings_.setValue(key, value);
}

QVariant SettingsService::GetValue(const QString& key, const QVariant& defaultValue) const
{
    return settings_.value(key, defaultValue);
}

bool SettingsService::Contains(const QString& key) const
{
    return settings_.contains(key);
}

void SettingsService::Remove(const QString& key)
{
    Logger::Instance().Info(QStringLiteral("SettingsService: Remove '%1'").arg(key));
    settings_.remove(key);
}

QString SettingsService::ComputeProjectKey(const QString& projectPath)
{
    const QString clean = QDir::cleanPath(projectPath).trimmed();
    if (clean.isEmpty()) return QStringLiteral("default");
    const QByteArray hash = QCryptographicHash::hash(clean.toUtf8(), QCryptographicHash::Sha1).toHex();
    return QString::fromLatin1(hash.left(12));
}

QString SettingsService::LayoutStateKey(const QString& projectPath)
{
    return QStringLiteral("layout/%1/state").arg(ComputeProjectKey(projectPath));
}

QString SettingsService::LayoutGeometryKey(const QString& projectPath)
{
    return QStringLiteral("layout/%1/geometry").arg(ComputeProjectKey(projectPath));
}

void SettingsService::SaveProjectLayout(const QString& projectPath, const QByteArray& state, const QByteArray& geometry)
{
    SetValue(LayoutStateKey(projectPath), state);
    SetValue(LayoutGeometryKey(projectPath), geometry);
}

bool SettingsService::RestoreProjectLayout(const QString& projectPath, QByteArray& outState, QByteArray& outGeometry) const
{
    const QString stateKey = LayoutStateKey(projectPath);
    if (!Contains(stateKey)) {
        return false;
    }
    outState = GetValue(stateKey).toByteArray();
    outGeometry = GetValue(LayoutGeometryKey(projectPath)).toByteArray();
    return !outState.isEmpty();
}

void SettingsService::ResetProjectLayout(const QString& projectPath)
{
    Remove(LayoutStateKey(projectPath));
    Remove(LayoutGeometryKey(projectPath));
}
