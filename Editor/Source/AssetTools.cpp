#include "MainWindow.h"
#include "SceneObjectsPanel.h"
#include "QtHost.h"
#include "MaterialEditorWindow.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QMessageBox>
#include <QCoreApplication>
#include <QRegularExpression>
#include <QSaveFile>

namespace
{
    bool IsTestRunning()
    {
        return qEnvironmentVariableIsSet("PAIN_EDITOR_TEST_MODE") ||
               (QCoreApplication::instance() && QCoreApplication::applicationFilePath().contains(QStringLiteral("Tests"), Qt::CaseInsensitive));
    }
}

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
                                          : QStringLiteral("Модели (*.glb *.gltf *.obj *.fbx *.blend)")
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
        if (IsTestRunning())
        {
            qWarning("AssetTools: Resource outside project: %s", qUtf8Printable(path));
            return;
        }

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

    const bool material = extension == QStringLiteral("mat");

    if (!model && !texture && !material)
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
            if (IsTestRunning())
            {
                qWarning("AssetTools: No object selected for material/texture assignment");
                return;
            }

            QMessageBox::information(
                this,
                material ? QStringLiteral("Назначение материала") : QStringLiteral("Назначение текстуры"),
                material ? QStringLiteral("Выберите объект в сцене, затем назначьте материал.")
                         : QStringLiteral("Выберите объект в сцене, затем откройте текстуру двойным щелчком.")
            );

            return;
        }

        c.type = material ? EditorCommand::SetMaterial : EditorCommand::SetTexture;
    }

    host_->PostCommand(c);
}

namespace
{
    // Путь внутри Content проекта: "Materials/Gold.mat"; пустой, если файл вне его.
    QString ContentRelativePath(const QString& contentRoot, const QString& path)
    {
        const QString relative = QDir(contentRoot).relativeFilePath(QFileInfo(path).absoluteFilePath());
        if (relative == QStringLiteral("..") ||
            relative.startsWith(QStringLiteral("../")) ||
            relative.startsWith(QStringLiteral("..\\")) ||
            QDir::isAbsolutePath(relative))
        {
            return {};
        }
        return QDir::fromNativeSeparators(relative);
    }

    const QRegularExpression& MaterialNamePattern()
    {
        static const QRegularExpression pattern(QStringLiteral("^[A-Za-z0-9_][A-Za-z0-9_.-]*$"));
        return pattern;
    }

    const char* const MaterialSurfaceTemplate =
        "// Surface of the material: inputs and outputs are described in Shaders/Material/MaterialCommon.hlsli.\n"
        "cbuffer MaterialParams : register(b1)\n"
        "{\n"
        "    float4 Tint = float4(1.0f, 1.0f, 1.0f, 1.0f);\n"
        "    float Gloss = 0.5f;\n"
        "};\n"
        "\n"
        "void Surface(MaterialInput i, inout MaterialOutput o)\n"
        "{\n"
        "    o.Albedo *= Tint.rgb;\n"
        "    o.SpecularWeight = Gloss;\n"
        "}\n";
}

