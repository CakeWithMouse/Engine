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

void EditorMainWindow::OpenUIEditor(const QString& path) {
    const auto key=QFileInfo(path).absoluteFilePath();
    if(uiEditors_.value(key)){uiEditors_[key]->show();uiEditors_[key]->raise();uiEditors_[key]->activateWindow();return;}
    auto* window=new UIEditorWindow(key,this);window->setWindowFlag(Qt::Window);uiEditors_[key]=window;window->show();
}
QString EditorMainWindow::CreateUI(const QString& supplied) {
    const auto root=QString::fromUtf8(host_->host().contentRoot);QDir(root).mkpath("UI");
    QString file=supplied.isEmpty()?QFileDialog::getSaveFileName(this,QStringLiteral("Новый интерфейс"),QDir(root).filePath("UI/Hud.ui"),"UI (*.ui)"):supplied;
    if(file.isEmpty())return {};if(!file.endsWith(".ui"))file+=".ui";
    try { ProjectHud::Resolve(std::filesystem::u8path(root.toStdString()),QDir(root).relativeFilePath(file).toUtf8().toStdString()); }
    catch(const std::exception& e){QMessageBox::warning(this,QStringLiteral("HUD"),QString::fromUtf8(e.what()));return {};}
    if(QFileInfo::exists(file)){QMessageBox::warning(this,QStringLiteral("HUD"),QStringLiteral("Файл уже существует. Откройте его для редактирования."));return {};}
    QSaveFile output(file);const QByteArray data("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<UI/>\n");
    if(!output.open(QIODevice::WriteOnly)||output.write(data)!=data.size()||!output.commit()){QMessageBox::warning(this,QStringLiteral("HUD"),output.errorString());return {};}
    OpenUIEditor(file);return file;
}
void EditorMainWindow::ShowHudSettings() {
    const auto root=QString::fromUtf8(host_->host().contentRoot);
    QDialog dialog(this);dialog.setObjectName("HudSettingsDialog");dialog.setWindowTitle(QStringLiteral("HUD игры"));dialog.resize(560,220);
    auto* form=new QFormLayout(&dialog);auto* choice=new QComboBox(&dialog);choice->setObjectName("ProjectHudChoice");
    QFile current(QDir(root).filePath("ProjectHud.txt"));QString selected;if(current.open(QIODevice::ReadOnly))selected=QString::fromUtf8(current.readLine()).trimmed();
    // Windows cannot atomically replace the settings while this read handle is open.
    current.close();
    auto populate=[&](const QString& chosen){choice->clear();choice->addItem(QStringLiteral("Нет"),QString());QDirIterator it(root,{"*.ui"},QDir::Files,QDirIterator::Subdirectories);while(it.hasNext()){const auto path=QDir(root).relativeFilePath(it.next());choice->addItem(path,path);}if(!chosen.isEmpty()&&choice->findData(chosen)<0)choice->addItem(chosen+QStringLiteral(" — файл отсутствует"),chosen);choice->setCurrentIndex(std::max(0,choice->findData(chosen)));};
    populate(selected);form->addRow(QStringLiteral("HUD"),choice);
    auto* create=new QPushButton(QStringLiteral("Создать…"),&dialog);auto* edit=new QPushButton(QStringLiteral("Открыть в редакторе UI"),&dialog);form->addRow(create,edit);
    connect(create,&QPushButton::clicked,&dialog,[&]{const auto file=CreateUI();if(!file.isEmpty())populate(QDir(root).relativeFilePath(file));});
    connect(edit,&QPushButton::clicked,&dialog,[&]{if(!choice->currentData().toString().isEmpty())OpenUIEditor(QDir(root).filePath(choice->currentData().toString()));});
    auto* hint=new QLabel(QStringLiteral("HUD создаётся при следующем запуске игры.\nИзменения сохранённой разметки видны в работающем Play."),&dialog);hint->setWordWrap(true);form->addRow(hint);
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
