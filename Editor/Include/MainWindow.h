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
class QMenu;
class SceneObjectsPanel;
class MaterialEditorWindow;

class EditorMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit EditorMainWindow(EditorQtHost* host, QWidget* parent = nullptr);
    ~EditorMainWindow() override;

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
    void RetranslateUi();
    void SpawnObjectByType(const QString& typeName, const QString& typeLabel);
    void RebuildAddMenu();

public slots:
    void AppendLine(const QString& line);

private slots:
    void RefreshStats();
    void ShowViewportContextMenu(const QPoint& globalPos);
    void ShowViewportAddMenu(const QPoint& globalPos);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void BuildCodeTools();
    void RunCodeTool(const QString& action, const QStringList& extra = {});
    void ShowProjectTypes();
    void BuildUi();
    void BuildToolBar();
    void UpdatePlayButtons();

    void StartExternalGame();
    void StopExternalGame();

    void BuildSceneDocks();
    void BuildSceneMenu();
    void ConnectPanels();
    QMenu* BuildAddObjectMenu(QWidget* parent, const std::array<double, 3>& spawnPos);

    void ApplyDefaultLayout();
    void ApplySavedOrInitialLayout();

    void RefreshSceneUi();

    void SaveScene(bool saveAs = false, std::function<void()> after = {});
    void MaybeSave(std::function<void()> after);
    void SubmitObjectCommand(int type);

    void OnTransformEdited(quint64 objectId, const InspectorTransformValue& value);
    void OnPointLightEdited(quint64 objectId, const InspectorPointLightValue& value);
    void OnLightColorRequested(quint64 objectId, const QColor& current);

    // Спрашивает о несохранённых материалах и закрывает их окна; false — закрытие отменено.
    bool CloseMaterialEditors(std::function<void()> afterSave = {});
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

    QAction* actNewScene_ = nullptr;
    QAction* actOpenScene_ = nullptr;
    QAction* actSaveAs_ = nullptr;
    QAction* actAddModel_ = nullptr;
    QAction* actImportModel_ = nullptr;
    QAction* actImportTexture_ = nullptr;
    QAction* actReloadShaders_ = nullptr;
    QAction* actCloseEditor_ = nullptr;

    QAction* actServiceObjects_ = nullptr;
    QAction* actConfigurePlay_ = nullptr;
    QAction* actLangRu_ = nullptr;
    QAction* actLangEn_ = nullptr;
    QAction* actResetLayout_ = nullptr;

    QMenu* fileMenu_ = nullptr;
    QMenu* resourcesMenu_ = nullptr;
    QMenu* editMenu_ = nullptr;
    QMenu* controlSchemeMenu_ = nullptr;
    QAction* actSchemeUnrealUnity_ = nullptr;
    QAction* actSchemeBlender_ = nullptr;
    QActionGroup* schemeGroup_ = nullptr;
    QActionGroup* spawnLocationGroup_ = nullptr;
    QAction* actSpawnUnderCursor_ = nullptr;
    QAction* actSpawnWorldOrigin_ = nullptr;
    QMenu* addMenu_ = nullptr;
    QMenu* helpMenu_ = nullptr;
    QAction* actDoc_ = nullptr;
    QComboBox* cameraModeCombo_ = nullptr;
    QMenu* sceneMenu_ = nullptr;
    QMenu* playMenu_ = nullptr;
    QMenu* viewMenu_ = nullptr;
    QMenu* projectMenu_ = nullptr;
    QMenu* cppMenu_ = nullptr;
    QMenu* langMenu_ = nullptr;
    QMenu* themeMenu_ = nullptr;
    QAction* actThemeDark_ = nullptr;
    QAction* actThemeLight_ = nullptr;
    QMenu* accentMenu_ = nullptr;
    QAction* actAccentMaxRed_ = nullptr;
    QAction* actAccentBlenderOrange_ = nullptr;
    QAction* actAccentUnrealBlue_ = nullptr;
    QAction* actAccentUnityCyan_ = nullptr;
    QAction* actAccentCustom_ = nullptr;
    int themeSubId_ = 0;

    QPushButton* langButton_ = nullptr;
    int langSubId_ = 0;

    QProcess* gameProc_ = nullptr;

    QLabel* playStateLabel_ = nullptr;
    QLabel* statsLabel_ = nullptr;

    QString scenePath_;
    QByteArray reloadTypesBefore_, reloadTypesBuilt_;
    bool reloadTypesExisted_ = false;

    uint64_t selectedId_ = 0;

    bool sceneBusy_ = false;
    bool closing_ = false;
    bool checkingChanges_ = false;

    std::function<void()> afterSave_;
    std::function<void()> afterCheck_;
};
