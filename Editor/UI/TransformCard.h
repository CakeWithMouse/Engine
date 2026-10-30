#pragma once

#include "CollapsibleCard.h"
#include "NoWheelDoubleSpinBox.h"
#include "EditorLocale.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <array>
#include <functional>

struct InspectorTransformValue;

class TransformCard : public CollapsibleCard
{
public:
    explicit TransformCard(QWidget* parent = nullptr)
        : CollapsibleCard(Tr("Трансформация", "Transform"), parent, true)
    {
        BuildCard();
    }

    void SetOnChanged(std::function<void()> cb)
    {
        onChanged_ = std::move(cb);
    }

    void SetValues(const double pos[3], const double rotDeg[3], const double scale[3],
                   std::function<bool(QWidget*)> canReplaceField)
    {
        for (int i = 0; i < 3; ++i)
        {
            if (canReplaceField(transform_[i]))
                transform_[i]->setValue(pos[i]);
            if (canReplaceField(transform_[i + 3]))
                transform_[i + 3]->setValue(rotDeg[i]);
            if (canReplaceField(transform_[i + 6]))
                transform_[i + 6]->setValue(scale[i]);
        }
    }

    void GetValues(double pos[3], double rotDeg[3], double scale[3]) const
    {
        for (int i = 0; i < 3; ++i)
        {
            pos[i] = transform_[i]->value();
            rotDeg[i] = transform_[i + 3]->value();
            scale[i] = transform_[i + 6]->value();
        }
    }

