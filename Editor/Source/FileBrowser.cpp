#include "FileBrowser.h"
#include "QtHost.h"
#include "MainWindow.h"

#include <QClipboard>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QGuiApplication>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMenu>
#include <QProcess>
#include <QRegularExpression>
#include <QTreeView>
#include <QUrl>
#include <QVBoxLayout>
#include <QPushButton>

#include <string>

FileBrowserDock::FileBrowserDock(EditorQtHost* host, QWidget* parent)
    : QDockWidget(QStringLiteral("Files"), parent)
    , host_(host)
{
    const QFileInfo projectFile(QString::fromUtf8(
        host_->host().projectFile ? host_->host().projectFile : ""));
    projectDir_ = projectFile.absoluteDir().absolutePath();
    contentDir_ = QString::fromUtf8(host_->host().contentRoot ? host_->host().contentRoot : "");
    if (projectDir_.isEmpty())
        projectDir_ = QDir::currentPath();

    BuildUi();
    SetRootPath(projectDir_);
}

void FileBrowserDock::BuildUi()
{
    auto* panel = new QWidget(this);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(2, 2, 2, 2);
    auto* imports = new QHBoxLayout;
    for (bool texture : {false, true})
    {
        auto* button = new QPushButton(texture ? QStringLiteral("Импорт текстуры…") : QStringLiteral("Импорт модели…"), panel);
        button->setObjectName(texture ? QStringLiteral("ImportTexture") : QStringLiteral("ImportModel"));
        button->setEnabled(host_->HasScene());
        connect(button, &QPushButton::clicked, this, [this, texture] {
            if (auto* editor = qobject_cast<EditorMainWindow*>(parentWidget())) editor->ImportAsset(texture);
        });
        imports->addWidget(button);
    }
    layout->addLayout(imports);

    // --- верхняя панель: фильтр + выбор корня ---
    auto* topBar = new QHBoxLayout;
    filterEdit_ = new QLineEdit(panel);
    filterEdit_->setPlaceholderText(QStringLiteral("filter: *.json;*.png ..."));
    connect(filterEdit_, &QLineEdit::textChanged, this,
            [this](const QString& t) { ApplyFilter(t); });
    topBar->addWidget(filterEdit_, 1);

    rootCombo_ = new QComboBox(panel);
    rootCombo_->addItem(QStringLiteral("Project"), projectDir_);
    rootCombo_->addItem(QStringLiteral("Content"), contentDir_);
    connect(rootCombo_, &QComboBox::currentIndexChanged, this,
            [this](int) { SetRootPath(rootCombo_->currentData().toString()); });
    topBar->addWidget(rootCombo_);
    layout->addLayout(topBar);

    // --- модель и дерево ---
    model_ = new QFileSystemModel(this);
    model_->setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);
    model_->setNameFilterDisables(false);   // не гасить неподходящее, а скрывать

    view_ = new QTreeView(panel);
    view_->setModel(model_);
    view_->hideColumn(3);                   // Date Modified не нужен
    view_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int c = 1; c < 3; ++c)
        view_->header()->setSectionResizeMode(c, QHeaderView::ResizeToContents);
    view_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(view_, &QTreeView::doubleClicked, this,
            [this](const QModelIndex& i) { OnDoubleClicked(i); });
    connect(view_, &QTreeView::customContextMenuRequested, this,
            [this](const QPoint& p) { OnContextMenu(p); });
    layout->addWidget(view_);

    setWidget(panel);
}

void FileBrowserDock::SetRootPath(const QString& path)
{
    if (path.isEmpty())
        return;
    view_->setRootIndex(model_->setRootPath(path));
}

void FileBrowserDock::ApplyFilter(const QString& text)
{
    QStringList filters;
    for (const QString& part : text.split(QRegularExpression(QStringLiteral("[;\\s]+")), Qt::SkipEmptyParts))
        filters << part.trimmed();
    if (filters.isEmpty())
        filters << QStringLiteral("*");
    model_->setNameFilters(filters);
}

void FileBrowserDock::OnDoubleClicked(const QModelIndex& index)
{
    if (!index.isValid() || model_->isDir(index))
        return;                             // каталоги раскрываются штатно

    const QString path = model_->filePath(index);
    if (path.endsWith(QStringLiteral(".scene.json"), Qt::CaseInsensitive))
    {
        if (auto* editor = qobject_cast<EditorMainWindow*>(parentWidget())) editor->OpenScene(path);
        return;
    }
    const QByteArray utf8 = path.toUtf8();  // именованный QByteArray: переживает вызов Log
    const std::string msg = "EditorOpenAsset: " + std::string(utf8.constData(), utf8.size());
    host_->Log(msg.c_str());                // аудит: лог движка + консоль редактора

    if (auto* editor = qobject_cast<EditorMainWindow*>(parentWidget())) editor->UseAsset(path);
}

void FileBrowserDock::OnContextMenu(const QPoint& pos)
{
    const QModelIndex index = view_->indexAt(pos);
    const bool hasFile = index.isValid();
    const QString path = hasFile ? model_->filePath(index) : QString();

    QMenu menu(this);
    QAction* use = hasFile && !model_->isDir(index) ? menu.addAction(QStringLiteral("Добавить модель / назначить текстуру")) : nullptr;
    QAction* open = hasFile ? menu.addAction(QStringLiteral("Open externally")) : nullptr;
    QAction* reveal = hasFile ? menu.addAction(QStringLiteral("Show in Explorer")) : nullptr;
    QAction* copy = hasFile ? menu.addAction(QStringLiteral("Copy path")) : nullptr;
    QAction* refresh = menu.addAction(QStringLiteral("Refresh"));

    QAction* picked = menu.exec(view_->viewport()->mapToGlobal(pos));
    if (!picked)
        return;

    if (picked == use)
    {
        if (auto* editor = qobject_cast<EditorMainWindow*>(parentWidget())) editor->UseAsset(path);
    }
    else if (picked == open)
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    else if (picked == reveal)
        // запятая после /select — обязательная часть синтаксиса explorer.exe
        QProcess::startDetached(QStringLiteral("explorer.exe"),
                                { QStringLiteral("/select,") + QDir::toNativeSeparators(path) });
    else if (picked == copy)
        QGuiApplication::clipboard()->setText(path);
    else if (picked == refresh)
        model_->setRootPath(model_->rootPath());   // принудительный рескан
}
