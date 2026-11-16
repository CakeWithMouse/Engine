#include "PlaySettingsDialog.h"
#include "EditorLocale.h"
#include <QFormLayout>
#include <QComboBox>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QJsonObject>
#include <QJsonDocument>
#include <algorithm>

PlaySettingsDialog::PlaySettingsDialog(const std::vector<PlaySettingsObjectView>& objects, QWidget* parent)
    : QDialog(parent), objects_(objects)
{
    setWindowTitle(Tr("Персонаж и камера Play", "Play Character & Camera"));
    BuildUi();
}

void PlaySettingsDialog::BuildUi()
{
    auto* form = new QFormLayout(this);
    character_ = new QComboBox(this);
    camera_ = new QComboBox(this);
    character_->setObjectName(QStringLiteral("PlayCharacter"));
    camera_->setObjectName(QStringLiteral("PlayCamera"));

    character_->addItem(Tr("Без управления персонажем", "No Character Control"), QVariant::fromValue<qulonglong>(0));
    camera_->addItem(Tr("Следовать за персонажем", "Follow Character"), QVariant::fromValue<qulonglong>(0));

    for (const auto& o : objects_) {
        auto* box = o.gameplayKind == 1 ? character_ : o.gameplayKind == 2 ? camera_ : nullptr;
        if (box) box->addItem(QString::fromStdString(o.name), QVariant::fromValue<qulonglong>(o.id));
    }

    standard_ = new QCheckBox(Tr("WASD: движение, ПКМ + мышь: обзор", "WASD: movement, RMB + mouse: look"), this);

    distance_ = new QDoubleSpinBox(this);
    distance_->setObjectName(QStringLiteral("PlayDistance"));
    distance_->setRange(0, 10000);

    height_ = new QDoubleSpinBox(this);
    height_->setObjectName(QStringLiteral("PlayHeight"));
    height_->setRange(-10000, 10000);

    sensitivity_ = new QDoubleSpinBox(this);
    sensitivity_->setObjectName(QStringLiteral("PlaySensitivity"));
    sensitivity_->setDecimals(4);
    sensitivity_->setSingleStep(0.001);
    sensitivity_->setRange(0, 1);

    form->addRow(Tr("Персонаж", "Character"), character_);
    form->addRow(Tr("Камера", "Camera"), camera_);
    form->addRow(standard_);
    form->addRow(Tr("Расстояние камеры", "Camera Distance"), distance_);
    form->addRow(Tr("Высота камеры", "Camera Height"), height_);
    form->addRow(Tr("Чувствительность обзора", "Look Sensitivity"), sensitivity_);
    form->addRow(new QLabel(Tr("Добавьте персонажа в категории Персонажи.\nФиксированная камера использует трансформацию объекта.\nSpace: Stop, P: Pause. Для управления щёлкните viewport.",
                               "Add a character in the Characters category.\nFixed camera uses object transform.\nSpace: Stop, P: Pause. Click viewport for controls."), this));

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

bool PlaySettingsDialog::Execute(uint64_t& outSettingsId, QString& outConfigJson)
{
    uint64_t settingsId = 0;
    QJsonObject data{{"version", 1}, {"character", 0}, {"camera", 0}, {"standardControl", true}, {"cameraDistance", 5}, {"cameraHeight", 2}, {"lookSensitivity", 0.005}};

    for (const auto& o : objects_) {
        if (o.gameplayKind == 3) {
            if (settingsId) {
                QMessageBox::warning(this, Tr("Настройки Play", "Play Settings"),
                    Tr("Обнаружены лишние записи «Настройки Play». Для сцены нужна одна запись.", "Duplicate «Play Settings» entries detected. Only one entry is needed for the scene."));
                return false;
            }
            settingsId = o.id;
            data = QJsonDocument::fromJson(QByteArray::fromStdString(o.projectData)).object();
        }
    }

    character_->setCurrentIndex((std::max)(0, character_->findData(QVariant::fromValue<qulonglong>(data["character"].toInteger()))));
    camera_->setCurrentIndex((std::max)(0, camera_->findData(QVariant::fromValue<qulonglong>(data["camera"].toInteger()))));
    standard_->setChecked(data["standardControl"].toBool());
    distance_->setValue(data["cameraDistance"].toDouble());
    height_->setValue(data["cameraHeight"].toDouble());
    sensitivity_->setValue(data["lookSensitivity"].toDouble());

    if (exec() != QDialog::Accepted) return false;

    data["character"] = static_cast<qint64>(character_->currentData().toULongLong());
    data["camera"] = static_cast<qint64>(camera_->currentData().toULongLong());
    data["standardControl"] = standard_->isChecked();
    data["cameraDistance"] = distance_->value();
    data["cameraHeight"] = height_->value();
    data["lookSensitivity"] = sensitivity_->value();

    outSettingsId = settingsId;
    outConfigJson = QString::fromUtf8(QJsonDocument(data).toJson(QJsonDocument::Compact));
    return true;
}
