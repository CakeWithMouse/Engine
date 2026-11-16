#pragma once

#include "FsmGraphSchema.h"
#include "GraphEditorWidget.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <functional>

// Холст автомата ИИ на общем GraphEditorWidget. В состоянии видны его задачи (▶), условия входа (если …),
// службы (⟳) и связанный автомат (⇢); двойной щелчок открывает окно состояния. Дети состояния обведены рамкой.
// Двойной щелчок по переходу — окно перехода: когда срабатывает, условия, задержка, приоритет.
// Память (переменные документа) и настройки («settings») правятся через SetVariables / SetSettings и
// отменяются вместе с остальными правками.
class FsmGraphWidget : public GraphEditorWidget
{
public:
    // Схема живёт дольше виджета (её держит окно автомата).
    explicit FsmGraphWidget(EditorCore::FsmGraphSchema& schema, QWidget* parent = nullptr);

    std::function<void()> changed;       // граф изменился: сохранить и проверить
    std::function<void()> panelsChanged; // память или настройки заменила правка, отмена или загрузка

    void LoadDocument(const EditorCore::GraphDocument& document);

    // Ключи памяти целиком; renames: старое имя -> новое, ссылки блоков и настроек переименовываются вместе с ними.
    void SetVariables(const QJsonArray& variables, const QHash<QString, QString>& renames = {});
    QJsonObject Settings() const;
    void SetSettings(const QJsonObject& settings);

    // Свойства ноды целиком — один шаг отмены. Имя состояния — properties.name (заголовок следует за ним).
    bool SetNodeProperties(int nodeId, const QVariantMap& properties);
    QStringList NodeSummary(const EditorCore::GraphNodeData& node) const;

protected:
    qreal NodeWidth(const EditorCore::GraphNodeData& node) const override;
    qreal NodeContentHeight(const EditorCore::GraphNodeData& node) const override;
    void PaintNodeContent(QPainter* painter, const EditorCore::GraphNodeData& node, const QRectF& content,
                          const QVariant& decoration) const override;
    QVector<GraphNodeFrame> NodeFrames() const override;
    void EditNode(int nodeId) override;
    QString EditActionText(const EditorCore::GraphNodeData& node) const override;
    void EditLink(const EditorCore::GraphConnectionData& link) override;
    void OnRestored(const EditorCore::GraphDocument& before) override;

private:
    bool FixOrphans(); // родитель удалён: состояние встаёт на верхний уровень

    EditorCore::FsmGraphSchema& schema_;
};
