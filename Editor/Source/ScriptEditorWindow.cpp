#include "ScriptEditorWindow.h"
#include "EditorLocale.h"
#include "EditorWindowSetup.h"
#include "ScriptGraphWidget.h"
#include "ScriptGraphPresets.h"
#include "ScriptMessages.h"
#include "Graph/GraphSchemaRegistry.h"
#include "Graph/GraphFileIo.h"
#include "VariablePanelWidget.h"
#include "IconProvider.h"
#include "InfoButton.h"

#include <QComboBox>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QSplitter>
#include <QStatusBar>
#include <QTableWidget>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

using EditorCore::CodeGraphSchema;
using EditorCore::GraphDocument;

namespace
{
struct ProblemRow
{
    int node = 0;
    QString object, message;
    bool runtime = false;
};

void ENGINE_MODULE_CALL CollectProblem(void* user, const EngineScriptProblem* problem)
{
    static_cast<QVector<ProblemRow>*>(user)->append({problem->node, QString::fromUtf8(problem->objectName),
                                                     EditorCore::LocalizeScriptMessage(QString::fromUtf8(problem->message)),
                                                     problem->runtime != 0});
}

// В комбобоксе переменных тип float подписан явно; TypeLabel schema остаётся для сообщений о пинах.
QString VariableTypeLabel(const QString& type)
{
    if (type == QStringLiteral("float")) return Tr("число [float]", "number [float]");
    return EditorCore::CodeGraphSchema::TypeLabel(type);
}
} // namespace

ScriptEditorWindow::ScriptEditorWindow(EditorQtHost* host, const QString& script, QWidget* parent)
    : QMainWindow(nullptr, Qt::Window), host_(host), script_(script), schema_(std::make_unique<CodeGraphSchema>())
{
    EditorWindowSetup::ConfigureSubWindow(this, parent);
    setAttribute(Qt::WA_DeleteOnClose);
    setObjectName(QStringLiteral("ScriptEditorWindow"));
    setWindowTitle(Tr("Скрипт — %1", "Script — %1").arg(QFileInfo(script).fileName()));
    resize(1200, 760);
    BuildUi();
    LoadCatalog();
}

ScriptEditorWindow::~ScriptEditorWindow()
{
    if (saveTimer_ && saveTimer_->isActive()) WriteFile();
}

