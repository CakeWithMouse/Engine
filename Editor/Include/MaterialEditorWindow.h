#pragma once

#include <QMainWindow>
#include <QString>
#include <QVector>
#include <array>
#include <memory>
#include <functional>

#include "QtHost.h"

class QComboBox;
class QCheckBox;
class QDoubleSpinBox;
class QFormLayout;
class QListWidget;
class QWidget;
class QLabel;
class QAction;
class QPushButton;
class QToolButton;
class QTabWidget;
class MaterialPreviewWidget;
class MaterialSourceEditor;
class MaterialGraphWidget;

// Состояние материала в том виде, в каком его вернул readMaterial (копия для потока UI).
struct MaterialEditorState
{
    struct Parameter
    {
        QString name;
        uint32_t components = 0;
        std::array<float, 4> value{}, defaultValue{};
    };
    struct Texture
    {
        QString name, path;
    };

    QString shader, errors;
    EngineMaterialRenderState render{};
    QVector<Parameter> parameters;
    QVector<Texture> textures;
    bool modified = false;
};

// Отдельное окно на один .mat: превью, нодовый граф + AI, HLSL код, PBR/ORM настройки и диагностика.
class MaterialEditorWindow : public QMainWindow
{
    Q_OBJECT

public:
    // material: /Game путь .mat
    MaterialEditorWindow(EditorQtHost* host, const QString& material, QWidget* parent = nullptr);
    ~MaterialEditorWindow() override;

    const QString& Material() const { return material_; }
    bool HasUnsavedChanges() const;

    // Спрашивает о несохранённых изменениях; false — пользователь продолжает редактирование.
    bool ConfirmClose(std::function<void()> afterSave = {});
    // Отпускает превью в потоке движка и удаляет окно; правки, от которых отказались, откатываются.
    void Shutdown();

    // Ctrl+S queues the engine save; completion reports whether both files were saved.
    bool Save(std::function<void(bool)> completion = {});
    void Revert();
    // Перечитывает состояние материала из движка (после F6): ошибки и параметры, текст HLSL не трогает.
    void Refresh();
    void RetranslateUi();

    void ApplyPbrOrmPreset(int presetId);
    void CreateMaterialInstance();
    void InsertOrmSamplingIntoHlsl();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    struct EngineSide
    {
        uint64_t preview = 0; // только в потоке движка
    };

    void BuildUi();
    void Engine(std::function<void(const EngineEditSessionApi&)> task);
    // Читает материал после task и показывает результат; rebuild пересоздаёт список параметров.
    void EngineThenRead(std::function<QString(const EngineEditSessionApi&)> task, bool rebuild,
        std::function<void(bool)> completion = {});
    void Apply(const MaterialEditorState& state, bool valid, bool rebuild, const QString& error);
    void RebuildSettings();
    void ShowErrors();
    void LoadShaderText();
    void UpdateTitle();
    void SendRenderState();
    void SetParameter(int index, const std::array<float, 4>& value);
    void ChooseTexture(int index, bool clear);
    void GoToError(int line, int column);
    void UpdateCompileStatus(bool success, int errorCount = 0);

    QString PhysicalPath(const QString& virtualPath) const;

    EditorQtHost* host_ = nullptr;
    QString material_, shaderFile_;
    std::shared_ptr<EngineSide> engine_ = std::make_shared<EngineSide>();
    MaterialEditorState state_;
    bool loaded_ = false, discard_ = false, shuttingDown_ = false;
    bool saving_ = false;
    bool edited_ = false; // правка отправлена, а ответ движка с флагом modified ещё не пришёл

    // Viewport & Preview
    MaterialPreviewWidget* preview_ = nullptr;
    QComboBox* shape_ = nullptr;
    QPushButton* btnResetView_ = nullptr;
    QLabel* shapeLabel_ = nullptr;

    // Central Tab Area: Node Graph & HLSL Source
    QTabWidget* mainTabs_ = nullptr;
    MaterialGraphWidget* graphWidget_ = nullptr;
    MaterialSourceEditor* source_ = nullptr;

    // Diagnostics
    QLabel* lblCompileStatus_ = nullptr;
    QListWidget* errors_ = nullptr;

    // Surface & Blend State
    QComboBox* blend_ = nullptr;
    QDoubleSpinBox* alphaClip_ = nullptr;
    QCheckBox* unlit_ = nullptr;
    QCheckBox* twoSided_ = nullptr;
    QCheckBox* castShadows_ = nullptr;
    QLabel* blendLabel_ = nullptr;
    QLabel* alphaClipLabel_ = nullptr;

    // PBR & ORM Quick Channels
    QWidget* pbrChannelsCard_ = nullptr;
    QPushButton* btnInsertOrm_ = nullptr;

    // Shader Parameters (Tweakables)
    QWidget* parameters_ = nullptr;
    QFormLayout* parameterForm_ = nullptr;
    QLabel* parametersTitle_ = nullptr;

    // Toolbar actions
    QAction* saveAction_ = nullptr;
    QAction* revertAction_ = nullptr;
    QToolButton* btnPresets_ = nullptr;
    QAction* actPresetPbrOrm_ = nullptr;
    QAction* actPresetUnlit_ = nullptr;
    QAction* actPresetLava_ = nullptr;
    QAction* actPresetHolo_ = nullptr;
    QAction* actPresetFoliage_ = nullptr;
    QAction* actCreateInstance_ = nullptr;

    int langSubId_ = 0;
};
