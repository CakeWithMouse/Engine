#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringList>

class Logger;

class FileService : public QObject
{
    Q_OBJECT
public:
    explicit FileService(Logger* logger = nullptr, QObject* parent = nullptr);
    ~FileService() override = default;

    void SetProjectRoot(const QString& projectRoot);
    QString GetProjectRoot() const { return projectRoot_; }

    bool IsInSandbox(const QString& path) const;
    QString ResolveSandboxPath(const QString& relativeOrAbsolutePath) const;

    bool ReadFile(const QString& path, QByteArray& outData, QString* outError = nullptr) const;
    bool WriteFileAtomic(const QString& path, const QByteArray& data, QString* outError = nullptr);
    bool DeleteFile(const QString& path, QString* outError = nullptr);
    bool DirectoryExists(const QString& path) const;
    bool CreateDirectoryPath(const QString& path, QString* outError = nullptr);

    QStringList ListFiles(const QString& subDir = QString(), const QStringList& nameFilters = {}) const;

private:
    void LogInfo(const QString& msg) const;
    void LogError(const QString& msg) const;

    QString projectRoot_;
    Logger* logger_ = nullptr;
};