void ScriptEditorWindow::BuildUi()
{
    auto* tools = addToolBar(Tr("Скрипт", "Script"));
    tools->setMovable(false);

    auto makeBtn = [tools](const QIcon& icon, const QString& name, const QString& tip) {
        auto* b = new QPushButton(icon, QString(), tools);
        b->setObjectName(name);
        b->setProperty("toolbarBtn", true);
        b->setFixedSize(EditorWindowSetup::kToolbarBtnWidth, EditorWindowSetup::kToolbarBtnHeight);
        b->setIconSize(QSize(EditorWindowSetup::kToolbarIconSize, EditorWindowSetup::kToolbarIconSize));
        b->setToolTip(tip);
        tools->addWidget(b);
        return b;
    };
    auto* save = makeBtn(IconProvider::SaveIcon(), QStringLiteral("ScriptSave"), Tr("Сохранить [Ctrl+S]", "Save [Ctrl+S]")); connect(save, &QPushButton::clicked, this, [this] { SaveNow(); });
    auto* revert = makeBtn(IconProvider::RevertIcon(), QStringLiteral("ScriptRevert"), Tr("Откатить все изменения", "Revert all changes")); connect(revert, &QPushButton::clicked, this, [this] { Revert(); });

    auto* templates = new QToolButton(tools);
    templates->setObjectName(QStringLiteral("ScriptTemplates"));
    templates->setProperty("toolbarBtn", true);
    templates->setIcon(IconProvider::TemplateIcon());
    templates->setFixedSize(EditorWindowSetup::kToolbarBtnWidth, EditorWindowSetup::kToolbarBtnHeight); templates->setIconSize(QSize(EditorWindowSetup::kToolbarIconSize, EditorWindowSetup::kToolbarIconSize));
    templates->setToolTip(Tr("Шаблоны скрипта", "Script templates"));
    templates->setPopupMode(QToolButton::InstantPopup);
    auto* menu = new QMenu(templates); menu->setObjectName(QStringLiteral("ScriptTemplateMenu"));
    menu->addAction(Tr("Движение по таймеру/каждый кадр (синусоида по Y)", "Motion every frame (sinusoid along Y)"), this, [this] { ApplyTemplate(1); });
    menu->addAction(Tr("Таймер: через N секунд выполнить действие", "Timer: action after N seconds"), this, [this] { ApplyTemplate(2); });
    menu->addAction(Tr("Счётчик с переменной (Get/Set +1 каждый кадр)", "Counter with variable (+1 every frame)"), this, [this] { ApplyTemplate(3); });
    menu->addAction(Tr("Условие: если переменная > порога", "Condition: if variable > threshold"), this, [this] { ApplyTemplate(4); });
    menu->addAction(Tr("Пустой", "Empty"), this, [this] { ApplyTemplate(0); });
    templates->setMenu(menu);
    tools->addWidget(templates);

    auto* help = new InfoButton(this);
    help->setObjectName(QStringLiteral("ScriptHelp"));
    help->SetToolbarStyle(true);
    help->setProperty("toolbarBtn", true);
    help->setFixedSize(EditorWindowSetup::kToolbarBtnWidth, EditorWindowSetup::kToolbarBtnHeight);
    help->setIconSize(QSize(EditorWindowSetup::kToolbarIconSize, EditorWindowSetup::kToolbarIconSize));
    help->SetInfoText(Tr("Редактор визуальных скриптов", "Visual Script Editor"),
        Tr("Визуальные скрипты [.vscript]:\n• Верхняя строка: Сохранить [Ctrl+S] и справка окна\n"
           "• Панель «Переменные»: именованные данные объекта [число, логический, вектор, строка]\n"
           "  - Кнопки под заголовком: Добавить, Дублировать, Удалить; «Показ.» [глаз]: публичные в инспекторе\n"
           "  - Перетаскивание на холст [DnD]: Get [чтение]; с Shift — Set [запись]\n"
           "• Панель «Граф»: логика из нод; кнопки: +Нода, Отменить, Повторить, Показать всё, Упорядочить, Комментарий, Копировать/Вставить\n"
           "• Шаблоны: готовые типовые графы с подтверждением замены; Откатить: возврат к сохранённому файлу\n"
           "• Горячие клавиши: Ctrl+S — сохранить, Ctrl+Z/Y — отмена/повтор, F — уместить, Delete — удалить\n"
           "• Отладка в Play: подсветка активных нод и счетчик срабатываний; Консоль: сообщения компилятора",
           "Visual Scripts [.vscript]:\n• Top bar: Save [Ctrl+S] and window help\n"
           "• Variables panel: named object data; buttons: Add, Duplicate, Delete; eye: public; DnD onto canvas\n"
           "• Graph panel: logic from nodes; buttons: +Node, Undo, Redo, Frame all, Arrange, Comment, Copy/Paste\n"
           "• Templates: predefined graphs with replacement confirmation; Revert: restore saved file\n"
           "• Hotkeys: Ctrl+S — save, Ctrl+Z/Y — undo/redo, F — frame, Delete — remove\n"
           "• Play debugging: highlight on active nodes and fire count; Console: compiler messages"), QString());
    tools->addWidget(help);

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    banner_ = new QLabel(central);
    banner_->setObjectName(QStringLiteral("ScriptBanner"));
    banner_->setWordWrap(true);
    banner_->setMargin(8);
    banner_->setStyleSheet(QStringLiteral("background:#7c2d12;color:white;"));
    banner_->hide();
    layout->addWidget(banner_);

    graph_ = new ScriptGraphWidget(*schema_, central);
    graph_->setEnabled(false); // до загрузки каталога и файла

    variables_ = new ScriptVariableTable(central);
    variables_->setObjectName(QStringLiteral("ScriptVariables"));
    variablesPanel_ = new VariablePanelWidget(variables_, QStringLiteral("Script"),
        CodeGraphSchema::VariableTypes(), Tr("Имя", "Name"), Tr("Значение", "Value"),
        VariableTypeLabel, Tr("Переменные", "Variables"), central);
    variablesPanel_->onVariablesChanged = [this](const QJsonArray& list, const QHash<QString, QString>& renames) {
        if (!ready_ || readOnly_) return;
        graph_->SetVariables(list, renames);
    };
    variablesPanel_->setEnabled(false);

    auto* splitter = new QSplitter(Qt::Horizontal, central);
    splitter->setHandleWidth(3);
    splitter->addWidget(variablesPanel_);
    splitter->addWidget(graph_);
    splitter->setStretchFactor(0, 0); splitter->setStretchFactor(1, 1);
    splitter->setSizes({280, 720});
    variablesPanel_->setMinimumWidth(260);
    layout->addWidget(splitter, 1);

    auto* console = new QWidget(central);
    auto* consoleLayout = new QVBoxLayout(console);
    consoleLayout->setContentsMargins(0, 0, 0, 0); consoleLayout->setSpacing(0);
    consoleLayout->addWidget(EditorWindowSetup::MakePanelHeader(Tr("Консоль", "Console"), console));

    problemList_ = new QListWidget(console);
    problemList_->setObjectName(QStringLiteral("ScriptProblems"));
    problemList_->setMaximumHeight(140);
    problemList_->setToolTip(Tr("Ошибки графа и Play; щелчок показывает ноду", "Graph and Play problems; click to show the node"));
    connect(problemList_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        if (const int node = item->data(Qt::UserRole).toInt()) graph_->FocusNode(node);
    });
    consoleLayout->addWidget(problemList_);
    layout->addWidget(console);
    setCentralWidget(central);
    ShowProblems();

    saveTimer_ = new QTimer(this);
    saveTimer_->setSingleShot(true);
    saveTimer_->setInterval(300);
    connect(saveTimer_, &QTimer::timeout, this, [this] { SaveNow(); });
    playTimer_ = new QTimer(this);
    playTimer_->setInterval(1000);
    connect(playTimer_, &QTimer::timeout, this, [this] { PollPlayProblems(); });

    // Отладка в Play: какие ноды выполняются у выбранного объекта.
    traceLabel_ = new QLabel(Tr("Объект в Play:", "Play object:"), this);
    traceObjects_ = new QComboBox(this);
    traceObjects_->setObjectName(QStringLiteral("ScriptTraceObject"));
    traceObjects_->setToolTip(Tr("Чьё выполнение показывать на нодах: зелёная рамка — нода работала только что, ×N — сколько раз",
                                 "Whose execution the nodes show: a green frame — the node ran just now, ×N — how many times"));
    traceObjects_->setMinimumContentsLength(14);
    connect(traceObjects_, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
        TraceObject(traceObjects_->itemData(index).toULongLong());
    });
    statusBar()->addPermanentWidget(traceLabel_);
    statusBar()->addPermanentWidget(traceObjects_);
    traceLabel_->hide(); traceObjects_->hide();
    traceTimer_ = new QTimer(this);
    traceTimer_->setInterval(200);
    connect(traceTimer_, &QTimer::timeout, this, [this] { PollTrace(); });

    graph_->structureChanged = [this] { ShowProblems(); };
    graph_->layoutChanged = [this] {};
    graph_->variablesChanged = [this] { RefreshVariables(); };
    statusBar()->showMessage(Tr("Загрузка каталога нод…", "Loading the node catalog…"));
}

