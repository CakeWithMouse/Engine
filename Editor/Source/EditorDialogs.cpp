#include "EditorDialogs.h"
#include "EditorLocale.h"

#include <QSettings>
#include <QDialog>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QListWidget>
#include <QTextBrowser>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDialogButtonBox>
#include <QCoreApplication>
#include <QTextDocument>

namespace {
    std::vector<std::pair<int, std::function<void(ControlScheme)>>> s_schemeSubscribers;
    int s_nextSubId = 1;
}

ControlScheme EditorSettings::GetControlScheme()
{
    QSettings settings(QStringLiteral("PainEngine"), QStringLiteral("Editor"));
    const QString val = settings.value(QStringLiteral("viewport/control_scheme"), QStringLiteral("unreal_unity")).toString();
    if (val.toLower() == QStringLiteral("blender")) {
        return ControlScheme::Blender;
    }
    return ControlScheme::UnrealUnity;
}

void EditorSettings::SetControlScheme(ControlScheme scheme)
{
    QSettings settings(QStringLiteral("PainEngine"), QStringLiteral("Editor"));
    settings.setValue(QStringLiteral("viewport/control_scheme"),
        scheme == ControlScheme::Blender ? QStringLiteral("blender") : QStringLiteral("unreal_unity"));

    for (const auto& sub : s_schemeSubscribers) {
        if (sub.second) sub.second(scheme);
    }
}

SpawnLocationMode EditorSettings::GetSpawnLocationMode()
{
    QSettings settings(QStringLiteral("PainEngine"), QStringLiteral("Editor"));
    const QString val = settings.value(QStringLiteral("viewport/spawn_location_mode"), QStringLiteral("cursor")).toString();
    if (val.toLower() == QStringLiteral("origin")) {
        return SpawnLocationMode::WorldOrigin;
    }
    return SpawnLocationMode::UnderCursor;
}

void EditorSettings::SetSpawnLocationMode(SpawnLocationMode mode)
{
    QSettings settings(QStringLiteral("PainEngine"), QStringLiteral("Editor"));
    settings.setValue(QStringLiteral("viewport/spawn_location_mode"),
        mode == SpawnLocationMode::WorldOrigin ? QStringLiteral("origin") : QStringLiteral("cursor"));
}

int EditorSettings::SubscribeControlSchemeChanged(std::function<void(ControlScheme)> callback)
{
    const int id = s_nextSubId++;
    s_schemeSubscribers.push_back({id, std::move(callback)});
    return id;
}

void EditorSettings::UnsubscribeControlSchemeChanged(int id)
{
    for (auto it = s_schemeSubscribers.begin(); it != s_schemeSubscribers.end(); ++it) {
        if (it->first == id) {
            s_schemeSubscribers.erase(it);
            break;
        }
    }
}