void EditorMainWindow::OpenMaterialEditor(const QString& path)
{
    if (!host_->HasMaterials())
        return;

    QString material = path;

    if (!material.startsWith(QStringLiteral("/Game/")))
    {
        const QString relative = ContentRelativePath(QString::fromUtf8(host_->host().contentRoot), path);

        if (relative.isEmpty())
        {
            if (IsTestRunning())
            {
                qWarning("AssetTools: Material outside project: %s", qUtf8Printable(path));
                return;
            }

            QMessageBox::warning(
                this,
                QStringLiteral("Материал вне проекта"),
                QStringLiteral("Редактировать можно только материалы из Content проекта.")
            );

            return;
        }

        material = QStringLiteral("/Game/") + relative;
    }

    // Скрытое окно уже закрывается: превью отпускается, новое окно встанет в очередь после него.
    const QString key = material.toLower();
    if (MaterialEditorWindow* open = materialEditors_.value(key); open && open->isVisible())
    {
        open->showNormal();
        open->raise();
        open->activateWindow();
#ifdef _WIN32
        SetWindowPos(reinterpret_cast<HWND>(open->winId()), HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        BringWindowToTop(reinterpret_cast<HWND>(open->winId()));
        SetForegroundWindow(reinterpret_cast<HWND>(open->winId()));
        if (!IsTestRunning())
        {
            SetWindowPos(reinterpret_cast<HWND>(open->winId()), HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        }
#endif
        return;
    }

    auto* editor = new MaterialEditorWindow(host_, material, this);
    materialEditors_.insert(key, editor);
    editor->show();
    editor->raise();
    editor->activateWindow();
#ifdef _WIN32
    SetWindowPos(reinterpret_cast<HWND>(editor->winId()), HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    BringWindowToTop(reinterpret_cast<HWND>(editor->winId()));
    SetForegroundWindow(reinterpret_cast<HWND>(editor->winId()));
    if (!IsTestRunning())
    {
        SetWindowPos(reinterpret_cast<HWND>(editor->winId()), HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    }
#endif
}

QString EditorMainWindow::CreateMaterial(const QString& directory, const QString& name)
{
    if (!host_->HasMaterials())
        return {};

    const QString content = QString::fromUtf8(host_->host().contentRoot);
    QString target = directory;

    if (target.isEmpty() || ContentRelativePath(content, target).isEmpty())
        target = QDir(content).filePath(QStringLiteral("Materials"));

    QString base = name;

    if (base.isEmpty())
    {
        bool accepted = false;
        base = QInputDialog::getText(
            this,
            QStringLiteral("Новый материал"),
            QStringLiteral("Имя материала:"),
            QLineEdit::Normal,
            QStringLiteral("NewMaterial"),
            &accepted
        ).trimmed();

        if (!accepted || base.isEmpty())
            return {};
    }

    const QDir folder(target);
    const QString materialFile = folder.filePath(base + QStringLiteral(".mat"));
    const QString shaderFile = folder.filePath(base + QStringLiteral(".hlsl"));

    QString problem;

    if (!MaterialNamePattern().match(base).hasMatch())
        problem = QStringLiteral("Имя может содержать латинские буквы, цифры, «_», «-» и «.».");
    else if (QFileInfo::exists(materialFile) || QFileInfo::exists(shaderFile))
    {
        if (IsTestRunning())
        {
            qWarning("CreateMaterial: Material %s already exists, reusing existing file", qUtf8Printable(materialFile));
            OpenMaterialEditor(materialFile);
            return materialFile;
        }
        problem = QStringLiteral("В каталоге уже есть %1.mat или %1.hlsl.").arg(base);
    }
    else if (!QDir().mkpath(target))
        problem = QStringLiteral("Не удалось создать каталог %1.").arg(target);
    else
    {
        QJsonObject document;
        document.insert(QStringLiteral("version"), 1);
        document.insert(QStringLiteral("shader"), base + QStringLiteral(".hlsl"));

        QSaveFile shader(shaderFile), material(materialFile);

        if (!shader.open(QIODevice::WriteOnly) || shader.write(MaterialSurfaceTemplate) < 0 || !shader.commit() ||
            !material.open(QIODevice::WriteOnly) || material.write(QJsonDocument(document).toJson()) < 0 || !material.commit())
        {
            problem = QStringLiteral("Не удалось записать файлы материала в %1.").arg(target);
        }
    }

    if (!problem.isEmpty())
    {
        if (IsTestRunning())
        {
            qWarning("CreateMaterial: %s", qUtf8Printable(problem));
            return {};
        }

        QMessageBox::warning(this, QStringLiteral("Новый материал"), problem);
        return {};
    }

    OpenMaterialEditor(materialFile);
    return materialFile;
}

void EditorMainWindow::CreateMaterialForSelection()
{
    const auto snapshot = host_->ReadSnapshot();

    if (!host_->HasMaterials() ||
        sceneBusy_ ||
        host_->PlayState() != EditorQtHost::EditorState::Edit ||
        !snapshot.selected)
    {
        return;
    }

    // Имя объекта, если оно годится для файла, иначе "Material"; занятые имена получают суффикс.
    QString base = QStringLiteral("Material");

    for (const auto& object : snapshot.objects)
    {
        const QString name = QString::fromStdString(object.name);

        if (object.id == snapshot.selected && MaterialNamePattern().match(name).hasMatch())
            base = name;
    }

    const QDir folder(QDir(QString::fromUtf8(host_->host().contentRoot)).filePath(QStringLiteral("Materials")));
    QString name = base;

    for (int i = 2; QFileInfo::exists(folder.filePath(name + QStringLiteral(".mat"))) ||
                    QFileInfo::exists(folder.filePath(name + QStringLiteral(".hlsl"))); ++i)
    {
        name = base + QStringLiteral("_") + QString::number(i);
    }

    if (CreateMaterial(folder.path(), name).isEmpty())
        return;

    EditorCommand c;
    c.type = EditorCommand::SetMaterial;
    c.objectId = snapshot.selected;
    c.path = QStringLiteral("/Game/Materials/") + name + QStringLiteral(".mat");

    host_->PostCommand(c);
}

void EditorMainWindow::ReloadShaders()
{
    if (!host_->HasMaterials()) return;
    EditorQtHost* host = host_;
    QPointer<EditorMainWindow> self(this);
    EditorCommand c;
    c.type = EditorCommand::RunTask;
    c.task = [host, self](const EngineEditSessionApi& api)
    {
        const uint32_t status = api.reloadShaderFile ? api.reloadShaderFile(api.context, "") : EngineSceneReadOnly;
        host->RunOnUi([self, status]
        {
            if (!self) return;
            self->AppendLine(status == EngineSceneOk ? QStringLiteral("Шейдеры и материалы перекомпилированы с диска.")
                                                     : QStringLiteral("Не удалось перекомпилировать шейдеры (код %1).").arg(status));
        });
    };
    host_->PostCommand(c);
    // Окна материалов перечитают ошибки и параметры после перекомпиляции: команды идут по очереди.
    for (const auto& editor : materialEditors_)
        if (editor) editor->Refresh();
}

bool EditorMainWindow::CloseMaterialEditors(std::function<void()> afterSave)
{
    QList<MaterialEditorWindow*> open;

    for (const auto& editor : materialEditors_)
        if (editor && editor->isVisible())
            open.push_back(editor);

    for (auto* editor : open)
        if (!editor->ConfirmClose(afterSave))
            return false;

    for (auto* editor : open)
        editor->Shutdown();

    return true;
}
