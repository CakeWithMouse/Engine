#include "AiMachineEditorWindow.h"
#include "CommandRegistry.h"
#include "EditorLocale.h"
#include "EditorWindowSetup.h"
#include "FsmGraphWidget.h"
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
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QStatusBar>
#include <QTableWidget>
#include <QTimer>
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

    memoryPanel_ = new QWidget(central);
    auto* panelLayout = new QVBoxLayout(memoryPanel_);
    panelLayout->setContentsMargins(8, 8, 8, 8);
    panelLayout->setSpacing(6);
    auto* headerWidget = new QWidget(memoryPanel_);
    auto* headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(4);
    headerLayout->addWidget(EditorWindowSetup::MakePanelHeader(Tr("Память и настройки", "Memory and settings"), headerWidget), 1);
    auto* help = new InfoButton(
        Tr("Память и настройки ИИ", "AI Memory and Settings"),
        Tr("Память автомата:\n"
           "• Ключи памяти хранят состояние и цели персонажа\n"
           "• Иконка глаза «Показ.» — публичный ключ (виден в инспекторе объекта)\n"
           "• Корзина в строке — быстрое удаление ключа\n\n"
           "Настройки персонажа:\n"
           "• «Цель восприятия» (тип Объект) — зрение персонажа записывает найденную цель\n"
           "• «Где видели» (тип Вектор) — координаты последней точки видимости\n"
           "• «Интервал решений» — секунды между пересчётами логики автомата",
           "AI State Machine memory:\n"
           "• Memory keys store character state and targets\n"
           "• Eye icon \"Show\" — public key (visible in object inspector)\n"
           "• Trash icon in row — quick delete key\n\n"
           "Character settings:\n"
           "• \"Perceived target\" (Object type) — vision writes detected target here\n"
           "• \"Last seen at\" (Vector type) — coordinates where target was last seen\n"
           "• \"Think interval\" — seconds between state machine decisions"),
        headerWidget);
    help->setObjectName(QStringLiteral("FsmMemoryHelp"));
    help->setFixedSize(22, 22);
    headerLayout->addWidget(help);
    panelLayout->addWidget(headerWidget);
    auto* memoryLabel = new QLabel(Tr("Память (ключи блоков)", "Memory (block keys)"), memoryPanel_);
    memoryLabel->setObjectName(QStringLiteral("CardSectionLabel"));
    panelLayout->addWidget(memoryLabel);
    variables_ = new QTableWidget(memoryPanel_);
    variables_->setObjectName(QStringLiteral("FsmVariables"));
    variablesPanel_ = new VariablePanelWidget(variables_, QStringLiteral("Fsm"),
        FsmGraphSchema::VariableTypes(), Tr("Ключ", "Key"), Tr("Начальное", "Initial"),
        [](const QString& t) { return FsmGraphSchema::TypeLabel(t); }, QString(), memoryPanel_);
    variablesPanel_->onVariablesChanged = [this](const QJsonArray& list, const QHash<QString, QString>& renames) {
        if (!ready_ || readOnly_) return;
        graph_->SetVariables(list, renames);
    };
    panelLayout->addWidget(variablesPanel_, 1);

    auto* settingsLabel = new QLabel(Tr("Настройки персонажа", "Character settings"), memoryPanel_);
    settingsLabel->setObjectName(QStringLiteral("CardSectionLabel"));
    panelLayout->addWidget(settingsLabel);
    auto* settings = new QFormLayout;
    settings->setContentsMargins(0, 0, 0, 0);
    settings->setSpacing(6);
    targetKey_ = new QComboBox(memoryPanel_);
    targetKey_->setObjectName(QStringLiteral("FsmTargetKey"));
    targetKey_->setToolTip(Tr("Ключ типа «Объект»: восприятие пишет сюда лучшую видимую цель (нужно зрение у персонажа).",
                              "An Object key: perception writes the best visible target here (the character needs sight)."));
    lastSeenKey_ = new QComboBox(memoryPanel_);
    lastSeenKey_->setObjectName(QStringLiteral("FsmLastSeenKey"));
    lastSeenKey_->setToolTip(Tr("Ключ типа «Вектор»: где цель видели последний раз.", "A Vector key: where the target was last seen."));
    thinkInterval_ = new QDoubleSpinBox(memoryPanel_);
    thinkInterval_->setObjectName(QStringLiteral("FsmThinkInterval"));
    thinkInterval_->setRange(-0.01, 60);
    thinkInterval_->setDecimals(2);
    thinkInterval_->setSingleStep(0.05);
    thinkInterval_->setSpecialValueText(Tr("как у персонажа", "the character's"));
    thinkInterval_->setToolTip(Tr("Секунды между решениями; 0 — каждый кадр.", "Seconds between thinks; 0 — every frame."));
    thinkInterval_->setKeyboardTracking(false);
    auto* memoryPanel = memoryPanel_;
    auto iconLabel = [memoryPanel](const QString& icon, const QString& text) {
        auto* row = new QWidget(memoryPanel);
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
    panelLayout->addLayout(settings);
    connect(targetKey_, &QComboBox::activated, this, [this] { EditSettings(); });
    connect(lastSeenKey_, &QComboBox::activated, this, [this] { EditSettings(); });
    connect(thinkInterval_, &QDoubleSpinBox::valueChanged, this, [this] { EditSettings(); });
    memoryPanel_->setEnabled(false);
    graph_->EmbedSidePanel(memoryPanel_);

    auto* save = new QPushButton(IconProvider::SaveIcon(), QString(), central);
    save->setObjectName(QStringLiteral("FsmSave"));
    save->setFixedSize(34, 26);
    save->setIconSize(QSize(16, 16));
    save->setToolTip(Tr("Сохранить: записать файл и проверить движком сразу", "Save: write the file and check with the engine now"));
    connect(save, &QPushButton::clicked, this, [this] { SaveNow(); });
    graph_->PrependToolbarWidget(save);
    for (auto* button : variablesPanel_->ToolbarButtons())
        graph_->AddToolbarWidget(button);
    auto* newMachine = new QPushButton(IconProvider::RobotIcon(), QString(), central);
    newMachine->setObjectName(QStringLiteral("FsmNewMachine"));
    newMachine->setFixedSize(34, 26);
    newMachine->setIconSize(QSize(16, 16));
    newMachine->setToolTip(Tr("Новый автомат: создать .fsm в той же папке", "New machine: create a .fsm in the same folder"));
    connect(newMachine, &QPushButton::clicked, this, [this] {
        CommandRegistry::Instance().Execute(QStringLiteral("Assets.CreateAiMachine"),
            {{QStringLiteral("directory"), QFileInfo(PhysicalPath()).absolutePath()}});
    });
    graph_->AddToolbarWidget(newMachine);

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
    ready_ = true;
    if (document.IsNewerFormat() || document.SchemaVersion() > schema_->Version())
        return SetReadOnly(Tr("Файл записан более новой версией редактора: он открыт только для чтения.",
                              "The file was written by a newer editor: it is open read-only."));
    graph_->setEnabled(true);
    memoryPanel_->setEnabled(true);
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
    memoryPanel_->setEnabled(false);
    statusBar()->showMessage(Tr("Только для чтения", "Read-only"));
}

bool AiMachineEditorWindow::WriteFile()
{
    if (!ready_ || readOnly_) return false;
    QString error;
    if (EditorCore::GraphFileIo::Write(ProjectRoot(), QString::fromUtf8(host_->host().contentRoot), PhysicalPath(),
                                       graph_->Document().ExportToJson().toUtf8(), &error))
    {
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
