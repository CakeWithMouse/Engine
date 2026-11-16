#include "AiMachineEditorWindow.h"
#include "EditorLocale.h"
#include "FsmGraphWidget.h"
#include "Graph/GraphSchemaRegistry.h"

#include <QComboBox>
#include <QDir>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStatusBar>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <tuple>

using EditorCore::FsmGraphSchema;
using EditorCore::GraphDocument;

namespace
{
QString ValueText(const QJsonValue& value, const QString& type)
{
    if (value.isUndefined() || value.isNull()) return {};
    if (type == QStringLiteral("bool")) return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    if (type == QStringLiteral("vec3"))
    {
        const QJsonArray list = value.toArray();
        QStringList parts;
        for (int i = 0; i < 3; ++i) parts << QString::number(i < list.size() ? list[i].toDouble() : 0.0, 'g', 6);
        return parts.join(QStringLiteral(", "));
    }
    if (type == QStringLiteral("float") || type == QStringLiteral("int")) return QString::number(value.toDouble(), 'g', 10);
    return value.toString();
}

// Начальное значение из текста ячейки; пусто — без начального значения (ключ пуст, пока его не запишут).
QJsonValue ValueOf(const QString& text, const QString& type)
{
    const QString trimmed = text.trimmed();
    if (type == QStringLiteral("object") || (trimmed.isEmpty() && type != QStringLiteral("string"))) return QJsonValue(QJsonValue::Undefined);
    if (type == QStringLiteral("bool"))
        return QStringList{QStringLiteral("true"), QStringLiteral("1"), QStringLiteral("да"), QStringLiteral("yes")}.contains(trimmed.toLower());
    if (type == QStringLiteral("int")) return trimmed.toInt();
    if (type == QStringLiteral("float")) return QString(trimmed).replace(QLatin1Char(','), QLatin1Char('.')).toDouble();
    if (type == QStringLiteral("vec3"))
    {
        const QStringList parts = trimmed.split(QRegularExpression(QStringLiteral("[,;\\s]+")), Qt::SkipEmptyParts);
        QJsonArray list;
        for (int i = 0; i < 3; ++i) list.append(i < parts.size() ? parts[i].toDouble() : 0.0);
        return list;
    }
    return text.isEmpty() ? QJsonValue(QJsonValue::Undefined) : QJsonValue(text);
}
} // namespace

AiMachineEditorWindow::AiMachineEditorWindow(EditorQtHost* host, const QString& machine, QWidget* parent)
    : QMainWindow(parent), host_(host), machine_(machine), schema_(std::make_unique<FsmGraphSchema>())
{
    setAttribute(Qt::WA_DeleteOnClose);
    setObjectName(QStringLiteral("AiMachineEditorWindow"));
    setWindowTitle(Tr("Автомат ИИ — %1", "AI State Machine — %1").arg(QFileInfo(machine).fileName()));
    resize(1280, 800);
    BuildUi();
    LoadCatalog();
}

AiMachineEditorWindow::~AiMachineEditorWindow()
{
    if (saveTimer_ && saveTimer_->isActive()) WriteFile();
}

