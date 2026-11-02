#pragma once

#include <QObject>
#include <QString>
#include <functional>
#include <mutex>
#include <vector>

enum class LogLevel
{
    Trace = 0,
    Debug,
    Info,
    Warning,
    Error
};

struct LogEntry
{
    LogLevel level = LogLevel::Info;
    QString message;
    qint64 timestampMs = 0;
};

class Logger : public QObject
{
    Q_OBJECT
public:
    explicit Logger(QObject* parent = nullptr);
    ~Logger() override = default;

    static Logger& Instance();

    void Log(LogLevel level, const QString& message);
    void Trace(const QString& msg) { Log(LogLevel::Trace, msg); }
    void Debug(const QString& msg) { Log(LogLevel::Debug, msg); }
    void Info(const QString& msg) { Log(LogLevel::Info, msg); }
    void Warning(const QString& msg) { Log(LogLevel::Warning, msg); }
    void Error(const QString& msg) { Log(LogLevel::Error, msg); }

    using SinkFn = std::function<void(const LogEntry&)>;
    void AddSink(SinkFn sink);
    void ClearSinks();

    std::vector<LogEntry> GetRecentEntries(size_t maxCount = 100) const;

signals:
    void EntryAdded(const LogEntry& entry);

private:
    mutable std::mutex mutex_;
    std::vector<LogEntry> history_;
    std::vector<SinkFn> sinks_;
};

Q_DECLARE_METATYPE(LogEntry)

