#include "ExternalGameRunner.h"
#include "QtHost.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

ExternalGameRunner::ExternalGameRunner(EditorQtHost* host, QAction* actExternal, QObject* parent)
    : QObject(parent), host_(host), actExternal_(actExternal)
{
}

bool ExternalGameRunner::IsRunning() const
{
    return proc_ && proc_->state() != QProcess::NotRunning;
}

void ExternalGameRunner::Start()
{
    if (IsRunning()) return;
    if (!proc_) {
        proc_ = new QProcess(this);
        proc_->setProcessChannelMode(QProcess::MergedChannels);
        connect(proc_, &QProcess::readyReadStandardOutput, this, [this] {
            if (host_) host_->Log(proc_->readAllStandardOutput().trimmed().constData());
        });
        connect(proc_, &QProcess::finished, this, [this](int code, QProcess::ExitStatus st) {
            const QByteArray tail = proc_->readAllStandardOutput();
            if (!tail.isEmpty() && host_) host_->Log(tail.trimmed().constData());
            if (host_) host_->Log(("ExternalGame exited code=" + std::to_string(code)
                + (st == QProcess::CrashExit ? " (crash)" : "")).c_str());
            if (actExternal_) actExternal_->setText(QStringLiteral("Запустить отдельным окном"));
        });
        connect(proc_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
            if (host_) host_->Log(("ExternalGame error=" + std::to_string(e)).c_str());
            if (actExternal_) actExternal_->setText(QStringLiteral("Запустить отдельным окном"));
        });
    }

    const QString runtimeDir = QCoreApplication::applicationDirPath();
    const QString runtime = QDir(runtimeDir).absoluteFilePath(QStringLiteral("EngineRuntime.exe"));
    const QString project = QString::fromUtf8(host_ && host_->host().projectFile ? host_->host().projectFile : "");

    if (!QFileInfo::exists(runtime)) {
        if (host_) host_->Log(("ExternalGame: runtime not found: " + runtime.toStdString()).c_str());
        return;
    }
    if (project.isEmpty()) {
        if (host_) host_->Log("ExternalGame: empty projectFile in host");
        return;
    }

    proc_->setProgram(runtime);
    proc_->setArguments({QStringLiteral("--project"), project, QStringLiteral("--mode"), QStringLiteral("game")});
    proc_->setWorkingDirectory(runtimeDir);

    if (host_) host_->Log(("ExternalGame start: " + runtime.toStdString() + " --project " + project.toStdString() + " --mode game").c_str());

    proc_->start();
    if (!proc_->waitForStarted(2000)) {
        if (host_) host_->Log("ExternalGame: failed to start (check EngineRuntime.exe path)");
    } else {
        if (actExternal_) actExternal_->setText(QStringLiteral("Остановить отдельную игру"));
    }
}

void ExternalGameRunner::Stop()
{
    if (IsRunning()) proc_->kill();
}