void AiMachineEditorWindow::BuildUi()
{
    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    banner_ = new QLabel(central);
    banner_->setObjectName(QStringLiteral("FsmBanner"));
    banner_->setWordWrap(true);
    banner_->setMargin(8);
    banner_->setStyleSheet(QStringLiteral("background:#7c2d12;color:white;"));
    banner_->hide();
    layout->addWidget(banner_);
    graph_ = new FsmGraphWidget(*schema_, central);
    graph_->setEnabled(false);
    layout->addWidget(graph_, 1);
    problemList_ = new QListWidget(central);
    problemList_->setObjectName(QStringLiteral("FsmProblems"));
    problemList_->setMaximumHeight(140);
    problemList_->setToolTip(Tr("Ошибки автомата по проверке движком; щелчок показывает ноду", "Machine problems from the engine's check; click to show the node"));
    connect(problemList_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        if (const int node = item->data(Qt::UserRole).toInt()) graph_->FocusNode(node);
    });
    layout->addWidget(problemList_);
    setCentralWidget(central);

    auto* dock = new QDockWidget(Tr("Память и настройки", "Memory and settings"), this);
    dock->setObjectName(QStringLiteral("FsmMemoryDock"));
    dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    auto* panel = new QWidget(dock);
    auto* panelLayout = new QVBoxLayout(panel);
    panelLayout->addWidget(new QLabel(Tr("Память (ключи, которыми пользуются блоки):", "Memory (keys the blocks use):"), panel));
    variables_ = new QTableWidget(0, 3, panel);
    variables_->setObjectName(QStringLiteral("FsmVariables"));
    variables_->setHorizontalHeaderLabels({Tr("Ключ", "Key"), Tr("Тип", "Type"), Tr("Начальное", "Initial")});
    variables_->horizontalHeader()->setStretchLastSection(true);
    variables_->verticalHeader()->hide();
    variables_->setSelectionBehavior(QAbstractItemView::SelectRows);
    connect(variables_, &QTableWidget::cellChanged, this, [this] { EditVariables(); });
    panelLayout->addWidget(variables_, 1);
    auto* buttons = new QHBoxLayout;
    auto* add = new QPushButton(Tr("+ Ключ", "+ Key"), panel);
    add->setObjectName(QStringLiteral("FsmAddVariable"));
    connect(add, &QPushButton::clicked, this, [this] { AddVariable(); });
    auto* remove = new QPushButton(Tr("Удалить", "Remove"), panel);
    remove->setObjectName(QStringLiteral("FsmRemoveVariable"));
    connect(remove, &QPushButton::clicked, this, [this] { RemoveVariable(); });
    buttons->addWidget(add);
    buttons->addWidget(remove);
    panelLayout->addLayout(buttons);
    auto* hint = new QLabel(Tr("Пустое начальное значение — ключ пуст, пока его не запишут. Вектор — три числа через запятую. "
                               "Значения у конкретного персонажа задаются в его карточке «Автомат ИИ».",
                               "An empty initial value leaves the key empty until written. A vector is three numbers. "
                               "Values of one character are set in its AI State Machine card."), panel);
    hint->setWordWrap(true);
    panelLayout->addWidget(hint);

    panelLayout->addWidget(new QLabel(Tr("Настройки персонажа:", "Character settings:"), panel));
    auto* settings = new QFormLayout;
    targetKey_ = new QComboBox(panel);
    targetKey_->setObjectName(QStringLiteral("FsmTargetKey"));
    targetKey_->setToolTip(Tr("Ключ типа «Объект»: восприятие пишет сюда лучшую видимую цель (нужно зрение у персонажа).",
                              "An Object key: perception writes the best visible target here (the character needs sight)."));
    lastSeenKey_ = new QComboBox(panel);
    lastSeenKey_->setObjectName(QStringLiteral("FsmLastSeenKey"));
    lastSeenKey_->setToolTip(Tr("Ключ типа «Вектор»: где цель видели последний раз.", "A Vector key: where the target was last seen."));
    thinkInterval_ = new QDoubleSpinBox(panel);
    thinkInterval_->setObjectName(QStringLiteral("FsmThinkInterval"));
    thinkInterval_->setRange(-0.01, 60);
    thinkInterval_->setDecimals(2);
    thinkInterval_->setSingleStep(0.05);
    thinkInterval_->setSpecialValueText(Tr("как у персонажа", "the character's"));
    thinkInterval_->setToolTip(Tr("Секунды между решениями; 0 — каждый кадр.", "Seconds between thinks; 0 — every frame."));
    thinkInterval_->setKeyboardTracking(false);
    settings->addRow(Tr("Цель восприятия", "Perceived target"), targetKey_);
    settings->addRow(Tr("Где видели", "Last seen at"), lastSeenKey_);
    settings->addRow(Tr("Интервал решений, с", "Think interval, s"), thinkInterval_);
    panelLayout->addLayout(settings);
    connect(targetKey_, &QComboBox::activated, this, [this] { EditSettings(); });
    connect(lastSeenKey_, &QComboBox::activated, this, [this] { EditSettings(); });
    connect(thinkInterval_, &QDoubleSpinBox::valueChanged, this, [this] { EditSettings(); });
    dock->setWidget(panel);
    addDockWidget(Qt::LeftDockWidgetArea, dock);
    dock->setEnabled(false);

    saveTimer_ = new QTimer(this);
    saveTimer_->setSingleShot(true);
    saveTimer_->setInterval(300);
    connect(saveTimer_, &QTimer::timeout, this, [this] { SaveNow(); });
    graph_->changed = [this] { saveTimer_->start(); };
    graph_->layoutChanged = [this] { if (!WriteFile()) saveTimer_->start(); }; // занятый файл — запись с повтором
    graph_->panelsChanged = [this] { RefreshPanels(); };
    statusBar()->showMessage(Tr("Загрузка каталога блоков…", "Loading the block catalog…"));
}

QString AiMachineEditorWindow::PhysicalPath() const
{
    return QDir(QString::fromUtf8(host_->host().contentRoot)).filePath(machine_.mid(6));
}

