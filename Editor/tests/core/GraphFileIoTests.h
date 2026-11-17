#pragma once

#include "../../Core/Graph/GraphFileIo.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <cstdio>
#include <cstdlib>

namespace
{

int gGraphFileIoChecks = 0;

void CheckGraphFileIo(bool cond, const char* msg)
{
    ++gGraphFileIoChecks;
    if (!cond)
    {
        std::fprintf(stderr, "FAIL (GraphFileIo): %s\n", msg);
        std::exit(1);
    }
}

inline int RunGraphFileIoTests()
{
    using namespace EditorCore;

    QTemporaryDir tempDir;
    CheckGraphFileIo(tempDir.isValid(), "Temporary directory created for GraphFileIo tests");

    const QString root = tempDir.path();
    const QString content = QDir(root).filePath(QStringLiteral("Content"));
    const QString path = QDir(content).filePath(QStringLiteral("Graphs/test.graph.json"));
    const QByteArray payload = QByteArrayLiteral("{\"format\":\"ITMOEngine.Graph\",\"version\":1}");

    // 1. До записи файла нет.
    CheckGraphFileIo(!GraphFileIo::Exists(root, content, path), "Exists false before write");

    // 2. Атомарная запись создаёт родительские каталоги и файл.
    QString error;
    CheckGraphFileIo(GraphFileIo::Write(root, content, path, payload, &error), "Write succeeds inside sandbox");
    CheckGraphFileIo(error.isEmpty(), "Write reports no error on success");
    CheckGraphFileIo(GraphFileIo::Exists(root, content, path), "Exists true after write");
    CheckGraphFileIo(QFile::exists(path), "File physically created at path");

    // 3. Чтение возвращает те же байты.
    QByteArray readBack;
    CheckGraphFileIo(GraphFileIo::Read(root, content, path, readBack, &error), "Read succeeds inside sandbox");
    CheckGraphFileIo(readBack == payload, "Read returns identical bytes");

    // 4. Перезапись заменяет содержимое.
    const QByteArray updated = QByteArrayLiteral("{\"version\":2}");
    CheckGraphFileIo(GraphFileIo::Write(root, content, path, updated, &error), "Rewrite succeeds");
    CheckGraphFileIo(GraphFileIo::Read(root, content, path, readBack, &error), "Read after rewrite succeeds");
    CheckGraphFileIo(readBack == updated, "Rewrite replaced content");

    // 5. Удаление убирает файл.
    CheckGraphFileIo(GraphFileIo::Delete(root, content, path, &error), "Delete succeeds inside sandbox");
    CheckGraphFileIo(!GraphFileIo::Exists(root, content, path), "Exists false after delete");
    CheckGraphFileIo(GraphFileIo::Delete(root, content, path, &error), "Delete of missing file is idempotent");

    // 6. Песочница: путь вне корня проекта отклоняется, файл не создаётся.
    const QString outside = QDir::cleanPath(QDir(root).filePath(QStringLiteral("../graphfileio_outside.json")));
    CheckGraphFileIo(!GraphFileIo::Write(root, content, outside, payload, &error), "Write outside sandbox refused");
    CheckGraphFileIo(!error.isEmpty(), "Write outside sandbox reports error");
    CheckGraphFileIo(!QFile::exists(outside), "No file created outside sandbox");
    QByteArray ignored;
    CheckGraphFileIo(!GraphFileIo::Read(root, content, outside, ignored, &error), "Read outside sandbox refused");
    CheckGraphFileIo(!GraphFileIo::Exists(root, content, outside), "Exists false outside sandbox");
    CheckGraphFileIo(!GraphFileIo::Delete(root, content, outside, &error), "Delete outside sandbox refused");

    return gGraphFileIoChecks;
}

} // namespace