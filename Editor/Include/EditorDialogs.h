#pragma once

#include <QString>
#include <QWidget>
#include <QDialog>
#include <functional>
#include <vector>

enum class ControlScheme
{
    UnrealUnity,
    Blender
};

enum class SpawnLocationMode
{
    UnderCursor,
    WorldOrigin
};

class EditorSettings
{
public:
    static ControlScheme GetControlScheme();
    static void SetControlScheme(ControlScheme scheme);

    static SpawnLocationMode GetSpawnLocationMode();
    static void SetSpawnLocationMode(SpawnLocationMode mode);

    static int SubscribeControlSchemeChanged(std::function<void(ControlScheme)> callback);
    static void UnsubscribeControlSchemeChanged(int id);
};

void ShowDocumentationDialog(QWidget* parent, const QString& projectRoot);

struct AssetImportSettings
{
    QString sourcePath;
    QString destinationDir;
    double scaleFactor = 1.0;
    int upAxis = 0; // 0: Z-Up (Convert to Y-Up), 1: Y-Up
    bool isSkeletalMesh = false;
    bool combineMeshes = true;
    bool generateNormalsTangents = true;
    bool applyModifiers = true;
    bool importEmbeddedTextures = true;
    bool autoCreatePbrMaterial = true;
    bool spawnInScene = true;
    QString blenderPath;
};

class QDoubleSpinBox;
class QComboBox;
class QCheckBox;
class QLabel;
class QPushButton;

class AssetImportDialog : public QDialog
{
public:
    explicit AssetImportDialog(const QString& sourcePath, const QString& destinationDir, QWidget* parent = nullptr);
    AssetImportSettings GetSettings() const;

private:
    void BuildUi();
    void DetectBlender();

    QString sourcePath_;
    QString destinationDir_;
    QString detectedBlenderPath_;
    bool isBlenderFound_ = false;

    QLabel* lblSourcePath_ = nullptr;
    QLabel* lblDetectedType_ = nullptr;
    QLabel* lblBlenderStatus_ = nullptr;
    QPushButton* btnBrowseBlender_ = nullptr;

    QDoubleSpinBox* spinScale_ = nullptr;
    QComboBox* comboUpAxis_ = nullptr;
    QComboBox* comboMeshType_ = nullptr;
    QCheckBox* chkCombineMeshes_ = nullptr;
    QCheckBox* chkNormalsTangents_ = nullptr;
    QCheckBox* chkApplyModifiers_ = nullptr;
    QCheckBox* chkImportTextures_ = nullptr;
    QCheckBox* chkAutoPbrMat_ = nullptr;
    QCheckBox* chkSpawnInScene_ = nullptr;
};
