#pragma once

#include <QDialog>
#include <QVariantMap>
#include <functional>

class QDoubleSpinBox;
class QPushButton;
class QDialogButtonBox;

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

class ImportSettingsDialog : public QDialog
{
public:
    using ExecHook = std::function<int(ImportSettingsDialog&)>;

    explicit ImportSettingsDialog(QWidget* parent = nullptr);
    ~ImportSettingsDialog() override = default;

    float Scale() const;
    void SetScale(float scale);

    static void SetExecHookForTesting(ExecHook hook);
    static ImportScalePromptResult Prompt(QWidget* parent = nullptr, float initialScale = 1.0f);

private:
    void BuildUi();

    QDoubleSpinBox* spinScale_ = nullptr;
    QPushButton* btnPreset1_ = nullptr;
    QPushButton* btnPreset01_ = nullptr;
    QPushButton* btnPreset001_ = nullptr;
    QDialogButtonBox* buttonBox_ = nullptr;
};

ModelImportScaleDecision ResolveModelImportScale(
    const QVariantMap& parameters,
    bool hasSource,
    QWidget* parent = nullptr);

