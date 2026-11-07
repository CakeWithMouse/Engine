#include "ImportSettingsDialog.h"
#include "EditorLocale.h"
#include "ImportSettings.h"

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

using EditorCore::ImportSettingsFile;

static ImportSettingsDialog::ExecHook s_execHookForTesting = nullptr;

void ImportSettingsDialog::SetExecHookForTesting(ExecHook hook)
{
    s_execHookForTesting = std::move(hook);
}

ImportScalePromptResult ImportSettingsDialog::Prompt(QWidget* parent, float initialScale)
{
    ImportSettingsDialog dialog(parent);
    if (initialScale > 0.0f)
    {
        dialog.SetScale(initialScale);
    }

    int res = QDialog::Rejected;
    if (s_execHookForTesting)
    {
        res = s_execHookForTesting(dialog);
    }
    else
    {
        res = dialog.exec();
    }

    if (res == QDialog::Accepted)
    {
        return { true, dialog.Scale() };
    }
    return { false, 1.0f };
}

ModelImportScaleDecision ResolveModelImportScale(
    const QVariantMap& parameters,
    bool hasSource,
    QWidget* parent)
{
    if (parameters.contains(QStringLiteral("scale")))
    {
        return { true, static_cast<float>(parameters.value(QStringLiteral("scale")).toDouble()) };
    }

    if (!hasSource || parameters.value(QStringLiteral("showDialog"), false).toBool())
    {
        const auto prompt = ImportSettingsDialog::Prompt(parent);
        if (!prompt.accepted)
        {
            return { false, 1.0f };
        }
        return { true, prompt.scale };
    }

    return { true, ImportSettingsFile::DefaultScale };
}

ImportSettingsDialog::ImportSettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    BuildUi();
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

void ImportSettingsDialog::BuildUi()
{
    setWindowTitle(Tr("Настройки импорта модели", "Model Import Settings"));
    setObjectName(QStringLiteral("ImportSettingsDialog"));
    setModal(true);

    auto* rootLayout = new QVBoxLayout(this);
    auto* formLayout = new QFormLayout;

    spinScale_ = new QDoubleSpinBox(this);
    spinScale_->setObjectName(QStringLiteral("ImportScaleSpin"));
    spinScale_->setRange(static_cast<double>(ImportSettingsFile::MinScale),
                         static_cast<double>(ImportSettingsFile::MaxScale));
    spinScale_->setDecimals(4);
    spinScale_->setSingleStep(0.1);
    spinScale_->setValue(static_cast<double>(ImportSettingsFile::DefaultScale));

    formLayout->addRow(Tr("Масштаб:", "Scale:"), spinScale_);

    auto* presetLayout = new QHBoxLayout;
    btnPreset1_ = new QPushButton(Tr("1 (м)", "1 (m)"), this);
    btnPreset1_->setObjectName(QStringLiteral("Preset1"));
    connect(btnPreset1_, &QPushButton::clicked, this, [this] {
        if (spinScale_) spinScale_->setValue(1.0);
    });
    presetLayout->addWidget(btnPreset1_);

    btnPreset01_ = new QPushButton(Tr("0.1 (дм)", "0.1 (dm)"), this);
    btnPreset01_->setObjectName(QStringLiteral("Preset01"));
    connect(btnPreset01_, &QPushButton::clicked, this, [this] {
        if (spinScale_) spinScale_->setValue(0.1);
    });
    presetLayout->addWidget(btnPreset01_);

    btnPreset001_ = new QPushButton(Tr("0.01 (см)", "0.01 (cm)"), this);
    btnPreset001_->setObjectName(QStringLiteral("Preset001"));
    connect(btnPreset001_, &QPushButton::clicked, this, [this] {
        if (spinScale_) spinScale_->setValue(0.01);
    });
    presetLayout->addWidget(btnPreset001_);

    formLayout->addRow(Tr("Пресеты:", "Presets:"), presetLayout);
    rootLayout->addLayout(formLayout);

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

    connect(buttonBox_, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox_, &QDialogButtonBox::rejected, this, &QDialog::reject);

    rootLayout->addWidget(buttonBox_);
}
