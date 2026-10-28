#pragma once

#include <QMainWindow>
#include <QHash>
#include <QPointer>
#include <functional>
#include <cstdint>

#include "ObjectInspectorPanel.h"

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
class SceneObjectsPanel;
class MaterialEditorWindow;

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

    // path: файл .mat или его /Game путь. Одно окно на материал.
    void OpenMaterialEditor(const QString& path);
    // Создаёт .mat и .hlsl из шаблона в каталоге проекта и открывает редактор; без имени спрашивает его.
    QString CreateMaterial(const QString& directory, const QString& name = {});
    // Для объекта с материалом по умолчанию: новый материал в Content/Materials, назначение и редактор.
    void CreateMaterialForSelection();
    // F6: перекомпилировать все шейдеры и материалы с диска во всех сценах и превью.
    void ReloadShaders();

public slots:
    void AppendLine(const QString& line);

private slots:
    void RefreshStats();
    void ShowViewportContextMenu(const QPoint& globalPos);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void BuildUi();
    void BuildToolBar();
    void UpdatePlayButtons();

    void StartExternalGame();
    void StopExternalGame();

    void BuildSceneDocks();
    void BuildSceneMenu();
    void ConnectPanels();

    void RefreshSceneUi();

    void SaveScene(bool saveAs = false, std::function<void()> after = {});
    void MaybeSave(std::function<void()> after);
    void SubmitObjectCommand(int type);

    void OnTransformEdited(quint64 objectId, const InspectorTransformValue& value);
    void OnPointLightEdited(quint64 objectId, const InspectorPointLightValue& value);
    void OnLightColorRequested(quint64 objectId, const QColor& current);

    // Спрашивает о несохранённых материалах и закрывает их окна; false — закрытие отменено.
    bool CloseMaterialEditors();
    QHash<QString, QPointer<MaterialEditorWindow>> materialEditors_;

    EditorQtHost* host_ = nullptr;

    ViewportWidget* viewport_ = nullptr;
    QDockWidget* viewportDock_ = nullptr;

    QPlainTextEdit* console_ = nullptr;
    QDockWidget* consoleDock_ = nullptr;

    FileBrowserDock* fileDock_ = nullptr;

    QDockWidget* sceneObjectsDock_ = nullptr;
    QDockWidget* inspectorDock_ = nullptr;

    SceneObjectsPanel* sceneObjectsPanel_ = nullptr;
    ObjectInspectorPanel* objectInspectorPanel_ = nullptr;

    QToolBar* toolBar_ = nullptr;

    QAction* actPlay_ = nullptr;
    QAction* actPause_ = nullptr;
    QAction* actStop_ = nullptr;
    QAction* actExternal_ = nullptr;

    QAction* saveAction_ = nullptr;
    QAction* undoAction_ = nullptr;
    QAction* redoAction_ = nullptr;
    QAction* duplicateAction_ = nullptr;
    QAction* deleteAction_ = nullptr;

    QProcess* gameProc_ = nullptr;

    QLabel* playStateLabel_ = nullptr;
    QLabel* statsLabel_ = nullptr;

    QString scenePath_;

    uint64_t selectedId_ = 0;

    bool sceneBusy_ = false;
    bool closing_ = false;
    bool checkingChanges_ = false;

    std::function<void()> afterSave_;
    std::function<void()> afterCheck_;
};
