#pragma once

#include <QDialog>

class QDoubleSpinBox;
class QPushButton;
class QDialogButtonBox;

class ImportSettingsDialog : public QDialog
{
public:
    explicit ImportSettingsDialog(QWidget* parent = nullptr);
    ~ImportSettingsDialog() override = default;

    float Scale() const;
    void SetScale(float scale);

private:
    void BuildUi();

    QDoubleSpinBox* spinScale_ = nullptr;
    QPushButton* btnPreset1_ = nullptr;
    QPushButton* btnPreset01_ = nullptr;
    QPushButton* btnPreset001_ = nullptr;
    QDialogButtonBox* buttonBox_ = nullptr;
};
