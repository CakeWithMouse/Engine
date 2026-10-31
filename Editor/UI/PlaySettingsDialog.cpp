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
}

bool PlaySettingsDialog::Execute(uint64_t& outSettingsId, QString& outConfigJson)
{
    uint64_t settingsId = 0;
    QJsonObject data{{"version", 1}, {"character", 0}, {"camera", 0}, {"standardControl", true}, {"cameraDistance", 5}, {"cameraHeight", 2}, {"lookSensitivity", 0.005}};

    for (const auto& o : objects_) {
        if (o.gameplayKind == 3) {
            if (settingsId) {
                QMessageBox::warning(this, Tr("Настройки Play", "Play Settings"),
                    Tr("Включите «Вид → Служебные объекты сцены» и удалите лишние записи «Настройки Play». Для сцены нужна одна запись.",
                       "Enable «View -> Scene Service Objects» and remove duplicate «Play Settings» entries. Only one entry is needed for the scene."));
                return false;
            }
            settingsId = o.id;
            data = QJsonDocument::fromJson(QByteArray::fromStdString(o.projectData)).object();
        }
    }

    auto* form = new QFormLayout(this);
    auto* character = new QComboBox(this);
    auto* camera = new QComboBox(this);
    character->setObjectName(QStringLiteral("PlayCharacter"));
    camera->setObjectName(QStringLiteral("PlayCamera"));

    character->addItem(Tr("Без управления персонажем", "No Character Control"), QVariant::fromValue<qulonglong>(0));
    camera->addItem(Tr("Следовать за персонажем", "Follow Character"), QVariant::fromValue<qulonglong>(0));

    for (const auto& o : objects_) {
        auto* box = o.gameplayKind == 1 ? character : o.gameplayKind == 2 ? camera : nullptr;
        if (box) box->addItem(QString::fromStdString(o.name), QVariant::fromValue<qulonglong>(o.id));
    }

    character->setCurrentIndex((std::max)(0, character->findData(QVariant::fromValue<qulonglong>(data["character"].toInteger()))));
    camera->setCurrentIndex((std::max)(0, camera->findData(QVariant::fromValue<qulonglong>(data["camera"].toInteger()))));

    auto* standard = new QCheckBox(Tr("WASD: движение, ПКМ + мышь: обзор", "WASD: movement, RMB + mouse: look"), this);
    standard->setChecked(data["standardControl"].toBool());

    auto* distance = new QDoubleSpinBox(this);
    distance->setRange(0, 10000);
    distance->setValue(data["cameraDistance"].toDouble());

    auto* height = new QDoubleSpinBox(this);
    height->setRange(-10000, 10000);
    height->setValue(data["cameraHeight"].toDouble());

    auto* sensitivity = new QDoubleSpinBox(this);
    sensitivity->setDecimals(4);
    sensitivity->setSingleStep(0.001);
    sensitivity->setRange(0, 1);
    sensitivity->setValue(data["lookSensitivity"].toDouble());

    form->addRow(Tr("Персонаж", "Character"), character);
    form->addRow(Tr("Камера", "Camera"), camera);
    form->addRow(standard);
    form->addRow(Tr("Расстояние камеры", "Camera Distance"), distance);
    form->addRow(Tr("Высота камеры", "Camera Height"), height);
    form->addRow(Tr("Чувствительность обзора", "Look Sensitivity"), sensitivity);
    form->addRow(new QLabel(Tr("Добавьте персонажа в категории Персонажи.\nФиксированная камера использует трансформацию объекта.\nSpace: Stop, P: Pause. Для управления щёлкните viewport.",
                               "Add a character in the Characters category.\nFixed camera uses object transform.\nSpace: Stop, P: Pause. Click viewport for controls."), this));

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    if (exec() != QDialog::Accepted) return false;

    data["character"] = static_cast<qint64>(character->currentData().toULongLong());
    data["camera"] = static_cast<qint64>(camera->currentData().toULongLong());
    data["standardControl"] = standard->isChecked();
    data["cameraDistance"] = distance->value();
    data["cameraHeight"] = height->value();
    data["lookSensitivity"] = sensitivity->value();

    outSettingsId = settingsId;
    outConfigJson = QString::fromUtf8(QJsonDocument(data).toJson(QJsonDocument::Compact));
    return true;
}