QString ScriptEditorWindow::PhysicalPath() const
{
    return QDir(QString::fromUtf8(host_->host().contentRoot)).filePath(script_.mid(6)); // без "/Game/"
}

QString ScriptEditorWindow::ProjectRoot() const
{
    return QFileInfo(QString::fromUtf8(host_->host().projectFile)).dir().absolutePath();
}

void ScriptEditorWindow::LoadCatalog()
{
    QPointer<ScriptEditorWindow> self(this);
    EditorQtHost* host = host_;
    EditorCommand c;
    c.type = EditorCommand::RunTask;
    c.task = [self, host](const EngineEditSessionApi&) {
        std::string catalog;
        if (const EngineScriptApi* api = host->ScriptApi())
            api->describe(api->context, [](void* user, const char* text) { *static_cast<std::string*>(user) = text ? text : ""; }, &catalog);
        host->RunOnUi([self, catalog = QString::fromStdString(catalog)] {
            if (self) self->LoadFile(catalog);
        });
    };
    host_->PostCommand(c);
}

void ScriptEditorWindow::LoadFile(const QString& catalog)
{
    QString error;
    if (catalog.isEmpty())
        return SetReadOnly(Tr("Игровой модуль проекта не поддерживает визуальные скрипты. Обновите Game.dll проекта.",
                              "The project's game module does not support visual scripts. Update the project's Game.dll."));
    if (!schema_->LoadCatalog(catalog, &error)) return SetReadOnly(error);

    GraphDocument document;
    const QString contentRoot = QString::fromUtf8(host_->host().contentRoot);
    if (!EditorCore::GraphFileIo::Exists(ProjectRoot(), contentRoot, PhysicalPath()))
        return SetReadOnly(Tr("Файл %1 не найден: его переименовали или удалили.", "%1 was not found: it was renamed or deleted.").arg(script_));
    QByteArray data;
    if (!EditorCore::GraphFileIo::Read(ProjectRoot(), contentRoot, PhysicalPath(), data) || !document.ImportFromJson(QString::fromUtf8(data)))
        return SetReadOnly(Tr("Файл %1 не читается как граф.", "Cannot read %1 as a graph.").arg(script_));
    if (document.FileFormat() != GraphDocument::Format::V1 || document.Schema() != schema_->Id())
    {
        const auto& reg = EditorCore::GraphSchemaRegistry::Instance();
        const QString kind = document.FileFormat() == GraphDocument::Format::Legacy ? QString() : document.Schema();
        if (reg.Find(kind))
            return SetReadOnly(Tr("%1 — граф «%2», а не визуальный скрипт.", "%1 is a \"%2\" graph, not a visual script.").arg(script_, reg.DisplayName(kind)));
        return SetReadOnly(Tr("%1 — не визуальный скрипт.", "%1 is not a visual script.").arg(script_));
    }

    graph_->LoadDocument(document);
    savedContent_ = data;
    ready_ = true;
    if (document.IsNewerFormat() || document.SchemaVersion() > schema_->Version())
        return SetReadOnly(Tr("Файл записан более новой версией редактора: он открыт только для чтения.",
                              "The file was written by a newer editor: it is open read-only."));
    graph_->setEnabled(true);
    variablesPanel_->setEnabled(true);
    statusBar()->showMessage(Tr("Сохранить: Ctrl+S. Двойной щелчок — параметры.", "Save: Ctrl+S. Double-click — properties."));
    Check();
    playTimer_->start();
    traceTimer_->start();
}

