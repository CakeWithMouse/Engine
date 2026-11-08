#include "ImportSettingsDialog.h"
#include <QTimer>
#include "CollapsibleCard.h"
#include "EditorLocale.h"
#include "IconProvider.h"
#include "ImportSettings.h"
#include "SettingsService.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

using EditorCore::ImportSettingsFile;
using EditorCore::ModelNormalsMode;

static ImportSettingsDialog::ExecHook s_execHookForTesting = nullptr;

void ImportSettingsDialog::SetExecHookForTesting(ExecHook hook)
{
    s_execHookForTesting = std::move(hook);
}

int ImportSettingsDialog::exec()
{
    if (s_execHookForTesting)
    {
        return s_execHookForTesting(*this);
    }
    return QDialog::exec();
}

EditorCore::ImportSettings ImportSettingsDialog::DefaultSettings()
{
    EditorCore::ImportSettings s;
    s.model.useFileUnitScale = true;
    return s;
}

ModelImportDecision ImportSettingsDialog::Prompt(QWidget* parent, const std::optional<EditorCore::ImportSettings>& initialSettings)
{
    ImportSettingsDialog dialog(parent);
    if (initialSettings.has_value())
    {
        dialog.SetSettings(*initialSettings);
    }

    const int res = dialog.exec();

    if (res == QDialog::Accepted)
    {
        dialog.SaveToSettingsService();
        return { true, dialog.Settings(), dialog.Scale() };
    }
    return { false, {}, 1.0f };
}

ImportScalePromptResult ImportSettingsDialog::PromptScale(QWidget* parent, float initialScale)
{
    EditorCore::ImportSettings s = DefaultSettings();
    s.model.scale = initialScale;
    const auto res = Prompt(parent, s);
    return { res.proceed, res.scale };
}

ModelImportDecision ResolveModelImportDecision(
    const QVariantMap& parameters,
    bool hasSource,
    QWidget* parent)
{
    if (parameters.contains(QStringLiteral("options")))
    {
        EditorCore::ImportSettings settings;
        QString err;
        if (ImportSettingsFile::Parse(parameters.value(QStringLiteral("options")).toString().toUtf8(), settings, &err))
        {
            return { true, settings, settings.model.scale };
        }
    }

    if (parameters.contains(QStringLiteral("scale")))
    {
        EditorCore::ImportSettings settings;
        settings.model.scale = static_cast<float>(parameters.value(QStringLiteral("scale")).toDouble());
        return { true, settings, settings.model.scale };
    }

    if (!hasSource || parameters.value(QStringLiteral("showDialog"), false).toBool())
    {
        const auto prompt = ImportSettingsDialog::Prompt(parent);
        if (!prompt.proceed)
        {
            return { false, {}, 1.0f };
        }
        return prompt;
    }

    ImportSettingsDialog dlg(parent);
    return { true, dlg.Settings(), dlg.Scale() };
}

ImportSettingsDialog::ImportSettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    BuildUi();
    SetupConnections();
    LoadFromSettingsService();
}

void ImportSettingsDialog::accept()
{
    SaveToSettingsService();
    QDialog::accept();
}

float ImportSettingsDialog::Scale() const
{
    return spinScale_ ? static_cast<float>(spinScale_->value()) : ImportSettingsFile::DefaultScale;
}

void ImportSettingsDialog::SetScale(float scale)
{
    if (spinScale_)
    {
        spinScale_->setValue(static_cast<double>(scale));
    }
}

EditorCore::ImportSettings ImportSettingsDialog::Settings() const
{
    EditorCore::ImportSettings s;
    s.version = 1;
    s.kind = QStringLiteral("model");
    s.model.scale = Scale();
    s.model.useFileUnitScale = checkUseFileUnitScale_ ? checkUseFileUnitScale_->isChecked() : false;
    s.model.importMaterials = checkImportMaterials_ ? checkImportMaterials_->isChecked() : true;
    s.model.importTextures = checkImportTextures_ ? checkImportTextures_->isChecked() : true;

    if (comboNormals_)
    {
        const int modeInt = comboNormals_->currentData().toInt();
        s.model.normals = static_cast<ModelNormalsMode>(modeInt);
    }
    else
    {
        s.model.normals = ModelNormalsMode::File;
    }

    s.model.smoothingAngle = spinSmoothingAngle_ ? static_cast<float>(spinSmoothingAngle_->value()) : ImportSettingsFile::DefaultSmoothingAngle;
    s.model.flipUv = checkFlipUv_ ? checkFlipUv_->isChecked() : false;
    s.model.mergeMeshes = checkMergeMeshes_ ? checkMergeMeshes_->isChecked() : true;
    return s;
}

