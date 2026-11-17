#pragma once

#include <QDialog>
#include <cstdint>
#include <string>
#include <vector>

class QComboBox;
class QCheckBox;
class QDoubleSpinBox;

struct PlaySettingsObjectView
{
    uint64_t id;
    uint32_t gameplayKind;
    std::string name;
    std::string projectData;
};

class PlaySettingsDialog : public QDialog
{
public:
    explicit PlaySettingsDialog(const std::vector<PlaySettingsObjectView>& objects, QWidget* parent = nullptr);

    bool Execute(uint64_t& outSettingsId, QString& outConfigJson);

private:
    void BuildUi();

    const std::vector<PlaySettingsObjectView>& objects_;
    QComboBox* character_{nullptr};
    QComboBox* camera_{nullptr};
    QCheckBox* standard_{nullptr};
    QDoubleSpinBox* distance_{nullptr};
    QDoubleSpinBox* height_{nullptr};
    QDoubleSpinBox* sensitivity_{nullptr};
};
