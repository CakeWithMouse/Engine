#pragma once

#include <QDialog>
#include <cstdint>
#include <string>
#include <vector>

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
    const std::vector<PlaySettingsObjectView>& objects_;
};
