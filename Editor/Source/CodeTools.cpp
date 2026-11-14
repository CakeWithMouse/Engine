#include "MainWindow.h"
#include "QtHost.h"
#include <Engine/Runtime/EditorReloadApi.h>
#include "CodeClassDialog.h"
#include "ImportMigration.h"
#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
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
#include <QProcessEnvironment>
#include <QPushButton>
#include <QSaveFile>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QLabel>

void EditorMainWindow::RunCodeTool(const QString& action, const QStringList& extra)
{
    if (sceneBusy_) return;
    if (action != QStringLiteral("AddClass") && host_->PlayState() != EditorQtHost::Edit) { QMessageBox::information(this,"C++",QStringLiteral("Сначала остановите Play.")); return; }
    const auto project = QString::fromUtf8(host_->host().projectFile);
    const auto runtime = QCoreApplication::applicationDirPath();
    const auto script = runtime + QStringLiteral("/SDK/ProjectTools.ps1");
    if (!QFileInfo::exists(script)) {
        QMessageBox::warning(this, QStringLiteral("C++"), QStringLiteral("В этом выпуске нет SDK. Обновите копию проекта на выпуск с поддержкой C++.")); return;
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
    // Existing sceneBusy_ guards and RefreshSceneUi gate editing while the
    // candidate is built. Do not recursively disable menus via the main window:
    // Qt 6.7 can crash in QMenu::changeEvent / QAction::setEnabled there.
    if (action != QStringLiteral("AddClass")) { sceneBusy_ = true; UpdatePlayButtons(); }
    auto* process = new QProcess(this);
    // Windows PowerShell must discover its own modules. An editor launched from
    // PowerShell 7 can inherit incompatible Utility modules (Get-FileHash fails).
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.remove(QStringLiteral("PSModulePath"));
    process->setProcessEnvironment(environment);
    process->setObjectName(QStringLiteral("projectCodeBuild"));
    process->setProcessChannelMode(QProcess::MergedChannels);
    connect(process, &QProcess::readyReadStandardOutput, this, [this,process] {
        auto pending=process->property("pendingOutput").toByteArray()+process->readAllStandardOutput();
        int end=0;while((end=pending.indexOf('\n'))>=0) { AppendLine(QString::fromUtf8(pending.left(end)));pending.remove(0,end+1); }
        process->setProperty("pendingOutput",pending);
    });
    connect(process, &QProcess::errorOccurred, this, [this,process](QProcess::ProcessError error) {
        AppendLine(process->errorString()); if (error == QProcess::FailedToStart) { sceneBusy_ = false; UpdatePlayButtons(); process->deleteLater(); }
    });
    connect(process, qOverload<int,QProcess::ExitStatus>(&QProcess::finished), this,
        [this,process,action,project,runtime,previousModule](int code, QProcess::ExitStatus state) {
            sceneBusy_ = false; UpdatePlayButtons();
            AppendLine(QString::fromUtf8(process->property("pendingOutput").toByteArray()+process->readAllStandardOutput()));
            process->deleteLater();
            if (code || state != QProcess::NormalExit) {
                AppendLine(QStringLiteral("[Ошибка C++] %1: код завершения %2. Проект: %3").arg(action).arg(code).arg(project));
                consoleDock_->show();consoleDock_->raise();
                QMessageBox::warning(this, QStringLiteral("C++"), QStringLiteral("Операция не завершена. Предыдущая DLL сохранена. Диагностика находится в Console.")); return;
            }
            if (action == QStringLiteral("AddClass")) {
                QMessageBox::information(this, QStringLiteral("C++"), QStringLiteral("Созданы .h и .cpp в Source. Соберите проект, чтобы класс появился в редакторе.")); return;
            }
            if (host_->HasScene() && host_->host().getService && host_->host().getService(host_->host().user, EngineEditorReloadService, 1)) {
                QFile manifest(project);
                if (!manifest.open(QIODevice::ReadOnly)) { QMessageBox::warning(this, "C++", QStringLiteral("Не удалось прочитать путь собранной DLL.")); return; }
                const auto module = QJsonDocument::fromJson(manifest.readAll()).object()["module"].toString();
                if (module.isEmpty()) { QMessageBox::warning(this, "C++", QStringLiteral("В Project.json отсутствует module.")); return; }
                QFile typesBuilt(QFileInfo(project).dir().filePath("ProjectTypes.tsv")); reloadTypesBuilt_.clear();
                if(typesBuilt.open(QIODevice::ReadOnly)) reloadTypesBuilt_=typesBuilt.readAll();
                sceneBusy_ = true; UpdatePlayButtons();
                EditorCommand c; c.type = EditorCommand::ReloadProject; c.name=previousModule; c.path = QFileInfo(project).dir().absoluteFilePath(module);
                host_->PostCommand(c); return;
            }
            QMessageBox::information(this, QStringLiteral("C++"), QStringLiteral("Этот runtime не поддерживает hot reload. Редактор будет открыт заново."));
            MaybeSave([this,project,runtime] {
                StopExternalGame();
                if (QProcess::startDetached(runtime + QStringLiteral("/EngineRuntime.exe"), {QStringLiteral("--project"), project, QStringLiteral("--mode"), QStringLiteral("editor"), QStringLiteral("--scene-api")}, runtime)) close();
                else QMessageBox::warning(this, QStringLiteral("C++"), QStringLiteral("Не удалось запустить редактор. Проект можно открыть из лаунчера."));
            });
        });
    QStringList args{QStringLiteral("-NoProfile"),QStringLiteral("-ExecutionPolicy"),QStringLiteral("Bypass"),QStringLiteral("-File"),script,
        QStringLiteral("-Action"),action,QStringLiteral("-Project"),project,QStringLiteral("-Runtime"),runtime};
    args += extra;
    AppendLine(QStringLiteral("C++: ") + action + (extra.isEmpty() ? QString() : " " + extra.join(' ')));
    process->start(QStringLiteral("powershell.exe"), args);
}

EditorMainWindow::ImportSupport EditorMainWindow::EnsureModelImportSupport()
{
    if (host_->AssetImportApi()) return ImportSupport::Options;
    const auto projectDir = QFileInfo(QString::fromUtf8(host_->host().projectFile)).absolutePath();
    if (ImportMigration::IsCodeProject(projectDir)) {
        QMessageBox box(this);
        box.setWindowTitle(Tr("Совместимость проекта", "Project Compatibility"));
        box.setText(Tr("Game.dll проекта не поддерживает импорт с настройками.",
                       "The project's Game.dll does not support import settings."));
        box.setInformativeText(Tr("Соберите проект текущим SDK или импортируйте с настройками по умолчанию.",
                                  "Build the project with the current SDK, or import with default settings."));
        auto* build = box.addButton(Tr("Собрать проект", "Build Project"), QMessageBox::AcceptRole);
        box.addButton(Tr("Открыть как есть", "Open As Is"), QMessageBox::RejectRole);
        box.setDefaultButton(build);
        box.exec();
        if (box.clickedButton() == build) {
            RunCodeTool(QStringLiteral("Build"));
            AppendLine(Tr("Импорт отменён: повторите его после сборки проекта.",
                          "Import cancelled: repeat it after the project build."));
            return ImportSupport::Cancelled;
        }
        return ImportSupport::Defaults;
    }
    QString previousModule;
    QFile manifest(QDir(projectDir).filePath(QStringLiteral("Project.json")));
    if (manifest.open(QIODevice::ReadOnly))
        previousModule = QJsonDocument::fromJson(manifest.readAll()).object()[QStringLiteral("module")].toString();
    QString error;
    const auto newModule = ImportMigration::MigrateNoCodeModule(projectDir, QCoreApplication::applicationDirPath(), &error);
    if (newModule.isEmpty()) {
        AppendLine(Tr("[Error] Не удалось обновить Game.dll из шаблона: %1",
                      "[Error] Failed to update Game.dll from the template: %1").arg(error));
        return ImportSupport::Defaults;
    }
    AppendLine(Tr("Game.dll проекта обновлена из шаблона; предыдущая версия сохранена в .painengine/GameDllBackups.",
                  "Project Game.dll updated from the template; the previous version is kept in .painengine/GameDllBackups."));
    QMessageBox::information(this, Tr("Совместимость проекта", "Project Compatibility"),
        Tr("Game.dll проекта обновлена из шаблона. Предыдущая версия сохранена в .painengine/GameDllBackups.",
           "Project Game.dll updated from the template. The previous version is kept in .painengine/GameDllBackups."));
    EditorCommand c; c.type = EditorCommand::ReloadProject; c.name = previousModule;
    c.path = QDir(projectDir).filePath(newModule);
    host_->PostCommand(c);
    return ImportSupport::Options;
}

void EditorMainWindow::BuildCodeTools()
{
    projectMenu_ = menuBar()->addMenu(Tr("Проект", "Project"));
    auto* menu = projectMenu_->addMenu(QStringLiteral("C++"));
    menu->addAction(Tr("Добавить C++ в проект…", "Add C++ to Project…"), this, [this] { MaybeSave([this]{ RunCodeTool(QStringLiteral("Enable")); }); });
    menu->addAction(Tr("Добавить C++ класс…", "Add C++ Class…"), this, [this] {
        CodeClassDialog dialog(QFileInfo(QString::fromUtf8(host_->host().projectFile)).absolutePath(), this);
        if (dialog.exec() == QDialog::Accepted) RunCodeTool(QStringLiteral("AddClass"), dialog.Arguments());
    });
    auto* actBuild = menu->addAction(Tr("Перекомпилировать / Hot Reload", "Recompile / Hot Reload"), this, [this] {
        if (host_->PlayState() != EditorQtHost::Edit) {
            QMessageBox::information(this, "C++", Tr("Сначала остановите Play.", "Stop Play mode first."));
            return;
        }
        RunCodeTool(QStringLiteral("Build"));
    });
    actBuild->setShortcut(QKeySequence(QStringLiteral("Ctrl+Alt+R")));
    menu->addAction(Tr("Типы C++ проекта…", "Project C++ Types…"), this, &EditorMainWindow::ShowProjectTypes);
    menu->addAction(Tr("Данные выбранного объекта…", "Selected Object Data…"), this, [this] {
        auto snapshot = host_->ReadSnapshot();
        for (const auto& object : snapshot.objects) if (object.id == snapshot.selected) {
            bool ok = false;
            auto data = QInputDialog::getMultiLineText(this, Tr("Данные объекта", "Object Data"), Tr("JSON объявленного кодека. Нужен LoadProjectData или ProjectObjectAdapter::Load.", "Declared codec JSON. Needs LoadProjectData or ProjectObjectAdapter::Load."), QString::fromStdString(object.projectData), &ok);
            if (ok) { EditorCommand cmd; cmd.type = EditorCommand::SetProjectData; cmd.objectId = object.id; cmd.path = data; host_->PostCommand(cmd); }
            return;
        }
    });
}

void EditorMainWindow::ShowProjectTypes()
{
    QDialog dialog(this); dialog.setWindowTitle(QStringLiteral("Типы C++ проекта")); dialog.resize(820,480);
    auto* layout = new QVBoxLayout(&dialog);
    auto* tree = new QTreeWidget(&dialog);
    tree->setHeaderLabels({QStringLiteral("Тип"),QStringLiteral("Назначение"),QStringLiteral("Родитель"),QStringLiteral("Действие")});
    const auto root = QFileInfo(QString::fromUtf8(host_->host().projectFile)).absolutePath();
    QFile manifest(root + QStringLiteral("/ProjectTypes.tsv"));
    QSet<QString> names;
    if (manifest.open(QIODevice::ReadOnly)) {
        const auto lines = QString::fromUtf8(manifest.readAll()).split('\n');
        for (int i = 1; i < lines.size(); ++i) {
            const auto cells = lines[i].trimmed().split('\t'); if (cells.size() < 3) continue;
            names.insert(cells[1]);
            auto* row = new QTreeWidgetItem(tree, {cells[1],cells[0],cells[2],cells[0] == "Game" ? QStringLiteral("Выбрать для следующего Play") : QStringLiteral("Добавить из панели сцены")});
            row->setData(0,Qt::UserRole,cells[0]);
        }
    }
    tree->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    layout->addWidget(new QLabel(QStringLiteral("Пользовательские типы из последней сборки. Компоненты добавляются в категории «Классы проекта».\nGame задаёт логику следующего запуска Play."), &dialog));
    layout->addWidget(tree);
    auto* choose = new QPushButton(QStringLiteral("Выбрать Game"),&dialog); layout->addWidget(choose);
    connect(choose,&QPushButton::clicked,&dialog,[&,root] {
        auto* row = tree->currentItem(); if (!row || row->data(0,Qt::UserRole).toString() != "Game") return;
        if (host_->PlayState() != EditorQtHost::Edit) { QMessageBox::warning(&dialog,"Game",QStringLiteral("Сначала остановите Play.")); return; }
        QFile project(QString::fromUtf8(host_->host().projectFile));
        if (!project.open(QIODevice::ReadOnly)) return;
        const auto json = QJsonDocument::fromJson(project.readAll()).object();
        QSaveFile file(QDir(root).filePath(json.value("content").toString()+"/ProjectGame.txt"));
        if (!file.open(QIODevice::WriteOnly) || file.write(row->text(0).toUtf8()+"\n") < 0 || !file.commit())
            QMessageBox::warning(&dialog,"Game",QStringLiteral("Не удалось сохранить выбор."));
        else dialog.accept();
    });
    dialog.exec();
}
