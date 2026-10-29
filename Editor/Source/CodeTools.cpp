#include "MainWindow.h"
#include "QtHost.h"
#include <Engine/Runtime/EditorReloadApi.h>
#include "CodeClassDialog.h"
#include "EditorLocale.h"
#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QInputDialog>
#include <QLineEdit>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QMenuBar>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSaveFile>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QLabel>

void EditorMainWindow::RunCodeTool(const QString& action, const QStringList& extra)
{
    if (action != QStringLiteral("AddClass") && host_->PlayState() != EditorQtHost::Edit) {
        QMessageBox::information(this, QStringLiteral("C++"), Tr("Сначала остановите Play.", "Stop Play mode first."));
        return;
    }
    const auto project = QString::fromUtf8(host_->host().projectFile);
    const auto runtime = QCoreApplication::applicationDirPath();
    const auto script = runtime + QStringLiteral("/SDK/ProjectTools.ps1");
    if (!QFileInfo::exists(script)) {
        QMessageBox::warning(this, QStringLiteral("C++"),
                             Tr("В этом выпуске нет SDK. Обновите копию проекта на выпуск с поддержкой C++.",
                                "No SDK found in this release. Update project to a release with C++ support."));
        return;
    }
    if (findChild<QProcess*>(QStringLiteral("projectCodeBuild"))) return;
    // Drain preview teardown before the new DLL is activated; honor unsaved material edits.
    if (action != QStringLiteral("AddClass") && !CloseMaterialEditors([this, action, extra] { RunCodeTool(action, extra); })) return;
    QFile before(project); QString previousModule;
    if (before.open(QIODevice::ReadOnly)) previousModule=QJsonDocument::fromJson(before.readAll()).object()["module"].toString();
    QFile typesBefore(QFileInfo(project).dir().filePath("ProjectTypes.tsv"));
    reloadTypesExisted_=typesBefore.exists(); reloadTypesBefore_.clear();
    if (typesBefore.open(QIODevice::ReadOnly)) reloadTypesBefore_=typesBefore.readAll();
    // The candidate validates saved scenes. Keep them unchanged until activation.
    if (action != QStringLiteral("AddClass")) { sceneBusy_ = true; setEnabled(false); }
    auto* process = new QProcess(this);
    process->setObjectName(QStringLiteral("projectCodeBuild"));
    process->setProcessChannelMode(QProcess::MergedChannels);
    connect(process, &QProcess::readyReadStandardOutput, this, [this,process] { AppendLine(QString::fromLocal8Bit(process->readAllStandardOutput())); });
    connect(process, &QProcess::errorOccurred, this, [this,process](QProcess::ProcessError error) {
        AppendLine(process->errorString()); if (error == QProcess::FailedToStart) { sceneBusy_ = false; setEnabled(true); process->deleteLater(); }
    });
    connect(process, qOverload<int,QProcess::ExitStatus>(&QProcess::finished), this,
        [this,process,action,project,runtime,previousModule](int code, QProcess::ExitStatus state) {
            sceneBusy_ = false; setEnabled(true);
            process->deleteLater();
            if (code || state != QProcess::NormalExit) {
                QMessageBox::warning(this, QStringLiteral("C++"),
                                     Tr("Операция не завершена. Предыдущая DLL сохранена. Диагностика находится в Console.",
                                        "Operation failed. Previous DLL preserved. See diagnostics in Console."));
                return;
            }
            if (action == QStringLiteral("AddClass")) return;
            QFile typesAfter(QFileInfo(project).dir().filePath("ProjectTypes.tsv"));
            reloadTypesBuilt_.clear();
            if (typesAfter.open(QIODevice::ReadOnly)) reloadTypesBuilt_=typesAfter.readAll();
            // Candidate: same project file, temporarily swap "module" to CandidateGame.dll.
            QFile manifest(project);
            if (!manifest.open(QIODevice::ReadOnly)) { AppendLine(Tr("C++: не удалось прочитать манифест проекта", "C++: failed to read project manifest")); return; }
            auto doc = QJsonDocument::fromJson(manifest.readAll()).object();
            manifest.close();
            const auto module = doc.value(QStringLiteral("module")).toString();
            // Hot reload protocol: write manifest with candidate, request reload, on success keep it, on failure revert.
            EditorCommand c; c.type = EditorCommand::ReloadProject; c.name = previousModule; c.path = QFileInfo(project).dir().absoluteFilePath(module);
            host_->PostCommand(c);
            AppendLine(Tr("C++: проект собран успешно, выполняется Hot Reload…", "C++: project built successfully, reloading module…"));
        });
    QStringList args{QStringLiteral("-NoProfile"),QStringLiteral("-ExecutionPolicy"),QStringLiteral("Bypass"),QStringLiteral("-File"),script,
        QStringLiteral("-Action"),action,QStringLiteral("-Project"),project,QStringLiteral("-Runtime"),runtime};
    args += extra;
    AppendLine(QStringLiteral("C++: ") + action + (extra.isEmpty() ? QString() : " " + extra.join(' ')));
    process->start(QStringLiteral("powershell.exe"), args);
}

