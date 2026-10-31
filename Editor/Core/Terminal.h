#pragma once

#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace EditorCore
{

struct ParsedCommand
{
    bool isValid = false;
    QString commandId;
    QVariantMap parameters;
    QString errorMessage;
};

class TerminalParser
{
public:
    static QStringList Tokenize(const QString& line);
    static ParsedCommand Parse(const QString& line);
};

} // namespace EditorCore
