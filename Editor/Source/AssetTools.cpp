#include "MainWindow.h"
#include "SceneObjectsPanel.h"
#include "QtHost.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>

void EditorMainWindow::ImportAsset(bool texture, const QString& source)
{
    if (!host_->HasScene() ||
        sceneBusy_ ||
        host_->PlayState() != EditorQtHost::EditorState::Edit)
    {
        return;
    }

    const QStringList paths = source.isEmpty()
                                  ? QFileDialog::getOpenFileNames(
                                      this,
                                      texture
                                          ? QStringLiteral("Импорт текстур в проект")
                                          : QStringLiteral("Импорт моделей в проект"),
                                      {},
                                      texture
                                          ? QStringLiteral("Текстуры (*.png *.jpg *.jpeg *.bmp *.tga)")
                                          : QStringLiteral("Модели (*.glb *.gltf *.obj *.fbx)")
                                  )
                                  : QStringList{source};

    for (const auto& path : paths)
    {
        EditorCommand c;

        c.type = texture
                     ? EditorCommand::ImportTexture
                     : EditorCommand::ImportModel;

        c.path = path;

        host_->PostCommand(c);
    }
}

void EditorMainWindow::AddModel(const QString& path)
{
    if (!host_->HasScene() ||
        sceneBusy_ ||
        host_->PlayState() != EditorQtHost::EditorState::Edit)
    {
        return;
    }

    const QString selected = path.isEmpty()
                                 ? QFileDialog::getOpenFileName(
                                     this,
                                     QStringLiteral("Добавить модель из проекта"),
                                     QString::fromUtf8(host_->host().contentRoot) + QStringLiteral("/Models"),
                                     QStringLiteral("Модели (*.glb *.gltf *.obj *.fbx)")
                                 )
                                 : path;

    if (!selected.isEmpty())
        UseAsset(selected);
}

void EditorMainWindow::UseAsset(const QString& path)
{
    if (!host_->HasScene() ||
        sceneBusy_ ||
        host_->PlayState() != EditorQtHost::EditorState::Edit)
    {
        return;
    }

    const QDir content(QString::fromUtf8(host_->host().contentRoot));
    const QString relative = content.relativeFilePath(QFileInfo(path).absoluteFilePath());

    if (relative == QStringLiteral("..") ||
        relative.startsWith(QStringLiteral("../")) ||
        relative.startsWith(QStringLiteral("..\\")) ||
        QDir::isAbsolutePath(relative))
    {
        QMessageBox::warning(
            this,
            QStringLiteral("Ресурс вне проекта"),
            QStringLiteral("Сначала импортируйте файл кнопкой «Импорт модели» или «Импорт текстуры».")
        );

        return;
    }

    const QString extension = QFileInfo(path).suffix().toLower();

    const bool model = QStringList{
        QStringLiteral("glb"),
        QStringLiteral("gltf"),
        QStringLiteral("obj"),
        QStringLiteral("fbx")
    }.contains(extension);

    const bool texture = QStringList{
        QStringLiteral("png"),
        QStringLiteral("jpg"),
        QStringLiteral("jpeg"),
        QStringLiteral("bmp"),
        QStringLiteral("tga")
    }.contains(extension);

    if (!model && !texture)
        return;

    EditorCommand c;

    c.path = QStringLiteral("/Game/") + QDir::fromNativeSeparators(relative);

    if (model)
    {
        c.type = EditorCommand::CreateModel;
        c.name = QFileInfo(path).completeBaseName();

        std::array<double, 3> spawn{};

        if (sceneObjectsPanel_)
            spawn = sceneObjectsPanel_->SpawnPosition();

        for (int i = 0; i < 3; ++i)
            c.transform.position[i] = static_cast<float>(spawn[i]);
    }
    else
    {
        c.objectId = host_->ReadSnapshot().selected;

        if (!c.objectId)
        {
            QMessageBox::information(
                this,
                QStringLiteral("Назначение текстуры"),
                QStringLiteral("Выберите объект в сцене, затем откройте текстуру двойным щелчком.")
            );

            return;
        }

        c.type = EditorCommand::SetTexture;
    }

    host_->PostCommand(c);
}