void ScriptEditorWindow::SetReadOnly(const QString& reason)
{
    readOnly_ = true;
    status_ = reason;
    banner_->setText(reason);
    banner_->show();
    graph_->setEnabled(false);
    variablesPanel_->setEnabled(false);
    statusBar()->showMessage(Tr("Только для чтения", "Read-only"));
}

bool ScriptEditorWindow::WriteFile()
{
    if (!ready_ || readOnly_) return false;
    QString error;
    const QByteArray data = graph_->Document().ExportToJson().toUtf8();
    if (EditorCore::GraphFileIo::Write(ProjectRoot(), QString::fromUtf8(host_->host().contentRoot), PhysicalPath(), data, &error))
    {
        savedContent_ = data;
        return true;
    }
    statusBar()->showMessage(Tr("Не удалось записать %1: %2", "Cannot write %1: %2").arg(script_, error));
    return false;
}

void ScriptEditorWindow::SaveNow()
{
    if (saveTimer_) saveTimer_->stop();
    if (WriteFile()) Check();
}

bool ScriptEditorWindow::HasUnsavedChanges() const { return graph_ && graph_->Document().ExportToJson().toUtf8() != savedContent_; }

void ScriptEditorWindow::Revert()
{
    if (!ready_ || readOnly_ || !HasUnsavedChanges()) return;
    if (QMessageBox::question(this, Tr("Откатить скрипт", "Revert script"),
            Tr("Отменить все несохранённые изменения скрипта?", "Revert all unsaved changes to the script?")) == QMessageBox::Yes)
        ReloadDocument();
}

void ScriptEditorWindow::ReloadDocument()
{
    const QString contentRoot = QString::fromUtf8(host_->host().contentRoot);
    QByteArray data; EditorCore::GraphDocument doc;
    if (!EditorCore::GraphFileIo::Read(ProjectRoot(), contentRoot, PhysicalPath(), data) || !doc.ImportFromJson(QString::fromUtf8(data))) return;
    savedContent_ = data; graph_->LoadDocument(doc); RefreshVariables(); Check();
}

