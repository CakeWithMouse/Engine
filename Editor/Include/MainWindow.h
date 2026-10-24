#pragma once

#include <QMainWindow>
#include <functional>
#include <cstdint>
struct EditorCommand;
class QTreeWidget;
class QLineEdit;
class QDoubleSpinBox;
class QComboBox;

class QProcess;
class QLabel;
class QPlainTextEdit;
class EditorQtHost;
class FileBrowserDock;
class ViewportWidget;
class QDockWidget;
class QPoint;
class QPushButton;
class QAction;

class EditorMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit EditorMainWindow(EditorQtHost* host, QWidget* parent = nullptr);

    void CommandFinished(const EditorCommand& command, uint32_t status, const QString& error);
    void OpenScene(const QString& path = {});
    void ImportAsset(bool texture, const QString& source = {});
    void AddModel(const QString& path = {});
    void UseAsset(const QString& path);

public slots:
    void AppendLine(const QString& line);

private slots:
    void RefreshStats();
    void ShowViewportContextMenu(const QPoint& globalPos);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void BuildSceneTools();
    void RefreshSceneTools();
    void ApplyTransform();
    void SaveScene(bool saveAs = false, std::function<void()> after = {});
    void MaybeSave(std::function<void()> after);
    void SubmitObjectCommand(int type);
    void BuildUi();
    void BuildToolBar();
    void UpdatePlayButtons();
    void StartExternalGame();
    void StopExternalGame();
    void BuildSceneDocks();
    void ConnectSceneTools();

    QTreeWidget* hierarchy_ = nullptr;
    QWidget* inspector_ = nullptr;
    QWidget* scenePanel_ = nullptr;
    QLineEdit* objectName_ = nullptr;
    QLineEdit* modelAsset_ = nullptr;
    QLineEdit* textureAsset_ = nullptr;
    void BuildAssetInspector(class QFormLayout* form);
    QComboBox* primitive_ = nullptr;
    QDoubleSpinBox* transform_[9]{};
    QDoubleSpinBox* spawn_[3]{};
    QAction* saveAction_ = nullptr;
    QAction* undoAction_ = nullptr;
    QAction* redoAction_ = nullptr;
    QAction* duplicateAction_ = nullptr;
    QAction* deleteAction_ = nullptr;
    QString scenePath_, hierarchyKey_;
    uint64_t selectedId_ = 0;
    bool sceneBusy_ = false, refreshing_ = false, closing_ = false;
    bool checkingChanges_ = false;
    std::function<void()> afterSave_;
    std::function<void()> afterCheck_;

    QToolBar* toolBar_ = nullptr;
    QAction* actPlay_ = nullptr;
    QAction* actPause_ = nullptr;
    QAction* actStop_ = nullptr;
    QAction* actExternal_ = nullptr;

    QProcess* gameProc_ = nullptr;

    QLabel* playStateLabel_ = nullptr;
    EditorQtHost* host_ = nullptr;

    QPlainTextEdit* console_ = nullptr;
    QLabel* statsLabel_ = nullptr;

    QDockWidget* consoleDock_ = nullptr;
    FileBrowserDock* fileDock_ = nullptr;
    ViewportWidget* viewport_ = nullptr;
    QDockWidget* viewportDock_ = nullptr;

    QDockWidget* sceneToolsDock_ = nullptr;
    QDockWidget* inspectorDock_ = nullptr;

    QPushButton* addPrimitiveButton_ = nullptr;
    QPushButton* duplicateObjectButton_ = nullptr;
    QPushButton* removeObjectButton_ = nullptr;

    QAction* removeShortcut_ = nullptr;
};
