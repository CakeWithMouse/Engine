#pragma once

#include "CodeGraphSchema.h"
#include "GraphEditorWidget.h"

#include <QHash>
#include <QJsonArray>
#include <QPoint>
#include <QTableWidget>
#include <functional>

class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;
class QMouseEvent;

// Таблица переменных скрипта: перетаскивание строки на холст графа создаёт ноду Variable.Get/Set
// (MIME ScriptVariableDrag, правило DropRules GraphVariable -> GraphCanvas). Имя и тип берутся из
// редакторов строки; без имени перетаскивание не начинается.
class ScriptVariableTable : public QTableWidget
{
public:
    using QTableWidget::QTableWidget;

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    QPoint dragStart_;
};

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
    // Drop переменной на холст: события приходят на viewport внутреннего QGraphicsView.
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void AcceptVariableDrag(QDragMoveEvent* event);
    void DropVariable(QDropEvent* event);
    EditorCore::CodeGraphSchema& schema_;
    QWidget* canvasViewport_ = nullptr;
};
