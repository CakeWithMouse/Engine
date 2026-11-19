#include "AiMachineEditorWindow.h"
#include "CommandRegistry.h"
#include "EditorLocale.h"
#include "EditorWindowSetup.h"
#include "FsmGraphWidget.h"
#include "FsmGraphPresets.h"
#include "Graph/GraphSchemaRegistry.h"
#include "Graph/GraphFileIo.h"
#include "VariablePanelWidget.h"
#include "IconProvider.h"
#include "InfoButton.h"

#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QLabel>
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
#include <algorithm>
#include <tuple>

using EditorCore::FsmGraphSchema;
using EditorCore::GraphDocument;



AiMachineEditorWindow::AiMachineEditorWindow(EditorQtHost* host, const QString& machine, QWidget* parent)
    : QMainWindow(nullptr, Qt::Window), host_(host), machine_(machine), schema_(std::make_unique<FsmGraphSchema>())
{
    EditorWindowSetup::ConfigureSubWindow(this, parent);
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
    auto* tools = addToolBar(Tr("Автомат ИИ", "AI State Machine"));
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
    auto* save = makeBtn(IconProvider::SaveIcon(), QStringLiteral("FsmSave"), Tr("Сохранить [Ctrl+S]", "Save [Ctrl+S]"));
    connect(save, &QPushButton::clicked, this, [this] { SaveNow(); });

    auto* revert = makeBtn(IconProvider::RevertIcon(), QStringLiteral("FsmRevert"), Tr("Откатить все изменения", "Revert all changes"));
    connect(revert, &QPushButton::clicked, this, [this] { Revert(); });

    auto* templates = new QToolButton(tools);
    templates->setObjectName(QStringLiteral("FsmTemplates"));
    templates->setProperty("toolbarBtn", true);
    templates->setIcon(IconProvider::TemplateIcon());
    templates->setFixedSize(EditorWindowSetup::kToolbarBtnWidth, EditorWindowSetup::kToolbarBtnHeight);
    templates->setIconSize(QSize(EditorWindowSetup::kToolbarIconSize, EditorWindowSetup::kToolbarIconSize));
    templates->setToolTip(Tr("Шаблоны автомата", "State machine templates"));
    templates->setPopupMode(QToolButton::InstantPopup);
    auto* tMenu = new QMenu(templates);
    tMenu->setObjectName(QStringLiteral("FsmTemplateMenu"));
    tMenu->addAction(Tr("Патруль → погоня → возврат", "Patrol → chase → return"), this, [this] { ApplyTemplate(0); });
    tMenu->addAction(Tr("Охрана: Idle ↔ Attack по дистанции", "Guard: Idle ↔ Attack by distance"), this, [this] { ApplyTemplate(1); });
    tMenu->addAction(Tr("Простая реакция: Idle → Flee", "Simple reaction: Idle → Flee"), this, [this] { ApplyTemplate(2); });
    templates->setMenu(tMenu);
    tools->addWidget(templates);

    auto* newMachine = makeBtn(IconProvider::RobotIcon(), QStringLiteral("FsmNewMachine"), Tr("Новый автомат: создать .fsm в той же папке", "New machine: create a .fsm in the same folder"));
    connect(newMachine, &QPushButton::clicked, this, [this] {
        CommandRegistry::Instance().Execute(QStringLiteral("Assets.CreateAiMachine"),
            {{QStringLiteral("directory"), QFileInfo(PhysicalPath()).absolutePath()}});
    });

    auto* help = new InfoButton(this);
    help->setObjectName(QStringLiteral("FsmHelp"));
    help->SetToolbarStyle(true);
    help->setProperty("toolbarBtn", true);
    help->setFixedSize(EditorWindowSetup::kToolbarBtnWidth, EditorWindowSetup::kToolbarBtnHeight);
    help->setIconSize(QSize(EditorWindowSetup::kToolbarIconSize, EditorWindowSetup::kToolbarIconSize));
    help->SetInfoText(Tr("Редактор автомата состояний ИИ", "AI State Machine Editor"),
        Tr("Конечный автомат ИИ [.fsm]:\n• Верхняя строка: Сохранить [Ctrl+S], Создать новый автомат и справка окна\n"
           "• Панель «Память и настройки»: Добавить, Дублировать, Удалить; глаз: публичные; настройки персонажа\n"
           "• Панель «Граф»: состояния [ноды] и переходы [связи]; 2×клик — редактирование свойств\n"
           "  - Кнопки графа: +Нода, Отменить, Повторить, Показать всё, Упорядочить, Комментарий, Копировать/Вставить\n"
           "• Шаблоны: типовые автоматы с подтверждением замены; Откатить: возврат к сохранённому файлу\n"
           "• Горячие клавиши: Ctrl+S — сохранить, Ctrl+Z/Y — отмена/повтор, F — уместить, Delete — удалить\n"
           "• Панель «Консоль»: ошибки компиляции автомата и проверки движком",
           "AI State Machine [.fsm]:\n• Top bar: Save [Ctrl+S], Create new machine and window help\n"
           "• Memory and settings: Add, Duplicate, Delete; eye: public; character settings\n"
           "• Graph panel: states [nodes] and transitions [links]; double click — edit properties\n"
           "  - Graph buttons: +Node, Undo, Redo, Frame all, Arrange, Comment, Copy/Paste\n"
           "• Templates: predefined state machines; Revert: restore saved file\n"
           "• Hotkeys: Ctrl+S — save, Ctrl+Z/Y — undo/redo, F — frame, Delete — remove\n"
           "• Console panel: machine compilation and engine validation errors"), QString());
    tools->addWidget(help);

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

    memoryPanel_ = new QWidget(central);
    auto* panelLayout = new QVBoxLayout(memoryPanel_);
    panelLayout->setContentsMargins(0, 0, 0, 0);
    panelLayout->setSpacing(0);
    panelLayout->addWidget(EditorWindowSetup::MakePanelHeader(Tr("Память и настройки", "Memory and settings"), memoryPanel_));

    auto* body = new QWidget(memoryPanel_);
    auto* bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(10, 8, 10, 10);
    bodyLayout->setSpacing(8);

    variables_ = new QTableWidget(body);
    variables_->setObjectName(QStringLiteral("FsmVariables"));
    variablesPanel_ = new VariablePanelWidget(variables_, QStringLiteral("Fsm"),
        FsmGraphSchema::VariableTypes(), Tr("Ключ", "Key"), Tr("Начальное", "Initial"),
        [](const QString& t) { return FsmGraphSchema::TypeLabel(t); }, QString(), body);
    variablesPanel_->onVariablesChanged = [this](const QJsonArray& list, const QHash<QString, QString>& renames) {
        if (!ready_ || readOnly_) return;
        graph_->SetVariables(list, renames);
    };

    auto* btnRow = new QWidget(body);
    auto* btnLayout = new QHBoxLayout(btnRow);
    btnLayout->setContentsMargins(0, 0, 0, 0);
    btnLayout->setSpacing(4);
    btnLayout->addWidget(variablesPanel_->AddButton());
    btnLayout->addWidget(variablesPanel_->DuplicateButton());
    btnLayout->addWidget(variablesPanel_->RemoveButton());
    btnLayout->addStretch(1);
    bodyLayout->addWidget(btnRow);

    bodyLayout->addSpacing(4);
    auto* memoryLabel = new QLabel(Tr("Память — ключи блоков", "Memory — block keys"), body);
    bodyLayout->addWidget(memoryLabel);
    bodyLayout->addWidget(variablesPanel_, 1);

    bodyLayout->addSpacing(8);
    auto* settingsLabel = new QLabel(Tr("Настройки персонажа", "Character settings"), body);
    bodyLayout->addWidget(settingsLabel);
    auto* settings = new QFormLayout;
    settings->setContentsMargins(0, 0, 0, 0);
    settings->setHorizontalSpacing(8);
    settings->setVerticalSpacing(8);
    targetKey_ = new QComboBox(body);
    targetKey_->setObjectName(QStringLiteral("FsmTargetKey"));
    targetKey_->setToolTip(Tr("Ключ типа «Объект»: восприятие пишет сюда лучшую видимую цель (нужно зрение у персонажа).",
                              "An Object key: perception writes the best visible target here (the character needs sight)."));
    lastSeenKey_ = new QComboBox(body);
    lastSeenKey_->setObjectName(QStringLiteral("FsmLastSeenKey"));
    lastSeenKey_->setToolTip(Tr("Ключ типа «Вектор»: где цель видели последний раз.", "A Vector key: where the target was last seen."));
    thinkInterval_ = new QDoubleSpinBox(body);
    thinkInterval_->setObjectName(QStringLiteral("FsmThinkInterval"));
    thinkInterval_->setRange(-0.01, 60);
    thinkInterval_->setDecimals(2);
    thinkInterval_->setSingleStep(0.05);
    thinkInterval_->setSpecialValueText(Tr("как у персонажа", "the character's"));
    thinkInterval_->setToolTip(Tr("Секунды между решениями; 0 — каждый кадр.", "Seconds between thinks; 0 — every frame."));
    thinkInterval_->setKeyboardTracking(false);
    auto iconLabel = [body](const QString& icon, const QString& text) {
        auto* row = new QWidget(body);
        auto* box = new QHBoxLayout(row);
        box->setContentsMargins(0, 0, 0, 0);
        box->setSpacing(6);
        auto* pic = new QLabel(row);
        pic->setPixmap(IconProvider::GetIcon(icon).pixmap(14, 14));
        box->addWidget(pic);
        box->addWidget(new QLabel(text, row));
        return row;
    };
    settings->addRow(iconLabel(QStringLiteral("focus-2"), Tr("Цель восприятия", "Perceived target")), targetKey_);
    settings->addRow(iconLabel(QStringLiteral("eye"), Tr("Где видели", "Last seen at")), lastSeenKey_);
    settings->addRow(iconLabel(QStringLiteral("refresh"), Tr("Интервал решений, с", "Think interval, s")), thinkInterval_);
    bodyLayout->addLayout(settings);
    panelLayout->addWidget(body, 1);
    connect(targetKey_, &QComboBox::activated, this, [this] { EditSettings(); });
    connect(lastSeenKey_, &QComboBox::activated, this, [this] { EditSettings(); });
    connect(thinkInterval_, &QDoubleSpinBox::valueChanged, this, [this] { EditSettings(); });
    memoryPanel_->setEnabled(false);

    auto* splitter = new QSplitter(Qt::Horizontal, central);
    splitter->setHandleWidth(3);
    splitter->addWidget(memoryPanel_);
    splitter->addWidget(graph_);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({280, 720});
    memoryPanel_->setMinimumWidth(260);
    layout->addWidget(splitter, 1);

    auto* console = new QWidget(central);
    auto* consoleLayout = new QVBoxLayout(console);
    consoleLayout->setContentsMargins(0, 0, 0, 0);
    consoleLayout->setSpacing(0);
    consoleLayout->addWidget(EditorWindowSetup::MakePanelHeader(Tr("Консоль", "Console"), console));

    problemList_ = new QListWidget(console);
    problemList_->setObjectName(QStringLiteral("FsmProblems"));
    problemList_->setMaximumHeight(140);
    problemList_->setToolTip(Tr("Ошибки автомата по проверке движком; щелчок показывает ноду", "Machine problems from the engine's check; click to show the node"));
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
    graph_->changed = [this] { ShowProblems(); };
    graph_->layoutChanged = [this] {};
    graph_->panelsChanged = [this] { RefreshPanels(); };
    statusBar()->showMessage(Tr("Загрузка каталога блоков…", "Loading the block catalog…"));
}

