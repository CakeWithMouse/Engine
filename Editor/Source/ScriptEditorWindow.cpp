#include "ScriptEditorWindow.h"
#include "EditorLocale.h"
#include "ScriptGraphWidget.h"

#include <QComboBox>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QFileInfo>
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

using EditorCore::CodeGraphSchema;
using EditorCore::GraphDocument;

namespace
{
QString VariableText(const QJsonValue& value, const QString& type)
{
    if (type == QStringLiteral("bool")) return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    if (type == QStringLiteral("vec3"))
    {
        const QJsonArray list = value.toArray();
        QStringList parts;
        for (int i = 0; i < 3; ++i) parts << QString::number(i < list.size() ? list[i].toDouble() : 0.0, 'g', 6);
        return parts.join(QStringLiteral(", "));
    }
    if (type == QStringLiteral("float")) return QString::number(value.toDouble(), 'g', 6);
    return value.toString();
}

// Значение переменной из текста ячейки; неразборчивый текст даёт ноль своего типа.
QJsonValue VariableValue(const QString& text, const QString& type)
{
    const QString trimmed = text.trimmed();
    if (type == QStringLiteral("bool"))
        return QStringList{QStringLiteral("true"), QStringLiteral("1"), QStringLiteral("да"), QStringLiteral("yes")}.contains(trimmed.toLower());
    if (type == QStringLiteral("vec3"))
    {
        const QStringList parts = trimmed.split(QRegularExpression(QStringLiteral("[,;\\s]+")), Qt::SkipEmptyParts);
        QJsonArray list;
        for (int i = 0; i < 3; ++i) list.append(i < parts.size() ? parts[i].toDouble() : 0.0);
        return list;
    }
    if (type == QStringLiteral("float")) return trimmed.toDouble();
    return text;
}

struct ProblemRow
{
    int node = 0;
    QString object, message;
    bool runtime = false;
};

void ENGINE_MODULE_CALL CollectProblem(void* user, const EngineScriptProblem* problem)
{
    static_cast<QVector<ProblemRow>*>(user)->append({problem->node, QString::fromUtf8(problem->objectName),
                                                     QString::fromUtf8(problem->message), problem->runtime != 0});
}
} // namespace

ScriptEditorWindow::ScriptEditorWindow(EditorQtHost* host, const QString& script, QWidget* parent)
    : QMainWindow(parent), host_(host), script_(script), schema_(std::make_unique<CodeGraphSchema>())
{
    setAttribute(Qt::WA_DeleteOnClose);
    setObjectName(QStringLiteral("ScriptEditorWindow"));
    setWindowTitle(Tr("Скрипт — %1", "Script — %1").arg(QFileInfo(script).fileName()));
    resize(1200, 760);
    BuildUi();
    LoadCatalog();
}

ScriptEditorWindow::~ScriptEditorWindow()
{
    // Правка, ждущая паузы, не теряется при закрытии окна.
    if (saveTimer_ && saveTimer_->isActive()) WriteFile();
}

void ScriptEditorWindow::BuildUi()
{
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
    layout->addWidget(graph_, 1);

    problemList_ = new QListWidget(central);
    problemList_->setObjectName(QStringLiteral("ScriptProblems"));
    problemList_->setMaximumHeight(140);
    problemList_->setToolTip(Tr("Ошибки графа и Play; щелчок показывает ноду", "Graph and Play problems; click to show the node"));
    connect(problemList_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        if (const int node = item->data(Qt::UserRole).toInt()) graph_->FocusNode(node);
    });
    layout->addWidget(problemList_);
    setCentralWidget(central);

    auto* dock = new QDockWidget(Tr("Переменные", "Variables"), this);
    dock->setObjectName(QStringLiteral("ScriptVariablesDock"));
    dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    auto* panel = new QWidget(dock);
    auto* panelLayout = new QVBoxLayout(panel);
    variables_ = new QTableWidget(0, 3, panel);
    variables_->setObjectName(QStringLiteral("ScriptVariables"));
    variables_->setHorizontalHeaderLabels({Tr("Имя", "Name"), Tr("Тип", "Type"), Tr("Значение", "Value")});
    variables_->horizontalHeader()->setStretchLastSection(true);
    variables_->verticalHeader()->hide();
    variables_->setSelectionBehavior(QAbstractItemView::SelectRows);
    connect(variables_, &QTableWidget::cellChanged, this, [this](int row, int column) { EditVariables(row, column); });
    panelLayout->addWidget(variables_, 1);
    auto* buttons = new QHBoxLayout;
    auto* add = new QPushButton(Tr("+ Переменная", "+ Variable"), panel);
    add->setObjectName(QStringLiteral("ScriptAddVariable"));
    connect(add, &QPushButton::clicked, this, [this] { AddVariable(); });
    auto* remove = new QPushButton(Tr("Удалить", "Remove"), panel);
    remove->setObjectName(QStringLiteral("ScriptRemoveVariable"));
    connect(remove, &QPushButton::clicked, this, [this] { RemoveVariable(); });
    buttons->addWidget(add);
    buttons->addWidget(remove);
    panelLayout->addLayout(buttons);
    auto* hint = new QLabel(Tr("Переменные хранят значения между кадрами. Вектор — три числа через запятую.",
                               "Variables keep values between frames. A vector is three numbers separated by commas."), panel);
    hint->setWordWrap(true);
    panelLayout->addWidget(hint);
    dock->setWidget(panel);
    addDockWidget(Qt::LeftDockWidgetArea, dock);
    dock->setEnabled(false);

    saveTimer_ = new QTimer(this);
    saveTimer_->setSingleShot(true);
    saveTimer_->setInterval(300);
    connect(saveTimer_, &QTimer::timeout, this, [this] { SaveNow(); });
    playTimer_ = new QTimer(this);
    playTimer_->setInterval(1000);
    connect(playTimer_, &QTimer::timeout, this, [this] { PollPlayProblems(); });

    graph_->structureChanged = [this] { saveTimer_->start(); };
    graph_->layoutChanged = [this] { WriteFile(); };
    graph_->variablesChanged = [this] { RefreshVariables(); };
    statusBar()->showMessage(Tr("Загрузка каталога нод…", "Loading the node catalog…"));
}