void AiMachineEditorWindow::LoadCatalog()
{
    QPointer<AiMachineEditorWindow> self(this);
    EditorQtHost* host = host_;
    EditorCommand c;
    c.type = EditorCommand::RunTask;
    c.task = [self, host](const EngineEditSessionApi&) {
        std::string catalog;
        if (const EngineAiMachineApi* api = host->AiMachineApi())
            api->describe(api->context, [](void* user, const char* text) { *static_cast<std::string*>(user) = text ? text : ""; }, &catalog);
        host->RunOnUi([self, catalog = QString::fromStdString(catalog)] {
            if (self) self->LoadFile(catalog);
        });
    };
    host_->PostCommand(c);
}

void AiMachineEditorWindow::LoadFile(const QString& catalog)
{
    QString error;
    if (catalog.isEmpty())
        return SetReadOnly(Tr("Игровой модуль проекта не поддерживает автоматы ИИ из файлов. Обновите Game.dll проекта: создайте проект "
                              "в новой версии движка или обновите копию проекта.",
                              "The project's game module does not support AI machines from files. Update the project's Game.dll."));
    if (!schema_->LoadCatalog(catalog, &error)) return SetReadOnly(error);
    QFile file(PhysicalPath());
    GraphDocument document;
    if (!file.exists())
        return SetReadOnly(Tr("Файл %1 не найден: его переименовали или удалили.", "%1 was not found: it was renamed or deleted.").arg(machine_));
    if (!file.open(QIODevice::ReadOnly) || !document.ImportFromJson(QString::fromUtf8(file.readAll())))
        return SetReadOnly(Tr("Файл %1 не читается как граф.", "Cannot read %1 as a graph.").arg(machine_));
    if (document.FileFormat() != GraphDocument::Format::V1 || document.Schema() != schema_->Id())
    {
        const auto& registry = EditorCore::GraphSchemaRegistry::Instance();
        const QString kind = document.FileFormat() == GraphDocument::Format::Legacy ? QString() : document.Schema();
        if (registry.Find(kind))
            return SetReadOnly(Tr("%1 — граф «%2», а не автомат ИИ.", "%1 is a \"%2\" graph, not an AI state machine.").arg(machine_, registry.DisplayName(kind)));
        return SetReadOnly(Tr("%1 — не автомат ИИ.", "%1 is not an AI state machine.").arg(machine_));
    }
    graph_->LoadDocument(document);
    ready_ = true;
    if (document.IsNewerFormat() || document.SchemaVersion() > schema_->Version())
        return SetReadOnly(Tr("Файл записан более новой версией редактора: он открыт только для чтения.",
                              "The file was written by a newer editor: it is open read-only."));
    graph_->setEnabled(true);
    findChild<QDockWidget*>(QStringLiteral("FsmMemoryDock"))->setEnabled(true);
    hint_ = Tr("Правки сохраняются сразу; Play читает файл при запуске. Двойной щелчок — свойства состояния или перехода.",
               "Edits are saved at once; Play reads the file when it starts. Double-click — state or transition properties.");
    statusBar()->showMessage(hint_);
    Check();
}

void AiMachineEditorWindow::SetReadOnly(const QString& reason)
{
    readOnly_ = true;
    status_ = reason;
    banner_->setText(reason);
    banner_->show();
    graph_->setEnabled(false);
    statusBar()->showMessage(Tr("Только для чтения", "Read-only"));
}

bool AiMachineEditorWindow::WriteFile()
{
    if (!ready_ || readOnly_) return false;
    QSaveFile file(PhysicalPath());
    if (file.open(QIODevice::WriteOnly) && file.write(graph_->Document().ExportToJson().toUtf8()) >= 0 && file.commit())
    {
        statusBar()->showMessage(hint_); // прежняя неудачная попытка больше не важна
        return true;
    }
    statusBar()->showMessage(Tr("Не удалось записать %1: %2", "Cannot write %1: %2").arg(machine_, file.errorString()));
    return false;
}

void AiMachineEditorWindow::SaveNow()
{
    saveTimer_->stop();
    if (WriteFile())
    {
        writeRetries_ = 0;
        Check();
    }
    // Файл может быть открыт на чтение проверкой движка (карточка персонажа проверяет его раз в секунду):
    // замена файла на Windows тогда отказывает. Повтор через мгновение.
    else if (ready_ && !readOnly_ && ++writeRetries_ <= 20)
        QTimer::singleShot(50, this, [this] { SaveNow(); });
    else
        writeRetries_ = 0;
}

