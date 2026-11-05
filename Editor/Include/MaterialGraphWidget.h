#pragma once

#include "MaterialGraphModel.h"

#include <QHash>
#include <QList>
#include <QPointF>
#include <QString>
#include <QWidget>
#include <array>
#include <functional>

class QGraphicsPathItem;
class QGraphicsScene;
class QGraphicsView;
class QToolButton;

// Нодовый редактор материала: ноды на сетке, связи кривыми, HLSL генерирует EditorCore::HlslGraphCompiler.
// Перенесён из ветки feature/editor-ui-overhaul на модель EditorCore. Без Q_OBJECT: о правках сообщает
// функциями обратного вызова, само окно материала решает, когда пересобрать HLSL.
class MaterialGraphWidget : public QWidget
{
public:
    explicit MaterialGraphWidget(QWidget* parent = nullptr);

    std::function<void()> structureChanged; // поменялся HLSL графа: ноды, связи, имена, тайлинг
    std::function<void()> layoutChanged;    // ноды сдвинуты: сохранить граф, HLSL прежний
    std::function<void(const QString& name, const std::array<float, 4>& value)> parameterEdited;
    std::function<void(const QString& textureName)> textureRequested;
    // Отмена вернула ноде другую текстуру (/Game путь, пустой — без текстуры): её надо вернуть и материалу.
    std::function<void(const QString& textureName, const QString& gamePath)> textureRestored;

    void SetModel(const EditorCore::MaterialGraphModel& model);
    const EditorCore::MaterialGraphModel& Model() const { return model_; }

    // Значение изменили в настройках материала: нода показывает его и запишет в HLSL как значение по умолчанию.
    void SetParameterValue(const QString& name, const std::array<float, 4>& value);
    // Назначенные текстуры: имя Texture2D -> файл на диске, для миниатюр в нодах.
    void SetTextures(const QHash<QString, QString>& files);
    // Текстура хранится и в ноде (свойство texture, /Game путь): её можно выбрать, пока нода не подключена
    // и компилятор её выбрасывает. Окно назначает её материалу, когда слот появится.
    void SetNodeTexture(const QString& textureName, const QString& gamePath);
    QHash<QString, QString> NodeTextures() const; // имя Texture2D -> /Game путь, только заданные

    // Свободные имена и параметры по умолчанию подставляются сами; возвращает id ноды.
    int AddNode(const QString& type, const QPointF& pos, const QVariantMap& props = {});
    // Выход -> вход; прежняя связь входа заменяется. false: пины не подходят.
    bool Connect(int fromNode, const QString& fromPin, int toNode, const QString& toPin);
    void RemoveNodes(const QList<int>& ids);
    bool RenameNode(int nodeId, const QString& name);
    void LoadPreset(int index);

    // История правок графа (Ctrl+Z, Ctrl+Y). Значения из настроек материала тоже попадают в неё.
    // SetModel начинает историю заново: это загрузка, а не правка.
    void Undo();
    void Redo();
    bool CanUndo() const { return !undo_.isEmpty(); }
    bool CanRedo() const { return !redo_.isEmpty(); }

    // Выделение и буфер обмена: ноды со связями между ними, JSON в буфере обмена системы,
    // так что ноды переносятся и между окнами материалов. Выход материала не копируется.
    QList<int> SelectedNodes() const;
    void SelectNodes(const QList<int>& ids);
    bool CopyNodes(const QList<int>& ids) const;
    bool CanPaste() const;
    QList<int> PasteNodes(const QPointF& topLeft); // id вставленных нод
    QList<int> DuplicateNodes(const QList<int>& ids);
    void FrameNodes(); // выделенные, а без выделения — весь граф

    // Ошибки на нодах: красная рамка, текст в подсказке. Ключ — id ноды.
    void SetNodeProblems(const QHash<int, QString>& problems);
    void FocusNode(int nodeId);

    // Список нод с поиском (ПКМ по холсту, Tab, «+ Нода»): выбранная нода встаёт в scenePos.
    void ShowNodeSearch(const QPoint& screenPos, const QPointF& scenePos);
    static QString NodeDisplayName(const QString& type);

    // Готовые графы проекта: <folder>/<имя>.graph.json (обычно Content/MaterialGraphs, общий через git).
    void SetPresetFolder(const QString& folder) { presetFolder_ = folder; }
    QStringList UserPresets() const;
    bool SaveUserPreset(const QString& name, QString& error);
    bool LoadUserPreset(const QString& name, QString& error);
    bool RemoveUserPreset(const QString& name);

    QString AiPrompt() const;
    // Текст ответа: JSON графа, можно внутри блока ```json. false: не граф с выходом материала.
    bool PasteFromAi(const QString& text);

    QGraphicsView* View() const { return view_; }
    QPointF PinScenePos(int nodeId, const QString& pin) const;
    static QStringList PresetNames();

    class NodeItem;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void BuildUi();
    void RebuildScene();
    void UpdateConnections();
    NodeItem* FindItem(int nodeId) const;
    // Пин под точкой сцены: id ноды и имя; false — пусто.
    bool PinAt(const QPointF& scenePos, int& nodeId, QString& pin, bool& input) const;
    void StartLink(const QPointF& scenePos);
    void FinishLink(const QPointF& scenePos);
    void ShowCanvasMenu(const QPoint& screenPos, const QPointF& scenePos);
    void ShowNodeMenu(const QPoint& screenPos, int nodeId);
    void EditNode(int nodeId);
    void RequestRename(int nodeId);
    void DeleteSelected();
    QString UniqueName(const QString& base) const;
    void NotifyStructure();
    QString UserPresetFile(const QString& name) const;
    void RequestSaveUserPreset();
    void NodeMoved(int nodeId, const QPointF& pos);
    void ReplaceModel(const EditorCore::MaterialGraphModel& model); // без сброса истории
    // Снимок до правки. Правки с одинаковым ключом подряд (тянущийся ползунок) — один шаг отмены.
    void Checkpoint(const QString& mergeKey = {});
    void PushSnapshot(const QString& snapshot, const QString& mergeKey);
    void RestoreSnapshot(const QString& snapshot);
    void UpdateHistoryButtons();
    QString ClipboardJson(const QList<int>& ids) const;
    QString ClipboardGraph() const;
    QList<int> InsertNodes(const QString& json, const QPointF& topLeft);
    QPointF CursorScenePos() const;

    EditorCore::MaterialGraphModel model_;
    QHash<QString, QString> textures_;
    QString presetFolder_;
    QGraphicsScene* scene_ = nullptr;
    QGraphicsView* view_ = nullptr;
    QList<QGraphicsPathItem*> links_;

    // Тянущаяся связь: от выхода (linkFromInput_ == false) или от входа.
    QGraphicsPathItem* dragLink_ = nullptr;
    int linkNode_ = 0;
    QString linkPin_;
    bool linkFromInput_ = false;
    QPointF linkStart_;

    bool panning_ = false, panMoved_ = false, moved_ = false;
    QPoint panStart_, panLast_;

    QStringList undo_, redo_;     // JSON графа до правки / до отмены
    QString undoKey_;             // ключ последнего снимка, см. Checkpoint
    QString pressSnapshot_;       // граф на нажатии ЛКМ: перетаскивание нод — один шаг отмены
    QString linkKey_;             // снятие связи со входа и её новое подключение — один шаг
    int linkSerial_ = 0;
    QHash<int, QString> problems_;
    QToolButton* undoButton_ = nullptr;
    QToolButton* redoButton_ = nullptr;
};
