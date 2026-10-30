#include "PlaySettingsDialog.h"
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
    setWindowTitle(QStringLiteral("Персонаж и камера Play"));
}

bool PlaySettingsDialog::Execute(uint64_t& outSettingsId, QString& outConfigJson)
{
    uint64_t settingsId = 0;
    QJsonObject data{{"version", 1}, {"character", 0}, {"camera", 0}, {"standardControl", true}, {"cameraDistance", 5}, {"cameraHeight", 2}, {"lookSensitivity", 0.005}};

    for (const auto& o : objects_) {
        if (o.gameplayKind == 3) {
            if (settingsId) {
                QMessageBox::warning(this, QStringLiteral("Play"), QStringLiteral("Включите «Вид → Служебные объекты сцены» и удалите лишние записи «Настройки Play». Для сцены нужна одна запись."));
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

    character->addItem(QStringLiteral("Без управления персонажем"), QVariant::fromValue<qulonglong>(0));
    camera->addItem(QStringLiteral("Следовать за персонажем"), QVariant::fromValue<qulonglong>(0));

    for (const auto& o : objects_) {
        auto* box = o.gameplayKind == 1 ? character : o.gameplayKind == 2 ? camera : nullptr;
        if (box) box->addItem(QString::fromStdString(o.name), QVariant::fromValue<qulonglong>(o.id));
    }

    character->setCurrentIndex((std::max)(0, character->findData(QVariant::fromValue<qulonglong>(data["character"].toInteger()))));
    camera->setCurrentIndex((std::max)(0, camera->findData(QVariant::fromValue<qulonglong>(data["camera"].toInteger()))));

    auto* standard = new QCheckBox(QStringLiteral("WASD: движение, ПКМ + мышь: обзор"), this);
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

    form->addRow(QStringLiteral("Персонаж"), character);
    form->addRow(QStringLiteral("Камера"), camera);
    form->addRow(standard);
    form->addRow(QStringLiteral("Расстояние камеры"), distance);
    form->addRow(QStringLiteral("Высота камеры"), height);
    form->addRow(QStringLiteral("Чувствительность обзора"), sensitivity);
    form->addRow(new QLabel(QStringLiteral("Добавьте персонажа в категории Персонажи.\nФиксированная камера использует трансформацию объекта.\nSpace: Stop, P: Pause. Для управления щёлкните viewport."), this));

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

