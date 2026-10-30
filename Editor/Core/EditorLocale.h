#pragma once

#include <QString>
#include <functional>
#include <vector>
#include <utility>

enum class EditorLanguage
{
    Ru,
    En
};

class EditorLocale
{
public:
    static EditorLocale& Instance();

    EditorLanguage Language() const { return currentLanguage_; }
    void SetLanguage(EditorLanguage lang);

    int SubscribeLanguageChanged(std::function<void()> callback);
    void UnsubscribeLanguageChanged(int id);

    QString Translate(const char* ruUtf8, const char* enUtf8) const;

private:
    EditorLocale();
    ~EditorLocale() = default;
    EditorLocale(const EditorLocale&) = delete;
    EditorLocale& operator=(const EditorLocale&) = delete;

    void LoadSettings();
    void SaveSettings();
    void NotifySubscribers();

    EditorLanguage currentLanguage_ = EditorLanguage::Ru;
    int nextSubscriptionId_ = 1;
    std::vector<std::pair<int, std::function<void()>>> subscribers_;
};

inline QString Tr(const char* ruUtf8, const char* enUtf8)
{
    return EditorLocale::Instance().Translate(ruUtf8, enUtf8);
}