void EditorMainWindow::BuildCodeTools()
{
    projectMenu_ = menuBar()->addMenu(Tr("Проект", "Project"));
    cppMenu_ = projectMenu_->addMenu(QStringLiteral("C++"));
    cppMenu_->addAction(Tr("Добавить C++ в проект…", "Add C++ to Project…"), this, [this] { MaybeSave([this]{ RunCodeTool(QStringLiteral("Enable")); }); });
    cppMenu_->addAction(Tr("Добавить C++ класс…", "Add C++ Class…"), this, [this] {
        CodeClassDialog dialog(QFileInfo(QString::fromUtf8(host_->host().projectFile)).absolutePath(), this);
        if (dialog.exec() == QDialog::Accepted) RunCodeTool(QStringLiteral("AddClass"), dialog.Arguments());
    });
    cppMenu_->addAction(Tr("Перекомпилировать / Hot Reload", "Rebuild / Hot Reload"), QKeySequence(QStringLiteral("Ctrl+Alt+R")), this, [this] {
        if (host_->PlayState() != EditorQtHost::Edit) {
            QMessageBox::information(this, QStringLiteral("C++"), Tr("Сначала остановите Play.", "Stop Play mode first."));
            return;
        }
        RunCodeTool(QStringLiteral("Build"));
    });
    cppMenu_->addAction(Tr("Типы C++ проекта…", "Project C++ Types…"), this, &EditorMainWindow::ShowProjectTypes);
    cppMenu_->addAction(Tr("Данные выбранного объекта…", "Selected Object Data…"), this, [this] {
        auto snapshot = host_->ReadSnapshot();
        for (const auto& object : snapshot.objects) if (object.id == snapshot.selected) {
            bool ok = false;
            auto data = QInputDialog::getMultiLineText(this, Tr("Данные объекта", "Object Data"),
                Tr("JSON объявленного кодека. Нужен LoadProjectData или ProjectObjectAdapter::Load.",
                   "JSON of registered codec. Requires LoadProjectData or ProjectObjectAdapter::Load."),
                QString::fromStdString(object.projectData), &ok);
            if (ok) { EditorCommand cmd; cmd.type = EditorCommand::SetProjectData; cmd.objectId = object.id; cmd.path = data; host_->PostCommand(cmd); }
            return;
        }
    });
}

void EditorMainWindow::ShowProjectTypes()
{
    QDialog dialog(this);
    dialog.setWindowTitle(Tr("Типы C++ проекта", "Project C++ Types"));
    dialog.resize(820, 480);
    auto* layout = new QVBoxLayout(&dialog);
    auto* tree = new QTreeWidget(&dialog);
    tree->setHeaderLabels({Tr("Тип", "Type"), Tr("Назначение", "Kind"), Tr("Родитель", "Parent"), Tr("Действие", "Action")});
    const auto root = QFileInfo(QString::fromUtf8(host_->host().projectFile)).absolutePath();
    QFile manifest(root + QStringLiteral("/ProjectTypes.tsv"));
    QSet<QString> names;
    if (manifest.open(QIODevice::ReadOnly)) {
        const auto lines = QString::fromUtf8(manifest.readAll()).split('\n');
        for (int i = 1; i < lines.size(); ++i) {
            const auto cells = lines[i].trimmed().split('\t'); if (cells.size() < 3) continue;
            names.insert(cells[1]);
            auto* row = new QTreeWidgetItem(tree, {cells[1], cells[0], cells[2],
                cells[0] == "Game" ? Tr("Выбрать для следующего Play", "Select for next Play")
                                   : Tr("Добавить из панели сцены", "Add from scene panel")});
            row->setData(0, Qt::UserRole, cells[0]);
        }
    }
    tree->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    layout->addWidget(new QLabel(Tr("Пользовательские типы из последней сборки. Компоненты добавляются в категории «Классы проекта».\nGame задаёт логику следующего запуска Play.",
                                    "Custom types from the latest build. Components are added under 'Project Classes'.\nGame specifies the logic for the next Play session."), &dialog));
    layout->addWidget(tree);
    auto* choose = new QPushButton(Tr("Выбрать Game", "Select Game"), &dialog);
    layout->addWidget(choose);
    connect(choose, &QPushButton::clicked, &dialog, [&, root] {
        auto* row = tree->currentItem(); if (!row || row->data(0, Qt::UserRole).toString() != "Game") return;
        if (host_->PlayState() != EditorQtHost::Edit) {
            QMessageBox::warning(&dialog, QStringLiteral("Game"), Tr("Сначала остановите Play.", "Stop Play mode first."));
            return;
        }
        QFile project(QString::fromUtf8(host_->host().projectFile));
        if (!project.open(QIODevice::ReadOnly)) return;
        const auto json = QJsonDocument::fromJson(project.readAll()).object();
        QSaveFile file(QDir(root).filePath(json.value("content").toString() + "/ProjectGame.txt"));
        if (!file.open(QIODevice::WriteOnly) || file.write(row->text(0).toUtf8() + "\n") < 0 || !file.commit())
            QMessageBox::warning(&dialog, QStringLiteral("Game"), Tr("Не удалось сохранить выбор.", "Failed to save selection."));
        else dialog.accept();
    });
    dialog.exec();
}
