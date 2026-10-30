#include "EditorDialogs.h"
#include "EditorLocale.h"

#include <QSettings>
#include <QDialog>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QListWidget>
#include <QTextBrowser>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDialogButtonBox>
#include <QCoreApplication>
#include <QTextDocument>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QFormLayout>
#include <QFileDialog>
#include <QProcess>
#include <QRegularExpression>

namespace {
    std::vector<std::pair<int, std::function<void(ControlScheme)>>> s_schemeSubscribers;
    int s_nextSubId = 1;
}

ControlScheme EditorSettings::GetControlScheme()
{
    QSettings settings(QStringLiteral("PainEngine"), QStringLiteral("Editor"));
    const QString val = settings.value(QStringLiteral("viewport/control_scheme"), QStringLiteral("unreal_unity")).toString();
    if (val.toLower() == QStringLiteral("blender")) {
        return ControlScheme::Blender;
    }
    return ControlScheme::UnrealUnity;
}

void EditorSettings::SetControlScheme(ControlScheme scheme)
{
    QSettings settings(QStringLiteral("PainEngine"), QStringLiteral("Editor"));
    settings.setValue(QStringLiteral("viewport/control_scheme"),
        scheme == ControlScheme::Blender ? QStringLiteral("blender") : QStringLiteral("unreal_unity"));

    for (const auto& sub : s_schemeSubscribers) {
        if (sub.second) sub.second(scheme);
    }
}

SpawnLocationMode EditorSettings::GetSpawnLocationMode()
{
    QSettings settings(QStringLiteral("PainEngine"), QStringLiteral("Editor"));
    const QString val = settings.value(QStringLiteral("viewport/spawn_location_mode"), QStringLiteral("cursor")).toString();
    if (val.toLower() == QStringLiteral("origin")) {
        return SpawnLocationMode::WorldOrigin;
    }
    return SpawnLocationMode::UnderCursor;
}

void EditorSettings::SetSpawnLocationMode(SpawnLocationMode mode)
{
    QSettings settings(QStringLiteral("PainEngine"), QStringLiteral("Editor"));
    settings.setValue(QStringLiteral("viewport/spawn_location_mode"),
        mode == SpawnLocationMode::WorldOrigin ? QStringLiteral("origin") : QStringLiteral("cursor"));
}

int EditorSettings::SubscribeControlSchemeChanged(std::function<void(ControlScheme)> callback)
{
    const int id = s_nextSubId++;
    s_schemeSubscribers.push_back({id, std::move(callback)});
    return id;
}

void EditorSettings::UnsubscribeControlSchemeChanged(int id)
{
    for (auto it = s_schemeSubscribers.begin(); it != s_schemeSubscribers.end(); ++it) {
        if (it->first == id) {
            s_schemeSubscribers.erase(it);
            break;
        }
    }
}