void ShowDocumentationDialog(QWidget* parent, const QString& projectRoot)
{
    auto* dialog = new QDialog(parent);
    dialog->setWindowTitle(Tr("Документация и Паспорт проекта", "Documentation & Project Passport"));
    dialog->resize(950, 620);

    auto* mainLayout = new QVBoxLayout(dialog);
    mainLayout->setContentsMargins(8, 8, 8, 8);

    auto* splitter = new QSplitter(Qt::Horizontal, dialog);

    // Левая панель: фильтр и список разделов
    auto* leftWidget = new QWidget(splitter);
    auto* leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    auto* sectionFilter = new QLineEdit(leftWidget);
    sectionFilter->setPlaceholderText(Tr("Поиск раздела…", "Search section…"));
    leftLayout->addWidget(sectionFilter);

    auto* listWidget = new QListWidget(leftWidget);
    listWidget->setMinimumWidth(220);
    leftLayout->addWidget(listWidget);

    // Правая панель: поиск по тексту и Markdown браузер
    auto* rightWidget = new QWidget(splitter);
    auto* rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    auto* searchBar = new QHBoxLayout;
    auto* textSearchEdit = new QLineEdit(rightWidget);
    textSearchEdit->setPlaceholderText(Tr("Поиск в документе…", "Find in document…"));
    auto* findNextBtn = new QPushButton(Tr("Найти далее", "Find Next"), rightWidget);
    searchBar->addWidget(textSearchEdit);
    searchBar->addWidget(findNextBtn);
    rightLayout->addLayout(searchBar);

    auto* browser = new QTextBrowser(rightWidget);
    browser->setOpenExternalLinks(true);
    rightLayout->addWidget(browser);

    splitter->addWidget(leftWidget);
    splitter->addWidget(rightWidget);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);
    mainLayout->addWidget(splitter);

    auto* btnBox = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    QObject::connect(btnBox, &QDialogButtonBox::rejected, dialog, &QDialog::accept);
    mainLayout->addWidget(btnBox);

    // Структура документов
    struct DocItem {
        QString title;
        QString content;
    };
    std::vector<DocItem> docs;

    // 1. Паспорт проекта и горячие клавиши
    const QString passportMd = QString::fromUtf8(R"MD(# PainEngine — Паспорт проекта и Управление

## Схемы управления во Вьюпорте

### Схема Unreal / Unity (по умолчанию)
* **ПКМ + перемещение мыши**: свободный обзор камеры (Look around).
* **WASD**: перемещение камеры вперёд, влево, назад, вправо.
* **Q / E**: опускание и подъём камеры по высоте.
* **СКМ (колёсико) + перемещение**: панорамирование (Pan).
* **W**: переключить гизмо на **Перемещение** (Translate).
* **E**: переключить гизмо на **Вращение** (Rotate).
* **R**: переключить гизмо на **Масштаб** (Scale).

### Схема Blender
* **СКМ (колёсико) + перемещение**: вращение камеры вокруг точки (Orbit).
* **Shift + СКМ + перемещение**: панорамирование (Pan).
* **ПКМ + мышь**: свободный полёт камеры с клавишами WASD.
* **G**: гизмо перемещения (Grab / Translate).
* **R**: гизмо вращения (Rotate).
* **S**: гизмо масштабирования (Scale).
* **Shift + A**: быстрое меню создания объекта под курсором.

---

## Общие горячие клавиши
* **Space**: запуск режима Play / остановка (Edit).
* **P**: пауза Play / возобновление.
* **1 / 2 / 3**: режимы гизмо Перемещение / Вращение / Масштаб (работают в любой схеме).
* **F**: сфокусировать камеру на выбранном объекте (Focus).
* **Tab**: циклический выбор объектов сцены.
* **Escape**: сброс выделения объекта.
* **Delete**: удалить выбранный объект (в окне Outliner).
* **Ctrl + D**: дублировать объект.
* **Ctrl + Z**: отмена действия (Undo).
* **Ctrl + Y**: повтор действия (Redo).
* **Ctrl + S**: сохранить сцену и назначить стартовой.
* **F6**: перекомпиляция всех шейдеров HLSL на лету.
* **F1**: справка и документация.
)MD");
    docs.push_back({Tr("Паспорт и Горячие клавиши", "Passport & Hotkeys"), passportMd});

    // 2. Сканирование Engine/docs/
    QString appDir = QCoreApplication::applicationDirPath();
    QDir engineDocsDir(appDir + QStringLiteral("/../../../Engine/docs"));
    if (!engineDocsDir.exists()) {
        engineDocsDir = QDir(QDir::currentPath() + QStringLiteral("/Engine/docs"));
    }
    if (!engineDocsDir.exists() && !projectRoot.isEmpty()) {
        engineDocsDir = QDir(projectRoot + QStringLiteral("/Engine/docs"));
    }

    if (engineDocsDir.exists()) {
        const auto files = engineDocsDir.entryInfoList({QStringLiteral("*.md")}, QDir::Files, QDir::Name);
        for (const auto& fi : files) {
            QFile f(fi.absoluteFilePath());
            if (f.open(QIODevice::ReadOnly)) {
                docs.push_back({fi.baseName(), QString::fromUtf8(f.readAll())});
            }
        }
    }

    // Заполнение списка
    auto updateList = [&](const QString& filter) {
        listWidget->clear();
        for (size_t i = 0; i < docs.size(); ++i) {
            if (filter.isEmpty() || docs[i].title.contains(filter, Qt::CaseInsensitive)) {
                auto* item = new QListWidgetItem(docs[i].title, listWidget);
                item->setData(Qt::UserRole, static_cast<int>(i));
            }
        }
        if (listWidget->count() > 0) {
            listWidget->setCurrentRow(0);
        }
    };

    updateList(QString());

    QObject::connect(sectionFilter, &QLineEdit::textChanged, dialog, updateList);

    QObject::connect(listWidget, &QListWidget::currentRowChanged, dialog, [&](int row) {
        if (row < 0 || row >= listWidget->count()) return;
        const int docIdx = listWidget->item(row)->data(Qt::UserRole).toInt();
        if (docIdx >= 0 && docIdx < static_cast<int>(docs.size())) {
            browser->setMarkdown(docs[docIdx].content);
        }
    });

    auto doFind = [browser, textSearchEdit] {
        const QString text = textSearchEdit->text();
        if (!text.isEmpty()) {
            browser->find(text);
        }
    };
    QObject::connect(findNextBtn, &QPushButton::clicked, dialog, doFind);
    QObject::connect(textSearchEdit, &QLineEdit::returnPressed, dialog, doFind);

    if (listWidget->count() > 0) {
        listWidget->setCurrentRow(0);
    }

    dialog->exec();
    dialog->deleteLater();
}

