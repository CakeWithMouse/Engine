#pragma once

#include <QString>
#include <QStringList>

enum class PromptResult
{
    Yes,
    No,
    Cancel,
    Ok
};

class IUserPrompt
{
public:
    virtual ~IUserPrompt() = default;

    virtual PromptResult AskYesNoCancel(const QString& title, const QString& message) = 0;
    virtual bool AskConfirmation(const QString& title, const QString& message) = 0;
    virtual void ShowMessage(const QString& title, const QString& message, bool isError = false) = 0;
    virtual QString AskOpenFileName(const QString& title, const QString& filter) = 0;
    virtual QStringList AskOpenFileNames(const QString& title, const QString& filter) = 0;
    virtual QString AskSaveFileName(const QString& title, const QString& filter) = 0;
};

