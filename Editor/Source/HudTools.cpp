#include "EditorLocale.h"
#include "CommandRegistry.h"
#include "AssetPickerDialog.h"
#include <QMenuBar>
#define NOMINMAX
#include "MainWindow.h"
#include "QtHost.h"
#include "UIEditorWindow.h"
#include <Engine/UI/ProjectHud.h>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QFileDialog>
#include <QDialog>
#include <QFormLayout>
#include <QComboBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QLabel>
#include <QJsonArray>
#include <QJsonDocument>

void EditorMainWindow::OpenUIEditor(const QString& path) {
    const auto key=QFileInfo(path).absoluteFilePath();
    if(uiEditors_.value(key)){uiEditors_[key]->show();uiEditors_[key]->raise();uiEditors_[key]->activateWindow();return;}
    auto* window=new UIEditorWindow(host_,key,this);window->setWindowFlag(Qt::Window);uiEditors_[key]=window;window->show();
}
QString EditorMainWindow::CreateUI(const QString& supplied) {
    const auto root=QString::fromUtf8(host_->host().contentRoot);QDir(root).mkpath("UI");
    QString file=supplied.isEmpty()?QFileDialog::getSaveFileName(this,Tr("Новый интерфейс", "New Interface"),QDir(root).filePath("UI/Hud.ui"),"UI (*.ui)"):supplied;
    if(file.isEmpty())return {};if(!file.endsWith(".ui"))file+=".ui";
    try { ProjectHud::Resolve(std::filesystem::u8path(root.toStdString()),QDir(root).relativeFilePath(file).toUtf8().toStdString()); }
    catch(const std::exception& e){QMessageBox::warning(this,QStringLiteral("HUD"),QString::fromUtf8(e.what()));return {};}
    if(QFileInfo::exists(file)){QMessageBox::warning(this,QStringLiteral("HUD"),Tr("Файл уже существует. Откройте его для редактирования.", "File already exists. Open it for editing."));return {};}
    QSaveFile output(file);const QByteArray data("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<UI/>\n");
    if(!output.open(QIODevice::WriteOnly)||output.write(data)!=data.size()||!output.commit()){QMessageBox::warning(this,QStringLiteral("HUD"),output.errorString());return {};}
    OpenUIEditor(file);return file;
}
void EditorMainWindow::ShowHudSettings() {
    const auto root=QString::fromUtf8(host_->host().contentRoot);
    QDialog dialog(this);dialog.setObjectName("HudSettingsDialog");dialog.setWindowTitle(Tr("HUD игры", "Game HUD"));dialog.resize(560,220);
    auto* form=new QFormLayout(&dialog);auto* choice=new QComboBox(&dialog);choice->setObjectName("ProjectHudChoice");
    QFile current(QDir(root).filePath("ProjectHud.txt"));QString selected;if(current.open(QIODevice::ReadOnly))selected=QString::fromUtf8(current.readLine()).trimmed();
    // Windows cannot atomically replace the settings while this read handle is open.
    current.close();
    auto populate=[&](const QString& chosen){choice->clear();choice->addItem(Tr("Нет", "None"),QString());QDirIterator it(root,{"*.ui"},QDir::Files,QDirIterator::Subdirectories);while(it.hasNext()){const auto path=QDir(root).relativeFilePath(it.next());choice->addItem(path,path);}if(!chosen.isEmpty()&&choice->findData(chosen)<0)choice->addItem(chosen+Tr(" — файл отсутствует", " — file missing"),chosen);choice->setCurrentIndex(std::max(0,choice->findData(chosen)));};
    populate(selected);form->addRow(QStringLiteral("HUD"),choice);
    auto* create=new QPushButton(Tr("Создать…", "Create…"),&dialog);auto* edit=new QPushButton(Tr("Открыть в редакторе UI", "Open in UI Editor"),&dialog);form->addRow(create,edit);
    connect(create,&QPushButton::clicked,&dialog,[&]{const auto file=CreateUI();if(!file.isEmpty())populate(QDir(root).relativeFilePath(file));});
    connect(edit,&QPushButton::clicked,&dialog,[&]{if(!choice->currentData().toString().isEmpty())OpenUIEditor(QDir(root).filePath(choice->currentData().toString()));});
    auto* hint=new QLabel(Tr("HUD создаётся при следующем запуске игры.\nИзменения сохранённой разметки видны в работающем Play.", "HUD is created upon next game launch.\nSaved markup changes are visible in running Play mode."),&dialog);hint->setWordWrap(true);form->addRow(hint);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);form->addRow(buttons);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]{
        const auto chosen=choice->currentData().toString();
        try { const auto file=ProjectHud::Resolve(std::filesystem::u8path(root.toStdString()),chosen.toUtf8().toStdString());if(!file.empty()&&!std::filesystem::is_regular_file(file))throw std::runtime_error("HUD file is missing"); }
        catch(const std::exception& e){QMessageBox::warning(&dialog,QStringLiteral("HUD"),QString::fromUtf8(e.what()));return;}
        QSaveFile output(QDir(root).filePath("ProjectHud.txt"));const auto data=chosen.toUtf8()+"\n";
        if(!output.open(QIODevice::WriteOnly)||output.write(data)!=data.size()||!output.commit()){QMessageBox::warning(&dialog,QStringLiteral("HUD"),output.errorString());return;}
        dialog.accept();
    });
    dialog.exec();
}

