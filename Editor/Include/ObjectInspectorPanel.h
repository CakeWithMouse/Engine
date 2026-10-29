#pragma once

#include <QWidget>
#include <QString>
#include <QColor>
#include <array>
#include <cstdint>
#include <string>

#include "QtHost.h"

class QLabel;
class QLineEdit;
class QDoubleSpinBox;
class QGroupBox;
class QCheckBox;
class QPushButton;
class QFormLayout;
class QSlider;
class QComboBox;
class QVBoxLayout;
class CollapsibleCard;

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
    void ClearModelRequested(quint64 objectId);
    void CharacterSpeedEditingFinished(quint64 objectId, double speed);
    void ChooseTextureRequested();
    void ClearTextureRequested();
    void ChooseMaterialRequested();
    void EditMaterialRequested(const QString& virtualPath);
    void ClearMaterialRequested();

    // Quick actions
    void FocusRequested(quint64 objectId);
    void DuplicateRequested(quint64 objectId);
    void DeleteRequested(quint64 objectId);

private:
    void BuildUi();
    void BuildHeaderCard(QVBoxLayout* layout);
    void BuildTransformCard(QVBoxLayout* layout);
    void BuildGeometryCard(QVBoxLayout* layout);
    void BuildMaterialCard(QVBoxLayout* layout);
    void BuildPointLightCard(QVBoxLayout* layout);
    void BuildCharacterCard(QVBoxLayout* layout);

    void UpdateLightColorButton(const float color[3]);

    InspectorTransformValue CurrentTransformValue() const;
    InspectorPointLightValue CurrentPointLightValue() const;

    bool HasFocusedChild() const;
    void UpdatePrimitiveParamsFromScale(const std::string& type, const float scale[3]);
    void ApplyPrimitiveParamsToScale(const std::string& type);

    quint64 selectedId_ = 0;
    std::string selectedType_;
    bool refreshing_ = false;
    bool primitiveUpdating_ = false;

    // Preserved widgets & pointers for compatibility
    QLabel* selectionLabel_ = nullptr;
    QLabel* typeBadgeLabel_ = nullptr;
    QPushButton* btnQuickFocus_ = nullptr;
    QPushButton* btnQuickDuplicate_ = nullptr;
    QPushButton* btnQuickDelete_ = nullptr;

    QWidget* characterInspector_ = nullptr;
    QDoubleSpinBox* characterSpeed_ = nullptr;
    QSlider* characterSpeedSlider_ = nullptr;
    QPushButton* chooseModel_ = nullptr;
    QPushButton* clearModel_ = nullptr;
    QLineEdit* objectName_ = nullptr;
    QLineEdit* modelAsset_ = nullptr;
    QLineEdit* textureAsset_ = nullptr;
    QLineEdit* materialAsset_ = nullptr;
    QWidget* materialActions_ = nullptr;
    QPushButton* editMaterialButton_ = nullptr;
    QPushButton* chooseMaterialButton_ = nullptr;
    QPushButton* clearMaterialButton_ = nullptr;
    QPushButton* chooseTextureButton_ = nullptr;
    QPushButton* clearTextureButton_ = nullptr;

    QGroupBox* generalBox_ = nullptr;
    QGroupBox* posBox_ = nullptr;
    QGroupBox* rotBox_ = nullptr;
    QGroupBox* scaleBox_ = nullptr;
    QGroupBox* visualBox_ = nullptr;
    QLabel* nameRowLabel_ = nullptr;
    QLabel* characterSpeedLabel_ = nullptr;
    QLabel* modelRowLabel_ = nullptr;
    QLabel* textureRowLabel_ = nullptr;
    QLabel* materialRowLabel_ = nullptr;
    QLabel* lightRowLabels_[5]{};

    QDoubleSpinBox* transform_[9]{};
    QPushButton* btnResetPos_ = nullptr;
    QPushButton* btnResetRot_ = nullptr;
    QPushButton* btnResetScale_ = nullptr;
    QLabel* labelPosRow_ = nullptr;
    QLabel* labelRotRow_ = nullptr;
    QLabel* labelScaleRow_ = nullptr;

    // Collapsible Cards
    CollapsibleCard* cardHeader_ = nullptr;
    CollapsibleCard* cardTransform_ = nullptr;
    CollapsibleCard* cardGeometry_ = nullptr;
    CollapsibleCard* cardMaterial_ = nullptr;
    CollapsibleCard* cardLight_ = nullptr;
    CollapsibleCard* cardCharacter_ = nullptr;

    // Primitive parameters (3ds Max style)
    QWidget* primitiveParamsGroup_ = nullptr;
    QWidget* meshParamsGroup_ = nullptr;
    QWidget* skeletalParamsGroup_ = nullptr;
    QLabel* lblPrimParamsTitle_ = nullptr;
    QWidget* rowPrimRadius_ = nullptr;
    QWidget* rowPrimHeight_ = nullptr;
    QWidget* rowPrimWidth_ = nullptr;
    QWidget* rowPrimDepth_ = nullptr;
    QLabel* lblPrimRadius_ = nullptr;
    QLabel* lblPrimHeight_ = nullptr;
    QLabel* lblPrimWidth_ = nullptr;
    QLabel* lblPrimDepth_ = nullptr;
    QDoubleSpinBox* spinPrimRadius_ = nullptr;
    QDoubleSpinBox* spinPrimHeight_ = nullptr;
    QDoubleSpinBox* spinPrimWidth_ = nullptr;
    QDoubleSpinBox* spinPrimDepth_ = nullptr;

    // Skeletal & Animation
    QLabel* lblSkeletonTitle_ = nullptr;
    QLabel* lblSkeletonStatus_ = nullptr;
    QLabel* lblAnimClip_ = nullptr;
    QComboBox* comboAnimationClip_ = nullptr;
    QLabel* lblAnimSpeed_ = nullptr;
    QDoubleSpinBox* spinPlaybackSpeed_ = nullptr;
    QCheckBox* chkAnimationLoop_ = nullptr;

    // Light
    QWidget* lightInspector_ = nullptr;
    QDoubleSpinBox* lightValues_[5]{}; // RGB, intensity, radius
    QSlider* lightIntensitySlider_ = nullptr;
    QSlider* lightRadiusSlider_ = nullptr;
    QCheckBox* lightEnabled_ = nullptr;
    QPushButton* lightColor_ = nullptr;

    int langSubId_ = 0;
};