QString ScriptEditorWindow::PhysicalPath() const
{
    return QDir(QString::fromUtf8(host_->host().contentRoot)).filePath(script_.mid(6)); // без "/Game/"
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
        return SetReadOnly(Tr("Игровой модуль проекта не поддерживает визуальные скрипты. Обновите Game.dll проекта: создайте проект "
                              "в новой версии движка или обновите копию проекта.",
                              "The project's game module does not support visual scripts. Update the project's Game.dll."));
    if (!schema_->LoadCatalog(catalog, &error)) return SetReadOnly(error);

    QFile file(PhysicalPath());
    GraphDocument document;
    if (!file.open(QIODevice::ReadOnly) || !document.ImportFromJson(QString::fromUtf8(file.readAll())))
        return SetReadOnly(Tr("Файл %1 не читается как граф.", "Cannot read %1 as a graph.").arg(script_));
    if (document.FileFormat() != GraphDocument::Format::V1 || document.Schema() != schema_->Id())
        return SetReadOnly(Tr("%1 — не визуальный скрипт.", "%1 is not a visual script.").arg(script_));

    graph_->LoadDocument(document);
    ready_ = true;
    if (document.IsNewerFormat() || document.SchemaVersion() > schema_->Version())
        return SetReadOnly(Tr("Файл записан более новой версией редактора: он открыт только для чтения.",
                              "The file was written by a newer editor: it is open read-only."));
    graph_->setEnabled(true);
    findChild<QDockWidget*>(QStringLiteral("ScriptVariablesDock"))->setEnabled(true);
    statusBar()->showMessage(Tr("Правки сохраняются сразу; Play берёт файл при запуске.", "Edits are saved at once; Play reads the file when it starts."));
    Check();
    playTimer_->start();
}

void ScriptEditorWindow::SetReadOnly(const QString& reason)
{
    readOnly_ = true;
    status_ = reason;
    banner_->setText(reason);
    banner_->show();
    graph_->setEnabled(false);
    statusBar()->showMessage(Tr("Только для чтения", "Read-only"));
}

bool ScriptEditorWindow::WriteFile()
{
    if (!ready_ || readOnly_) return false;
    QSaveFile file(PhysicalPath());
    if (file.open(QIODevice::WriteOnly) && file.write(graph_->Document().ExportToJson().toUtf8()) >= 0 && file.commit()) return true;
    statusBar()->showMessage(Tr("Не удалось записать %1: %2", "Cannot write %1: %2").arg(script_, file.errorString()));
    return false;
}

void ScriptEditorWindow::SaveNow()
{
    saveTimer_->stop();
    if (WriteFile()) Check();
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
            for (const auto& row : rows) self->checkProblems_.append({row.node, {}, row.message, false});
            if (!checked) self->checkProblems_.append({0, {}, Tr("Движок не смог проверить скрипт.", "The engine could not check the script."), false});
            ++self->checks_;
            self->ShowProblems();
        });
    };
    host_->PostCommand(c);
}

void ScriptEditorWindow::PollPlayProblems()
{
    if (host_->PlayState() == EditorQtHost::Edit) return; // после Stop остаются ошибки последнего Play
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
            for (const auto& row : rows) self->playProblems_.append({row.node, row.object, row.message, true});
            self->ShowProblems();
        });
    };
    host_->PostCommand(c);
}

