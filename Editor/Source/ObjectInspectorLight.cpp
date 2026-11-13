#include "ObjectInspectorPanel.h"
#include "CollapsibleCard.h"
#include "NoWheelDoubleSpinBox.h"
#include "EditorLocale.h"
#include "IconProvider.h"

#include <QColorDialog>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

static QWidget* CreateAxisCell(NoWheelDoubleSpinBox* spin, const QString& axisLetter, const QString& badgeColor, QWidget* parent)
{
    auto* cell = new QWidget(parent);
    auto* layout = new QHBoxLayout(cell);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* badge = new QLabel(axisLetter, cell);
    badge->setFixedSize(16, 22);
    badge->setAlignment(Qt::AlignCenter);
    badge->setStyleSheet(QStringLiteral(
        "background-color: %1; color: white; font-weight: bold; font-size: 10px; "
        "border-top-left-radius: 4px; border-bottom-left-radius: 4px;"
    ).arg(badgeColor));
    layout->addWidget(badge);

    spin->setParent(cell);
    spin->setFixedHeight(22);
    spin->setProperty("axisCell", true);
    layout->addWidget(spin, 1);
    return cell;
}

void ObjectInspectorPanel::BuildPointLightCard(QVBoxLayout* layout)
{
    cardLight_ = new CollapsibleCard(Tr("Свет", "Light"), this, true);
    lightInspector_ = cardLight_;
    auto* cl = cardLight_->ContentLayout();
    auto emitChanged = [this] {
        if (refreshing_ || selectedId_ == 0) return;
        if (selectedType_ == "Light.Directional") {
            SubmitProperty(QStringLiteral("/intensity"), lightValues_[3]->value());
            SubmitProperty(QStringLiteral("/shadowDistance"), lightValues_[4]->value());
            return;
        }
        emit PointLightEditingFinished(selectedId_, CurrentPointLightValue());
    };
    auto* rowTop = new QWidget(cardLight_->Content()); auto* lTop = new QHBoxLayout(rowTop);
    lTop->setContentsMargins(0, 0, 0, 0); lTop->setSpacing(6);
    lightEnabled_ = new QCheckBox(Tr("Включён", "Enabled"), rowTop);
    lightEnabled_->setObjectName(QStringLiteral("LightEnabled")); lightEnabled_->hide();
    connect(lightEnabled_, &QCheckBox::clicked, this, emitChanged);
    lightShadows_ = new QCheckBox(Tr("Тени", "Shadows"), rowTop);
    lightShadows_->setObjectName(QStringLiteral("LightShadows"));
    connect(lightShadows_, &QCheckBox::clicked, this, [this, emitChanged] {
        if (refreshing_ || selectedId_ == 0) return;
        if (selectedType_ == "Light.Directional") { SubmitProperty(QStringLiteral("/shadows"), lightShadows_->isChecked()); return; }
        emitChanged();
    });
    lTop->addWidget(lightShadows_);
    lightColor_ = new QPushButton(Tr("Выбрать цвет…", "Choose Color…"), rowTop);
    lightColor_->setObjectName(QStringLiteral("LightColor")); lightColor_->setFixedHeight(24); lightColor_->setIcon(IconProvider::BulbIcon());
    connect(lightColor_, &QPushButton::clicked, this, [this] {
        if (refreshing_ || selectedId_ == 0) return;
        const QColor cur = QColor::fromRgbF(lightValues_[0]->value(), lightValues_[1]->value(), lightValues_[2]->value());
        if (selectedType_ == "Light.Directional") {
            const auto chosen = QColorDialog::getColor(cur, this, Tr("Цвет света", "Light Color"));
            if (chosen.isValid()) {
                const float c[3] = {static_cast<float>(chosen.redF()), static_cast<float>(chosen.greenF()), static_cast<float>(chosen.blueF())};
                UpdateLightColorButton(c); for (int i = 0; i < 3; ++i) lightValues_[i]->setValue(c[i]);
                SubmitProperty(QStringLiteral("/color"), QJsonArray{c[0], c[1], c[2]});
            } return;
        }
        emit LightColorRequested(selectedId_, cur);
    });
    lTop->addWidget(lightColor_, 1); cl->addWidget(rowTop);
    auto* rowRgb = new QWidget(cardLight_->Content()); auto* lRgb = new QHBoxLayout(rowRgb);
    lRgb->setContentsMargins(0, 0, 0, 0); lRgb->setSpacing(4);
    const QString rgbL[3] = {QStringLiteral("R"), QStringLiteral("G"), QStringLiteral("B")};
    const QString rgbC[3] = {QStringLiteral("#EF4444"), QStringLiteral("#22C55E"), QStringLiteral("#3B82F6")};
    for (int i = 0; i < 3; ++i) {
        auto* spin = lightValues_[i] = new NoWheelDoubleSpinBox(rowRgb);
        spin->setObjectName(QStringLiteral("LightValue%1").arg(i)); spin->setRange(0.0, 1.0); spin->setDecimals(2); spin->setSingleStep(0.05); spin->setKeyboardTracking(false);
        connect(spin, &QDoubleSpinBox::editingFinished, this, [this, emitChanged] {
            if (refreshing_ || selectedId_ == 0) return;
            if (selectedType_ == "Light.Directional") {
                const float c[3] = {static_cast<float>(lightValues_[0]->value()), static_cast<float>(lightValues_[1]->value()), static_cast<float>(lightValues_[2]->value())};
                UpdateLightColorButton(c); SubmitProperty(QStringLiteral("/color"), QJsonArray{c[0], c[1], c[2]}); return;
            }
            emitChanged();
        });
        lRgb->addWidget(CreateAxisCell(spin, rgbL[i], rgbC[i], rowRgb), 1);
    }
    cl->addWidget(rowRgb);
    auto addFieldRow = [&](int idx, const QString& label, double minV, double maxV, double step) {
        auto* row = new QWidget(cardLight_->Content()); auto* l = new QHBoxLayout(row);
        l->setContentsMargins(0, 0, 0, 0); l->setSpacing(6);
        lightRowLabels_[idx] = new QLabel(label, row); lightRowLabels_[idx]->setObjectName(QStringLiteral("CardFieldLabel")); lightRowLabels_[idx]->setFixedWidth(80);
        l->addWidget(lightRowLabels_[idx]);
        auto* spin = lightValues_[idx] = new NoWheelDoubleSpinBox(row);
        spin->setObjectName(QStringLiteral("LightValue%1").arg(idx)); spin->setFixedHeight(22); spin->setRange(minV, maxV); spin->setDecimals(2); spin->setSingleStep(step); spin->setKeyboardTracking(false);
        connect(spin, &QDoubleSpinBox::editingFinished, this, emitChanged); l->addWidget(spin, 1); cl->addWidget(row);
    };
    addFieldRow(3, Tr("Интенсивность", "Intensity"), 0.0, 10000.0, 0.5);
    addFieldRow(4, Tr("Радиус", "Radius"), 0.01, 10000.0, 0.5);
    layout->addWidget(cardLight_); cardLight_->hide();
}

InspectorPointLightValue ObjectInspectorPanel::CurrentPointLightValue() const
{
    InspectorPointLightValue value;
    for (int i = 0; i < 3; ++i) value.color[i] = lightValues_[i]->value();
    value.intensity = lightValues_[3]->value();
    value.radius = lightValues_[4]->value();
    value.enabled = lightEnabled_->isChecked();
    value.castShadows = lightShadows_ ? lightShadows_->isChecked() : false;
    return value;
}

void ObjectInspectorPanel::UpdateLightColorButton(const float color[3])
{
    const QColor backgroundColor = QColor::fromRgbF(color[0], color[1], color[2]);
    const double luminance = color[0] * 0.299 + color[1] * 0.587 + color[2] * 0.114;
    lightColor_->setStyleSheet(QStringLiteral("background-color: %1; color: %2;")
                               .arg(backgroundColor.name())
                               .arg(luminance > 0.5 ? QStringLiteral("black") : QStringLiteral("white")));
}
