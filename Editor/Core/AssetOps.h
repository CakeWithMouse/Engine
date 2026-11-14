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
    bool CreateFolder(const QString& relPath, QString* outError = nullptr, bool exclusive = false);

    bool Rename(const QString& oldRelPath, const QString& newRelPath, QString* outError = nullptr, bool renameCompanions = false);
    bool Duplicate(const QString& srcRelPath, const QString& dstRelPath = QString(), QString* outError = nullptr, QString* outCreatedPath = nullptr, bool duplicateCompanions = false);
    bool Delete(const QString& relPath, QString* outError = nullptr, bool deleteCompanions = false);

    bool Exists(const QString& relPath) const;
    bool IsFolder(const QString& relPath) const;
    QStringList List(const QString& relDir = QString()) const;

    static QString GenerateDuplicateName(const QString& relPath);
    static const QStringList& CompanionSuffixes();

private:
    FileService* fileService_ = nullptr;
};

} // namespace EditorCore
