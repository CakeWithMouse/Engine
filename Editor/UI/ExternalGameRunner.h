#pragma once

#include <QObject>
#include <QProcess>
#include <QAction>

class EditorQtHost;

class ExternalGameRunner : public QObject
{
public:
    explicit ExternalGameRunner(EditorQtHost* host, QAction* actExternal, QObject* parent = nullptr);

    void Start();
    void Stop();
    bool IsRunning() const;

private:
    EditorQtHost* host_ = nullptr;
    QAction* actExternal_ = nullptr;
    QProcess* proc_ = nullptr;
};