void EditorMainWindow::BuildToolsMenu()
{
    CommandRegistry::Instance().RegisterCommand({
        QStringLiteral("UI.Create"),
        QStringLiteral("Create UI"),
        QString(),
        QStringLiteral("Create a new game UI asset"),
        [this](const CommandContext&) {
            CreateUI();
            return true;
        },
        nullptr
    });
    CommandRegistry::Instance().RegisterCommand({
        QStringLiteral("UI.OpenEditor"),
        QStringLiteral("Open UI Editor"),
        QString(),
        QStringLiteral("Open an existing UI file in UI Editor"),
        [this](const CommandContext& ctx) {
            QString file = ctx.parameters.value(QStringLiteral("file")).toString();
            if (file.isEmpty()) {
                AssetPickerDialog picker(EditorCore::AssetKind::UI, QString::fromUtf8(host_->host().contentRoot), QString(), this);
                if (picker.exec() != QDialog::Accepted) return false;
                file = picker.SelectedDiskPath();
            }
            if (!file.isEmpty()) OpenUIEditor(file);
            return true;
        },
        nullptr
    });
    CommandRegistry::Instance().RegisterCommand({
        QStringLiteral("Scene.AssignScript"),
        QStringLiteral("Assign Script"),
        QString(),
        QStringLiteral("Assign visual script to selected scene object"),
        [this](const CommandContext& ctx) {
            if (!host_->HasScene() || sceneBusy_ || host_->PlayState() != EditorQtHost::EditorState::Edit)
                return false;
            const uint64_t targetId = ctx.parameters.value(QStringLiteral("objectId"), selectedId_).toULongLong();
            if (targetId == 0)
                return false;
            QString script = ctx.parameters.value(QStringLiteral("script")).toString();
            if (script.isEmpty()) {
                AssetPickerDialog picker(EditorCore::AssetKind::Script, QString::fromUtf8(host_->host().contentRoot), QString(), this);
                if (picker.exec() != QDialog::Accepted) return false;
                script = picker.SelectedVirtualPath();
            }
            if (script.isEmpty()) return false;
            const QString normalized = EditorCore::NormalizeVirtualPath(script, QString::fromUtf8(host_->host().contentRoot));
            EditorCommand c;
            c.type = EditorCommand::SetScript;
            c.objectId = targetId;
            c.path = normalized;
            host_->PostCommand(c);
            return true;
        },
        [this]() {
            return host_ && host_->HasScene() && !sceneBusy_ && host_->PlayState() == EditorQtHost::EditorState::Edit && selectedId_ != 0;
        }
    });
    CommandRegistry::Instance().RegisterCommand({
        QStringLiteral("Scene.AssignAiMachine"),
        QStringLiteral("Assign AI Machine"),
        QString(),
        QStringLiteral("Assign AI state machine to selected scene object"),
        [this](const CommandContext& ctx) {
            if (!host_->HasScene() || sceneBusy_ || host_->PlayState() != EditorQtHost::EditorState::Edit)
                return false;
            const uint64_t targetId = ctx.parameters.value(QStringLiteral("objectId"), selectedId_).toULongLong();
            if (targetId == 0)
                return false;
            QString machine = ctx.parameters.value(QStringLiteral("machine")).toString();
            if (machine.isEmpty()) {
                AssetPickerDialog picker(EditorCore::AssetKind::AiMachine, QString::fromUtf8(host_->host().contentRoot), QString(), this);
                if (picker.exec() != QDialog::Accepted) return false;
                machine = picker.SelectedVirtualPath();
            }
            if (machine.isEmpty()) return false;
            const QString normalized = EditorCore::NormalizeVirtualPath(machine, QString::fromUtf8(host_->host().contentRoot));
            const QByteArray json = QJsonDocument(QJsonArray{normalized}).toJson(QJsonDocument::Compact);
            EditorCommand c;
            c.type = EditorCommand::SetProperty;
            c.objectId = targetId;
            c.path = QStringLiteral("/stateMachine");
            c.name = QString::fromUtf8(json);
            host_->PostCommand(c);
            return true;
        },
        [this]() {
            return host_ && host_->HasScene() && !sceneBusy_ && host_->PlayState() == EditorQtHost::EditorState::Edit && selectedId_ != 0;
        }
    });

    CommandRegistry::Instance().RegisterCommand({
        QStringLiteral("Tools.ToggleAiDiagnostics"),
        QStringLiteral("Navigation and Diagnostics"),
        QString(),
        QStringLiteral("Toggle AI navigation and diagnostics dock panel"),
        [this](const CommandContext&) {
            if (aiDiagnosticsDock_) {
                aiDiagnosticsDock_->setVisible(!aiDiagnosticsDock_->isVisible());
                if (aiDiagnosticsDock_->isVisible()) {
                    aiDiagnosticsDock_->raise();
                }
            }
            return true;
        },
        nullptr
    });

    toolsMenu_ = menuBar()->addMenu(Tr("Инструменты", "Tools"));
    aiMenu_ = toolsMenu_->addMenu(Tr("ИИ", "AI"));
    aiMenu_->addAction(Tr("Навигация", "Navigation"), this, [] {
        CommandRegistry::Instance().Execute(QStringLiteral("View.ToggleAiDiagnostics"));
    });
    aiMenu_->addAction(Tr("Диагностика", "Diagnostics"), this, [] {
        CommandRegistry::Instance().Execute(QStringLiteral("Tools.ToggleAiDiagnostics"));
    });
    aiMenu_->addAction(Tr("Открыть редактор автомата ИИ…", "Open AI Machine Editor…"), this, [this] {
        AssetPickerDialog picker(EditorCore::AssetKind::AiMachine, QString::fromUtf8(host_->host().contentRoot), QString(), this);
        if (picker.exec() == QDialog::Accepted && !picker.SelectedVirtualPath().isEmpty()) {
            OpenAiMachineEditor(picker.SelectedVirtualPath());
        }
    });

    uiSubMenu_ = toolsMenu_->addMenu(Tr("Интерфейс (UI)", "Interface (UI)"));
    actCreateUI_ = uiSubMenu_->addAction(Tr("Создать интерфейс…", "Create UI…"), this, [this] {
        CommandRegistry::Instance().Execute(QStringLiteral("UI.Create"));
    });
    actOpenUIEditor_ = uiSubMenu_->addAction(Tr("Открыть редактор UI…", "Open UI Editor…"), this, [this] {
        CommandRegistry::Instance().Execute(QStringLiteral("UI.OpenEditor"));
    });
}
