#pragma once

#include <QWidget>
#include <QString>
#include <QColor>
#include <array>
#include <cstdint>

#include "QtHost.h"

class QLabel;
class QLineEdit;
class QDoubleSpinBox;
class QGroupBox;
class QCheckBox;
class QPushButton;
class QFormLayout;

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

    void SetEnabled(bool enabled);
    void SetMaterialsAvailable(bool available);

    void Refresh(
        const EditorQtHost::Snapshot& view,
        quint64 selectedId,
        bool selectionChanged
    );

    void ClearFocusIfInside();

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
    void CharacterSpeedEditingFinished(quint64 objectId, double speed);
    void ChooseTextureRequested();
    void ClearTextureRequested();
    void ChooseMaterialRequested();
    void EditMaterialRequested(const QString& virtualPath);
    void ClearMaterialRequested();

private:
    void BuildUi();

    void BuildAssetInspector(QFormLayout* form);

    void UpdateLightColorButton(const float color[3]);

    InspectorTransformValue CurrentTransformValue() const;
    InspectorPointLightValue CurrentPointLightValue() const;

    bool HasFocusedChild() const;

    quint64 selectedId_ = 0;
    bool refreshing_ = false;

    QLabel* selectionLabel_ = nullptr;
    QWidget* characterInspector_ = nullptr;
    QDoubleSpinBox* characterSpeed_ = nullptr;
    QPushButton* chooseModel_ = nullptr;
    QLineEdit* objectName_ = nullptr;
    QLineEdit* modelAsset_ = nullptr;
    QLineEdit* textureAsset_ = nullptr;
    QLineEdit* materialAsset_ = nullptr;
    QWidget* materialActions_ = nullptr;
    QPushButton* editMaterialButton_ = nullptr;

    QDoubleSpinBox* transform_[9]{};

    QWidget* lightInspector_ = nullptr;
    QDoubleSpinBox* lightValues_[5]{}; // RGB, intensity, radius
    QCheckBox* lightEnabled_ = nullptr;
    QPushButton* lightColor_ = nullptr;
};