void ImportSettingsDialog::SetSettings(const EditorCore::ImportSettings& settings)
{
    SetScale(settings.model.scale);

    if (checkUseFileUnitScale_)
        checkUseFileUnitScale_->setChecked(settings.model.useFileUnitScale);
    if (checkImportMaterials_)
        checkImportMaterials_->setChecked(settings.model.importMaterials);
    if (checkImportTextures_)
        checkImportTextures_->setChecked(settings.model.importTextures);

    if (comboNormals_)
    {
        const int idx = comboNormals_->findData(static_cast<int>(settings.model.normals));
        if (idx >= 0)
            comboNormals_->setCurrentIndex(idx);
    }

    if (spinSmoothingAngle_)
        spinSmoothingAngle_->setValue(static_cast<double>(settings.model.smoothingAngle));
    if (checkFlipUv_)
        checkFlipUv_->setChecked(settings.model.flipUv);
    if (checkMergeMeshes_)
        checkMergeMeshes_->setChecked(settings.model.mergeMeshes);

    UpdateDependencies();
}

void ImportSettingsDialog::SetEditMode(bool editMode)
{
    isEditMode_ = editMode;
    if (checkImportMaterials_)
    {
        checkImportMaterials_->setEnabled(!editMode);
        if (editMode)
        {
            checkImportMaterials_->setToolTip(Tr(
                "Материалы уже созданы. Для пересоздания выполните полный реимпорт.",
                "Materials already exist. To recreate them, perform a full reimport."));
        }
    }
    if (checkImportTextures_)
    {
        checkImportTextures_->setEnabled(!editMode);
        if (editMode)
        {
            checkImportTextures_->setToolTip(Tr(
                "Текстуры уже созданы. Для пересоздания выполните полный реимпорт.",
                "Textures already exist. To recreate them, perform a full reimport."));
        }
    }
    UpdateDependencies();
}

void ImportSettingsDialog::ResetToDefaults()
{
    SetSettings(DefaultSettings());
}

void ImportSettingsDialog::UpdateDependencies()
{
    if (checkImportMaterials_ && checkImportTextures_)
    {
        if (isEditMode_)
        {
            checkImportMaterials_->setEnabled(false);
            checkImportTextures_->setEnabled(false);
        }
        else
        {
            checkImportTextures_->setEnabled(checkImportMaterials_->isChecked());
        }
    }

    if (comboNormals_ && spinSmoothingAngle_)
    {
        const bool isSmooth = comboNormals_->currentData().toInt() == static_cast<int>(ModelNormalsMode::Smooth);
        spinSmoothingAngle_->setEnabled(isSmooth);
        if (labelSmoothingAngle_)
            labelSmoothingAngle_->setEnabled(isSmooth);

        const QString tip = isSmooth
            ? Tr("Максимальный угол между смежными гранями для сглаживания нормалей",
                 "Maximum angle between adjacent faces for smooth normals")
            : Tr("Доступно только при сглаженных нормалях",
                 "Available only when normals are set to Smooth");
        spinSmoothingAngle_->setToolTip(tip);
        if (labelSmoothingAngle_)
            labelSmoothingAngle_->setToolTip(tip);
    }
}