void ScriptEditorWindow::ApplyTemplate(int kind)
{
    if (!ready_ || readOnly_) return;
    if (!graph_->Document().Nodes().isEmpty() &&
        QMessageBox::question(this, Tr("Шаблон скрипта", "Script template"),
            Tr("Заменить текущий граф графом шаблона?", "Replace current graph with template?")) != QMessageBox::Yes)
        return;
    const EditorCore::GraphDocument doc = (kind == 0) ? EditorCore::ScriptGraphPresets::BuildEmpty()
        : (kind == 1) ? EditorCore::ScriptGraphPresets::BuildSinMoveY()
        : (kind == 2) ? EditorCore::ScriptGraphPresets::BuildTimerAction()
        : (kind == 3) ? EditorCore::ScriptGraphPresets::BuildCounterVariable()
        : (kind == 4) ? EditorCore::ScriptGraphPresets::BuildConditionThreshold() : EditorCore::GraphDocument{};
    if (doc.Schema().isEmpty()) return;
    graph_->ApplyTemplate(doc);
    RefreshVariables();
    ShowProblems();
}

void ScriptEditorWindow::Check()
{
    QPointer<ScriptEditorWindow> self(this);
    EditorQtHost* host = host_;
    const QByteArray script = script_.toUtf8();
    EditorCommand c;
    c.type = EditorCommand::RunTask;
    c.task = [self, host, script](const EngineEditSessionApi&) {
        QVector<ProblemRow> rows;
        const EngineScriptApi* api = host->ScriptApi();
        const bool checked = api && api->check(api->context, script.constData(), CollectProblem, &rows) == EngineSceneOk;
        host->RunOnUi([self, rows, checked] {
            if (!self) return;
            self->checkProblems_.clear();
            for (const auto& r : rows) self->checkProblems_.append({r.node, {}, r.message, false});
            if (!checked) self->checkProblems_.append({0, {}, Tr("Движок не смог проверить скрипт.", "The engine could not check the script."), false});
            ++self->checks_;
            self->ShowProblems();
        });
    };
    host_->PostCommand(c);
}

void ScriptEditorWindow::PollPlayProblems()
{
    if (host_->PlayState() == EditorQtHost::Edit) return;
    QPointer<ScriptEditorWindow> self(this);
    EditorQtHost* host = host_;
    const std::string script = script_.toStdString();
    EditorCommand c;
    c.type = EditorCommand::RunTask;
    c.task = [self, host, script](const EngineEditSessionApi&) {
        struct Collected { std::string script; QVector<ProblemRow> rows; } collected{script, {}};
        if (const EngineScriptApi* api = host->ScriptApi())
            api->readProblems(api->context, [](void* user, const EngineScriptProblem* problem) {
                auto& data = *static_cast<Collected*>(user);
                if (data.script == problem->script) CollectProblem(&data.rows, problem);
            }, &collected);
        host->RunOnUi([self, rows = collected.rows] {
            if (!self) return;
            self->playProblems_.clear();
            for (const auto& r : rows) self->playProblems_.append({r.node, r.object, r.message, true});
            self->ShowProblems();
        });
    };
    host_->PostCommand(c);
}

void ScriptEditorWindow::TraceObject(quint64 objectId)
{
    tracedObject_ = objectId;
    PollTrace();
}

