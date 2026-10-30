#pragma once

#include <QWidget>
#include <QString>
#include <QColor>
#include <array>
#include <cstdint>

#include "QtHost.h"

class QLabel;
class QLineEdit;
class QCheckBox;
class QPushButton;
class QVBoxLayout;
class NoWheelDoubleSpinBox;
class CollapsibleCard;
class TransformCard;
class MaterialCard;
class GeometryCard;

struct EditorSnapshot;

struct InspectorTransformValue
{
    std::array<double, 3> position{};
    std::array<double, 3> euler{};
    std::array<double, 3> scale{};
};

struct InspectorPointLightValue
{
    std::array<double, 3> color{};
    double intensity = 0.0;
    double radius = 0.0;
    bool enabled = false;
};

class ObjectInspectorPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ObjectInspectorPanel(QWidget* parent = nullptr);
    ~ObjectInspectorPanel() override;

    void SetEnabled(bool enabled);
    void SetMaterialsAvailable(bool available);

    void Refresh(
        const EditorQtHost::Snapshot& view,
        quint64 selectedId,
        bool selectionChanged
    );

    void ClearFocusIfInside();
    void RetranslateUi();
    QSize minimumSizeHint() const override { return QSize(80, 60); }

signals:
    void NameEditingFinished(quint64 objectId, const QString& name);

    void TransformEditingFinished(
        quint64 objectId,
        const InspectorTransformValue& value
    );

    void PointLightEditingFinished(
        quint64 objectId,
        const InspectorPointLightValue& value
    );

    void LightColorRequested(quint64 objectId, const QColor& current);

    void ChooseModelRequested();
    void ClearModelRequested();
    void CharacterSpeedEditingFinished(quint64 objectId, double speed);
    void ChooseTextureRequested();
    void ClearTextureRequested();
    void ChooseMaterialRequested();
    void EditMaterialRequested(const QString& virtualPath);
    void ClearMaterialRequested();

private:
    void BuildUi();
    void BuildGeneralCard(QVBoxLayout* layout);
    void BuildPointLightCard(QVBoxLayout* layout);
    void BuildCharacterCard(QVBoxLayout* layout);

    void UpdateLightColorButton(const float color[3]);
    InspectorTransformValue CurrentTransformValue() const;
    InspectorPointLightValue CurrentPointLightValue() const;
    bool HasFocusedChild() const;

    quint64 selectedId_ = 0;
    bool refreshing_ = false;
    int langSubId_ = 0;

    // Placeholder when no object is selected
    QLabel* noSelectionLabel_ = nullptr;

    // Cards
    CollapsibleCard* cardGeneral_ = nullptr;
    TransformCard* cardTransform_ = nullptr;
    GeometryCard* cardGeometry_ = nullptr;
    MaterialCard* cardMaterial_ = nullptr;
    CollapsibleCard* cardLight_ = nullptr;
    CollapsibleCard* cardCharacter_ = nullptr;

    // General
    QLabel* selectionLabel_ = nullptr;
    QLabel* typeBadgeLabel_ = nullptr;
    QLabel* labelName_ = nullptr;
    QLineEdit* objectName_ = nullptr;

    // Point Light
    QWidget* lightInspector_ = nullptr;
    NoWheelDoubleSpinBox* lightValues_[5]{};
    QCheckBox* lightEnabled_ = nullptr;
    QPushButton* lightColor_ = nullptr;
    QLabel* lightRowLabels_[5]{};

    // Character
    QWidget* characterInspector_ = nullptr;
    NoWheelDoubleSpinBox* characterSpeed_ = nullptr;
    QLabel* characterSpeedLabel_ = nullptr;
};
