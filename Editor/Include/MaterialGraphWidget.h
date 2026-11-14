#pragma once

#include "GraphEditorWidget.h"
#include "MaterialGraphModel.h"

#include <QHash>
#include <QPointF>
#include <QString>
#include <array>
#include <functional>

// Нодовый редактор материала на общем GraphEditorWidget: схема MaterialGraphSchema, HLSL генерирует
// EditorCore::HlslGraphCompiler. Здесь только материальное: параметры и текстуры в нодах, миниатюры,
// готовые графы и обмен с ИИ. Само окно материала решает, когда пересобрать HLSL.
class MaterialGraphWidget : public GraphEditorWidget
{
public:
    explicit MaterialGraphWidget(QWidget* parent = nullptr);

    std::function<void(const QString& name, const std::array<float, 4>& value)> parameterEdited;
    std::function<void(const QString& textureName)> textureRequested;
    // Отмена вернула ноде другую текстуру (/Game путь, пустой — без текстуры): её надо вернуть и материалу.
    std::function<void(const QString& textureName, const QString& gamePath)> textureRestored;

    // SetModel начинает историю заново: это загрузка, а не правка.
    void SetModel(const EditorCore::GraphDocument& model) { SetDocument(model); }
    const EditorCore::GraphDocument& Model() const { return Document(); }

    // Значение изменили в настройках материала: нода показывает его и запишет в HLSL как значение по умолчанию.
    void SetParameterValue(const QString& name, const std::array<float, 4>& value);
    // Назначенные текстуры: имя Texture2D -> файл на диске, для миниатюр в нодах.
    void SetTextures(const QHash<QString, QString>& files);
    // Текстура хранится и в ноде (свойство texture, /Game путь): её можно выбрать, пока нода не подключена
    // и компилятор её выбрасывает. Окно назначает её материалу, когда слот появится.
    void SetNodeTexture(const QString& textureName, const QString& gamePath);
    QHash<QString, QString> NodeTextures() const; // имя Texture2D -> /Game путь, только заданные

    void LoadPreset(int index);

    static QString NodeDisplayName(const QString& type);

    // Готовые графы проекта: <folder>/<имя>.graph.json (обычно Content/MaterialGraphs, общий через git).
    void SetPresetFolder(const QString& folder) { presetFolder_ = folder; }
    QStringList UserPresets() const;
    bool SaveUserPreset(const QString& name, QString& error);
    bool LoadUserPreset(const QString& name, QString& error);
    bool RemoveUserPreset(const QString& name);

    QString AiPrompt() const;
    // JSON, можно внутри блока ```json. Проверяет выход и ошибки компилятора до изменения графа/истории.
    bool PasteFromAi(const QString& text, QString* error = nullptr);

    static QStringList PresetNames();

protected:
    qreal NodeWidth(const EditorCore::GraphNodeData& node) const override;
    qreal NodeContentHeight(const EditorCore::GraphNodeData& node) const override;
    QVariant NodeDecoration(const EditorCore::GraphNodeData& node) const override;
    void PaintNodeContent(QPainter* painter, const EditorCore::GraphNodeData& node, const QRectF& content,
                          const QVariant& decoration) const override;
    void EditNode(int nodeId) override;
    QString EditActionText(const EditorCore::GraphNodeData& node) const override;
    QString RenameTitle() const override;
    void OnRestored(const EditorCore::GraphDocument& before) override;

private:
    void BuildMaterialToolbar();
    QString UserPresetFile(const QString& name) const;
    void RequestSaveUserPreset();

    QHash<QString, QString> textures_;
    QString presetFolder_;
};
