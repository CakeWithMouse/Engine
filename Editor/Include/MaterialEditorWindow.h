#pragma once

#include <QMainWindow>
#include <QString>
#include <QVector>
#include <array>
#include <memory>

#include "QtHost.h"

class QComboBox;
class QCheckBox;
class QDoubleSpinBox;
class QFormLayout;
class QListWidget;
class QWidget;
class MaterialPreviewWidget;
class MaterialSourceEditor;

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

// Отдельное окно на один .mat: превью, текст поверхности (HLSL), настройки и ошибки компиляции.
// Все вызовы движка идут через EditorQtHost в поток движка, ответы возвращаются в поток UI.
class MaterialEditorWindow : public QMainWindow
{
    Q_OBJECT

public:
    // material: /Game путь .mat
    MaterialEditorWindow(EditorQtHost* host, const QString& material, QWidget* parent = nullptr);

    const QString& Material() const { return material_; }
    bool HasUnsavedChanges() const;

    // Спрашивает о несохранённых изменениях; false — пользователь продолжает редактирование.
    bool ConfirmClose();
    // Отпускает превью в потоке движка и удаляет окно; правки, от которых отказались, откатываются.
    void Shutdown();

    // Ctrl+S: записывает HLSL, перекомпилирует его и сохраняет .mat. false — файл не записан.
    bool Save();
    void Revert();

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
    void EngineThenRead(std::function<QString(const EngineEditSessionApi&)> task, bool rebuild);
    void Apply(const MaterialEditorState& state, bool valid, bool rebuild, const QString& error);
    void RebuildSettings();
    void ShowErrors();
    void LoadShaderText();
    void UpdateTitle();
    void SendRenderState();
    void SetParameter(int index, const std::array<float, 4>& value);
    void ChooseTexture(int index, bool clear);
    void GoToError(int line, int column);

    QString PhysicalPath(const QString& virtualPath) const;

    EditorQtHost* host_ = nullptr;
    QString material_, shaderFile_;
    std::shared_ptr<EngineSide> engine_ = std::make_shared<EngineSide>();
    MaterialEditorState state_;
    bool loaded_ = false, discard_ = false, shuttingDown_ = false;
    bool edited_ = false; // правка отправлена, а ответ движка с флагом modified ещё не пришёл

    MaterialPreviewWidget* preview_ = nullptr;
    QComboBox* shape_ = nullptr;
    MaterialSourceEditor* source_ = nullptr;
    QListWidget* errors_ = nullptr;

    QComboBox* blend_ = nullptr;
    QDoubleSpinBox* alphaClip_ = nullptr;
    QCheckBox* unlit_ = nullptr;
    QCheckBox* twoSided_ = nullptr;
    QCheckBox* castShadows_ = nullptr;
    QWidget* parameters_ = nullptr;
    QFormLayout* parameterForm_ = nullptr;
};
