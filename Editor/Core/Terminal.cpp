#include "Terminal.h"

#include <QLocale>

namespace EditorCore
{

QStringList TerminalParser::Tokenize(const QString& line)
{
    QStringList tokens;
    QString current;
    bool inDoubleQuote = false;
    bool inSingleQuote = false;
    bool escape = false;

    for (int i = 0; i < line.length(); ++i)
    {
        const QChar c = line[i];

        if (escape)
        {
            current += c;
            escape = false;
            continue;
        }

        if (c == QLatin1Char('\\'))
        {
            escape = true;
            continue;
        }

        if (c == QLatin1Char('"') && !inSingleQuote)
        {
            inDoubleQuote = !inDoubleQuote;
            continue;
        }

        if (c == QLatin1Char('\'') && !inDoubleQuote)
        {
            inSingleQuote = !inSingleQuote;
            continue;
        }

        if (c.isSpace() && !inDoubleQuote && !inSingleQuote)
        {
            if (!current.isEmpty())
            {
                tokens.append(current);
                current.clear();
            }
            continue;
        }

        current += c;
    }

    if (!current.isEmpty())
    {
        tokens.append(current);
    }

    return tokens;
}

ParsedCommand TerminalParser::Parse(const QString& line)
{
    ParsedCommand result;
    const QStringList tokens = Tokenize(line.trimmed());
    if (tokens.isEmpty())
    {
        result.isValid = false;
        result.errorMessage = QStringLiteral("Empty command line");
        return result;
    }

    const QString verb = tokens[0].toLower();
    QVariantMap params;

    auto parseDouble = [](const QString& s, bool* ok) -> double {
        return QLocale::c().toDouble(s, ok);
    };

    if (verb == QStringLiteral("help"))
    {
        result.commandId = QStringLiteral("App.Help");
        if (tokens.size() > 1)
        {
            params[QStringLiteral("topic")] = tokens[1];
        }
        result.isValid = true;
    }
    else if (verb == QStringLiteral("spawn"))
    {
        if (tokens.size() < 2)
        {
            result.isValid = false;
            result.errorMessage = QStringLiteral("Usage: spawn <type> [name] [count] [x y z]");
            return result;
        }
        result.commandId = QStringLiteral("Scene.Spawn");
        params[QStringLiteral("type")] = tokens[1];

        int argIdx = 2;
        // Check for count or name or coordinates
        while (argIdx < tokens.size())
        {
            const QString& arg = tokens[argIdx];
            if (arg == QStringLiteral("-count") && argIdx + 1 < tokens.size())
            {
                params[QStringLiteral("count")] = tokens[argIdx + 1].toInt();
                argIdx += 2;
                continue;
            }

            bool isNum = false;
            double num = parseDouble(arg, &isNum);
            if (isNum)
            {
                // If 3 consecutive numbers remain, treat as x y z
                if (argIdx + 2 < tokens.size())
                {
                    bool okY = false, okZ = false;
                    double y = parseDouble(tokens[argIdx + 1], &okY);
                    double z = parseDouble(tokens[argIdx + 2], &okZ);
                    if (okY && okZ)
                    {
                        params[QStringLiteral("x")] = num;
                        params[QStringLiteral("y")] = y;
                        params[QStringLiteral("z")] = z;
                        argIdx += 3;
                        continue;
                    }
                }
                // Single number is count
                params[QStringLiteral("count")] = static_cast<int>(num);
                argIdx++;
                continue;
            }

            if (!params.contains(QStringLiteral("name")))
            {
                params[QStringLiteral("name")] = arg;
            }
            argIdx++;
        }
        result.isValid = true;
    }
    else if (verb == QStringLiteral("select"))
    {
        if (tokens.size() < 2)
        {
            result.isValid = false;
            result.errorMessage = QStringLiteral("Usage: select <name|id>");
            return result;
        }
        result.commandId = QStringLiteral("Scene.Select");
        bool isId = false;
        const qulonglong id = tokens[1].toULongLong(&isId);
        if (isId)
        {
            params[QStringLiteral("id")] = static_cast<quint64>(id);
        }
        else
        {
            params[QStringLiteral("name")] = tokens[1];
        }
        result.isValid = true;
    }
    else if (verb == QStringLiteral("deselect"))
    {
        result.commandId = QStringLiteral("Scene.Deselect");
        result.isValid = true;
    }
    else if (verb == QStringLiteral("pos"))
    {
        if (tokens.size() < 2)
        {
            result.isValid = false;
            result.errorMessage = QStringLiteral("Usage: pos <x y z> | pos <x|y|z> <val>");
            return result;
        }
        result.commandId = QStringLiteral("Scene.SetPosition");
        if (tokens.size() == 3 && (tokens[1].toLower() == QStringLiteral("x") ||
                                   tokens[1].toLower() == QStringLiteral("y") ||
                                   tokens[1].toLower() == QStringLiteral("z")))
        {
            bool ok = false;
            const double val = parseDouble(tokens[2], &ok);
            if (!ok) { result.isValid = false; result.errorMessage = QStringLiteral("Invalid coordinate value"); return result; }
            params[tokens[1].toLower()] = val;
        }
        else if (tokens.size() >= 4)
        {
            bool okX = false, okY = false, okZ = false;
            params[QStringLiteral("x")] = parseDouble(tokens[1], &okX);
            params[QStringLiteral("y")] = parseDouble(tokens[2], &okY);
            params[QStringLiteral("z")] = parseDouble(tokens[3], &okZ);
            if (!okX || !okY || !okZ) { result.isValid = false; result.errorMessage = QStringLiteral("Invalid x y z values"); return result; }
        }
        else
        {
            result.isValid = false;
            result.errorMessage = QStringLiteral("Usage: pos <x y z>");
            return result;
        }
        result.isValid = true;
    }
    else if (verb == QStringLiteral("rot"))
    {
        if (tokens.size() < 2)
        {
            result.isValid = false;
            result.errorMessage = QStringLiteral("Usage: rot <pitch yaw roll> | rot <x|y|z> <val>");
            return result;
        }
        result.commandId = QStringLiteral("Scene.SetRotation");
        if (tokens.size() == 3 && (tokens[1].toLower() == QStringLiteral("x") ||
                                   tokens[1].toLower() == QStringLiteral("y") ||
                                   tokens[1].toLower() == QStringLiteral("z")))
        {
            bool ok = false;
            const double val = parseDouble(tokens[2], &ok);
            if (!ok) { result.isValid = false; result.errorMessage = QStringLiteral("Invalid rotation angle"); return result; }
            params[tokens[1].toLower()] = val;
        }
        else if (tokens.size() >= 4)
        {
            bool okX = false, okY = false, okZ = false;
            params[QStringLiteral("x")] = parseDouble(tokens[1], &okX);
            params[QStringLiteral("y")] = parseDouble(tokens[2], &okY);
            params[QStringLiteral("z")] = parseDouble(tokens[3], &okZ);
            if (!okX || !okY || !okZ) { result.isValid = false; result.errorMessage = QStringLiteral("Invalid rotation values"); return result; }
        }
        else
        {
            result.isValid = false;
            result.errorMessage = QStringLiteral("Usage: rot <x y z>");
            return result;
        }
        result.isValid = true;
    }
    else if (verb == QStringLiteral("scale"))
    {
        if (tokens.size() < 2)
        {
            result.isValid = false;
            result.errorMessage = QStringLiteral("Usage: scale <uniform> | scale <x y z> | scale <x|y|z> <val>");
            return result;
        }
        result.commandId = QStringLiteral("Scene.SetScale");
        if (tokens.size() == 2)
        {
            bool ok = false;
            const double s = parseDouble(tokens[1], &ok);
            if (!ok) { result.isValid = false; result.errorMessage = QStringLiteral("Invalid scale factor"); return result; }
            params[QStringLiteral("x")] = s;
            params[QStringLiteral("y")] = s;
            params[QStringLiteral("z")] = s;
        }
        else if (tokens.size() == 3 && (tokens[1].toLower() == QStringLiteral("x") ||
                                        tokens[1].toLower() == QStringLiteral("y") ||
                                        tokens[1].toLower() == QStringLiteral("z")))
        {
            bool ok = false;
            const double val = parseDouble(tokens[2], &ok);
            if (!ok) { result.isValid = false; result.errorMessage = QStringLiteral("Invalid scale value"); return result; }
            params[tokens[1].toLower()] = val;
        }
        else if (tokens.size() >= 4)
        {
            bool okX = false, okY = false, okZ = false;
            params[QStringLiteral("x")] = parseDouble(tokens[1], &okX);
            params[QStringLiteral("y")] = parseDouble(tokens[2], &okY);
            params[QStringLiteral("z")] = parseDouble(tokens[3], &okZ);
            if (!okX || !okY || !okZ) { result.isValid = false; result.errorMessage = QStringLiteral("Invalid scale values"); return result; }
        }
        result.isValid = true;
    }
    else if (verb == QStringLiteral("delete"))
    {
        result.commandId = QStringLiteral("Scene.Delete");
        result.isValid = true;
    }
    else if (verb == QStringLiteral("duplicate"))
    {
        result.commandId = QStringLiteral("Scene.Duplicate");
        if (tokens.size() > 1)
        {
            bool ok = false;
            int count = tokens[1].toInt(&ok);
            if (ok)
            {
                params[QStringLiteral("count")] = count;
            }
        }
        result.isValid = true;
    }
    else if (verb == QStringLiteral("focus"))
    {
        result.commandId = QStringLiteral("Viewport.Focus");
        result.isValid = true;
    }
    else if (verb == QStringLiteral("play"))
    {
        result.commandId = QStringLiteral("Session.Play");
        result.isValid = true;
    }
    else if (verb == QStringLiteral("pause"))
    {
        result.commandId = QStringLiteral("Session.Pause");
        result.isValid = true;
    }
    else if (verb == QStringLiteral("stop"))
    {
        result.commandId = QStringLiteral("Session.Stop");
        result.isValid = true;
    }
    else if (verb == QStringLiteral("clear"))
    {
        result.commandId = QStringLiteral("Terminal.Clear");
        result.isValid = true;
    }
    else
    {
        result.isValid = false;
        result.errorMessage = QStringLiteral("Unknown command '%1'. Type 'help' for available commands.").arg(tokens[0]);
        return result;
    }

    result.parameters = params;
    return result;
}

} // namespace EditorCore