    void RetranslateUi()
    {
        SetTitle(Tr("Трансформация", "Transform"));
        if (labelPosRow_) labelPosRow_->setText(Tr("Позиция", "Position"));
        if (labelRotRow_) labelRotRow_->setText(Tr("Поворот", "Rotation"));
        if (labelScaleRow_) labelScaleRow_->setText(Tr("Масштаб", "Scale"));
        if (btnResetPos_) btnResetPos_->setToolTip(Tr("Сброс позиции (0, 0, 0)", "Reset Position (0, 0, 0)"));
        if (btnResetRot_) btnResetRot_->setToolTip(Tr("Сброс поворота (0, 0, 0)", "Reset Rotation (0, 0, 0)"));
        if (btnResetScale_) btnResetScale_->setToolTip(Tr("Сброс масштаба (1, 1, 1)", "Reset Scale (1, 1, 1)"));
    }

private:
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
        spin->setStyleSheet(QStringLiteral("border-left: none; border-top-left-radius: 0px; border-bottom-left-radius: 0px;"));
        layout->addWidget(spin, 1);
        return cell;
    }

    void BuildCard()
    {
        auto* cl = ContentLayout();
        const QString axisLetters[3] = {QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("Z")};
        const QString axisColors[3] = {QStringLiteral("#EF4444"), QStringLiteral("#22C55E"), QStringLiteral("#3B82F6")};

        const auto makeResetBtn = [this](const QString& toolTip) {
            auto* btn = new QPushButton(QStringLiteral("↺"), Content());
            btn->setFixedSize(22, 22);
            btn->setToolTip(toolTip);

            return btn;
        };

        auto onSpinFinished = [this] {
            if (onChanged_) onChanged_();
        };

        // 1. Position
        auto* rowPos = new QWidget(Content());
        auto* lPos = new QHBoxLayout(rowPos);
        lPos->setContentsMargins(0, 0, 0, 0);
        lPos->setSpacing(4);

        labelPosRow_ = new QLabel(Tr("Позиция", "Position"), rowPos);
        labelPosRow_->setObjectName(QStringLiteral("CardFieldLabel"));
        labelPosRow_->setFixedWidth(56);
        lPos->addWidget(labelPosRow_);

        for (int i = 0; i < 3; ++i)
        {
            auto* spin = transform_[i] = new NoWheelDoubleSpinBox(rowPos);
            spin->setObjectName(QStringLiteral("Transform%1").arg(i));
            spin->setRange(-10000.0, 10000.0);
            spin->setDecimals(3);
            spin->setSingleStep(0.25);
            spin->setKeyboardTracking(false);
            QObject::connect(spin, &QDoubleSpinBox::editingFinished, onSpinFinished);
            lPos->addWidget(CreateAxisCell(spin, axisLetters[i], axisColors[i], rowPos), 1);
        }
        btnResetPos_ = makeResetBtn(Tr("Сброс позиции (0, 0, 0)", "Reset Position (0, 0, 0)"));
        QObject::connect(btnResetPos_, &QPushButton::clicked, [this, onSpinFinished] {
            transform_[0]->setValue(0.0); transform_[1]->setValue(0.0); transform_[2]->setValue(0.0);
            onSpinFinished();
        });
        lPos->addWidget(btnResetPos_);
        cl->addWidget(rowPos);

        // 2. Rotation
        auto* rowRot = new QWidget(Content());
        auto* lRot = new QHBoxLayout(rowRot);
        lRot->setContentsMargins(0, 0, 0, 0);
        lRot->setSpacing(4);

        labelRotRow_ = new QLabel(Tr("Поворот", "Rotation"), rowRot);
        labelRotRow_->setObjectName(QStringLiteral("CardFieldLabel"));
        labelRotRow_->setFixedWidth(56);
        lRot->addWidget(labelRotRow_);

        for (int i = 3; i < 6; ++i)
        {
            auto* spin = transform_[i] = new NoWheelDoubleSpinBox(rowRot);
            spin->setObjectName(QStringLiteral("Transform%1").arg(i));
            spin->setRange(-10000.0, 10000.0);
            spin->setDecimals(2);
            spin->setSingleStep(15.0);
            spin->setKeyboardTracking(false);
            QObject::connect(spin, &QDoubleSpinBox::editingFinished, onSpinFinished);
            lRot->addWidget(CreateAxisCell(spin, axisLetters[i - 3], axisColors[i - 3], rowRot), 1);
        }
        btnResetRot_ = makeResetBtn(Tr("Сброс поворота (0, 0, 0)", "Reset Rotation (0, 0, 0)"));
        QObject::connect(btnResetRot_, &QPushButton::clicked, [this, onSpinFinished] {
            transform_[3]->setValue(0.0); transform_[4]->setValue(0.0); transform_[5]->setValue(0.0);
            onSpinFinished();
        });
        lRot->addWidget(btnResetRot_);
        cl->addWidget(rowRot);

        // 3. Scale
        auto* rowScale = new QWidget(Content());
        auto* lScale = new QHBoxLayout(rowScale);
        lScale->setContentsMargins(0, 0, 0, 0);
        lScale->setSpacing(4);

        labelScaleRow_ = new QLabel(Tr("Масштаб", "Scale"), rowScale);
        labelScaleRow_->setObjectName(QStringLiteral("CardFieldLabel"));
        labelScaleRow_->setFixedWidth(56);
        lScale->addWidget(labelScaleRow_);

        for (int i = 6; i < 9; ++i)
        {
            auto* spin = transform_[i] = new NoWheelDoubleSpinBox(rowScale);
            spin->setObjectName(QStringLiteral("Transform%1").arg(i));
            spin->setRange(0.001, 10000.0);
            spin->setDecimals(3);
            spin->setSingleStep(0.25);
            spin->setKeyboardTracking(false);
            QObject::connect(spin, &QDoubleSpinBox::editingFinished, onSpinFinished);
            lScale->addWidget(CreateAxisCell(spin, axisLetters[i - 6], axisColors[i - 6], rowScale), 1);
        }
        btnResetScale_ = makeResetBtn(Tr("Сброс масштаба (1, 1, 1)", "Reset Scale (1, 1, 1)"));
        QObject::connect(btnResetScale_, &QPushButton::clicked, [this, onSpinFinished] {
            transform_[6]->setValue(1.0); transform_[7]->setValue(1.0); transform_[8]->setValue(1.0);
            onSpinFinished();
        });
        lScale->addWidget(btnResetScale_);
        cl->addWidget(rowScale);
    }

    std::function<void()> onChanged_;
    NoWheelDoubleSpinBox* transform_[9]{};
    QLabel* labelPosRow_ = nullptr;
    QLabel* labelRotRow_ = nullptr;
    QLabel* labelScaleRow_ = nullptr;
    QPushButton* btnResetPos_ = nullptr;
    QPushButton* btnResetRot_ = nullptr;
    QPushButton* btnResetScale_ = nullptr;
};
