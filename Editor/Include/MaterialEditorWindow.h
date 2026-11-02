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
    bool ConfirmClose(std::function<void()> afterSave = {});
    // Отпускает превью в потоке движка и удаляет окно; правки, от которых отказались, откатываются.
    void Shutdown();

    // Ctrl+S queues the engine save; completion reports whether both files were saved.
    bool Save(std::function<void(bool)> completion = {});
    void Revert();
    // Перечитывает состояние материала из движка (после F6): ошибки и параметры, текст HLSL не трогает.
    void Refresh();

    // file: файл на диске или /Game путь, пустой — белая текстура. Файл вне Content импортируется.
    void AssignTexture(const QString& slot, const QString& file);
    // Слот — это правка HLSL: файл записывается и компилируется сразу, .mat остаётся на «Сохранить».
    void AddTextureSlot(const QString& name, int role, const QString& file);
    void RemoveTextureSlot(const QString& name);

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
    void PickTexture(const QString& slot);
    void RequestAddTexture();
    void RequestRemoveTexture(const QString& slot);
    void GoToError(int line, int column);

    bool WriteSource();
    // Заменяет текст через курсор: Ctrl+Z работает, позиция прокрутки сохраняется.
    void ReplaceSource(const QString& text);

    QString PhysicalPath(const QString& virtualPath) const;
    // /Game путь файла; пустой, если файл вне Content.
    QString VirtualPath(const QString& file) const;

    EditorQtHost* host_ = nullptr;
    QString material_, shaderFile_;
    std::shared_ptr<EngineSide> engine_ = std::make_shared<EngineSide>();
    MaterialEditorState state_;
    bool loaded_ = false, discard_ = false, shuttingDown_ = false;
    bool saving_ = false;
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
