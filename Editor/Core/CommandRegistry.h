#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

struct CommandContext
{
    QString commandId;
    QVariantMap parameters;
};

using CommandHandler = std::function<bool(const CommandContext&)>;

struct CommandDef
{
    QString id;
    QString title;
    QString description;
    QString shortcut;
    CommandHandler handler;
    std::function<bool()> canExecute;
};

class CommandRegistry : public QObject
{
    Q_OBJECT
public:
    explicit CommandRegistry(QObject* parent = nullptr);
    ~CommandRegistry() override = default;

    static CommandRegistry& Instance();

    bool RegisterCommand(const CommandDef& def);
    bool UnregisterCommand(const QString& id);
    bool HasCommand(const QString& id) const;
    const CommandDef* FindCommand(const QString& id) const;

    bool CanExecute(const QString& id) const;
    bool Execute(const QString& id, const QVariantMap& params = {});

    std::vector<QString> GetRegisteredCommandIds() const;
    void Clear();

signals:
    void CommandExecuted(const QString& id, bool success);

private:
    std::unordered_map<QString, CommandDef> commands_;
};

