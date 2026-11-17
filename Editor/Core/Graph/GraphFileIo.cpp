#include "GraphFileIo.h"
#include "../FileService.h"

namespace EditorCore
{
namespace
{
    struct ScopedService
    {
        FileService fs;
        ScopedService(const QString& projectRoot, const QString& contentRoot)
        {
            fs.SetProjectRoot(projectRoot);
            fs.SetContentRoot(contentRoot);
        }
    };
}

namespace GraphFileIo
{
    bool Exists(const QString& projectRoot, const QString& contentRoot, const QString& path)
    {
        return ScopedService(projectRoot, contentRoot).fs.FileExists(path);
    }

    bool Read(const QString& projectRoot, const QString& contentRoot, const QString& path,
              QByteArray& outData, QString* outError)
    {
        return ScopedService(projectRoot, contentRoot).fs.ReadFile(path, outData, outError);
    }

    bool Write(const QString& projectRoot, const QString& contentRoot, const QString& path,
               const QByteArray& data, QString* outError)
    {
        return ScopedService(projectRoot, contentRoot).fs.WriteFileAtomic(path, data, outError);
    }

    bool Delete(const QString& projectRoot, const QString& contentRoot, const QString& path,
                QString* outError)
    {
        return ScopedService(projectRoot, contentRoot).fs.DeleteFile(path, outError);
    }
}
}