void ShowDocumentationDialog(QWidget* parent, const QString& projectRoot)
{
    auto* dialog = new QDialog(parent);
    dialog->setWindowTitle(Tr("Документация и Паспорт проекта", "Documentation & Project Passport"));
    dialog->resize(950, 620);

    auto* mainLayout = new QVBoxLayout(dialog);
    mainLayout->setContentsMargins(8, 8, 8, 8);

    auto* splitter = new QSplitter(Qt::Horizontal, dialog);

    // Левая панель: фильтр и список разделов
    auto* leftWidget = new QWidget(splitter);
    auto* leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    auto* sectionFilter = new QLineEdit(leftWidget);
    sectionFilter->setPlaceholderText(Tr("Поиск раздела…", "Search section…"));
    leftLayout->addWidget(sectionFilter);

    auto* listWidget = new QListWidget(leftWidget);
    listWidget->setMinimumWidth(220);
    leftLayout->addWidget(listWidget);

    // Правая панель: поиск по тексту и Markdown браузер
    auto* rightWidget = new QWidget(splitter);
    auto* rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    auto* searchBar = new QHBoxLayout;
    auto* textSearchEdit = new QLineEdit(rightWidget);
    textSearchEdit->setPlaceholderText(Tr("Поиск в документе…", "Find in document…"));
    auto* findNextBtn = new QPushButton(Tr("Найти далее", "Find Next"), rightWidget);
    searchBar->addWidget(textSearchEdit);
    searchBar->addWidget(findNextBtn);
    rightLayout->addLayout(searchBar);

    auto* browser = new QTextBrowser(rightWidget);
    browser->setOpenExternalLinks(true);
    rightLayout->addWidget(browser);

    splitter->addWidget(leftWidget);
    splitter->addWidget(rightWidget);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);
    mainLayout->addWidget(splitter);

    auto* btnBox = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    QObject::connect(btnBox, &QDialogButtonBox::rejected, dialog, &QDialog::accept);
    mainLayout->addWidget(btnBox);

    // Структура документов
    struct DocItem {
        QString title;
        QString content;
    };
    std::vector<DocItem> docs;

    // 1. Паспорт проекта и горячие клавиши
    const QString passportMd = QString::fromUtf8(R"MD(# PainEngine — Паспорт проекта и Управление

## Схемы управления во Вьюпорте

### Схема Unreal / Unity (по умолчанию)
* **ПКМ + перемещение мыши**: свободный обзор камеры (Look around).
* **WASD**: перемещение камеры вперёд, влево, назад, вправо.
* **Q / E**: опускание и подъём камеры по высоте.
* **СКМ (колёсико) + перемещение**: панорамирование (Pan).
* **W**: переключить гизмо на **Перемещение** (Translate).
* **E**: переключить гизмо на **Вращение** (Rotate).
* **R**: переключить гизмо на **Масштаб** (Scale).

### Схема Blender
* **СКМ (колёсико) + перемещение**: вращение камеры вокруг точки (Orbit).
* **Shift + СКМ + перемещение**: панорамирование (Pan).
* **ПКМ + мышь**: свободный полёт камеры с клавишами WASD.
* **G**: гизмо перемещения (Grab / Translate).
* **R**: гизмо вращения (Rotate).
* **S**: гизмо масштабирования (Scale).
* **Shift + A**: быстрое меню создания объекта под курсором.

---

## Общие горячие клавиши
* **Space**: запуск режима Play / остановка (Edit).
* **P**: пауза Play / возобновление.
* **1 / 2 / 3**: режимы гизмо Перемещение / Вращение / Масштаб (работают в любой схеме).
* **F**: сфокусировать камеру на выбранном объекте (Focus).
* **Tab**: циклический выбор объектов сцены.
* **Escape**: сброс выделения объекта.
* **Delete**: удалить выбранный объект (в окне Outliner).
* **Ctrl + D**: дублировать объект.
* **Ctrl + Z**: отмена действия (Undo).
* **Ctrl + Y**: повтор действия (Redo).
* **Ctrl + S**: сохранить сцену и назначить стартовой.
* **F6**: перекомпиляция всех шейдеров HLSL на лету.
* **F1**: справка и документация.
)MD");
    docs.push_back({Tr("Паспорт и Горячие клавиши", "Passport & Hotkeys"), passportMd});

    // 2. Сканирование Engine/docs/
    QString appDir = QCoreApplication::applicationDirPath();
    QDir engineDocsDir(appDir + QStringLiteral("/../../../Engine/docs"));
    if (!engineDocsDir.exists()) {
        engineDocsDir = QDir(QDir::currentPath() + QStringLiteral("/Engine/docs"));
    }
    if (!engineDocsDir.exists() && !projectRoot.isEmpty()) {
        engineDocsDir = QDir(projectRoot + QStringLiteral("/Engine/docs"));
    }

    if (engineDocsDir.exists()) {
        const auto files = engineDocsDir.entryInfoList({QStringLiteral("*.md")}, QDir::Files, QDir::Name);
        for (const auto& fi : files) {
            QFile f(fi.absoluteFilePath());
            if (f.open(QIODevice::ReadOnly)) {
                docs.push_back({fi.baseName(), QString::fromUtf8(f.readAll())});
            }
        }
    }

    // Заполнение списка
    auto updateList = [&](const QString& filter) {
        listWidget->clear();
        for (size_t i = 0; i < docs.size(); ++i) {
            if (filter.isEmpty() || docs[i].title.contains(filter, Qt::CaseInsensitive)) {
                auto* item = new QListWidgetItem(docs[i].title, listWidget);
                item->setData(Qt::UserRole, static_cast<int>(i));
            }
        }
        if (listWidget->count() > 0) {
            listWidget->setCurrentRow(0);
        }
    };

    updateList(QString());

    QObject::connect(sectionFilter, &QLineEdit::textChanged, dialog, updateList);

    QObject::connect(listWidget, &QListWidget::currentRowChanged, dialog, [&](int row) {
        if (row < 0 || row >= listWidget->count()) return;
        const int docIdx = listWidget->item(row)->data(Qt::UserRole).toInt();
        if (docIdx >= 0 && docIdx < static_cast<int>(docs.size())) {
            browser->setMarkdown(docs[docIdx].content);
        }
    });

    auto doFind = [browser, textSearchEdit] {
        const QString text = textSearchEdit->text();
        if (!text.isEmpty()) {
            browser->find(text);
        }
    };
    QObject::connect(findNextBtn, &QPushButton::clicked, dialog, doFind);
    QObject::connect(textSearchEdit, &QLineEdit::returnPressed, dialog, doFind);

    if (listWidget->count() > 0) {
        listWidget->setCurrentRow(0);
    }

    dialog->exec();
    dialog->deleteLater();
}

