#include "ScriptGraphWidget.h"
#include "ScriptVariableDrag.h"
#include "DropRules.h"
#include "AssetExtensions.h"

#include <QApplication>
#include <QComboBox>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QGraphicsView>
#include <QLineEdit>
#include <QMimeData>
#include <QMouseEvent>
#include <QVariantMap>

using namespace EditorCore;

void ScriptVariableTable::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) dragStart_ = event->pos();
    QTableWidget::mousePressEvent(event);
}

void ScriptVariableTable::mouseMoveEvent(QMouseEvent* event)
{
    if (!(event->buttons() & Qt::LeftButton) ||
        (event->pos() - dragStart_).manhattanLength() < QApplication::startDragDistance())
    {
        QTableWidget::mouseMoveEvent(event);
        return;
    }
    const QModelIndex index = indexAt(dragStart_);
    auto* nameEdit = index.isValid() ? qobject_cast<QLineEdit*>(cellWidget(index.row(), 0)) : nullptr;
    const QString name = nameEdit ? nameEdit->text().trimmed() : QString();
    if (name.isEmpty())
    {
        QTableWidget::mouseMoveEvent(event);
        return;
    }
    auto* typeCombo = qobject_cast<QComboBox*>(cellWidget(index.row(), 1));
    ScriptVariableDrag::Payload payload;
    payload.name = name;
    payload.type = typeCombo ? typeCombo->currentText() : QString();
    auto* mime = new QMimeData;
    mime->setData(QLatin1String(ScriptVariableDrag::kMimeType), ScriptVariableDrag::Encode(payload));
    auto* drag = new QDrag(this);
    drag->setMimeData(mime);
    drag->exec(Qt::CopyAction);
}

bool ScriptGraphWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (canvasViewport_ && watched == canvasViewport_)
    {
        switch (event->type())
        {
        case QEvent::DragEnter: AcceptVariableDrag(static_cast<QDragEnterEvent*>(event)); return true;
        case QEvent::DragMove: AcceptVariableDrag(static_cast<QDragMoveEvent*>(event)); return true;
        case QEvent::Drop: DropVariable(static_cast<QDropEvent*>(event)); return true;
        default: break;
        }
    }
    return GraphEditorWidget::eventFilter(watched, event);
}

void ScriptGraphWidget::AcceptVariableDrag(QDragMoveEvent* event)
{
    const QMimeData* mime = event->mimeData();
    const DropDecision decision = DecideDrop(DragSource::GraphVariable, DropTarget::GraphCanvas, AssetKind::Unknown);
    if (!mime || !ScriptVariableDrag::HasMime(*mime) || !decision.allowed)
    {
        event->ignore();
        return;
    }
    event->setDropAction(Qt::CopyAction);
    event->accept();
}

void ScriptGraphWidget::DropVariable(QDropEvent* event)
{
    const QMimeData* mime = event->mimeData();
    const ScriptVariableDrag::Payload payload = mime ? ScriptVariableDrag::FromMime(*mime) : ScriptVariableDrag::Payload{};
    const DropDecision decision = DecideDrop(DragSource::GraphVariable, DropTarget::GraphCanvas, AssetKind::Unknown);
    auto* view = canvasViewport_ ? qobject_cast<QGraphicsView*>(canvasViewport_->parentWidget()) : nullptr;
    if (payload.name.isEmpty() || !decision.allowed || decision.action != DropAction::CreateVariableNode || !view)
    {
        event->ignore();
        return;
    }
    // Shift selects Set, otherwise Get; AddNode takes one Undo checkpoint and notifies the window to save.
    const bool set = event->modifiers().testFlag(Qt::ShiftModifier);
    QVariantMap props;
    props.insert(QStringLiteral("variable"), payload.name);
    AddNode(set ? QStringLiteral("Variable.Set") : QStringLiteral("Variable.Get"), view->mapToScene(event->position().toPoint()), props);
    event->setDropAction(Qt::CopyAction);
    event->accept();
}