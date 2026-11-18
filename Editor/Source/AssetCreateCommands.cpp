#include "MainWindow.h"
#include "CommandRegistry.h"
#include "FileService.h"
#include "Logger.h"
#include "EditorLocale.h"

#include <QDir>
#include <QFileInfo>
#include <QInputDialog>
#include <QMessageBox>
#include <QRegularExpression>
#include <QVariantMap>

void EditorMainWindow::RegisterAssetCreateCommands()
{
    auto& registry = CommandRegistry::Instance();

    registry.RegisterCommand({
        QStringLiteral("Assets.CreateMaterial"),
        QStringLiteral("Create Material"),
        QStringLiteral("Create a new material in a project folder"),
        QString(),
        [this](const CommandContext& ctx) {
            const QString directory = ctx.parameters.value(QStringLiteral("directory")).toString();
            return !CreateMaterial(directory).isEmpty();
        },
        [this]() { return host_ && host_->HasMaterials(); }
    });

    registry.RegisterCommand({
        QStringLiteral("Assets.CreateScript"),
        QStringLiteral("Create Script"),
        QStringLiteral("Create a new visual script in a project folder"),
        QString(),
        [this](const CommandContext& ctx) {
            const QString directory = ctx.parameters.value(QStringLiteral("directory")).toString();
            return !CreateScript(directory).isEmpty();
        },
        [this]() { return host_ && host_->ReadSnapshot().scripts; }
    });

    registry.RegisterCommand({
        QStringLiteral("Assets.CreateAiMachine"),
        QStringLiteral("Create AI State Machine"),
        QStringLiteral("Create a new AI state machine in a project folder"),
        QString(),
        [this](const CommandContext& ctx) {
            const QString directory = ctx.parameters.value(QStringLiteral("directory")).toString();
            return !CreateAiMachine(directory).isEmpty();
        },
        nullptr
    });

    registry.RegisterCommand({
        QStringLiteral("Assets.CreateCppClass"),
        QStringLiteral("Create C++ Class"),
        QStringLiteral("Create a new C++ class (.h/.cpp) in a project folder"),
        QString(),
        [this](const CommandContext& ctx) {
            const QString directory = ctx.parameters.value(QStringLiteral("directory")).toString();
            const QString name = ctx.parameters.value(QStringLiteral("name")).toString();
            return CreateCppClass(directory, name);
        },
        nullptr
    });
}

void EditorMainWindow::UnregisterAssetCreateCommands()
{
    CommandRegistry::Instance().UnregisterCommand(QStringLiteral("Assets.CreateMaterial"));
    CommandRegistry::Instance().UnregisterCommand(QStringLiteral("Assets.CreateScript"));
    CommandRegistry::Instance().UnregisterCommand(QStringLiteral("Assets.CreateAiMachine"));
    CommandRegistry::Instance().UnregisterCommand(QStringLiteral("Assets.CreateCppClass"));
}

bool EditorMainWindow::CreateCppClass(const QString& directory, const QString& name)
{
    const QString projectRoot = QFileInfo(ProjectPath()).dir().absolutePath();
    const QString contentRoot = host_ ? QString::fromUtf8(host_->host().contentRoot) : QString();

    QString target = directory;
    if (target.isEmpty())
    {
        target = QDir(projectRoot).filePath(QStringLiteral("Source"));
        if (!QDir(target).exists())
            target = contentRoot;
    }

    QString base = name.trimmed();
    if (base.isEmpty())
    {
        bool accepted = false;
        base = QInputDialog::getText(this, Tr("Новый C++ класс", "New C++ Class"),
                                     Tr("Имя класса:", "Class Name:"),
                                     QLineEdit::Normal, QStringLiteral("MyClass"), &accepted).trimmed();
        if (!accepted || base.isEmpty())
            return false;
    }

    static const QRegularExpression identPattern(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*$"));
    if (!identPattern.match(base).hasMatch())
    {
        const QString err = Tr("Имя класса должно быть корректным идентификатором C++.",
                               "Class name must be a valid C++ identifier.");
        AppendLine(err);
        QMessageBox::warning(this, Tr("Новый C++ класс", "New C++ Class"), err);
        return false;
    }

    FileService fs(&Logger::Instance(), this);
    fs.SetProjectRoot(projectRoot);
    fs.SetContentRoot(contentRoot);

    if (!fs.DirectoryExists(target))
    {
        QString createErr;
        if (!fs.CreateDirectoryPath(target, &createErr))
        {
            const QString err = Tr("Не удалось создать каталог: %1", "Failed to create directory: %1").arg(createErr);
            AppendLine(err);
            QMessageBox::warning(this, Tr("Новый C++ класс", "New C++ Class"), err);
            return false;
        }
    }

    const QString headerPath = QDir(target).filePath(base + QStringLiteral(".h"));
    const QString sourcePath = QDir(target).filePath(base + QStringLiteral(".cpp"));

    if (fs.FileExists(headerPath) || fs.FileExists(sourcePath))
    {
        const QString err = Tr("В каталоге уже есть %1.h или %1.cpp.",
                               "Directory already contains %1.h or %1.cpp.").arg(base);
        AppendLine(err);
        QMessageBox::warning(this, Tr("Новый C++ класс", "New C++ Class"), err);
        return false;
    }

    const QString headerContent = QStringLiteral(
        "#pragma once\n\n"
        "class %1\n"
        "{\n"
        "public:\n"
        "    %1();\n"
        "    virtual ~%1() = default;\n"
        "};\n").arg(base);

    const QString sourceContent = QStringLiteral(
        "#include \"%1.h\"\n\n"
        "%1::%1()\n"
        "{\n"
        "}\n").arg(base);

    QString writeErr;
    if (!fs.WriteFileAtomic(headerPath, headerContent.toUtf8(), &writeErr) ||
        !fs.WriteFileAtomic(sourcePath, sourceContent.toUtf8(), &writeErr))
    {
        const QString err = Tr("Не удалось записать файлы C++ класса: %1",
                               "Failed to write C++ class files: %1").arg(writeErr);
        AppendLine(err);
        QMessageBox::warning(this, Tr("Новый C++ класс", "New C++ Class"), err);
        return false;
    }

    AppendLine(Tr("Создан C++ класс %1 (%2, %3)",
                  "Created C++ class %1 (%2, %3)")
               .arg(base, QFileInfo(headerPath).fileName(), QFileInfo(sourcePath).fileName()));
    return true;
}
