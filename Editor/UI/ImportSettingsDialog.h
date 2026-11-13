#pragma once

#include "ImportSettings.h"
#include <QDialog>
#include <QVariantMap>
#include <functional>
#include <optional>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QDialogButtonBox;
class CollapsibleCard;

struct ImportScalePromptResult
{
    bool accepted = false;
    float scale = 1.0f;
};

struct ModelImportScaleDecision
{
    bool proceed = false;
    float scale = 1.0f;
};

struct ModelImportDecision
{
    bool proceed = false;
    EditorCore::ImportSettings settings;
    float scale = 1.0f;
};

class ImportSettingsDialog : public QDialog
{
public:
    using ExecHook = std::function<int(ImportSettingsDialog&)>;

    explicit ImportSettingsDialog(QWidget* parent = nullptr);
    ~ImportSettingsDialog() override = default;

    EditorCore::ImportSettings Settings() const;
    void SetSettings(const EditorCore::ImportSettings& settings);

    float Scale() const;
    void SetScale(float scale);

    void SetEditMode(bool editMode);
    bool IsEditMode() const { return isEditMode_; }

    void ResetToDefaults();

    int exec() override;
    void accept() override;

    static void SetExecHookForTesting(ExecHook hook);
    static EditorCore::ImportSettings DefaultSettings();
    static ModelImportDecision Prompt(QWidget* parent = nullptr, const std::optional<EditorCore::ImportSettings>& initialSettings = std::nullopt);
    static ImportScalePromptResult PromptScale(QWidget* parent = nullptr, float initialScale = 1.0f);

private:
    void BuildUi();
    void SetupConnections();
    void UpdateDependencies();
    void LoadFromSettingsService();
    void SaveToSettingsService();

    // Basic group
    QDoubleSpinBox* spinScale_ = nullptr;
    QPushButton* btnPreset1_ = nullptr;
    QPushButton* btnPreset01_ = nullptr;
    QPushButton* btnPreset001_ = nullptr;
    QCheckBox* checkUseFileUnitScale_ = nullptr;
    QCheckBox* checkImportMaterials_ = nullptr;
    QCheckBox* checkImportTextures_ = nullptr;

    // Advanced group (collapsible, collapsed by default)
    CollapsibleCard* advancedCard_ = nullptr;
    QComboBox* comboNormals_ = nullptr;
    QLabel* labelSmoothingAngle_ = nullptr;
    QDoubleSpinBox* spinSmoothingAngle_ = nullptr;
    QCheckBox* checkFlipUv_ = nullptr;
    QCheckBox* checkMergeMeshes_ = nullptr;

    // Action buttons
    QPushButton* btnResetDefaults_ = nullptr;
    QDialogButtonBox* buttonBox_ = nullptr;
    bool isEditMode_ = false;
};

ModelImportDecision ResolveModelImportDecision(
    const QVariantMap& parameters,
    bool hasSource,
    QWidget* parent = nullptr);

inline ModelImportScaleDecision ResolveModelImportScale(
    const QVariantMap& parameters,
    bool hasSource,
    QWidget* parent = nullptr)
{
    const auto d = ResolveModelImportDecision(parameters, hasSource, parent);
    return { d.proceed, d.scale };
}
