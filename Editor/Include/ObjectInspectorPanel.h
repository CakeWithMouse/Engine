#pragma once

#include <QWidget>
#include <QString>
#include <QColor>
#include <array>
#include <cstdint>

#include "QtHost.h"

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

    void ChooseTextureRequested();
    void ClearTextureRequested();

private:
    void BuildUi();

    // Если у вас уже есть собственная реализация asset-инспектора,
    // перенесите её сюда из MainWindow.
    void BuildAssetInspector(QFormLayout* form);

    void UpdateLightColorButton(const float color[3]);

    InspectorTransformValue CurrentTransformValue() const;
    InspectorPointLightValue CurrentPointLightValue() const;

    bool HasFocusedChild() const;

    quint64 selectedId_ = 0;
    bool refreshing_ = false;

    QLineEdit* objectName_ = nullptr;
    QLineEdit* modelAsset_ = nullptr;
    QLineEdit* textureAsset_ = nullptr;

    QDoubleSpinBox* transform_[9]{};

    QWidget* lightInspector_ = nullptr;
    QDoubleSpinBox* lightValues_[5]{}; // RGB, intensity, radius
    QCheckBox* lightEnabled_ = nullptr;
    QPushButton* lightColor_ = nullptr;
};