void ImportSettingsDialog::LoadFromSettingsService()
{
    auto& s = SettingsService::Instance();
    EditorCore::ImportSettings settings = DefaultSettings();

    if (s.Contains(QStringLiteral("import/model/scale")))
        settings.model.scale = static_cast<float>(s.GetValue(QStringLiteral("import/model/scale"), 1.0).toDouble());
    settings.model.useFileUnitScale = s.GetValue(QStringLiteral("import/model/useFileUnitScale"), true).toBool();
    if (s.Contains(QStringLiteral("import/model/importMaterials")))
        settings.model.importMaterials = s.GetValue(QStringLiteral("import/model/importMaterials"), true).toBool();
    if (s.Contains(QStringLiteral("import/model/importTextures")))
        settings.model.importTextures = s.GetValue(QStringLiteral("import/model/importTextures"), true).toBool();
    if (s.Contains(QStringLiteral("import/model/normals")))
    {
        ModelNormalsMode mode{};
        if (ImportSettingsFile::NormalsModeFromString(s.GetValue(QStringLiteral("import/model/normals")).toString(), mode))
            settings.model.normals = mode;
    }
    if (s.Contains(QStringLiteral("import/model/smoothingAngle")))
        settings.model.smoothingAngle = static_cast<float>(s.GetValue(QStringLiteral("import/model/smoothingAngle"), 60.0).toDouble());
    if (s.Contains(QStringLiteral("import/model/flipUv")))
        settings.model.flipUv = s.GetValue(QStringLiteral("import/model/flipUv"), false).toBool();
    if (s.Contains(QStringLiteral("import/model/mergeMeshes")))
        settings.model.mergeMeshes = s.GetValue(QStringLiteral("import/model/mergeMeshes"), true).toBool();

    SetSettings(settings);
}

void ImportSettingsDialog::SaveToSettingsService()
{
    const auto s = Settings();
    auto& svc = SettingsService::Instance();
    svc.SetValue(QStringLiteral("import/model/scale"), static_cast<double>(s.model.scale));
    svc.SetValue(QStringLiteral("import/model/useFileUnitScale"), s.model.useFileUnitScale);
    svc.SetValue(QStringLiteral("import/model/importMaterials"), s.model.importMaterials);
    svc.SetValue(QStringLiteral("import/model/importTextures"), s.model.importTextures);
    svc.SetValue(QStringLiteral("import/model/normals"), ImportSettingsFile::NormalsModeToString(s.model.normals));
    svc.SetValue(QStringLiteral("import/model/smoothingAngle"), static_cast<double>(s.model.smoothingAngle));
    svc.SetValue(QStringLiteral("import/model/flipUv"), s.model.flipUv);
    svc.SetValue(QStringLiteral("import/model/mergeMeshes"), s.model.mergeMeshes);
}

