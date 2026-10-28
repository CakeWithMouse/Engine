#include "MainWindow.h"
#include "QtHost.h"
#include <Engine/Runtime/EditorReloadApi.h>
#include "CodeClassDialog.h"
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
    if (action != QStringLiteral("AddClass") && host_->PlayState() != EditorQtHost::Edit) { QMessageBox::information(this,"C++",QStringLiteral("Сначала остановите Play.")); return; }
    const auto project = QString::fromUtf8(host_->host().projectFile);
    const auto runtime = QCoreApplication::applicationDirPath();
    const auto script = runtime + QStringLiteral("/SDK/ProjectTools.ps1");
    if (!QFileInfo::exists(script)) {
        QMessageBox::warning(this, QStringLiteral("C++"), QStringLiteral("В этом выпуске нет SDK. Обновите копию проекта на выпуск с поддержкой C++.")); return;
    }
    if (findChild<QProcess*>(QStringLiteral("projectCodeBuild"))) return;
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
                sceneBusy_ = true; setEnabled(false);
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

void EditorMainWindow::BuildCodeTools()
{
    auto* menu = menuBar()->addMenu(QStringLiteral("Проект"))->addMenu(QStringLiteral("C++"));
    menu->addAction(QStringLiteral("Добавить C++ в проект…"), this, [this] { MaybeSave([this]{ RunCodeTool(QStringLiteral("Enable")); }); });
    menu->addAction(QStringLiteral("Добавить C++ класс…"), this, [this] {
        CodeClassDialog dialog(QFileInfo(QString::fromUtf8(host_->host().projectFile)).absolutePath(), this);
        if (dialog.exec() == QDialog::Accepted) RunCodeTool(QStringLiteral("AddClass"), dialog.Arguments());
    });
    menu->addAction(QStringLiteral("Перекомпилировать / Hot Reload"), this, [this] { if (host_->PlayState() != EditorQtHost::Edit) { QMessageBox::information(this,"C++",QStringLiteral("Сначала остановите Play.")); return; } RunCodeTool(QStringLiteral("Build")); }, QKeySequence(QStringLiteral("Ctrl+Alt+R")));
    menu->addAction(QStringLiteral("Типы C++ проекта…"), this, &EditorMainWindow::ShowProjectTypes);
    menu->addAction(QStringLiteral("Данные выбранного объекта…"), this, [this] {
        auto snapshot = host_->ReadSnapshot();
        for (const auto& object : snapshot.objects) if (object.id == snapshot.selected) {
            bool ok = false;
            auto data = QInputDialog::getMultiLineText(this, QStringLiteral("Данные объекта"), QStringLiteral("JSON объявленного кодека. Нужен LoadProjectData или ProjectObjectAdapter::Load."), QString::fromStdString(object.projectData), &ok);
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