QHash<int, QString> ScriptEditorWindow::Problems() const
{
    QHash<int, QString> problems;
    for (const auto* list : {&checkProblems_, &playProblems_})
        for (const auto& problem : *list)
        {
            if (!problem.node) continue;
            const QString text = problem.object.isEmpty() ? problem.message : QStringLiteral("Play, %1: %2").arg(problem.object, problem.message);
            problems[problem.node] = problems.value(problem.node).isEmpty() ? text : problems.value(problem.node) + QLatin1Char('\n') + text;
        }
    return problems;
}

void ScriptEditorWindow::ShowProblems()
{
    problemList_->clear();
    for (const auto* list : {&checkProblems_, &playProblems_})
        for (const auto& problem : *list)
        {
            QString where = problem.node ? Tr("нода %1", "node %1").arg(problem.node) : Tr("файл", "file");
            if (problem.runtime || !problem.object.isEmpty()) where = QStringLiteral("Play · %1 · %2").arg(problem.object, where);
            auto* item = new QListWidgetItem(QStringLiteral("%1: %2").arg(where, problem.message), problemList_);
            item->setData(Qt::UserRole, problem.node);
        }
    graph_->SetNodeProblems(Problems());
    if (problemList_->count() == 0)
        (new QListWidgetItem(Tr("Ошибок нет", "No problems"), problemList_))->setFlags(Qt::NoItemFlags);
}

void ScriptEditorWindow::RefreshVariables()
{
    refreshingVariables_ = true;
    const QJsonArray list = graph_->Document().Variables();
    variables_->setRowCount(list.size());
    for (int row = 0; row < list.size(); ++row)
    {
        const QJsonObject variable = list[row].toObject();
        const QString name = variable.value(QStringLiteral("name")).toString();
        const QString type = variable.value(QStringLiteral("type")).toString();
        auto* nameItem = new QTableWidgetItem(name);
        nameItem->setData(Qt::UserRole, name); // прежнее имя: переименование переносится на ноды
        variables_->setItem(row, 0, nameItem);
        auto* combo = new QComboBox(variables_);
        combo->setObjectName(QStringLiteral("ScriptVariableType_%1").arg(row));
        for (const QString& candidate : CodeGraphSchema::VariableTypes()) combo->addItem(CodeGraphSchema::TypeLabel(candidate), candidate);
        combo->setCurrentIndex(std::max(0, combo->findData(type)));
        connect(combo, &QComboBox::currentIndexChanged, this, [this, row] { EditVariables(row, 1); });
        variables_->setCellWidget(row, 1, combo);
        variables_->setItem(row, 2, new QTableWidgetItem(VariableText(variable.value(QStringLiteral("value")), type)));
    }
    refreshingVariables_ = false;
}

void ScriptEditorWindow::EditVariables(int, int)
{
    if (refreshingVariables_ || !ready_ || readOnly_) return;
    const QJsonArray before = graph_->Document().Variables();
    QJsonArray list;
    QHash<QString, QString> renames;
    for (int row = 0; row < variables_->rowCount(); ++row)
    {
        const auto* nameItem = variables_->item(row, 0);
        const auto* combo = static_cast<QComboBox*>(variables_->cellWidget(row, 1));
        const auto* valueItem = variables_->item(row, 2);
        if (!nameItem || !combo) continue;
        const QString name = nameItem->text().trimmed(), old = nameItem->data(Qt::UserRole).toString();
        const QString type = combo->currentData().toString();
        const QString oldType = row < before.size() ? before[row].toObject().value(QStringLiteral("type")).toString() : type;
        // Новый тип начинает с нуля: число "5" не превращается в вектор.
        const QJsonValue value = type == oldType ? VariableValue(valueItem ? valueItem->text() : QString(), type) : VariableValue(QString(), type);
        list.append(QJsonObject{{QStringLiteral("name"), name}, {QStringLiteral("type"), type}, {QStringLiteral("value"), value}});
        if (!old.isEmpty() && old != name) renames.insert(old, name);
    }
    // Перестройка таблицы из обработчика её же сигнала — после возврата из него.
    QTimer::singleShot(0, this, [this, list, renames] { graph_->SetVariables(list, renames); });
}

void ScriptEditorWindow::AddVariable()
{
    if (!ready_ || readOnly_) return;
    QJsonArray list = graph_->Document().Variables();
    QStringList taken;
    for (const auto& value : list) taken << value.toObject().value(QStringLiteral("name")).toString();
    QString name = QStringLiteral("Var");
    for (int i = 2; taken.contains(name); ++i) name = QStringLiteral("Var%1").arg(i);
    list.append(QJsonObject{{QStringLiteral("name"), name}, {QStringLiteral("type"), QStringLiteral("float")}, {QStringLiteral("value"), 0}});
    graph_->SetVariables(list);
}

void ScriptEditorWindow::RemoveVariable()
{
    const int row = variables_->currentRow();
    if (!ready_ || readOnly_ || row < 0) return;
    QJsonArray list = graph_->Document().Variables();
    if (row >= list.size()) return;
    list.removeAt(row);
    graph_->SetVariables(list);
}
