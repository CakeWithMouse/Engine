#pragma once

#include "GraphDocument.h"

#include <QColor>
#include <QStringList>

namespace EditorCore
{

// Группа нод в поиске и меню; по ней красится заголовок ноды.
struct GraphNodeGroup
{
    QString name;       // на языке редактора
    QString searchText; // имена группы на всех языках для поиска
    QStringList types;
    QColor color;
};

// Схема графа: какие ноды бывают, какие у них пины, что с чем соединяется и что значит имя ноды.
// Общее ядро (документ, история, буфер обмена, виджет) смысла нод не знает и спрашивает схему.
// Граф материала — MaterialGraphSchema, граф кода и графы ИИ — свои схемы.
class GraphSchema
{
public:
    virtual ~GraphSchema() = default;

    // Идентификатор в заголовке файла V1 ("code", "ai.bt"); у графа материала пустой — формат Legacy.
    virtual QString Id() const = 0;
    virtual int Version() const { return 1; }
    // Префикс objectName виджетов ("MaterialGraph" -> MaterialGraphView, MaterialGraphNodeSearch…).
    virtual QString ObjectPrefix() const = 0;
    // Буфер обмена: MIME-тип и ключ-метка в JSON, по которой вставка узнаёт ноды этой схемы в тексте.
    virtual QString ClipboardMime() const = 0;
    virtual QString ClipboardMarker() const = 0;

    // Каталог.
    virtual QVector<GraphNodeGroup> Groups() const = 0;
    virtual bool IsKnownNode(const QString& type) const = 0;
    virtual QString DisplayName(const QString& type) const { return type; }
    virtual QString Hint(const QString& type) const { return {}; }
    virtual QString SearchKeywords(const QString& type) const { return {}; } // синонимы и описание для поиска
    virtual QString SearchPlaceholder() const { return {}; }
    virtual QColor NodeColor(const GraphNodeData& node) const; // по группе, иначе серый
    virtual QColor PinColor(const GraphPin& pin) const { return QColor(0xE2, 0xE8, 0xF0); }
    virtual QColor LinkColor(const GraphPin& output) const { return QColor(0x38, 0xBD, 0xF8); }

    // Пины считаются от ноды, а не только от типа: число выходов может задаваться свойством.
    virtual QVector<GraphPin> Pins(const GraphNodeData& node) const = 0;

    // Правила связей. У входа по умолчанию одна связь, выход расходится на много входов, циклы запрещены.
    virtual bool InputTakesOneLink(const GraphPin& input) const { return true; }
    virtual bool OutputTakesOneLink(const GraphPin& output) const { return false; }
    virtual bool AllowsCycle(const GraphPin& output, const GraphPin& input) const { return false; }
    // Дополнительная проверка схемы (типы пинов): пустая строка — связь допустима.
    virtual QString LinkProblem(const GraphPin& output, const GraphPin& input) const { return {}; }

    // Обязательная нода (выход материала): не удаляется, не копируется и есть в графе всегда.
    virtual bool IsFixed(const GraphNodeData& node) const { return false; }
    // Добавляет обязательные ноды графу, где их нет (старый формат, ответ ИИ).
    virtual void EnsureRequiredNodes(GraphDocument& document) const {}

    // Имя ноды, видимое пользователю и уникальное в графе (параметр, переменная); пустое — у ноды его нет.
    virtual QString NodeName(const GraphNodeData& node) const { return {}; }
    // Почему имя не подходит; пустая строка — подходит. Уникальность проверяет ядро.
    virtual QString NameProblem(const QString& name) const { return {}; }
    virtual void SetNodeName(GraphNodeData& node, const QString& name) const {}
    virtual QString NodeTitle(const GraphNodeData& node) const; // заголовок на холсте

    // Свойства и заголовок новой ноды (свободные имена, значения по умолчанию).
    virtual void PrepareNewNode(const GraphDocument& document, const QString& type, QVariantMap& properties, QString& title) const {}
    // Нода из буфера обмена перед вставкой в document: по умолчанию занятое имя заменяется свободным.
    virtual void PreparePastedNode(const GraphDocument& document, GraphNodeData& node) const;

    // Общие проверки ядра на этих правилах.
    QString UniqueName(const GraphDocument& document, const QString& base) const;
    const GraphPin* FindPin(const QVector<GraphPin>& pins, const QString& name, bool input) const;
    // Почему связь output -> input недопустима; пустая строка — допустима. Занятость пинов не проверяется:
    // при соединении занятый одиночный пин освобождается.
    QString ConnectionProblem(const GraphDocument& document, int fromNode, const QString& fromPin, int toNode, const QString& toPin) const;
};

} // namespace EditorCore
