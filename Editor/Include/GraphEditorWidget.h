#pragma once

#include "Graph/GraphDocument.h"
#include "Graph/GraphHistory.h"
#include "Graph/GraphSchema.h"

#include <QHash>
#include <QList>
#include <QPointF>
#include <QString>
#include <QVariant>
#include <QWidget>
#include <functional>

class QGraphicsPathItem;
class QGraphicsScene;
class QGraphicsView;
class QHBoxLayout;
class QPainter;
class QToolButton;

// Общий нодовый редактор: холст с сеткой, ноды и связи, выделение, история, буфер обмена, поиск нод,
// ошибки на нодах. Что за ноды и что с чем соединяется, говорит GraphSchema; как выглядит содержимое
// ноды и как правятся её свойства, решает наследник (MaterialGraphWidget, редактор скрипта).
// Без Q_OBJECT: о правках сообщает функциями обратного вызова.
class GraphEditorWidget : public QWidget
{
public:
    // Схема живёт дольше виджета (обычно статический экземпляр).
    explicit GraphEditorWidget(const EditorCore::GraphSchema& schema, QWidget* parent = nullptr);
    ~GraphEditorWidget() override;

    std::function<void()> structureChanged; // ноды, связи, имена или свойства, от которых зависит результат
    std::function<void()> layoutChanged;    // ноды сдвинуты: сохранить граф, результат прежний

    const EditorCore::GraphSchema& Schema() const { return schema_; }

    // Загрузка: история начинается заново, ошибки нод сбрасываются.
    void SetDocument(const EditorCore::GraphDocument& document);
    const EditorCore::GraphDocument& Document() const { return document_; }

    // Свободные имена и свойства по умолчанию подставляет схема; возвращает id ноды.
    int AddNode(const QString& type, const QPointF& pos, const QVariantMap& props = {});
    // Выход -> вход; занятый одиночный пин освобождается. false: связь недопустима (см. GraphSchema).
    bool Connect(int fromNode, const QString& fromPin, int toNode, const QString& toPin);
    void RemoveNodes(const QList<int>& ids); // обязательные ноды остаются
    bool RenameNode(int nodeId, const QString& name);

    // История правок (Ctrl+Z, Ctrl+Y).
    void Undo();
    void Redo();
    bool CanUndo() const { return history_.CanUndo(); }
    bool CanRedo() const { return history_.CanRedo(); }

    // Выделение и буфер обмена: ноды со связями между ними, JSON в буфере обмена системы,
    // так что ноды переносятся и между окнами. Обязательные ноды не копируются.
    QList<int> SelectedNodes() const;
    void SelectNodes(const QList<int>& ids);
    bool CopyNodes(const QList<int>& ids) const;
    bool CanPaste() const;
    QList<int> PasteNodes(const QPointF& topLeft); // id вставленных нод
    QList<int> DuplicateNodes(const QList<int>& ids);
    void FrameNodes(); // выделенные, а без выделения — весь граф

    // Ошибки на нодах: красная рамка, значок, текст в подсказке. Ключ — id ноды.
    void SetNodeProblems(const QHash<int, QString>& problems);
    void FocusNode(int nodeId);

    // Список нод с поиском (ПКМ по холсту, Tab, «+ Нода»): выбранная нода встаёт в scenePos.
    void ShowNodeSearch(const QPoint& screenPos, const QPointF& scenePos);

    QGraphicsView* View() const { return view_; }
    QPointF PinScenePos(int nodeId, const QString& pin) const;

    class NodeItem;

protected:
    // Внешний вид ноды. decoration — данные, посчитанные один раз при построении ноды (миниатюра и т.п.).
    virtual qreal NodeWidth(const EditorCore::GraphNodeData& node) const { return 150; }
    virtual qreal NodeContentHeight(const EditorCore::GraphNodeData& node) const { return 0; }
    virtual QVariant NodeDecoration(const EditorCore::GraphNodeData& node) const { return {}; }
    virtual void PaintNodeContent(QPainter* painter, const EditorCore::GraphNodeData& node, const QRectF& content,
                                  const QVariant& decoration) const {}

    // Свойства ноды: двойной щелчок и первый пункт меню ноды.
    virtual void EditNode(int nodeId) {}
    virtual QString EditActionText(const EditorCore::GraphNodeData& node) const;
    virtual QString RenameTitle() const;
    // Отмена или повтор заменили граф; before — граф до замены.
    virtual void OnRestored(const EditorCore::GraphDocument& before) {}

    // Кнопка на панели после стандартных (+ Нода, отмена, повтор, «Показать всё»).
    void AddToolbarWidget(QWidget* widget);

    EditorCore::GraphDocument& MutableDocument() { return document_; }
    // Снимок до правки; правки с одинаковым ключом подряд — один шаг отмены.
    void Checkpoint(const QString& mergeKey = {});
    // Замена графа без сброса истории: обязательные ноды добавляются, вид встаёт на граф.
    void ReplaceDocument(const EditorCore::GraphDocument& document);
    void RebuildScene();
    void NotifyStructure();
    void RequestRename(int nodeId);
    static QPointF Snap(const QPointF& point);
    static constexpr qreal GridStep = 20.0;

    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void BuildUi();
    void UpdateConnections();
    NodeItem* FindItem(int nodeId) const;
    // Пин под точкой сцены: id ноды и имя; false — пусто.
    bool PinAt(const QPointF& scenePos, int& nodeId, QString& pin, bool& input) const;
    void StartLink(const QPointF& scenePos);
    void FinishLink(const QPointF& scenePos);
    void ShowNodeMenu(const QPoint& screenPos, int nodeId);
    void DeleteSelected();
    void NodeMoved(int nodeId, const QPointF& pos);
    void RestoreSnapshot(const QString& snapshot);
    void UpdateHistoryButtons();
    QString ClipboardJson(const QList<int>& ids) const;
    QString ClipboardGraph() const;
    QList<int> InsertNodes(const QString& json, const QPointF& topLeft);
    QPointF CursorScenePos() const;
    bool IsFixed(int nodeId) const;

    const EditorCore::GraphSchema& schema_;
    EditorCore::GraphDocument document_;
    EditorCore::GraphHistory history_;
    QGraphicsScene* scene_ = nullptr;
    QGraphicsView* view_ = nullptr;
    QHBoxLayout* toolbar_ = nullptr;
    int toolbarIndex_ = 0;
    QList<QGraphicsPathItem*> links_;

    // Тянущаяся связь: от выхода (linkFromInput_ == false) или от входа.
    QGraphicsPathItem* dragLink_ = nullptr;
    int linkNode_ = 0;
    QString linkPin_;
    bool linkFromInput_ = false;
    QPointF linkStart_;

    bool panning_ = false, panMoved_ = false, moved_ = false;
    QPoint panStart_, panLast_;

    QString pressSnapshot_; // граф на нажатии ЛКМ: перетаскивание нод — один шаг отмены
    QString linkKey_;       // снятие связи с пина и её новое подключение — один шаг
    int linkSerial_ = 0;
    QHash<int, QString> problems_;
    QToolButton* undoButton_ = nullptr;
    QToolButton* redoButton_ = nullptr;
};