void ImportSettingsDialog::BuildUi()
{
    setWindowTitle(Tr("Настройки импорта модели", "Model Import Settings"));
    setObjectName(QStringLiteral("ImportSettingsDialog"));
    setModal(true);
    setMinimumWidth(380);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setSpacing(8);

    // --- Basic group ---
    auto* basicGroup = new QGroupBox(Tr("Базовые настройки", "Basic Settings"), this);
    basicGroup->setObjectName(QStringLiteral("BasicSettingsGroup"));
    auto* basicForm = new QFormLayout(basicGroup);
    basicForm->setSpacing(6);

    // Scale
    spinScale_ = new QDoubleSpinBox(basicGroup);
    spinScale_->setObjectName(QStringLiteral("ImportScaleSpin"));
    spinScale_->setRange(static_cast<double>(ImportSettingsFile::MinScale),
                         static_cast<double>(ImportSettingsFile::MaxScale));
    spinScale_->setDecimals(4);
    spinScale_->setSingleStep(0.1);
    spinScale_->setValue(static_cast<double>(ImportSettingsFile::DefaultScale));

    basicForm->addRow(Tr("Масштаб:", "Scale:"), spinScale_);

    // Presets
    auto* presetLayout = new QHBoxLayout;
    btnPreset1_ = new QPushButton(Tr("1 (м)", "1 (m)"), basicGroup);
    btnPreset1_->setObjectName(QStringLiteral("Preset1"));
    presetLayout->addWidget(btnPreset1_);

    btnPreset01_ = new QPushButton(Tr("0.1 (дм)", "0.1 (dm)"), basicGroup);
    btnPreset01_->setObjectName(QStringLiteral("Preset01"));
    presetLayout->addWidget(btnPreset01_);

    btnPreset001_ = new QPushButton(Tr("0.01 (см)", "0.01 (cm)"), basicGroup);
    btnPreset001_->setObjectName(QStringLiteral("Preset001"));
    presetLayout->addWidget(btnPreset001_);

    basicForm->addRow(Tr("Пресеты:", "Presets:"), presetLayout);

    // Use file unit scale
    checkUseFileUnitScale_ = new QCheckBox(Tr("Учитывать масштаб из файла", "Use file unit scale"), basicGroup);
    checkUseFileUnitScale_->setObjectName(QStringLiteral("UseFileUnitScaleCheck"));
    checkUseFileUnitScale_->setChecked(true);
    checkUseFileUnitScale_->setToolTip(Tr("Учитывать единицы измерения из файла. Исправляет увеличение в 100 раз у FBX из Blender.",
                                          "Apply unit scale from file metadata. Fixes 100x scale factor on FBX exported from Blender."));
    basicForm->addRow(QString(), checkUseFileUnitScale_);

    // Import materials
    checkImportMaterials_ = new QCheckBox(Tr("Импортировать материалы", "Import materials"), basicGroup);
    checkImportMaterials_->setObjectName(QStringLiteral("ImportMaterialsCheck"));
    checkImportMaterials_->setChecked(true);
    checkImportMaterials_->setToolTip(Tr("Создавать .mat файлы материалов для меша",
                                         "Generate .mat material files for the mesh"));
    basicForm->addRow(QString(), checkImportMaterials_);

    // Import textures
    checkImportTextures_ = new QCheckBox(Tr("Импортировать текстуры", "Import textures"), basicGroup);
    checkImportTextures_->setObjectName(QStringLiteral("ImportTexturesCheck"));
    checkImportTextures_->setChecked(true);
    checkImportTextures_->setToolTip(Tr("Извлекать и сохранять текстуры модели",
                                        "Extract and copy textures referenced by the model"));
    basicForm->addRow(QString(), checkImportTextures_);

    rootLayout->addWidget(basicGroup);

    // --- Advanced group (collapsible, collapsed by default) ---
    advancedCard_ = new CollapsibleCard(Tr("Расширенные настройки", "Advanced Settings"), this, false);
    advancedCard_->setObjectName(QStringLiteral("AdvancedSettingsCard"));

    auto* advForm = new QFormLayout;
    advForm->setSpacing(6);

    // Normals mode
    comboNormals_ = new QComboBox(advancedCard_->Content());
    comboNormals_->setObjectName(QStringLiteral("NormalsCombo"));
    comboNormals_->addItem(Tr("Из файла", "From File"), static_cast<int>(ModelNormalsMode::File));
    comboNormals_->addItem(Tr("Плоские", "Flat"), static_cast<int>(ModelNormalsMode::Flat));
    comboNormals_->addItem(Tr("Сглаженные", "Smooth"), static_cast<int>(ModelNormalsMode::Smooth));
    comboNormals_->setToolTip(Tr("Режим генерации нормалей вершин (из файла, плоские или сглаженные)",
                                 "Vertex normal generation mode (from file, flat, or smooth)"));
    advForm->addRow(Tr("Нормали:", "Normals:"), comboNormals_);

    // Smoothing angle
    spinSmoothingAngle_ = new QDoubleSpinBox(advancedCard_->Content());
    spinSmoothingAngle_->setObjectName(QStringLiteral("SmoothingAngleSpin"));
    spinSmoothingAngle_->setRange(static_cast<double>(ImportSettingsFile::MinAngle),
                                  static_cast<double>(ImportSettingsFile::MaxAngle));
    spinSmoothingAngle_->setDecimals(1);
    spinSmoothingAngle_->setSingleStep(1.0);
    spinSmoothingAngle_->setValue(static_cast<double>(ImportSettingsFile::DefaultSmoothingAngle));
    spinSmoothingAngle_->setToolTip(Tr("Максимальный угол между смежными гранями для сглаживания нормалей",
                                       "Maximum angle between adjacent faces for smooth normals"));
    labelSmoothingAngle_ = new QLabel(Tr("Угол сглаживания (°):", "Smoothing angle (°):"), advancedCard_->Content());
    labelSmoothingAngle_->setObjectName(QStringLiteral("SmoothingAngleLabel"));
    advForm->addRow(labelSmoothingAngle_, spinSmoothingAngle_);

    // Flip UV
    checkFlipUv_ = new QCheckBox(Tr("Перевернуть UV (V)", "Flip UVs (V)"), advancedCard_->Content());
    checkFlipUv_->setObjectName(QStringLiteral("FlipUvCheck"));
    checkFlipUv_->setChecked(false);
    checkFlipUv_->setToolTip(Tr("Инвертировать координату текстуры V (Y)",
                                "Invert texture V (Y) coordinate"));
    advForm->addRow(QString(), checkFlipUv_);

    // Merge meshes
    checkMergeMeshes_ = new QCheckBox(Tr("Склеивать меши", "Merge meshes"), advancedCard_->Content());
    checkMergeMeshes_->setObjectName(QStringLiteral("MergeMeshesCheck"));
    checkMergeMeshes_->setChecked(true);
    checkMergeMeshes_->setToolTip(Tr("Склеивать меши с одним материалом в единый буфер геометрии",
                                     "Merge submeshes sharing the same material into a single geometry buffer"));
    advForm->addRow(QString(), checkMergeMeshes_);

    advancedCard_->ContentLayout()->addLayout(advForm);
    rootLayout->addWidget(advancedCard_);

    // --- Action buttons (Reset to Defaults + OK/Cancel) ---
    auto* actionLayout = new QHBoxLayout;
    btnResetDefaults_ = new QPushButton(IconProvider::ResetIcon(), Tr("Сбросить к умолчанию", "Reset to Defaults"), this);
    btnResetDefaults_->setObjectName(QStringLiteral("ResetDefaultsBtn"));
    btnResetDefaults_->setToolTip(Tr("Восстановить стандартные настройки импорта",
                                     "Reset import settings to default values"));
    actionLayout->addWidget(btnResetDefaults_);
    actionLayout->addStretch();

    buttonBox_ = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    auto* btnOk = buttonBox_->button(QDialogButtonBox::Ok);
    if (btnOk)
    {
        btnOk->setObjectName(QStringLiteral("ImportOkBtn"));
        btnOk->setText(Tr("Импорт", "Import"));
    }
    auto* btnCancel = buttonBox_->button(QDialogButtonBox::Cancel);
    if (btnCancel)
    {
        btnCancel->setObjectName(QStringLiteral("ImportCancelBtn"));
        btnCancel->setText(Tr("Отмена", "Cancel"));
    }
    actionLayout->addWidget(buttonBox_);

    rootLayout->addLayout(actionLayout);

    UpdateDependencies();
}

void ImportSettingsDialog::SetupConnections()
{
    connect(btnPreset1_, &QPushButton::clicked, this, [this] {
        if (spinScale_) spinScale_->setValue(1.0);
    });
    connect(btnPreset01_, &QPushButton::clicked, this, [this] {
        if (spinScale_) spinScale_->setValue(0.1);
    });
    connect(btnPreset001_, &QPushButton::clicked, this, [this] {
        if (spinScale_) spinScale_->setValue(0.01);
    });

    connect(btnResetDefaults_, &QPushButton::clicked, this, &ImportSettingsDialog::ResetToDefaults);

    connect(checkImportMaterials_, &QCheckBox::toggled, this, &ImportSettingsDialog::UpdateDependencies);
    connect(comboNormals_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ImportSettingsDialog::UpdateDependencies);

    if (advancedCard_)
    {
        advancedCard_->SetToggledCallback([this](bool /*exp*/) {
            QTimer::singleShot(0, this, [this] {
                adjustSize();
            });
        });
    }

    connect(buttonBox_, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox_, &QDialogButtonBox::rejected, this, &QDialog::reject);
}