AssetImportDialog::AssetImportDialog(const QString& sourcePath, const QString& destinationDir, QWidget* parent)
    : QDialog(parent)
    , sourcePath_(sourcePath)
    , destinationDir_(destinationDir)
{
    setWindowTitle(Tr("Настройки импорта ассета", "Asset Import Settings") + QStringLiteral(" — ") + QFileInfo(sourcePath_).fileName());
    setMinimumWidth(540);
    DetectBlender();
    BuildUi();
}

void AssetImportDialog::DetectBlender()
{
    isBlenderFound_ = false;
    detectedBlenderPath_.clear();

    // 1. Поиск через where.exe в системном PATH
    QProcess proc;
    proc.start(QStringLiteral("where.exe"), { QStringLiteral("blender") });
    if (proc.waitForFinished(1000) && proc.exitCode() == 0) {
        const QString out = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        if (!out.isEmpty()) {
            const QString firstLine = out.split(QRegularExpression(QStringLiteral("[\r\n]+")), Qt::SkipEmptyParts).first().trimmed();
            if (QFile::exists(firstLine)) {
                detectedBlenderPath_ = firstLine;
                isBlenderFound_ = true;
                return;
            }
        }
    }

    // 2. Поиск в стандартных каталогах установки Blender Foundation
    const QStringList baseSearchDirs = {
        QStringLiteral("C:/Program Files/Blender Foundation"),
        QStringLiteral("C:/Program Files (x86)/Blender Foundation"),
        QDir::homePath() + QStringLiteral("/AppData/Local/Programs/Blender Foundation")
    };

    for (const auto& basePath : baseSearchDirs) {
        QDir baseDir(basePath);
        if (baseDir.exists()) {
            const QStringList subDirs = baseDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed);
            for (const auto& sub : subDirs) {
                const QString candidate = baseDir.filePath(sub + QStringLiteral("/blender.exe"));
                if (QFile::exists(candidate)) {
                    detectedBlenderPath_ = candidate;
                    isBlenderFound_ = true;
                    return;
                }
            }
        }
    }
}

