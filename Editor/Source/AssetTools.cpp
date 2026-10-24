#include "MainWindow.h"
#include "QtHost.h"
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QDoubleSpinBox>

void EditorMainWindow::ImportAsset(bool texture, const QString& source)
{
    if (!host_->HasScene() || sceneBusy_ || host_->PlayState() != EditorQtHost::Edit) return;
    const auto paths = source.isEmpty() ? QFileDialog::getOpenFileNames(this,
        texture ? QStringLiteral("Импорт текстур в проект") : QStringLiteral("Импорт моделей в проект"), {},
        texture ? QStringLiteral("Текстуры (*.png *.jpg *.jpeg *.bmp *.tga)") :
            QStringLiteral("Модели (*.glb *.gltf *.obj *.fbx)")) : QStringList{source};
    for (const auto& path : paths)
    {
        EditorCommand c; c.type = texture ? EditorCommand::ImportTexture : EditorCommand::ImportModel;
        c.path = path; host_->PostCommand(c);
    }
}
void EditorMainWindow::AddModel(const QString& path)
{
    if (!host_->HasScene() || sceneBusy_ || host_->PlayState() != EditorQtHost::Edit) return;
    const auto selected = path.isEmpty() ? QFileDialog::getOpenFileName(this,
        QStringLiteral("Добавить модель из проекта"),
        QString::fromUtf8(host_->host().contentRoot) + QStringLiteral("/Models"),
        QStringLiteral("Модели (*.glb *.gltf *.obj *.fbx)")) : path;
    if (!selected.isEmpty()) UseAsset(selected);
}
void EditorMainWindow::UseAsset(const QString& path)
{
    if (!host_->HasScene() || sceneBusy_ || host_->PlayState() != EditorQtHost::Edit) return;
    const QDir content(QString::fromUtf8(host_->host().contentRoot));
    const auto relative = content.relativeFilePath(QFileInfo(path).absoluteFilePath());
    if (relative == QStringLiteral("..") || relative.startsWith(QStringLiteral("../")) || QDir::isAbsolutePath(relative))
    {
        QMessageBox::warning(this, QStringLiteral("Ресурс вне проекта"), QStringLiteral("Сначала импортируйте файл кнопкой «Импорт модели» или «Импорт текстуры»."));
        return;
    }
    const auto extension = QFileInfo(path).suffix().toLower();
    const bool model = QStringList{"glb", "gltf", "obj", "fbx"}.contains(extension);
    const bool texture = QStringList{"png", "jpg", "jpeg", "bmp", "tga"}.contains(extension);
    if (!model && !texture) return;
    EditorCommand c;
    c.path = QStringLiteral("/Game/") + QDir::fromNativeSeparators(relative);
    if (model)
    {
        c.type = EditorCommand::CreateModel; c.name = QFileInfo(path).completeBaseName();
        for (int i = 0; i < 3; ++i) if (spawn_[i]) c.transform.position[i] = static_cast<float>(spawn_[i]->value());
    }
    else
    {
        c.objectId = host_->ReadSnapshot().selected;
        if (!c.objectId)
        {
            QMessageBox::information(this, QStringLiteral("Назначение текстуры"), QStringLiteral("Выберите объект в сцене, затем откройте текстуру двойным щелчком."));
            return;
        }
        c.type = EditorCommand::SetTexture;
    }
    host_->PostCommand(c);
}
void EditorMainWindow::BuildAssetInspector(QFormLayout* form)
{
    modelAsset_ = new QLineEdit(inspector_); modelAsset_->setReadOnly(true);
    modelAsset_->setObjectName(QStringLiteral("ModelAsset"));
    form->addRow(QStringLiteral("Модель"), modelAsset_);
    textureAsset_ = new QLineEdit(inspector_); textureAsset_->setReadOnly(true);
    textureAsset_->setObjectName(QStringLiteral("TextureAsset"));
    form->addRow(QStringLiteral("Текстура"), textureAsset_);
    auto* row = new QHBoxLayout;
    auto* choose = new QPushButton(QStringLiteral("Выбрать текстуру…"), inspector_);
    choose->setObjectName(QStringLiteral("ChooseTexture"));
    auto* clear = new QPushButton(QStringLiteral("Убрать"), inspector_);
    clear->setObjectName(QStringLiteral("ClearTexture"));
    row->addWidget(choose); row->addWidget(clear); form->addRow(row);
    connect(choose, &QPushButton::clicked, this, [this] {
        const auto path = QFileDialog::getOpenFileName(this, QStringLiteral("Текстура из проекта"),
            QString::fromUtf8(host_->host().contentRoot), QStringLiteral("Текстуры (*.png *.jpg *.jpeg *.bmp *.tga)"));
        if (!path.isEmpty()) UseAsset(path);
    });
    connect(clear, &QPushButton::clicked, this, [this] {
        if (!selectedId_ || sceneBusy_ || host_->PlayState() != EditorQtHost::Edit) return;
        EditorCommand c; c.type = EditorCommand::SetTexture; c.objectId = selectedId_; host_->PostCommand(c);
    });
}
