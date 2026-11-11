#pragma once

#include "CodeGraphSchema.h"
#include "GraphEditorWidget.h"

#include <QHash>
#include <QJsonArray>
#include <functional>

// Холст визуального скрипта на общем GraphEditorWidget. В ноде видны её свойства (клавиша, переменная,
// значение) и значения неподключённых входов; двойной щелчок открывает их диалог. Переменные графа
// правятся через SetVariables и отменяются вместе с остальными правками.
class ScriptGraphWidget : public GraphEditorWidget
{
public:
    // Схема живёт дольше виджета (её держит окно скрипта).
    explicit ScriptGraphWidget(EditorCore::CodeGraphSchema& schema, QWidget* parent = nullptr);

    std::function<void()> variablesChanged; // переменные заменила правка, отмена или загрузка

    void LoadDocument(const EditorCore::GraphDocument& document);

    // Переменные целиком; renames: старое имя -> новое, ноды переменных переименовываются вместе с ними.
    void SetVariables(const QJsonArray& variables, const QHash<QString, QString>& renames = {});

    // Свойства ноды и значения неподключённых входов (properties.inputs) одной правкой.
    bool SetNodeProperties(int nodeId, const QVariantMap& properties);
    // Что видно в ноде под пинами, по строке.
    QStringList NodeSummary(const EditorCore::GraphNodeData& node) const;
    // Входы данных без связи, которым можно задать значение в ноде (не объект и не поток).
    QVector<EditorCore::GraphPin> EditableInputs(const EditorCore::GraphNodeData& node) const;

protected:
    qreal NodeWidth(const EditorCore::GraphNodeData& node) const override { return 180; }
    qreal NodeContentHeight(const EditorCore::GraphNodeData& node) const override;
    void PaintNodeContent(QPainter* painter, const EditorCore::GraphNodeData& node, const QRectF& content,
                          const QVariant& decoration) const override;
    void EditNode(int nodeId) override;
    QString EditActionText(const EditorCore::GraphNodeData& node) const override;
    void OnRestored(const EditorCore::GraphDocument& before) override;

private:
    EditorCore::CodeGraphSchema& schema_;
};