void AssetImportDialog::BuildUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(10);

    const QFileInfo fi(sourcePath_);
    const QString ext = fi.suffix().toLower();
    const bool isBlend = (ext == QStringLiteral("blend"));
    const bool isFbx = (ext == QStringLiteral("fbx"));
    const bool isObj = (ext == QStringLiteral("obj"));
    const bool isGltf = (ext == QStringLiteral("gltf") || ext == QStringLiteral("glb"));

    QString typeName = Tr("3D Модель", "3D Model");
    if (isBlend) typeName = QStringLiteral("Blender 3D (.blend)");
    else if (isFbx) typeName = QStringLiteral("Autodesk Filmbox (.fbx)");
    else if (isObj) typeName = QStringLiteral("Wavefront 3D (.obj)");
    else if (isGltf) typeName = QStringLiteral("glTF 2.0 (.gltf/.glb)");

    // 1. Шапка файла
    auto* headerBox = new QFrame(this);
    headerBox->setStyleSheet(QStringLiteral("background: #181A20; border: 1px solid #282C34; border-radius: 6px; padding: 6px;"));
    auto* headerLayout = new QVBoxLayout(headerBox);
    headerLayout->setContentsMargins(6, 6, 6, 6);
    headerLayout->setSpacing(4);

    auto* lblName = new QLabel(QStringLiteral("<b>%1</b> (%2)").arg(fi.fileName(), typeName), headerBox);
    lblName->setStyleSheet(QStringLiteral("font-size: 13px; color: #60A5FA;"));
    headerLayout->addWidget(lblName);

    lblSourcePath_ = new QLabel(Tr("Источник: ", "Source: ") + sourcePath_, headerBox);
    lblSourcePath_->setStyleSheet(QStringLiteral("font-size: 10px; color: #94A3B8;"));
    lblSourcePath_->setWordWrap(true);
    headerLayout->addWidget(lblSourcePath_);

    QString sizeStr = QString::number(fi.size() / 1024.0, 'f', 1) + QStringLiteral(" KB");
    if (fi.size() > 1024 * 1024)
        sizeStr = QString::number(fi.size() / (1024.0 * 1024.0), 'f', 2) + QStringLiteral(" MB");
    auto* lblSize = new QLabel(Tr("Размер: ", "Size: ") + sizeStr, headerBox);
    lblSize->setStyleSheet(QStringLiteral("font-size: 10px; color: #64748B;"));
    headerLayout->addWidget(lblSize);

    mainLayout->addWidget(headerBox);

    // 2. Статус Blender
    auto* blenderBox = new QFrame(this);
    blenderBox->setStyleSheet(QStringLiteral("background: #1A1D24; border: 1px solid #2B303C; border-radius: 6px;"));
    auto* blenderLayout = new QHBoxLayout(blenderBox);
    blenderLayout->setContentsMargins(8, 6, 8, 6);
    blenderLayout->setSpacing(6);

    lblBlenderStatus_ = new QLabel(blenderBox);
    lblBlenderStatus_->setWordWrap(true);
    if (isBlenderFound_) {
        lblBlenderStatus_->setText(Tr("✓ Обнаружен Blender: ", "✓ Blender detected: ") + detectedBlenderPath_);
        lblBlenderStatus_->setStyleSheet(QStringLiteral("color: #10B981; font-weight: bold; font-size: 11px;"));
    } else {
        lblBlenderStatus_->setText(Tr("⚠ Blender не найден в системе (требуется для файлов .blend)",
                                      "⚠ Blender not detected (required for .blend files)"));
        lblBlenderStatus_->setStyleSheet(QStringLiteral("color: #F59E0B; font-weight: bold; font-size: 11px;"));
    }
    blenderLayout->addWidget(lblBlenderStatus_, 1);

    btnBrowseBlender_ = new QPushButton(Tr("Обзор…", "Browse…"), blenderBox);
    btnBrowseBlender_->setFixedWidth(70);
    connect(btnBrowseBlender_, &QPushButton::clicked, this, [this] {
        QString file = QFileDialog::getOpenFileName(this, Tr("Укажите blender.exe", "Select blender.exe"), {}, QStringLiteral("Blender (blender.exe)"));
        if (!file.isEmpty() && QFile::exists(file)) {
            detectedBlenderPath_ = file;
            isBlenderFound_ = true;
            lblBlenderStatus_->setText(Tr("✓ Выбран Blender: ", "✓ Selected Blender: ") + detectedBlenderPath_);
            lblBlenderStatus_->setStyleSheet(QStringLiteral("color: #10B981; font-weight: bold; font-size: 11px;"));
        }
    });
    blenderLayout->addWidget(btnBrowseBlender_);

    if (isBlend || isBlenderFound_) {
        mainLayout->addWidget(blenderBox);
    }

    // 3. Секция Transform
    auto* grpTransform = new QGroupBox(Tr("Трансформация (Transform)", "Transform"), this);
    auto* formTransform = new QFormLayout(grpTransform);
    formTransform->setContentsMargins(8, 8, 8, 8);
    formTransform->setSpacing(6);

    spinScale_ = new QDoubleSpinBox(grpTransform);
    spinScale_->setRange(0.0001, 10000.0);
    spinScale_->setDecimals(4);
    spinScale_->setSingleStep(0.1);
    spinScale_->setValue(1.0);
    formTransform->addRow(Tr("Масштаб (Scale Factor):", "Scale Factor:"), spinScale_);

    comboUpAxis_ = new QComboBox(grpTransform);
    comboUpAxis_->addItem(Tr("Z-Up (конвертировать в Y-Up движка)", "Z-Up (convert to Y-Up)"), 0);
    comboUpAxis_->addItem(Tr("Y-Up (без конвертации)", "Y-Up (no conversion)"), 1);
    comboUpAxis_->setCurrentIndex(isBlend ? 0 : 1);
    formTransform->addRow(Tr("Ориентация высоты (Up Axis):", "Up Axis:"), comboUpAxis_);

    mainLayout->addWidget(grpTransform);

    // 4. Секция Меша и Геометрии
    auto* grpMesh = new QGroupBox(Tr("Геометрия и Сетка (Mesh Options)", "Mesh Options"), this);
    auto* layoutMesh = new QVBoxLayout(grpMesh);
    layoutMesh->setContentsMargins(8, 8, 8, 8);
    layoutMesh->setSpacing(4);

    auto* rowMeshType = new QHBoxLayout;
    rowMeshType->addWidget(new QLabel(Tr("Тип меша:", "Mesh Type:"), grpMesh));
    comboMeshType_ = new QComboBox(grpMesh);
    comboMeshType_->addItem(Tr("Static Mesh (Статический меш)", "Static Mesh"), 0);
    comboMeshType_->addItem(Tr("Skeletal Mesh (Скелетный меш с костями)", "Skeletal Mesh"), 1);
    rowMeshType->addWidget(comboMeshType_, 1);
    layoutMesh->addLayout(rowMeshType);

    chkCombineMeshes_ = new QCheckBox(Tr("Объединить подсетки (Combine Meshes)", "Combine Meshes"), grpMesh);
    chkCombineMeshes_->setChecked(true);
    layoutMesh->addWidget(chkCombineMeshes_);

    chkNormalsTangents_ = new QCheckBox(Tr("Генерировать нормали и тангенты (Normals & Tangents)", "Generate Normals & Tangents"), grpMesh);
    chkNormalsTangents_->setChecked(true);
    layoutMesh->addWidget(chkNormalsTangents_);

    chkApplyModifiers_ = new QCheckBox(Tr("Применять модификаторы (Blender Apply Modifiers)", "Apply Modifiers (Blender)"), grpMesh);
    chkApplyModifiers_->setChecked(true);
    chkApplyModifiers_->setEnabled(isBlend);
    layoutMesh->addWidget(chkApplyModifiers_);

    mainLayout->addWidget(grpMesh);

    // 5. Материалы и Текстуры
    auto* grpMat = new QGroupBox(Tr("Материалы и Текстуры (Materials & Textures)", "Materials & Textures"), this);
    auto* layoutMat = new QVBoxLayout(grpMat);
    layoutMat->setContentsMargins(8, 8, 8, 8);
    layoutMat->setSpacing(4);

    chkImportTextures_ = new QCheckBox(Tr("Импортировать встроенные текстуры (Embedded Textures)", "Import Embedded Textures"), grpMat);
    chkImportTextures_->setChecked(true);
    layoutMat->addWidget(chkImportTextures_);

    chkAutoPbrMat_ = new QCheckBox(Tr("Автоматически создать PBR-материал (.mat)", "Auto-Create PBR Material (.mat)"), grpMat);
    chkAutoPbrMat_->setChecked(true);
    layoutMat->addWidget(chkAutoPbrMat_);

    mainLayout->addWidget(grpMat);

    // 6. Действие в сцене
    chkSpawnInScene_ = new QCheckBox(Tr("Добавить объект в сцену сразу после импорта", "Spawn in scene after import"), this);
    chkSpawnInScene_->setChecked(true);
    chkSpawnInScene_->setStyleSheet(QStringLiteral("font-weight: bold; color: #60A5FA;"));
    mainLayout->addWidget(chkSpawnInScene_);

    // 7. Кнопки Ok / Cancel
    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttonBox->button(QDialogButtonBox::Ok)->setText(Tr("Импортировать", "Import"));
    buttonBox->button(QDialogButtonBox::Ok)->setStyleSheet(QStringLiteral(
        "QPushButton { background: #2563EB; color: #FFFFFF; font-weight: bold; border-radius: 4px; padding: 5px 16px; }"
        "QPushButton:hover { background: #1D4ED8; }"
    ));
    buttonBox->button(QDialogButtonBox::Cancel)->setText(Tr("Отмена", "Cancel"));
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);
}

AssetImportSettings AssetImportDialog::GetSettings() const
{
    AssetImportSettings s;
    s.sourcePath = sourcePath_;
    s.destinationDir = destinationDir_;
    s.scaleFactor = spinScale_ ? spinScale_->value() : 1.0;
    s.upAxis = comboUpAxis_ ? comboUpAxis_->currentIndex() : 0;
    s.isSkeletalMesh = comboMeshType_ && comboMeshType_->currentIndex() == 1;
    s.combineMeshes = chkCombineMeshes_ && chkCombineMeshes_->isChecked();
    s.generateNormalsTangents = chkNormalsTangents_ && chkNormalsTangents_->isChecked();
    s.applyModifiers = chkApplyModifiers_ && chkApplyModifiers_->isChecked();
    s.importEmbeddedTextures = chkImportTextures_ && chkImportTextures_->isChecked();
    s.autoCreatePbrMaterial = chkAutoPbrMat_ && chkAutoPbrMat_->isChecked();
    s.spawnInScene = chkSpawnInScene_ && chkSpawnInScene_->isChecked();
    s.blenderPath = detectedBlenderPath_;
    return s;
}


