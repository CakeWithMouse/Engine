#include "CommandRegistry.h"
#include "Logger.h"

CommandRegistry::CommandRegistry(QObject* parent)
    : QObject(parent)
{
}

CommandRegistry& CommandRegistry::Instance()
{
    static CommandRegistry sInstance;
    return sInstance;
}

bool CommandRegistry::RegisterCommand(const CommandDef& def)
{
    if (def.id.trimmed().isEmpty())
    {
        Logger::Instance().Error(QStringLiteral("CommandRegistry: cannot register command with empty id"));
        return false;
    }

    if (commands_.find(def.id) != commands_.end())
    {
        Logger::Instance().Warning(QStringLiteral("CommandRegistry: duplicate command registration '%1'").arg(def.id));
        return false;
    }

    if (!def.handler)
    {
        Logger::Instance().Error(QStringLiteral("CommandRegistry: command '%1' has no handler").arg(def.id));
        return false;
    }

    commands_[def.id] = def;
    return true;
}

bool CommandRegistry::UnregisterCommand(const QString& id)
{
    auto it = commands_.find(id);
    if (it != commands_.end())
    {
        commands_.erase(it);
        return true;
    }
    return false;
}

bool CommandRegistry::HasCommand(const QString& id) const
{
    return commands_.find(id) != commands_.end();
}

const CommandDef* CommandRegistry::FindCommand(const QString& id) const
{
    auto it = commands_.find(id);
    if (it != commands_.end())
    {
        return &it->second;
    }
    return nullptr;
}

bool CommandRegistry::CanExecute(const QString& id) const
{
    const CommandDef* def = FindCommand(id);
    if (!def)
    {
        return false;
    }
    if (def->canExecute)
    {
        return def->canExecute();
    }
    return true;
}

bool CommandRegistry::Execute(const QString& id, const QVariantMap& params)
{
    const CommandDef* def = FindCommand(id);
    if (!def)
    {
        Logger::Instance().Error(QStringLiteral("CommandRegistry: command not found '%1'").arg(id));
        emit CommandExecuted(id, false);
        return false;
    }

    if (def->canExecute && !def->canExecute())
    {
        Logger::Instance().Warning(QStringLiteral("CommandRegistry: command cannot execute '%1'").arg(id));
        emit CommandExecuted(id, false);
        return false;
    }

    CommandContext ctx;
    ctx.commandId = id;
    ctx.parameters = params;

    bool result = false;
    try
    {
        result = def->handler(ctx);
    }
    catch (const std::exception& e)
    {
        Logger::Instance().Error(QStringLiteral("CommandRegistry: exception in '%1': %2").arg(id, QString::fromUtf8(e.what())));
        result = false;
    }
    catch (...)
    {
        Logger::Instance().Error(QStringLiteral("CommandRegistry: unknown exception in '%1'").arg(id));
        result = false;
    }

    emit CommandExecuted(id, result);
    return result;
}

std::vector<QString> CommandRegistry::GetRegisteredCommandIds() const
{
    std::vector<QString> ids;
    ids.reserve(commands_.size());
    for (const auto& pair : commands_)
    {
        ids.push_back(pair.first);
    }
    return ids;
}

void CommandRegistry::Clear()
{
    commands_.clear();
}

