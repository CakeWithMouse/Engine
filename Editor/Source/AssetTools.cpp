#include "EditorLocale.h"
#include "MainWindow.h"
#include "SceneObjectsPanel.h"
#include "QtHost.h"
#include "MaterialEditorWindow.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTimer>

void EditorMainWindow::UpdateAsset(const QString& path,bool move,const QString& supplied)
{
    if(sceneBusy_||host_->PlayState()!=EditorQtHost::Edit||!host_->ReadSnapshot().authoring)return;
    if(!CloseMaterialEditors([this,path,move,supplied]{UpdateAsset(path,move,supplied);}))return;
    const QString content=QString::fromUtf8(host_->host().contentRoot);
    const auto relative=QDir(content).relativeFilePath(path);
    if(relative.startsWith("..")||QDir::isAbsolutePath(relative))return;
    QString argument=supplied;
    if(move && argument.isEmpty()) {
        bool ok=false;
        argument=QInputDialog::getText(this,Tr("Переместить ресурс", "Move Asset"),
            Tr("Новый путь относительно Content. Для модели перемещается вся её папка; имя файла модели сохраняется.\nСсылки в сценах и материалах будут обновлены. История Undo сцены будет очищена.", "New path relative to Content. For a model, its entire folder is moved; the model file name is preserved.\nReferences in scenes and materials will be updated. Scene undo history will be cleared."),QLineEdit::Normal,relative,&ok);
        if(!ok||argument.isEmpty())return;
    } else if(!move && argument.isEmpty()) {
        const auto metadata=RecoveryStore::Read(path+".source.json");
        const auto original=metadata["source"].toString();
        argument=QFileDialog::getOpenFileName(this,Tr("Исходный файл для переимпорта", "Source File for Reimport"),original,
            Tr("Ресурсы (*.fbx *.blend *.glb *.gltf *.obj *.png *.jpg *.jpeg *.bmp *.tga)", "Assets (*.fbx *.blend *.glb *.gltf *.obj *.png *.jpg *.jpeg *.bmp *.tga)"));
        if(argument.isEmpty())return;
        if(QMessageBox::question(this,Tr("Переимпорт ресурса", "Reimport Asset"),Tr("Обновить ресурс во всех сценах? Назначения и правки материалов сохранятся. История Undo сцены будет очищена. Прежние файлы останутся в .painengine/AssetBackups.", "Update asset across all scenes? Material assignments and edits will be preserved. Scene undo history will be cleared. Old files will remain in .painengine/AssetBackups."))!=QMessageBox::Yes)return;
    }
    if(move) {argument=QDir::fromNativeSeparators(argument);if(!argument.startsWith("/Game/"))argument="/Game/"+argument;}
    // Material preview detaches are queued first; the asset operation then runs
    // on the engine thread with no preview holding the replaced resources.
    EditorCommand c;c.type=EditorCommand::UpdateAsset;c.w=move?1:0;c.path="/Game/"+QDir::fromNativeSeparators(relative);c.name=argument;
    sceneBusy_=true;host_->PostCommand(c);
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
                                     Tr("Добавить модель из проекта", "Add Model from Project"),
                                     QString::fromUtf8(host_->host().contentRoot) + QStringLiteral("/Models"),
                                     Tr("Модели (*.glb *.gltf *.obj *.fbx)", "Models (*.glb *.gltf *.obj *.fbx)")
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
            Tr("Ресурс вне проекта", "Asset Outside Project"),
            Tr("Сначала импортируйте файл кнопкой «Импорт модели» или «Импорт текстуры».", "First import the file via Import Model or Import Texture.")
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

        const float s = GetModelSpawnScale(path);
        c.transform.scale[0] = s;
        c.transform.scale[1] = s;
        c.transform.scale[2] = s;
    }
    else
    {
        c.objectId = host_->ReadSnapshot().selected;

        if (!c.objectId)
        {
            QMessageBox::information(
                this,
                material ? Tr("Назначение материала", "Assign Material") : Tr("Назначение текстуры", "Assign Texture"),
                material ? Tr("Выберите объект в сцене, затем назначьте материал.", "Select an object in the scene, then assign the material.")
                         : Tr("Выберите объект в сцене, затем откройте текстуру двойным щелчком.", "Select an object in the scene, then double-click the texture to open.")
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
        "Texture2D BaseMap : register(t6);\n"
        "\n"
        "void Surface(MaterialInput i, inout MaterialOutput o)\n"
        "{\n"
        "    o.Albedo *= Tint.rgb;\n"
        "    o.Alpha *= Tint.a;\n"
        "    o.SpecularWeight = Gloss;\n"
        "    float4 baseMapColor = BaseMap.Sample(MaterialSampler, i.UV);\n"
        "    o.Albedo *= baseMapColor.rgb;\n"
        "    o.Alpha *= baseMapColor.a;\n"
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
            QMessageBox::warning(
                this,
                Tr("Материал вне проекта", "Material Outside Project"),
                Tr("Редактировать можно только материалы из Content проекта.", "Only materials from the project Content folder can be edited.")
            );

            return;
        }

        material = QStringLiteral("/Game/") + relative;
    }

    const QString content = QString::fromUtf8(host_->host().contentRoot);
    const QString rel = material.startsWith(QStringLiteral("/Game/")) ? material.mid(6) : material;
    const QString diskPath = QDir(content).filePath(rel);
    if (!QFileInfo::exists(diskPath))
    {
        QMessageBox::warning(
            this,
            Tr("Файл не найден", "File Not Found"),
            Tr("Файл материала не найден на диске: %1", "Material file not found on disk: %1").arg(diskPath)
        );
        return;
    }

    // Скрытое окно уже закрывается: превью отпускается, новое окно встанет в очередь после него.
    const QString key = material.toLower();
    if (MaterialEditorWindow* open = materialEditors_.value(key); open && open->isVisible())
    {
        open->showNormal();
        open->raise();
        open->activateWindow();
        return;
    }

    auto* editor = new MaterialEditorWindow(host_, material, this);
    materialEditors_.insert(key, editor);
    editor->show();
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
            Tr("Новый материал", "New Material"),
            Tr("Имя материала:", "Material Name:"),
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
        problem = Tr("Имя может содержать латинские буквы, цифры, «_», «-» и «.».", "Name can contain latin letters, digits, _, - and .");
    else if (QFileInfo::exists(materialFile) || QFileInfo::exists(shaderFile))
        problem = Tr("В каталоге уже есть %1.mat или %1.hlsl.", "Directory already contains %1.mat or %1.hlsl.").arg(base);
    else if (!QDir().mkpath(target))
        problem = Tr("Не удалось создать каталог %1.", "Failed to create directory %1.").arg(target);
    else
    {
        QJsonObject document;
        document.insert(QStringLiteral("version"), 1);
        document.insert(QStringLiteral("shader"), base + QStringLiteral(".hlsl"));

        QSaveFile shader(shaderFile), material(materialFile);

        if (!shader.open(QIODevice::WriteOnly) || shader.write(MaterialSurfaceTemplate) < 0 || !shader.commit() ||
            !material.open(QIODevice::WriteOnly) || material.write(QJsonDocument(document).toJson()) < 0 || !material.commit())
        {
            problem = Tr("Не удалось записать файлы материала в %1.", "Failed to write material files to %1.").arg(target);
        }
    }

    if (!problem.isEmpty())
    {
        QMessageBox::warning(this, Tr("Новый материал", "New Material"), problem);
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
            self->AppendLine(status == EngineSceneOk ? Tr("Шейдеры и материалы перекомпилированы с диска.", "Shaders and materials recompiled from disk.")
                                                     : Tr("Не удалось перекомпилировать шейдеры (код %1).", "Failed to recompile shaders (code %1).").arg(status));
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
        if (!editor->Shutdown())
            return false;

    return true;
}
