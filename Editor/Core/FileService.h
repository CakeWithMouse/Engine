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

    void SetContentRoot(const QString& contentRoot);
    QString GetContentRoot() const;

    static bool IsProtectedDirectory(const QString& path, const QString& projectRoot, const QString& contentRoot = QString());
    bool IsProtectedDirectory(const QString& path) const;

    bool IsInSandbox(const QString& path) const;
    QString ResolveSandboxPath(const QString& relativeOrAbsolutePath) const;

    bool ReadFile(const QString& path, QByteArray& outData, QString* outError = nullptr) const;
    bool WriteFileAtomic(const QString& path, const QByteArray& data, QString* outError = nullptr);
    bool DeleteFile(const QString& path, QString* outError = nullptr);
    bool DeleteDirectory(const QString& path, QString* outError = nullptr);
    bool FileExists(const QString& path) const;
    bool DirectoryExists(const QString& path) const;
    bool CreateDirectoryPath(const QString& path, QString* outError = nullptr);

    QStringList ListFiles(const QString& subDir = QString(), const QStringList& nameFilters = {}) const;

private:
    void LogInfo(const QString& msg) const;
    void LogError(const QString& msg) const;

    QString projectRoot_;
    QString contentRoot_;
    Logger* logger_ = nullptr;
};