void AiMachineEditorWindow::Check()
{
    QPointer<AiMachineEditorWindow> self(this);
    EditorQtHost* host = host_;
    const QByteArray machine = machine_.toUtf8();
    EditorCommand c;
    c.type = EditorCommand::RunTask;
    c.task = [self, host, machine](const EngineEditSessionApi&) {
        struct Row { int node; QString message; };
        QVector<Row> rows;
        const EngineAiMachineApi* api = host->AiMachineApi();
        const bool checked = api && api->check(api->context, machine.constData(), [](void* user, const EngineAiProblem* problem) {
            static_cast<QVector<Row>*>(user)->append({problem->node, EditorCore::LocalizeAiMessage(QString::fromUtf8(problem->message))});
        }, &rows) == EngineSceneOk;
        host->RunOnUi([self, rows, checked] {
            if (!self) return;
            self->problems_.clear();
            for (const auto& row : rows)
                self->problems_[row.node] = self->problems_.value(row.node).isEmpty() ? row.message : self->problems_.value(row.node) + QLatin1Char('\n') + row.message;
            if (!checked) self->problems_[0] = Tr("Движок не смог проверить автомат.", "The engine could not check the machine.");
            ++self->checks_;
            self->ShowProblems();
        });
    };
    host_->PostCommand(c);
}

void AiMachineEditorWindow::ShowProblems()
{
    warnings_ = schema_->Warnings(graph_->Document());
    problemList_->clear();
    QList<int> nodes = problems_.keys();
    std::sort(nodes.begin(), nodes.end());
    for (const int node : nodes)
        for (const QString& message : problems_.value(node).split(QLatin1Char('\n')))
        {
            const QString where = node ? Tr("нода %1", "node %1").arg(node) : Tr("файл", "file");
            auto* item = new QListWidgetItem(QStringLiteral("%1: %2").arg(where, message), problemList_);
            item->setData(Qt::UserRole, node);
        }
    QList<int> warned = warnings_.keys();
    std::sort(warned.begin(), warned.end());
    for (const int node : warned)
    {
        auto* item = new QListWidgetItem(Tr("Предупреждение · нода %1: %2", "Warning · node %1: %2").arg(node).arg(warnings_.value(node)), problemList_);
        item->setData(Qt::UserRole, node);
        item->setForeground(QColor(0xEA, 0xB3, 0x08));
    }
    QHash<int, QString> onNodes = problems_;
    onNodes.remove(0);
    graph_->SetNodeWarnings(warnings_);
    graph_->SetNodeProblems(onNodes);
    if (problemList_->count() == 0)
        (new QListWidgetItem(Tr("Ошибок нет: автомат запустится в Play", "No problems: the machine will run in Play"), problemList_))->setFlags(Qt::NoItemFlags);
}

void AiMachineEditorWindow::RefreshPanels()
{
    refreshing_ = true;
    const QJsonArray list = graph_->Document().Variables();
    variables_->setRowCount(0); // строки строятся заново: старые ячейки и выпадающие списки уходят целиком
    variables_->setRowCount(list.size());
    // Ячейки — поля ввода: текстовые элементы таблицы рядом с выпадающими списками в этой теме не рисовались.
    for (int row = 0; row < list.size(); ++row)
    {
        const QJsonObject variable = list[row].toObject();
        const QString name = variable.value(QStringLiteral("name")).toString();
        const QString type = variable.value(QStringLiteral("type")).toString();
        auto* nameField = new QLineEdit(name);
        nameField->setObjectName(QStringLiteral("FsmVariableName_%1").arg(row));
        nameField->setProperty("previous", name); // прежнее имя: переименование переносится на блоки
        connect(nameField, &QLineEdit::editingFinished, this, [this] { EditVariables(); });
        variables_->setCellWidget(row, 0, nameField);
        auto* combo = new QComboBox;
        combo->setObjectName(QStringLiteral("FsmVariableType_%1").arg(row));
        for (const QString& candidate : FsmGraphSchema::VariableTypes()) combo->addItem(FsmGraphSchema::TypeLabel(candidate), candidate);
        combo->setCurrentIndex(std::max(0, combo->findData(type)));
        connect(combo, &QComboBox::currentIndexChanged, this, [this] { EditVariables(); });
        variables_->setCellWidget(row, 1, combo);
        auto* valueField = new QLineEdit(ValueText(variable.value(QStringLiteral("value")), type));
        valueField->setObjectName(QStringLiteral("FsmVariableValue_%1").arg(row));
        valueField->setPlaceholderText(type == QStringLiteral("vec3") ? QStringLiteral("x, y, z") : Tr("пусто", "empty"));
        if (type == QStringLiteral("object"))
        {
            valueField->setEnabled(false);
            valueField->setPlaceholderText(QString());
            valueField->setToolTip(Tr("У объекта нет начального значения: его пишет восприятие или «Найти объект».",
                                      "An object has no initial value: perception or Find object writes it."));
        }
        connect(valueField, &QLineEdit::editingFinished, this, [this] { EditVariables(); });
        variables_->setCellWidget(row, 2, valueField);
    }

    const QJsonObject settings = graph_->Settings();
    for (auto [combo, key, type] : {std::tuple{targetKey_, "targetKey", "object"}, std::tuple{lastSeenKey_, "lastSeenKey", "vec3"}})
    {
        combo->clear();
        combo->addItem(Tr("— нет —", "— none —"), QString());
        for (const QString& name : schema_->KeysFor(QStringLiteral("key:") + QLatin1String(type))) combo->addItem(name, name);
        const QString chosen = settings.value(QLatin1String(key)).toString();
        if (!chosen.isEmpty() && combo->findData(chosen) < 0) combo->addItem(chosen, chosen);
        combo->setCurrentIndex(std::max(0, combo->findData(chosen)));
    }
    thinkInterval_->setValue(settings.contains(QStringLiteral("thinkInterval")) ? settings.value(QStringLiteral("thinkInterval")).toDouble() : thinkInterval_->minimum());
    refreshing_ = false;
}

