#pragma once

#include <QByteArray>
#include <QString>

namespace EditorCore
{
// Единая точка дискового ввода-вывода графовых редакторов (Script/AiMachine/Material).
// Оборачивает FileService: песочница проекта, атомарная запись, лог каждой операции.
// Заменяет прямые QFile/QSaveFile в окнах, поведение для путей внутри проекта сохраняется.
namespace GraphFileIo
{
    bool Exists(const QString& projectRoot, const QString& contentRoot, const QString& path);
    bool Read(const QString& projectRoot, const QString& contentRoot, const QString& path,
              QByteArray& outData, QString* outError = nullptr);
    bool Write(const QString& projectRoot, const QString& contentRoot, const QString& path,
               const QByteArray& data, QString* outError = nullptr);
    bool Delete(const QString& projectRoot, const QString& contentRoot, const QString& path,
                QString* outError = nullptr);
}
}