void ScriptEditorWindow::PollTrace()
{
    const bool playing = host_->PlayState() != EditorQtHost::Edit;
    if (!playing || !host_->GraphTraceApi())
    {
        graph_->SetNodeStatuses({});
        traceLabel_->hide();
        traceObjects_->hide();
        return;
    }
    if (tracePending_) return;
    tracePending_ = true;
    QPointer<ScriptEditorWindow> self(this);
    EditorQtHost* host = host_;
    const std::string script = script_.toStdString();
    const quint64 wanted = tracedObject_;
    EditorCommand c;
    c.type = EditorCommand::RunTask;
    c.task = [self, host, script, wanted](const EngineEditSessionApi&) {
        struct Object { quint64 id; QString name; bool failed; };
        struct Value { int node; QString pin, text; };
        struct Collected { QVector<Object> objects; QVector<EngineGraphTraceNode> nodes; QVector<Value> values; } collected;
        if (const EngineGraphTraceApi* api = host->GraphTraceApi())
        {
            api->listObjects(api->context, script.c_str(), [](void* user, const EngineGraphTraceObject* o) {
                static_cast<Collected*>(user)->objects.push_back({o->objectId, QString::fromUtf8(o->objectName), o->failed != 0});
            }, &collected);
            quint64 id = 0;
            for (const auto& o : collected.objects) if (o.id == wanted) id = wanted;
            api->read(api->context, script.c_str(), id, [](void* user, const EngineGraphTraceNode* n) {
                static_cast<Collected*>(user)->nodes.push_back(*n);
            }, &collected);
            api->readValues(api->context, script.c_str(), id, [](void* user, const EngineGraphTraceValue* v) {
                static_cast<Collected*>(user)->values.push_back({v->node, QString::fromUtf8(v->pin), QString::fromUtf8(v->text)});
            }, &collected);
        }
        host->RunOnUi([self, collected] {
            if (!self) return;
            self->tracePending_ = false;
            if (self->host_->PlayState() == EditorQtHost::Edit) return self->PollTrace();
            QComboBox* combo = self->traceObjects_;
            const quint64 cur = combo->currentIndex() >= 0 ? combo->currentData().toULongLong() : 0;
            combo->blockSignals(true);
            combo->clear();
            for (const auto& o : collected.objects)
                combo->addItem(o.failed ? Tr("%1 (остановлен)", "%1 (stopped)").arg(o.name) : o.name, o.id);
            int idx = combo->findData(self->tracedObject_ ? self->tracedObject_ : cur);
            combo->setCurrentIndex(idx >= 0 ? idx : 0);
            combo->blockSignals(false);
            self->traceLabel_->setVisible(!collected.objects.isEmpty());
            combo->setVisible(!collected.objects.isEmpty());

            QHash<int, GraphNodeStatus> statuses;
            for (const auto& n : collected.nodes)
            {
                GraphNodeStatus s;
                s.state = n.state == EngineGraphNodeFailed ? GraphNodeState::Failed
                        : n.framesAgo <= ActiveFrames ? GraphNodeState::Active : GraphNodeState::Done;
                if (n.count) s.badge = QStringLiteral("\u00d7%1").arg(n.count);
                statuses.insert(n.node, s);
            }
            for (const auto& v : collected.values) statuses[v.node].values.insert(v.pin, v.text);
            self->graph_->SetNodeStatuses(statuses);
        });
    };
    host_->PostCommand(c);
}

QHash<int, QString> ScriptEditorWindow::Problems() const
{
    QHash<int, QString> problems;
    for (const auto* list : {&checkProblems_, &playProblems_})
        for (const auto& p : *list)
            if (p.node)
            {
                const QString t = p.object.isEmpty() ? p.message : QStringLiteral("Play, %1: %2").arg(p.object, p.message);
                problems[p.node] = problems.value(p.node).isEmpty() ? t : problems.value(p.node) + QLatin1Char('\n') + t;
            }
    return problems;
}

void ScriptEditorWindow::ShowProblems()
{
    warnings_ = schema_->Warnings(graph_->Document());
    problemList_->clear();
    for (const auto* list : {&checkProblems_, &playProblems_})
        for (const auto& p : *list)
        {
            QString where = p.node ? Tr("нода %1", "node %1").arg(p.node) : Tr("файл", "file");
            if (p.runtime || !p.object.isEmpty()) where = QStringLiteral("Play · %1 · %2").arg(p.object, where);
            auto* item = new QListWidgetItem(QStringLiteral("%1: %2").arg(where, p.message), problemList_);
            item->setData(Qt::UserRole, p.node);
        }
    QList<int> warned = warnings_.keys();
    std::sort(warned.begin(), warned.end());
    for (const int node : warned)
    {
        auto* item = new QListWidgetItem(Tr("Предупреждение · нода %1: %2", "Warning · node %1: %2").arg(node).arg(warnings_.value(node)), problemList_);
        item->setData(Qt::UserRole, node);
        item->setForeground(QColor(0xEA, 0xB3, 0x08));
    }
    graph_->SetNodeWarnings(warnings_);
    graph_->SetNodeProblems(Problems());
    if (problemList_->count() == 0)
    {
        auto* item = new QListWidgetItem(Tr("Нет сообщений", "No messages"), problemList_);
        item->setFlags(Qt::NoItemFlags);
        item->setForeground(QColor(0x80, 0x80, 0x80));
    }
}

void ScriptEditorWindow::RefreshVariables()
{
    if (variablesPanel_ && graph_) variablesPanel_->SetVariables(graph_->Document().Variables());
}