void AiMachineEditorWindow::EditVariables()
{
    if (refreshing_ || !ready_ || readOnly_) return;
    const QJsonArray before = graph_->Document().Variables();
    QJsonArray list;
    QHash<QString, QString> renames;
    for (int row = 0; row < variables_->rowCount(); ++row)
    {
        const auto* nameField = qobject_cast<QLineEdit*>(variables_->cellWidget(row, 0));
        const auto* combo = qobject_cast<QComboBox*>(variables_->cellWidget(row, 1));
        const auto* valueField = qobject_cast<QLineEdit*>(variables_->cellWidget(row, 2));
        if (!nameField || !combo) continue;
        const QString name = nameField->text().trimmed(), old = nameField->property("previous").toString();
        const QString type = combo->currentData().toString();
        const QString oldType = row < before.size() ? before[row].toObject().value(QStringLiteral("type")).toString() : type;
        QJsonObject variable{{QStringLiteral("name"), name}, {QStringLiteral("type"), type}};
        // Новый тип начинает без значения: число 5 не превращается в вектор.
        const QJsonValue value = type == oldType ? ValueOf(valueField ? valueField->text() : QString(), type) : QJsonValue(QJsonValue::Undefined);
        if (!value.isUndefined()) variable.insert(QStringLiteral("value"), value);
        list.append(variable);
        if (!old.isEmpty() && old != name) renames.insert(old, name);
    }
    if (list == before && renames.isEmpty()) return; // уход фокуса без правки — без шага отмены
    QTimer::singleShot(0, this, [this, list, renames] { graph_->SetVariables(list, renames); });
}

void AiMachineEditorWindow::AddVariable()
{
    if (!ready_ || readOnly_) return;
    QJsonArray list = graph_->Document().Variables();
    QStringList taken;
    for (const auto& value : list) taken << value.toObject().value(QStringLiteral("name")).toString();
    QString name = QStringLiteral("Key");
    for (int i = 2; taken.contains(name); ++i) name = QStringLiteral("Key%1").arg(i);
    list.append(QJsonObject{{QStringLiteral("name"), name}, {QStringLiteral("type"), QStringLiteral("float")}});
    graph_->SetVariables(list);
}

void AiMachineEditorWindow::RemoveVariable()
{
    const int row = variables_->currentRow();
    if (!ready_ || readOnly_ || row < 0) return;
    QJsonArray list = graph_->Document().Variables();
    if (row >= list.size()) return;
    list.removeAt(row);
    graph_->SetVariables(list);
}

void AiMachineEditorWindow::EditSettings()
{
    if (refreshing_ || !ready_ || readOnly_) return;
    QJsonObject settings = graph_->Settings();
    auto put = [&](const char* key, const QJsonValue& value) {
        if (value.isUndefined() || (value.isString() && value.toString().isEmpty())) settings.remove(QLatin1String(key));
        else settings.insert(QLatin1String(key), value);
    };
    put("targetKey", targetKey_->currentData().toString());
    put("lastSeenKey", lastSeenKey_->currentData().toString());
    put("thinkInterval", thinkInterval_->value() >= 0 ? QJsonValue(thinkInterval_->value()) : QJsonValue(QJsonValue::Undefined));
    graph_->SetSettings(settings);
}
