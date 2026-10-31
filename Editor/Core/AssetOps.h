#pragma once

#include "FileService.h"
#include <QByteArray>
#include <QString>
#include <QStringList>

namespace EditorCore
{

class AssetOps
{
public:
    explicit AssetOps(FileService* fileService);

    bool CreateFile(const QString& relPath, const QByteArray& content, QString* outError = nullptr);
    bool CreateFolder(const QString& relPath, QString* outError = nullptr);

    bool Rename(const QString& oldRelPath, const QString& newRelPath, QString* outError = nullptr);
    bool Duplicate(const QString& srcRelPath, const QString& dstRelPath = QString(), QString* outError = nullptr, QString* outCreatedPath = nullptr);
    bool Delete(const QString& relPath, QString* outError = nullptr);

    bool Exists(const QString& relPath) const;
    bool IsFolder(const QString& relPath) const;
    QStringList List(const QString& relDir = QString()) const;

    static QString GenerateDuplicateName(const QString& relPath);

private:
    FileService* fileService_ = nullptr;
};

} // namespace EditorCore
