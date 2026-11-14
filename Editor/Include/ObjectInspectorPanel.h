#pragma once

#include <QWidget>
#include <QString>
#include <QColor>
#include <QJsonObject>
#include <array>
#include <cstdint>

#include "QtHost.h"

class QLabel;
class QLineEdit;
class QCheckBox;
class QComboBox;
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
    bool castShadows = false;
};

// Rigid body settings of one scene object. bodyType/shapeType follow the engine
// constants (0/1/2), the combos keep the same order.
struct InspectorPhysicsValue
{
    bool enabled = false;
    int bodyType = 1;   // 0 static, 1 dynamic, 2 kinematic
    int shapeType = 0;  // 0 box, 1 sphere, 2 capsule
    double friction = 0.5;
    double restitution = 0.0;
    double density = 1000.0;
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
    void PropertyEditingFinished(quint64 objectId, const QString& path, const QString& value);
    void NameEditingFinished(quint64 objectId, const QString& name);

    void TransformEditingFinished(
        quint64 objectId,
        const InspectorTransformValue& value
    );

    void PointLightEditingFinished(
        quint64 objectId,
        const InspectorPointLightValue& value
    );

    void PhysicsEditingFinished(
        quint64 objectId,
        const InspectorPhysicsValue& value
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
    void RefreshProjectProperties(const EditorQtHost::Snapshot&, const EditorQtHost::SceneObject&);
    void SubmitProperty(const QString& path, const QJsonValue& value);
    QWidget* properties_ = nullptr;
    QVBoxLayout* propertiesLayout_ = nullptr;
    QString propertiesKey_;
    std::vector<std::function<void(const QJsonObject&)>> propertyRefresh_;
    void BuildGeneralCard(QVBoxLayout* layout);
    void BuildPointLightCard(QVBoxLayout* layout);
    void BuildCharacterCard(QVBoxLayout* layout);
    void BuildPhysicsCard(QVBoxLayout* layout);

    void UpdateLightColorButton(const float color[3]);
    InspectorTransformValue CurrentTransformValue() const;
    InspectorPointLightValue CurrentPointLightValue() const;
    InspectorPhysicsValue CurrentPhysicsValue() const;
    bool HasFocusedChild() const;

    quint64 selectedId_ = 0; std::string selectedType_;
    EditorQtHost::Snapshot lastSnapshot_;
    bool refreshing_ = false;
    int langSubId_ = 0;
    int themeSubId_ = 0;
    QMap<QString, QJsonValue> pendingProperties_;

    // Placeholder when no object is selected
    QLabel* noSelectionLabel_ = nullptr;

    // Cards
    CollapsibleCard* cardGeneral_ = nullptr;
    TransformCard* cardTransform_ = nullptr;
    GeometryCard* cardGeometry_ = nullptr;
    MaterialCard* cardMaterial_ = nullptr;
    CollapsibleCard* cardLight_ = nullptr;
    CollapsibleCard* cardCharacter_ = nullptr;
    CollapsibleCard* cardPhysics_ = nullptr;

    // General
    QLabel* selectionLabel_ = nullptr;
    QLabel* typeBadgeLabel_ = nullptr;
    QLabel* labelName_ = nullptr;
    QLineEdit* objectName_ = nullptr;

    // Point Light
    QWidget* lightInspector_ = nullptr;
    NoWheelDoubleSpinBox* lightValues_[5]{};
    QCheckBox* lightEnabled_ = nullptr;
    QCheckBox* lightShadows_ = nullptr;
    QPushButton* lightColor_ = nullptr;
    QLabel* lightRowLabels_[5]{};

    // Character
    QWidget* characterInspector_ = nullptr;
    NoWheelDoubleSpinBox* characterSpeed_ = nullptr;
    QLabel* characterSpeedLabel_ = nullptr;

    // Physics
    QWidget* physicsInspector_ = nullptr;
    QCheckBox* physicsEnabled_ = nullptr;
    QComboBox* physicsBodyType_ = nullptr;
    QComboBox* physicsShape_ = nullptr;
    NoWheelDoubleSpinBox* physicsFriction_ = nullptr;
    NoWheelDoubleSpinBox* physicsRestitution_ = nullptr;
    NoWheelDoubleSpinBox* physicsDensity_ = nullptr;
    QLabel* physicsRowLabels_[5]{};
    QLabel* physicsEnabledNote_ = nullptr;
};