QString AiMachineEditorWindow::PhysicalPath() const
{
    return QDir(QString::fromUtf8(host_->host().contentRoot)).filePath(machine_.mid(6));
}

QString AiMachineEditorWindow::ProjectRoot() const
{
    return QFileInfo(QString::fromUtf8(host_->host().projectFile)).dir().absolutePath();
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
    GraphDocument document;
    const QString contentRoot = QString::fromUtf8(host_->host().contentRoot);
    if (!EditorCore::GraphFileIo::Exists(ProjectRoot(), contentRoot, PhysicalPath()))
        return SetReadOnly(Tr("Файл %1 не найден: его переименовали или удалили.", "%1 was not found: it was renamed or deleted.").arg(machine_));
    QByteArray data;
    if (!EditorCore::GraphFileIo::Read(ProjectRoot(), contentRoot, PhysicalPath(), data) || !document.ImportFromJson(QString::fromUtf8(data)))
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
    savedContent_ = data;
    ready_ = true;
    if (document.IsNewerFormat() || document.SchemaVersion() > schema_->Version())
        return SetReadOnly(Tr("Файл записан более новой версией редактора: он открыт только для чтения.",
                              "The file was written by a newer editor: it is open read-only."));
    graph_->setEnabled(true);
    memoryPanel_->setEnabled(true);
    hint_ = Tr("Сохранить: Ctrl+S. Двойной щелчок — свойства состояния или перехода.",
               "Save: Ctrl+S. Double-click — state or transition properties.");
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
    memoryPanel_->setEnabled(false);
    statusBar()->showMessage(Tr("Только для чтения", "Read-only"));
}

