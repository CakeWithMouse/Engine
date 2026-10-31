#include "EditorLocale.h"
#include "SettingsService.h" 
#include <algorithm>

EditorLocale& EditorLocale::Instance()
{
    static EditorLocale instance;
    return instance;
}

EditorLocale::EditorLocale()
{
    LoadSettings();
}

void EditorLocale::LoadSettings()
{
    const QString langStr = SettingsService::Instance().GetValue(QStringLiteral("ui/language"), QStringLiteral("ru")).toString().toLower();
    if (langStr == QStringLiteral("en"))
    {
        currentLanguage_ = EditorLanguage::En;
    }
    else
    {
        currentLanguage_ = EditorLanguage::Ru;
    }
}

void EditorLocale::SaveSettings()
{
    SettingsService::Instance().SetValue(QStringLiteral("ui/language"),
                      currentLanguage_ == EditorLanguage::En ? QStringLiteral("en") : QStringLiteral("ru"));
}

void EditorLocale::SetLanguage(EditorLanguage lang)
{
    if (currentLanguage_ == lang)
        return;

    currentLanguage_ = lang;
    SaveSettings();
    NotifySubscribers();
}

int EditorLocale::SubscribeLanguageChanged(std::function<void()> callback)
{
    if (!callback)
        return 0;

    const int id = nextSubscriptionId_++;
    subscribers_.emplace_back(id, std::move(callback));
    return id;
}

void EditorLocale::UnsubscribeLanguageChanged(int id)
{
    if (id <= 0)
        return;

    auto it = std::remove_if(subscribers_.begin(), subscribers_.end(),
                             [id](const std::pair<int, std::function<void()>>& item)
                             {
                                 return item.first == id;
                             });
    subscribers_.erase(it, subscribers_.end());
}

void EditorLocale::NotifySubscribers()
{
    auto copy = subscribers_;
    for (const auto& item : copy)
    {
        if (item.second)
        {
            item.second();
        }
    }
}

QString EditorLocale::Translate(const char* ruUtf8, const char* enUtf8) const
{
    if (currentLanguage_ == EditorLanguage::En)
    {
        return QString::fromUtf8(enUtf8 ? enUtf8 : (ruUtf8 ? ruUtf8 : ""));
    }
    return QString::fromUtf8(ruUtf8 ? ruUtf8 : (enUtf8 ? enUtf8 : ""));
}

