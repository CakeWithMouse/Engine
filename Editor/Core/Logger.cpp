#include "Logger.h"

#include <QDateTime>
#include <cstdio>

Logger::Logger(QObject* parent)
    : QObject(parent)
{
}

Logger& Logger::Instance()
{
    static Logger sInstance;
    return sInstance;
}

void Logger::Log(LogLevel level, const QString& message)
{
    LogEntry entry;
    entry.level = level;
    entry.message = message;
    entry.timestampMs = QDateTime::currentMSecsSinceEpoch();

    std::vector<SinkFn> sinksCopy;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        history_.push_back(entry);
        if (history_.size() > 5000)
        {
            history_.erase(history_.begin(), history_.begin() + 1000);
        }
        sinksCopy = sinks_;
    }

    // Default console output
    const char* tag = "[Info]";
    switch (level)
    {
    case LogLevel::Trace:   tag = "[Trace]"; break;
    case LogLevel::Debug:   tag = "[Debug]"; break;
    case LogLevel::Info:    tag = "[Info]"; break;
    case LogLevel::Warning: tag = "[Warning]"; break;
    case LogLevel::Error:   tag = "[Error]"; break;
    }

    const QByteArray utf8 = message.toUtf8();
    if (level == LogLevel::Error)
    {
        std::fprintf(stderr, "%s %s\n", tag, utf8.constData());
        std::fflush(stderr);
    }
    else
    {
        std::printf("%s %s\n", tag, utf8.constData());
        std::fflush(stdout);
    }

    for (const auto& sink : sinksCopy)
    {
        if (sink)
        {
            sink(entry);
        }
    }

    emit EntryAdded(entry);
}

void Logger::AddSink(SinkFn sink)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (sink)
    {
        sinks_.push_back(std::move(sink));
    }
}

void Logger::ClearSinks()
{
    std::lock_guard<std::mutex> lock(mutex_);
    sinks_.clear();
}

std::vector<LogEntry> Logger::GetRecentEntries(size_t maxCount) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (history_.size() <= maxCount)
    {
        return history_;
    }
    return std::vector<LogEntry>(history_.end() - maxCount, history_.end());
}