bool AiMachineEditorWindow::WriteFile()
{
    if (!ready_ || readOnly_) return false;
    QString error;
    const QByteArray data = graph_->Document().ExportToJson().toUtf8();
    if (EditorCore::GraphFileIo::Write(ProjectRoot(), QString::fromUtf8(host_->host().contentRoot), PhysicalPath(), data, &error))
    {
        savedContent_ = data;
        statusBar()->showMessage(hint_); // прежняя неудачная попытка больше не важна
        return true;
    }
    statusBar()->showMessage(Tr("Не удалось записать %1: %2", "Cannot write %1: %2").arg(machine_, error));
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

bool AiMachineEditorWindow::HasUnsavedChanges() const { return graph_ && graph_->Document().ExportToJson().toUtf8() != savedContent_; }

void AiMachineEditorWindow::Revert()
{
    if (!ready_ || readOnly_ || !HasUnsavedChanges()) return;
    if (QMessageBox::question(this, Tr("Откатить автомат", "Revert machine"),
            Tr("Отменить все несохранённые изменения автомата?", "Revert all unsaved changes to the state machine?")) == QMessageBox::Yes)
        ReloadDocument();
}

void AiMachineEditorWindow::ReloadDocument()
{
    const QString contentRoot = QString::fromUtf8(host_->host().contentRoot);
    QByteArray data; EditorCore::GraphDocument doc;
    if (!EditorCore::GraphFileIo::Read(ProjectRoot(), contentRoot, PhysicalPath(), data) || !doc.ImportFromJson(QString::fromUtf8(data))) return;
    savedContent_ = data; graph_->LoadDocument(doc); RefreshPanels(); Check();
}

void AiMachineEditorWindow::ApplyTemplate(int kind)
{
    if (!ready_ || readOnly_) return;
    if (!graph_->Document().Nodes().isEmpty() &&
        QMessageBox::question(this, Tr("Шаблон автомата", "Machine template"),
            Tr("Заменить текущий граф графом шаблона?", "Replace current graph with template?")) != QMessageBox::Yes)
        return;
    const EditorCore::GraphDocument doc = (kind == 0) ? EditorCore::FsmGraphPresets::BuildPatrolChaseReturn()
                                        : (kind == 1) ? EditorCore::FsmGraphPresets::BuildGuardAttackDistance()
                                        : (kind == 2) ? EditorCore::FsmGraphPresets::BuildIdleFlee()
                                        : EditorCore::GraphDocument{};
    if (doc.Schema().isEmpty()) return;
    graph_->ApplyTemplate(doc);
    RefreshPanels();
    ShowProblems();
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
    {
        auto* item = new QListWidgetItem(Tr("Нет сообщений", "No messages"), problemList_);
        item->setFlags(Qt::NoItemFlags);
        item->setForeground(QColor(0x80, 0x80, 0x80));
    }
}

void AiMachineEditorWindow::RefreshPanels()
{
    refreshing_ = true;
    if (variablesPanel_ && graph_) variablesPanel_->SetVariables(graph_->Document().Variables());

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

void AiMachineEditorWindow::AddVariable()
{
    if (variablesPanel_) variablesPanel_->AddVariable();
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
