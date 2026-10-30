#include "EditorSettings.h"
#include <QSettings>
#include <QString>

EditorSettings& EditorSettings::Instance()
{
    static EditorSettings instance;
    static bool initialized = false;
    if (!initialized)
    {
        initialized = true;
        QSettings settings(QStringLiteral("PainEngine"), QStringLiteral("Editor"));
        const QString val = settings.value(QStringLiteral("viewport/control_scheme"), QStringLiteral("unreal_unity")).toString();
        instance.scheme_ = (val.toLower() == QStringLiteral("blender")) ? ControlScheme::Blender : ControlScheme::UnrealUnity;
    }
    return instance;
}

ControlScheme EditorSettings::GetControlScheme() const
{
    return scheme_;
}

void EditorSettings::SetControlScheme(ControlScheme scheme)
{
    if (scheme_ == scheme)
        return;

    scheme_ = scheme;

    QSettings settings(QStringLiteral("PainEngine"), QStringLiteral("Editor"));
    settings.setValue(QStringLiteral("viewport/control_scheme"),
                      scheme == ControlScheme::Blender ? QStringLiteral("blender") : QStringLiteral("unreal_unity"));

    for (const auto& sub : subscribers_)
    {
        if (sub.second)
            sub.second(scheme_);
    }
}

int EditorSettings::SubscribeControlSchemeChanged(std::function<void(ControlScheme)> callback)
{
    const int id = nextSubId_++;
    subscribers_.push_back({id, std::move(callback)});
    return id;
}

void EditorSettings::UnsubscribeControlSchemeChanged(int id)
{
    for (auto it = subscribers_.begin(); it != subscribers_.end(); ++it)
    {
        if (it->first == id)
        {
            subscribers_.erase(it);
            break;
        }
    }
}
