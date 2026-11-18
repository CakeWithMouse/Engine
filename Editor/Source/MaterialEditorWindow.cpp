#include "MaterialEditorWindow.h"
#include "MaterialTextureTools.h"
#include "MaterialGraphWidget.h"
#include "HlslGraphCompiler.h"
#include "HlslToGraph.h"
#include "Graph/GraphFileIo.h"
#include "CollapsibleCard.h"
#include "EditorTheme.h"
#include "EditorWindowSetup.h"
#include "InfoButton.h"
#include "MaterialEvents.h"
#include "PreviewWidget.h"
#include "IconProvider.h"

#include <QAction>
#include <QCheckBox>
#include <QCloseEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTabWidget>
#include <QStatusBar>
#include <QSyntaxHighlighter>
#include <QTextBlock>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

namespace
{
QString Utf8(const char* text) { return QString::fromUtf8(text ? text : ""); }

void ENGINE_MODULE_CALL ReadVisitor(void* user, const EngineMaterialInfo* info)
{
    auto& state = *static_cast<MaterialEditorState*>(user);
    state.shader = Utf8(info->shader);
    state.errors = Utf8(info->errors);
    state.render = info->render;
    state.modified = info->modified != 0;
    for (uint32_t i = 0; i < info->parameterCount; ++i)
    {
        const EngineMaterialParameter& source = info->parameters[i];
        MaterialEditorState::Parameter parameter;
        parameter.name = Utf8(source.name);
        parameter.components = source.components;
        std::copy(source.value, source.value + 4, parameter.value.begin());
        std::copy(source.defaultValue, source.defaultValue + 4, parameter.defaultValue.begin());
        state.parameters.push_back(parameter);
    }
    for (uint32_t i = 0; i < info->textureCount; ++i)
        state.textures.push_back({Utf8(info->textures[i].name), Utf8(info->textures[i].path)});
}

// Поток движка. Пустая строка — успех.
QString ReadMaterial(const EngineEditSessionApi& api, const QByteArray& material, MaterialEditorState& state)
{
    char error[4096]{};
    if (api.readMaterial(api.context, material.constData(), ReadVisitor, &state, error, sizeof(error)) == EngineSceneOk)
        return {};
    return error[0] ? Utf8(error) : QStringLiteral("Не удалось прочитать материал");
}

QString StatusText(uint32_t status, const char* error)
{
    if (status == EngineSceneOk) return {};
    if (error && *error) return Utf8(error);
    return status == EngineSceneNotFound
               ? QStringLiteral("Материал не загружен ни в одной сцене")
               : QStringLiteral("Операция с материалом не выполнена (код %1)").arg(status);
}

// Поток движка. outside: файл вне проекта, сначала копируется в Content/Textures, его /Game путь
// пишется в importedPath. apply = false: только импорт (слота у материала пока нет).
QString SetTexture(const EngineEditSessionApi& api, const QByteArray& material, const QByteArray& slot,
                   const QByteArray& path, const QByteArray& outside, bool apply = true, QByteArray* importedPath = nullptr)
{
    QByteArray texture = path;
    char error[4096]{};
    if (!outside.isEmpty())
    {
        char imported[4096]{};
        const uint32_t status = api.importAsset(api.context, outside.constData(), 1, imported, sizeof(imported), error, sizeof(error));
        if (status == EngineSceneReadOnly) return QStringLiteral("Импорт текстуры недоступен во время Play");
        if (status != EngineSceneOk) return StatusText(status, error);
        texture = imported;
        if (importedPath) *importedPath = texture;
    }
    if (!apply) return {};
    return StatusText(api.setMaterialTexture(api.context, material.constData(), slot.constData(), texture.constData(),
                                             error, sizeof(error)), error);
}

QColor ToColor(const std::array<float, 4>& value, uint32_t components)
{
    auto channel = [](float v) { return std::clamp(v, 0.0f, 1.0f); };
    return QColor::fromRgbF(channel(value[0]), channel(value[1]), channel(value[2]),
                            components == 4 ? channel(value[3]) : 1.0f);
}

class HlslHighlighter final : public QSyntaxHighlighter
{
public:
    explicit HlslHighlighter(QTextDocument* document) : QSyntaxHighlighter(document)
    {
        auto rule = [this](const char* pattern, const QColor& color) {
            QTextCharFormat format;
            format.setForeground(color);
            rules_.push_back({QRegularExpression(QString::fromLatin1(pattern)), format});
        };
        rule(R"(\b(if|else|for|while|do|return|break|continue|discard|switch|case|default|struct|cbuffer|register|)"
             R"(static|const|in|out|inout|uniform|true|false)\b)", QColor(86, 156, 214));
        rule(R"(\b(void|bool|int|uint|half|float|double)([1-4](x[1-4])?)?\b|)"
             R"(\b(Texture2D|Texture3D|TextureCube|SamplerState|MaterialInput|MaterialOutput)\b)", QColor(78, 201, 176));
        rule(R"((\b[0-9]+\.?[0-9]*|\.[0-9]+)([eE][+-]?[0-9]+)?[fFhHuU]?\b)", QColor(181, 206, 168));
        rule(R"(^\s*#\s*\w+)", QColor(197, 134, 192));
        comment_.setForeground(QColor(106, 153, 85));
    }

protected:
    void highlightBlock(const QString& text) override
    {
        for (const auto& rule : rules_)
            for (auto match = rule.pattern.globalMatch(text); match.hasNext();)
            {
                const auto m = match.next();
                setFormat(static_cast<int>(m.capturedStart()), static_cast<int>(m.capturedLength()), rule.format);
            }

        // Комментарии последними: перекрывают всё внутри себя. Состояние блока 1 — внутри /* */.
        setCurrentBlockState(0);
        int start = previousBlockState() == 1 ? 0 : -1;
        int from = 0;
        while (true)
        {
            if (start < 0)
            {
                const int line = text.indexOf(QStringLiteral("//"), from);
                const int block = text.indexOf(QStringLiteral("/*"), from);
                if (line >= 0 && (block < 0 || line < block))
                {
                    setFormat(line, static_cast<int>(text.size()) - line, comment_);
                    return;
                }
                if (block < 0) return;
                start = block;
                from = block + 2;
            }
            const int end = text.indexOf(QStringLiteral("*/"), from);
            if (end < 0)
            {
                setFormat(start, static_cast<int>(text.size()) - start, comment_);
                setCurrentBlockState(1);
                return;
            }
            setFormat(start, end + 2 - start, comment_);
            from = end + 2;
            start = -1;
        }
    }

private:
    struct Rule
    {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    std::vector<Rule> rules_;
    QTextCharFormat comment_;
};
}

// Текст поверхности с номерами строк; строки с ошибками подсвечены.
class MaterialSourceEditor final : public QPlainTextEdit
{
public:
    explicit MaterialSourceEditor(QWidget* parent) : QPlainTextEdit(parent), numbers_(new Numbers(this))
    {
        setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        setTabStopDistance(4 * fontMetrics().horizontalAdvance(QLatin1Char(' ')));
        setLineWrapMode(QPlainTextEdit::NoWrap);
        new HlslHighlighter(document());
        connect(this, &QPlainTextEdit::blockCountChanged, this, [this] { UpdateMargin(); });
        connect(this, &QPlainTextEdit::updateRequest, this, [this](const QRect& rect, int dy)
        {
            if (dy) numbers_->scroll(0, dy);
            else numbers_->update(0, rect.y(), numbers_->width(), rect.height());
        });
        UpdateMargin();
    }

    void SetErrorLines(const QSet<int>& lines)
    {
        errorLines_ = lines;
        QList<QTextEdit::ExtraSelection> selections;
        for (const int line : lines)
        {
            const QTextBlock block = document()->findBlockByNumber(line - 1);
            if (!block.isValid()) continue;
            QTextEdit::ExtraSelection selection;
            selection.format.setBackground(QColor(120, 40, 40, 90));
            selection.format.setProperty(QTextFormat::FullWidthSelection, true);
            selection.cursor = QTextCursor(block);
            selections.push_back(selection);
        }
        setExtraSelections(selections);
        numbers_->update();
    }

protected:
    void resizeEvent(QResizeEvent* event) override
    {
        QPlainTextEdit::resizeEvent(event);
        const QRect area = contentsRect();
        numbers_->setGeometry(area.left(), area.top(), MarginWidth(), area.height());
    }

private:
    class Numbers final : public QWidget
    {
    public:
        explicit Numbers(MaterialSourceEditor* editor) : QWidget(editor), editor_(editor) {}

    protected:
        void paintEvent(QPaintEvent* event) override { editor_->PaintNumbers(event); }

    private:
        MaterialSourceEditor* editor_;
    };

    int MarginWidth() const
    {
        const int digits = std::max(3, static_cast<int>(QString::number(blockCount()).size()));
        return 12 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
    }

    void UpdateMargin() { setViewportMargins(MarginWidth(), 0, 0, 0); }

    void PaintNumbers(QPaintEvent* event)
    {
        QPainter painter(numbers_);
        painter.fillRect(event->rect(), palette().window());
        QTextBlock block = firstVisibleBlock();
        int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
        while (block.isValid() && top <= event->rect().bottom())
        {
            const int bottom = top + qRound(blockBoundingRect(block).height());
            if (block.isVisible() && bottom >= event->rect().top())
            {
                const int line = block.blockNumber() + 1;
                painter.setPen(errorLines_.contains(line) ? QColor(230, 90, 90) : palette().placeholderText().color());
                painter.drawText(0, top, numbers_->width() - 6, fontMetrics().height(), Qt::AlignRight,
                                 QString::number(line));
            }
            block = block.next();
            top = bottom;
        }
    }

    Numbers* numbers_;
    QSet<int> errorLines_;
};

MaterialEditorWindow::MaterialEditorWindow(EditorQtHost* host, const QString& material, QWidget* parent)
    : QMainWindow(nullptr, Qt::Window), host_(host), material_(material)
{
    EditorWindowSetup::ConfigureSubWindow(this, parent);
    setAttribute(Qt::WA_DeleteOnClose, false); // удаляет Shutdown, после отключения превью
    setObjectName(QStringLiteral("MaterialEditor"));
    BuildUi();
    UpdateTitle();
    resize(1200, 760);
}

QString MaterialEditorWindow::PhysicalPath(const QString& virtualPath) const
{
    if (!virtualPath.startsWith(QStringLiteral("/Game/"))) return {};
    return QDir(QString::fromUtf8(host_->host().contentRoot)).filePath(virtualPath.mid(6));
}

QString MaterialEditorWindow::ProjectRoot() const
{
    return QFileInfo(QString::fromUtf8(host_->host().projectFile)).dir().absolutePath();
}

QString MaterialEditorWindow::ContentRoot() const
{
    return QString::fromUtf8(host_->host().contentRoot);
}

void MaterialEditorWindow::BuildUi()
{
    auto* tools = addToolBar(QStringLiteral("Материал"));
    tools->setMovable(false);
    auto* save = tools->addAction(IconProvider::SaveIcon(), QString(), this, [this] { Save(); });
    save->setObjectName(QStringLiteral("MaterialSave"));
    save->setShortcut(QKeySequence::Save);
    save->setToolTip(Tr("Сохранить (Ctrl+S)", "Save (Ctrl+S)"));
    auto* revert = tools->addAction(IconProvider::RevertIcon(), QString(), this, [this] { Revert(); });
    revert->setObjectName(QStringLiteral("MaterialRevert"));
    revert->setToolTip(Tr("Откатить к сохранённым файлам", "Revert to saved files"));
    tools->addSeparator();
    tools->addWidget(new QLabel(QStringLiteral(" Форма: "), tools));
    shape_ = new QComboBox(tools);
    shape_->setObjectName(QStringLiteral("MaterialPreviewShape"));
    shape_->addItems({QStringLiteral("Сфера"), QStringLiteral("Куб"), QStringLiteral("Плоскость")});
    tools->addWidget(shape_);
    tools->addSeparator();
    graphAction_ = tools->addAction(IconProvider::SchemaIcon(), QString(), this, [this] {
        const bool toCode = graphMode_;
        const QString question = toCode
            ? QStringLiteral("Удалить граф? HLSL останется как есть и будет правиться руками.")
            : QStringLiteral("Создать нодовый граф? HLSL будет генерироваться из графа, текущий код заменится.");
        if (QMessageBox::question(this, QStringLiteral("Нодовый граф"), question +
                QStringLiteral("\nДо сохранения это отменяется кнопкой «Откатить».")) != QMessageBox::Yes)
            return;
        if (toCode) DisableGraph();
        else EnableGraph();
    });
    graphAction_->setObjectName(QStringLiteral("MaterialGraphToggle"));
    graphAction_->setToolTip(Tr("Нодовый граф / Перейти на код", "Node graph / Switch to code"));
    graphAction_->setEnabled(false); // до загрузки HLSL
    BuildGraphTools(tools);

    auto* help = new InfoButton(this);
    help->setObjectName(QStringLiteral("MaterialHelpButton"));
    help->SetToolbarStyle(true);
    help->SetInfoText(
        Tr("Редактор материала", "Material Editor"),
        Tr("• Превью: ЛКМ/ПКМ — вращение, колесо — приближение (0.2x..10x от исходного расстояния)\n• Нодовый граф: ПКМ по ноде Constant — «Сделать параметром» (правка без перекомпиляции)\n• Глаз в «Параметрах»: показывать параметр в инспекторе объекта (появляется сразу)\n• Шаблоны: Simple, ORM и базовые эффекты; «Откатить» возвращает файлы до сохранения",
           "• Preview: LMB/RMB — orbit, wheel — zoom (0.2x..10x of the initial distance)\n• Node graph: RMB a Constant node — Make parameter (editable without recompiling)\n• Eye in Parameters: show the parameter in the object inspector (appears at once)\n• Templates: Simple, ORM and base effects; Revert restores files until saved"),
        QString());
    tools->addWidget(help);

    preview_ = new PreviewWidget(this);
    preview_->setObjectName(QStringLiteral("MaterialPreview"));

    auto* settings = new QWidget(this);
    auto* settingsLayout = new QVBoxLayout(settings);
    settingsLayout->setContentsMargins(4, 4, 4, 4);
    settingsLayout->setSpacing(6);
    renderCard_ = new CollapsibleCard(Tr("Рендер", "Rendering"), settings);
    renderCard_->setObjectName(QStringLiteral("MaterialRenderCard"));
    auto* settingsForm = new QFormLayout;
    settingsForm->setContentsMargins(0, 0, 0, 0);
    settingsForm->setSpacing(4);
    settingsForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    renderCard_->ContentLayout()->addLayout(settingsForm);
    blend_ = new QComboBox(renderCard_->Content());
    blend_->setObjectName(QStringLiteral("MaterialBlend"));
    blend_->addItems({QStringLiteral("Непрозрачный"), QStringLiteral("С вырезом (masked)"),
                      QStringLiteral("Полупрозрачный"), QStringLiteral("Аддитивный")});
    alphaClip_ = new QDoubleSpinBox(renderCard_->Content());
    alphaClip_->setObjectName(QStringLiteral("MaterialAlphaClip"));
    alphaClip_->setRange(0.0, 1.0);
    alphaClip_->setSingleStep(0.05);
    alphaClip_->setDecimals(3);
    unlit_ = new QCheckBox(QStringLiteral("Без освещения"), renderCard_->Content());
    unlit_->setObjectName(QStringLiteral("MaterialUnlit"));
    twoSided_ = new QCheckBox(QStringLiteral("Двусторонний"), renderCard_->Content());
    twoSided_->setObjectName(QStringLiteral("MaterialTwoSided"));
    castShadows_ = new QCheckBox(QStringLiteral("Отбрасывает тени"), renderCard_->Content());
    castShadows_->setObjectName(QStringLiteral("MaterialCastShadows"));
    settingsForm->addRow(QStringLiteral("Смешивание"), blend_);
    settingsForm->addRow(QStringLiteral("Порог альфы"), alphaClip_);
    settingsForm->addRow(unlit_);
    settingsForm->addRow(twoSided_);
    settingsForm->addRow(castShadows_);
    settingsLayout->addWidget(renderCard_);

    texturesCard_ = new CollapsibleCard(Tr("Текстуры", "Textures"), settings);
    texturesCard_->setObjectName(QStringLiteral("MaterialTexturesCard"));
    texturesForm_ = new QFormLayout;
    texturesForm_->setContentsMargins(0, 0, 0, 0);
    texturesForm_->setSpacing(4);
    texturesCard_->ContentLayout()->addLayout(texturesForm_);
    settingsLayout->addWidget(texturesCard_);

    paramsCard_ = new CollapsibleCard(Tr("Параметры", "Parameters"), settings);
    paramsCard_->setObjectName(QStringLiteral("MaterialParamsCard"));
    paramsForm_ = new QFormLayout;
    paramsForm_->setContentsMargins(0, 0, 0, 0);
    paramsForm_->setSpacing(4);
    paramsCard_->ContentLayout()->addLayout(paramsForm_);
    settingsLayout->addWidget(paramsCard_);
    settingsLayout->addStretch(1);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setWidget(settings);
    settings->setEnabled(false); // до первого ответа движка

    source_ = new MaterialSourceEditor(this);
    source_->setObjectName(QStringLiteral("MaterialSource"));
    errors_ = new QListWidget(this);
    errors_->setObjectName(QStringLiteral("MaterialErrors"));

    auto* left = new QSplitter(Qt::Vertical, this);
    left->setHandleWidth(3);
    left->setChildrenCollapsible(false);
    left->addWidget(preview_);
    left->addWidget(scroll);
    left->setStretchFactor(0, 1);
    left->setStretchFactor(1, 1);
    left->setSizes({300, 400}); // слоты текстур видны без прокрутки
    graph_ = new MaterialGraphWidget(this);
    graph_->SetPresetFolder(QDir(QString::fromUtf8(host_->host().contentRoot)).filePath(QStringLiteral("MaterialGraphs")));
    graphPage_ = graph_;
    sourceTabs_ = new QTabWidget(this);
    sourceTabs_->setObjectName(QStringLiteral("MaterialSourceTabs"));
    sourceTabs_->addTab(source_, QStringLiteral("HLSL"));
    graph_->hide(); // вкладка появляется в режиме графа

    // Правки графа копятся 250 мс: перетаскивание связи не пересобирает шейдер на каждом шаге.
    graphApply_ = new QTimer(this);
    graphApply_->setSingleShot(true);
    graphApply_->setInterval(250);
    connect(graphApply_, &QTimer::timeout, this, [this] { ApplyGraph(); });
    graph_->structureChanged = [this] {
        BackupGraphFiles();
        graphApply_->start();
        UpdateTitle();
    };
    graph_->layoutChanged = [this] {
        BackupGraphFiles();
        WriteGraphFile();
        UpdateTitle();
    };
    graph_->parameterEdited = [this](const QString& name, const std::array<float, 4>& value) { SetParameterByName(name, value); };
    graph_->textureRequested = [this](const QString& name) { PickTexture(name); };
    graph_->textureRestored = [this](const QString& name, const QString& gamePath) { AssignTexture(name, gamePath); };
    graph_->exposeRequested = [this] { ExposeSelectedAsParameter(); };

    auto* right = new QSplitter(Qt::Vertical, this);
    right->setHandleWidth(3);
    right->setChildrenCollapsible(false);
    right->addWidget(sourceTabs_);
    right->addWidget(errors_);
    right->setStretchFactor(0, 4);
    right->setStretchFactor(1, 1);
    auto* main = new QSplitter(Qt::Horizontal, this);
    main->setHandleWidth(3);
    main->setChildrenCollapsible(false);
    main->addWidget(left);
    main->addWidget(right);
    main->setStretchFactor(0, 2);
    main->setStretchFactor(1, 3);
    setCentralWidget(main);
    statusBar();

    const QByteArray material = material_.toUtf8();
    auto engine = engine_;
    preview_->attach = [this, material, engine](void* hwnd, uint32_t w, uint32_t h)
    {
        const uint32_t shape = static_cast<uint32_t>(shape_->currentIndex());
        EngineThenRead([material, engine, hwnd, w, h, shape](const EngineEditSessionApi& api)
        {
            char error[4096]{};
            uint32_t status = api.attachMaterialPreview(api.context, hwnd, w, h, &engine->preview);
            if (status != EngineSceneOk) return QStringLiteral("Не удалось создать превью (код %1)").arg(status);
            status = api.showMaterialPreview(api.context, engine->preview, material.constData(), shape, error, sizeof(error));
            return StatusText(status, error);
        }, true);
    };
    preview_->resized = [this, engine](uint32_t w, uint32_t h)
    {
        Engine([engine, w, h](const EngineEditSessionApi& api)
        {
            if (engine->preview) api.resizeMaterialPreview(api.context, engine->preview, w, h);
        });
    };
    preview_->orbit = [this, engine](float yaw, float pitch, float zoom)
    {
        Engine([engine, yaw, pitch, zoom](const EngineEditSessionApi& api)
        {
            if (engine->preview) api.materialPreviewCamera(api.context, engine->preview, yaw, pitch, zoom);
        });
    };
    preview_->detach = [this, engine]()
    {
        Engine([engine](const EngineEditSessionApi& api)
        {
            if (engine->preview)
            {
                api.detachMaterialPreview(api.context, engine->preview);
                engine->preview = 0;
            }
        });
    };
    connect(shape_, &QComboBox::currentIndexChanged, this, [this, material, engine](int index)
    {
        EngineThenRead([material, engine, index](const EngineEditSessionApi& api)
        {
            if (!engine->preview) return QString();
            char error[4096]{};
            return StatusText(api.showMaterialPreview(api.context, engine->preview, material.constData(),
                                                      static_cast<uint32_t>(index), error, sizeof(error)), error);
        }, false);
    });

    connect(blend_, &QComboBox::currentIndexChanged, this, [this] { SendRenderState(); });
    connect(alphaClip_, &QDoubleSpinBox::valueChanged, this, [this] { SendRenderState(); });
    for (auto* box : {unlit_, twoSided_, castShadows_})
        connect(box, &QCheckBox::toggled, this, [this] { SendRenderState(); });

    connect(source_->document(), &QTextDocument::modificationChanged, this, [this] { UpdateTitle(); });
    connect(errors_, &QListWidget::itemActivated, this, [this](QListWidgetItem* item)
    {
        // Ошибка графа ведёт к ноде, ошибка кода — к строке HLSL.
        if (const int node = item->data(Qt::UserRole + 2).toInt(); node > 0 && graphMode_)
        {
            sourceTabs_->setCurrentWidget(graphPage_);
            graph_->FocusNode(node);
            return;
        }
        const int line = item->data(Qt::UserRole).toInt();
        if (line > 0) GoToError(line, item->data(Qt::UserRole + 1).toInt());
    });
}

void MaterialEditorWindow::Engine(std::function<void(const EngineEditSessionApi&)> task)
{
    EditorCommand c;
    c.type = EditorCommand::RunTask;
    c.task = std::move(task);
    host_->PostCommand(c);
}

void MaterialEditorWindow::EngineThenRead(std::function<QString(const EngineEditSessionApi&)> task, bool rebuild,
    std::function<void(bool)> completion)
{
    QPointer<MaterialEditorWindow> self(this);
    EditorQtHost* host = host_;
    const QByteArray material = material_.toUtf8();
    Engine([self, host, material, task = std::move(task), rebuild, completion = std::move(completion)](const EngineEditSessionApi& api)
    {
        QString error = task ? task(api) : QString();
        MaterialEditorState state;
        const QString readError = ReadMaterial(api, material, state);
        if (error.isEmpty()) error = readError;
        const bool valid = readError.isEmpty();
        host->RunOnUi([self, state, valid, rebuild, error, completion]
        {
            if (self) {
                self->Apply(state, valid, rebuild, error);
                if (completion) completion(valid && error.isEmpty() && !state.modified);
            }
        });
    });
}

void MaterialEditorWindow::Apply(const MaterialEditorState& state, bool valid, bool rebuild, const QString& error)
{
    if (valid)
    {
        const bool first = !loaded_;
        const QVector<MaterialEditorState::Texture> previous = state_.textures;
        // Удалённые параметры убираем из publicParams_ и уведомляем инспектор
        if (!first && loaded_)
        {
            QSet<QString> currentNames;
            for (const auto& p : state.parameters) currentNames.insert(p.name);
            bool paramsChanged = false;
            for (auto it = publicParams_.begin(); it != publicParams_.end(); )
            {
                if (!currentNames.contains(*it)) { it = publicParams_.erase(it); paramsChanged = true; }
                else ++it;
            }
            if (paramsChanged && !material_.isEmpty())
                EditorCore::MaterialEvents::Instance().NotifyChanged(material_);
        }
        state_ = state;
        loaded_ = true;
        if (!state_.modified) edited_ = false;
        if (first)
        {
            shaderFile_ = PhysicalPath(state_.shader);
            LoadShaderText();
            LoadGraph();
            LoadPublicParams();
        }
        if (graphMode_) SyncGraphTextures();
        // Смена режима смешивания тоже меняет слоты: компилятор выбрасывает то, что режим не читает.
        const bool slotsChanged = !std::equal(previous.begin(), previous.end(), state_.textures.begin(), state_.textures.end(),
            [](const auto& a, const auto& b) { return a.name == b.name; });
        if (first || rebuild || slotsChanged) RebuildSettings();
        ShowErrors();
    }
    else if (!loaded_)
    {
        errors_->clear();
        errors_->addItem(error);
    }

    if (!error.isEmpty()) statusBar()->showMessage(error);
    else statusBar()->showMessage(state_.errors.isEmpty() ? QStringLiteral("Шейдер скомпилирован")
                                                          : QStringLiteral("Ошибки компиляции: см. список ошибок (двойной щелчок — к месту ошибки)"));
    UpdateTitle();
}

void MaterialEditorWindow::LoadShaderText()
{
    QByteArray data;
    sourceLoaded_ = !shaderFile_.isEmpty() && EditorCore::GraphFileIo::Read(ProjectRoot(), ContentRoot(), shaderFile_, data);
    graphAction_->setEnabled(sourceLoaded_);
    if (templates_) templates_->setEnabled(sourceLoaded_);
    if (!sourceLoaded_)
    {
        source_->setPlainText({});
        source_->setReadOnly(true);
        statusBar()->showMessage(QStringLiteral("Не удалось открыть HLSL: %1").arg(shaderFile_));
        return;
    }
    source_->setReadOnly(graphMode_);
    source_->setPlainText(QString::fromUtf8(data));
    source_->document()->setModified(false);
}

QString MaterialEditorWindow::GraphFile() const
{
    const QString material = PhysicalPath(material_);
    return material.isEmpty() ? QString() : material + QStringLiteral(".graph.json");
}

void MaterialEditorWindow::LoadGraph()
{
    const QString graphFile = GraphFile();
    if (!sourceLoaded_ || !EditorCore::GraphFileIo::Exists(ProjectRoot(), ContentRoot(), graphFile))
    {
        SetGraphMode(false);
        return;
    }
    EditorCore::MaterialGraphModel model;
    QByteArray data;
    if (!EditorCore::GraphFileIo::Read(ProjectRoot(), ContentRoot(), graphFile, data) || !model.ImportFromJson(QString::fromUtf8(data)))
    {
        // Битый граф не трогаем: HLSL остаётся последним скомпилированным, правится руками.
        SetGraphMode(false);
        statusBar()->showMessage(QStringLiteral("Граф %1 не читается, материал открыт как код").arg(graphFile));
        return;
    }
    graph_->SetModel(model);
    SetGraphMode(true);
}

void MaterialEditorWindow::SetGraphMode(bool on)
{
    graphMode_ = on;
    const int index = sourceTabs_->indexOf(graphPage_);
    if (on && index < 0) sourceTabs_->insertTab(0, graphPage_, QStringLiteral("Граф"));
    if (!on && index >= 0) sourceTabs_->removeTab(index);
    graphPage_->setVisible(on);
    if (on) sourceTabs_->setCurrentWidget(graphPage_);
    sourceTabs_->setTabText(sourceTabs_->indexOf(source_), on ? QStringLiteral("HLSL (из графа)") : QStringLiteral("HLSL"));
    source_->setReadOnly(on || !sourceLoaded_);
    graphAction_->setIcon(on ? IconProvider::CodeIcon() : IconProvider::SchemaIcon());
    graphAction_->setText(QString());
    graphAction_->setToolTip(on ? Tr("Перейти на код HLSL", "Switch to HLSL code") : Tr("Нодовый граф", "Node graph"));
    if (loaded_) RebuildSettings();
}

void MaterialEditorWindow::BackupGraphFiles()
{
    if (graphBackup_.active) return;
    QByteArray hlsl, graph;
    graphBackup_.hlsl = EditorCore::GraphFileIo::Read(ProjectRoot(), ContentRoot(), shaderFile_, hlsl) ? hlsl : QByteArray();
    graphBackup_.hadGraph = EditorCore::GraphFileIo::Read(ProjectRoot(), ContentRoot(), GraphFile(), graph);
    graphBackup_.graph = graphBackup_.hadGraph ? graph : QByteArray();
    graphBackup_.active = true;
}

bool MaterialEditorWindow::RestoreGraphFiles()
{
    if (!graphBackup_.active) return true;
    graphApply_->stop();
    const auto failed = [this](const QString& path, const QString& error) {
        QMessageBox::warning(this, QStringLiteral("Откат не выполнен"),
            QStringLiteral("Не удалось восстановить %1: %2\nРезервная копия сохранена в памяти. "
                           "Устраните причину ошибки и повторите откат.").arg(path, error));
        return false;
    };
    const auto write = [&failed, this](const QString& path, const QByteArray& bytes) {
        QString error;
        if (!EditorCore::GraphFileIo::Write(ProjectRoot(), ContentRoot(), path, bytes, &error))
            return failed(path, error);
        return true;
    };
    if (!write(shaderFile_, graphBackup_.hlsl)) return false;
    if (graphBackup_.hadGraph)
    {
        if (!write(GraphFile(), graphBackup_.graph)) return false;
    }
    else if (EditorCore::GraphFileIo::Exists(ProjectRoot(), ContentRoot(), GraphFile()))
    {
        QString error;
        if (!EditorCore::GraphFileIo::Delete(ProjectRoot(), ContentRoot(), GraphFile(), &error)) return failed(GraphFile(), error);
    }
    // A partial restore can be retried: retain both original files until every operation succeeds.
    graphBackup_.active = false;
    return true;
}

bool MaterialEditorWindow::WriteGraphFile()
{
    if (!graphMode_) return true;
    QString error;
    if (!EditorCore::GraphFileIo::Write(ProjectRoot(), ContentRoot(), GraphFile(), graph_->Model().ExportToJson().toUtf8(), &error))
    {
        QMessageBox::warning(this, QStringLiteral("Граф не сохранён"),
                             QStringLiteral("Не удалось записать %1: %2").arg(GraphFile(), error));
        return false;
    }
    return true;
}

void MaterialEditorWindow::EnableGraph()
{
    if (!loaded_ || graphMode_ || !sourceLoaded_ || saving_) return;
    BackupGraphFiles();
    const QString hlsl = source_->toPlainText();
    if (!savedGraphJson_.isEmpty() && savedGraphHlsl_ == hlsl)
    {
        EditorCore::MaterialGraphModel model;
        if (model.ImportFromJson(savedGraphJson_))
        {
            graph_->SetModel(model);
            SyncGraphTextures();
        }
        else
        {
            const auto parsed = EditorCore::HlslToGraph::Parse(hlsl);
            if (parsed.ok) { graph_->SetModel(parsed.model); SyncGraphTextures(); }
        }
    }
    else
    {
        const auto parsed = EditorCore::HlslToGraph::Parse(hlsl);
        if (parsed.ok)
        {
            graph_->SetModel(parsed.model);
            SyncGraphTextures();
        }
        else
        {
            EditorCore::MaterialGraphModel model;
            model.SetupDefaultMasterNode();
            graph_->SetModel(model);
            statusBar()->showMessage(EditorCore::HlslToGraph::IsGenerated(hlsl)
                ? Tr("Граф разобран не полностью, создан пустой граф: %1", "Graph parsed partially, empty graph created: %1")
                      .arg(parsed.problems.join(QStringLiteral("; ")))
                : Tr("Шейдер не является нодовым графом: создан пустой граф, «Откатить» вернёт код",
                     "Shader is not a node graph: empty graph created, Revert restores the code"));
        }
    }
    SetGraphMode(true);
    ApplyGraph();
}

void MaterialEditorWindow::DisableGraph()
{
    if (!graphMode_ || saving_) return;
    ApplyGraph(); // HLSL должен соответствовать последнему виду графа
    BackupGraphFiles();
    savedGraphJson_ = graph_->Model().ExportToJson();
    savedGraphHlsl_ = source_->toPlainText();
    EditorCore::GraphFileIo::Delete(ProjectRoot(), ContentRoot(), GraphFile());
    SetGraphMode(false);
    UpdateTitle();
}

void MaterialEditorWindow::ApplyGraph()
{
    graphApply_->stop();
    if (!graphMode_ || !loaded_) return;
    BackupGraphFiles();
    if (!WriteGraphFile()) return;
    const QString hlsl = EditorCore::HlslGraphCompiler::CompileGraphToHlsl(graph_->Model());
    if (hlsl != source_->toPlainText())
    {
        ReplaceSource(hlsl);
        if (!WriteSource()) return;
        const QByteArray shader = shaderFile_.toUtf8();
        EngineThenRead([shader](const EngineEditSessionApi& api)
        {
            api.reloadShaderFile(api.context, shader.constData());
            return QString();
        }, true);
    }
    UpdateTitle();
}

void MaterialEditorWindow::SetParameterByName(const QString& name, const std::array<float, 4>& value)
{
    const auto parameter = std::find_if(state_.parameters.begin(), state_.parameters.end(),
                                        [&](const MaterialEditorState::Parameter& p) { return p.name == name; });
    if (parameter == state_.parameters.end())
    {
        // Параметр ещё не скомпилирован: значение уйдёт в HLSL как значение по умолчанию.
        if (graph_->structureChanged) graph_->structureChanged();
        return;
    }
    BackupGraphFiles();
    WriteGraphFile();
    edited_ = true;
    UpdateTitle();
    const QByteArray material = material_.toUtf8(), parameterName = name.toUtf8();
    EngineThenRead([material, parameterName, value](const EngineEditSessionApi& api)
    {
        return StatusText(api.setMaterialParameter(api.context, material.constData(), parameterName.constData(), value.data()), nullptr);
    }, true);
}

void MaterialEditorWindow::RebuildSettings()
{
    {
        const QSignalBlocker blockBlend(blend_), blockClip(alphaClip_), blockUnlit(unlit_),
                             blockTwoSided(twoSided_), blockShadows(castShadows_);
        blend_->setCurrentIndex(static_cast<int>(state_.render.blend));
        alphaClip_->setValue(state_.render.alphaClip);
        unlit_->setChecked(state_.render.unlit != 0);
        twoSided_->setChecked(state_.render.twoSided != 0);
        castShadows_->setChecked(state_.render.castShadows != 0);
    }
    alphaClip_->setEnabled(state_.render.blend == EngineMaterialMasked);

    while (texturesForm_->rowCount()) texturesForm_->removeRow(0);
    while (paramsForm_->rowCount()) paramsForm_->removeRow(0);

    for (const auto& texture : state_.textures)
    {
        const QString name = texture.name;
        auto* row = new MaterialTextureSlot(name, texture.path, PhysicalPath(texture.path), texturesCard_->Content());
        // Диалоги открываются из очереди: ответ движка может пересоздать эту строку под ними.
        row->choose = [this, name] { QTimer::singleShot(0, this, [this, name] { PickTexture(name); }); };
        row->remove = [this, name] { QTimer::singleShot(0, this, [this, name] { RequestRemoveTexture(name); }); };
        row->clear = [this, name] { AssignTexture(name, {}); };
        row->dropped = [this, name](const QString& file) { AssignTexture(name, file); };
        // У материала из графа HLSL правит граф: слоты добавляются и удаляются нодами.
        if (auto* remove = row->findChild<QPushButton*>(QStringLiteral("MaterialTextureRemove_%1").arg(name)))
            remove->setVisible(!graphMode_);
        texturesForm_->addRow(name, row);
    }

    if (!graphMode_)
    {
        auto* add = new QPushButton(QStringLiteral("Добавить текстуру…"), texturesCard_->Content());
        add->setObjectName(QStringLiteral("MaterialAddTextureButton"));
        add->setEnabled(!source_->isReadOnly());
        connect(add, &QPushButton::clicked, this, [this] { QTimer::singleShot(0, this, [this] { RequestAddTexture(); }); });
        texturesForm_->addRow(add);
    }
    texturesCard_->setVisible(!state_.textures.isEmpty() || !graphMode_);

    for (int index = 0; index < state_.parameters.size(); ++index)
    {
        auto* row = new QWidget(paramsCard_->Content());
        auto* layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        BuildParameterRow(row, layout, index);
        paramsForm_->addRow(state_.parameters[index].name, row);
    }
    paramsCard_->setVisible(!state_.parameters.isEmpty());

    renderCard_->parentWidget()->setEnabled(true);
}

void MaterialEditorWindow::ShowErrors()
{
    errors_->clear();
    QSet<int> lines;
    const QString surface = QFileInfo(shaderFile_).absoluteFilePath();
    static const QRegularExpression location(QStringLiteral(R"(^(.*)\((\d+),(\d+)(?:-\d+)?\):\s*)"));

    // В режиме графа ошибка показывается на ноде: строки HLSL знают, из каких нод собраны.
    // Карта верна, только пока HLSL на диске — это HLSL текущего графа.
    EditorCore::GraphCompileResult compiled;
    const bool graph = graphMode_ && (compiled = EditorCore::HlslGraphCompiler::Compile(graph_->Model())).hlsl == source_->toPlainText();
    QHash<int, QString> nodeProblems;
    auto describe = [this](int nodeId) {
        const EditorCore::GraphNodeData* node = graph_->Model().FindNode(nodeId);
        if (!node) return QStringLiteral("Нода");
        const QString kind = MaterialGraphWidget::NodeDisplayName(node->type);
        return node->title.isEmpty() || node->title == node->type ? QStringLiteral("Нода «%1»").arg(kind)
                                                                   : QStringLiteral("Нода «%1» (%2)").arg(node->title, kind);
    };
    auto addNodeItem = [&](int nodeId, const QString& text, const QString& tip) {
        auto* item = new QListWidgetItem(text, errors_);
        item->setData(Qt::UserRole + 2, nodeId);
        item->setToolTip(tip.isEmpty() ? QStringLiteral("Двойной щелчок — показать ноду") : tip + QStringLiteral("\n\nДвойной щелчок — показать ноду"));
        if (!nodeProblems.contains(nodeId)) nodeProblems.insert(nodeId, text);
        return item;
    };
    if (graph)
        for (auto it = compiled.problems.begin(); it != compiled.problems.end(); ++it)
            addNodeItem(it.key(), QStringLiteral("%1: %2").arg(describe(it.key()), it.value()), {});

    for (const QString& text : state_.errors.split(QLatin1Char('\n'), Qt::SkipEmptyParts))
    {
        const auto match = location.match(text);
        const QString file = match.hasMatch() ? QFileInfo(match.captured(1).trimmed()).absoluteFilePath() : QString();
        const bool inSurface = match.hasMatch() && !surface.isEmpty() && file.compare(surface, Qt::CaseInsensitive) == 0;
        const int line = inSurface ? match.captured(2).toInt() : 0;
        if (graph && line > 0 && compiled.lineNodes.contains(line))
        {
            // Без пути и номера строки: дизайнеру нужна нода, а не координаты в сгенерированном коде.
            QString message = text.mid(match.capturedLength()).trimmed();
            if (message.contains(QStringLiteral("redefinition")))
                message += QStringLiteral(" — имя уже занято, переименуйте ноду");
            const QList<int> nodes = compiled.lineNodes.value(line);
            const QString pin = compiled.lineOutputs.value(line);
            const QString where = pin.isEmpty() ? describe(nodes.first()) : QStringLiteral("Выход материала, вход %1").arg(pin);
            auto* item = addNodeItem(nodes.first(), QStringLiteral("%1: %2").arg(where, message), text.trimmed());
            item->setData(Qt::UserRole, line);
            item->setData(Qt::UserRole + 1, match.captured(3).toInt());
            lines.insert(line);
            continue;
        }
        auto* item = new QListWidgetItem(text.trimmed(), errors_);
        if (line <= 0) continue;
        item->setData(Qt::UserRole, line);
        item->setData(Qt::UserRole + 1, match.captured(3).toInt());
        item->setToolTip(QStringLiteral("Двойной щелчок — перейти к строке %1").arg(line));
        lines.insert(line);
    }
    source_->SetErrorLines(lines);
    if (graphMode_) graph_->SetNodeProblems(nodeProblems);
}

void MaterialEditorWindow::GoToError(int line, int column)
{
    const QTextBlock block = source_->document()->findBlockByNumber(line - 1);
    if (!block.isValid()) return;
    QTextCursor cursor(block);
    cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, std::max(0, std::min(column - 1, block.length() - 1)));
    source_->setTextCursor(cursor);
    source_->centerCursor();
    source_->setFocus();
}

void MaterialEditorWindow::UpdateTitle()
{
    setWindowTitle(QStringLiteral("%1[*] — Материал").arg(QFileInfo(material_).fileName()));
    setWindowModified(HasUnsavedChanges());
}

bool MaterialEditorWindow::HasUnsavedChanges() const
{
    return edited_ || state_.modified || graphBackup_.active || (source_ && source_->document()->isModified());
}

void MaterialEditorWindow::SendRenderState()
{
    EngineMaterialRenderState render{};
    render.blend = static_cast<uint32_t>(std::max(0, blend_->currentIndex()));
    render.alphaClip = static_cast<float>(alphaClip_->value());
    render.unlit = unlit_->isChecked() ? 1u : 0u;
    render.twoSided = twoSided_->isChecked() ? 1u : 0u;
    render.castShadows = castShadows_->isChecked() ? 1u : 0u;
    alphaClip_->setEnabled(render.blend == EngineMaterialMasked);
    edited_ = true;
    UpdateTitle();
    const QByteArray material = material_.toUtf8();
    EngineThenRead([material, render](const EngineEditSessionApi& api)
    {
        return StatusText(api.setMaterialRenderState(api.context, material.constData(), &render), nullptr);
    }, false);
}

void MaterialEditorWindow::SetParameter(int index, const std::array<float, 4>& value)
{
    if (graphMode_)
    {
        // Значение в ноде графа: при следующей сборке оно станет значением по умолчанию в HLSL.
        BackupGraphFiles();
        graph_->SetParameterValue(state_.parameters[index].name, value);
        WriteGraphFile();
    }
    edited_ = true;
    UpdateTitle();
    const QByteArray material = material_.toUtf8(), name = state_.parameters[index].name.toUtf8();
    EngineThenRead([material, name, value](const EngineEditSessionApi& api)
    {
        return StatusText(api.setMaterialParameter(api.context, material.constData(), name.constData(), value.data()), nullptr);
    }, false);
}

QString MaterialEditorWindow::VirtualPath(const QString& file) const
{
    if (file.startsWith(QStringLiteral("/Game/"))) return file;
    const QDir content(QString::fromUtf8(host_->host().contentRoot));
    const QString relative = content.relativeFilePath(QFileInfo(file).absoluteFilePath());
    if (relative == QStringLiteral("..") || relative.startsWith(QStringLiteral("../")) || QDir::isAbsolutePath(relative))
        return {};
    return QStringLiteral("/Game/") + relative;
}

void MaterialEditorWindow::AssignTexture(const QString& slot, const QString& file)
{
    const QString texture = file.isEmpty() ? QString() : VirtualPath(file);
    const bool outside = !file.isEmpty() && texture.isEmpty();
    const bool compiled = std::any_of(state_.textures.begin(), state_.textures.end(),
                                      [&](const MaterialEditorState::Texture& t) { return t.name == slot; });
    if (graphMode_)
    {
        BackupGraphFiles();
        if (!outside)
        {
            graph_->SetNodeTexture(slot, texture);
            WriteGraphFile();
        }
        if (!compiled && !outside)
        {
            SyncGraphTextures();
            statusBar()->showMessage(QStringLiteral("Текстура %1 запомнена в графе и назначится, когда нода будет подключена к выходу").arg(slot));
            UpdateTitle();
            return;
        }
    }
    // У материала из графа неподключённая нода не имеет слота: файл только импортируется и запоминается в ноде.
    const bool apply = compiled || !graphMode_;
    if (apply) edited_ = true;
    UpdateTitle();
    const QByteArray material = material_.toUtf8(), name = slot.toUtf8(), path = texture.toUtf8(),
                     source = outside ? file.toUtf8() : QByteArray();
    auto imported = std::make_shared<QByteArray>();
    EngineThenRead([material, name, path, source, apply, imported](const EngineEditSessionApi& api)
    {
        return SetTexture(api, material, name, path, source, apply, imported.get());
    }, true, [this, slot, imported](bool)
    {
        if (!graphMode_ || imported->isEmpty()) return;
        graph_->SetNodeTexture(slot, QString::fromUtf8(*imported));
        WriteGraphFile();
        SyncGraphTextures();
    });
}

void MaterialEditorWindow::SyncGraphTextures()
{
    const QHash<QString, QString> stored = graph_->NodeTextures();
    QHash<QString, QString> files;
    for (auto it = stored.begin(); it != stored.end(); ++it) files.insert(it.key(), PhysicalPath(it.value()));
    for (const auto& texture : state_.textures)
    {
        if (!texture.path.isEmpty()) files.insert(texture.name, PhysicalPath(texture.path));
        // Текстуру выбрали, пока нода не была подключена: слот появился — назначаем, один раз на файл.
        const QString wanted = stored.value(texture.name);
        const QString attempt = texture.name + QLatin1Char('|') + wanted;
        if (wanted.isEmpty() || wanted.compare(texture.path, Qt::CaseInsensitive) == 0 || autoAssigned_.contains(attempt)) continue;
        autoAssigned_.insert(attempt);
        QTimer::singleShot(0, this, [this, name = texture.name, wanted] { AssignTexture(name, wanted); });
    }
    graph_->SetTextures(files);
}

void MaterialEditorWindow::PickTexture(const QString& slot)
{
    QString current;
    for (const auto& texture : state_.textures)
        if (texture.name == slot) current = PhysicalPath(texture.path);
    if (current.isEmpty() && graphMode_) current = PhysicalPath(graph_->NodeTextures().value(slot));
    MaterialTexturePicker picker(QString::fromUtf8(host_->host().contentRoot), current, slot, this);
    if (picker.exec() == QDialog::Accepted) AssignTexture(slot, picker.Selected());
}

void MaterialEditorWindow::RequestAddTexture()
{
    MaterialAddTextureDialog dialog(source_->toPlainText(), this);
    if (dialog.exec() != QDialog::Accepted) return;
    MaterialTexturePicker picker(QString::fromUtf8(host_->host().contentRoot), {}, dialog.Name(), this);
    if (picker.exec() == QDialog::Accepted) AddTextureSlot(dialog.Name(), static_cast<int>(dialog.Role()), picker.Selected());
}

void MaterialEditorWindow::RequestRemoveTexture(const QString& slot)
{
    if (QMessageBox::question(this, QStringLiteral("Удалить слот"),
                              QStringLiteral("Убрать текстуру %1 из HLSL? Файл текстуры останется в проекте.").arg(slot)) == QMessageBox::Yes)
        RemoveTextureSlot(slot);
}

void MaterialEditorWindow::AddTextureSlot(const QString& name, int role, const QString& file)
{
    if (!loaded_ || saving_ || source_->isReadOnly()) return;
    const auto slotRole = static_cast<MaterialTextures::Role>(role);
    QString text = source_->toPlainText(), error;
    if (!MaterialTextures::AddSlot(text, name, slotRole, error))
    {
        QMessageBox::warning(this, QStringLiteral("Новая текстура"), error);
        return;
    }
    ReplaceSource(text);
    if (!WriteSource()) return;

    // Непрозрачный материал не читает альфу: компилятор выбросил бы текстуру вместе со слотом.
    const bool mask = slotRole == MaterialTextures::Role::Opacity && state_.render.blend == EngineMaterialOpaque;
    EngineMaterialRenderState render = state_.render;
    if (mask) render.blend = EngineMaterialMasked;
    if (mask || !file.isEmpty()) edited_ = true;
    UpdateTitle();

    const QString texture = file.isEmpty() ? QString() : VirtualPath(file);
    const QByteArray material = material_.toUtf8(), shader = shaderFile_.toUtf8(), slot = name.toUtf8(), path = texture.toUtf8(),
                     outside = !file.isEmpty() && texture.isEmpty() ? file.toUtf8() : QByteArray();
    const bool assign = !file.isEmpty();
    EngineThenRead([material, shader, slot, path, outside, assign, mask, render](const EngineEditSessionApi& api)
    {
        if (mask) api.setMaterialRenderState(api.context, material.constData(), &render);
        api.reloadShaderFile(api.context, shader.constData()); // компиляция во всех играх, слот готов сразу
        return assign ? SetTexture(api, material, slot, path, outside) : QString();
    }, true, [this, name](bool)
    {
        const bool found = std::any_of(state_.textures.begin(), state_.textures.end(),
                                       [&](const MaterialEditorState::Texture& texture) { return texture.name == name; });
        if (!found && state_.errors.isEmpty())
            statusBar()->showMessage(QStringLiteral("Шейдер не читает %1 при текущих настройках материала").arg(name));
    });
}

void MaterialEditorWindow::RemoveTextureSlot(const QString& name)
{
    if (!loaded_ || saving_ || source_->isReadOnly()) return;
    QString text = source_->toPlainText(), error;
    if (!MaterialTextures::RemoveSlot(text, name, error))
    {
        QMessageBox::warning(this, QStringLiteral("Удалить слот"), error);
        return;
    }
    ReplaceSource(text);
    if (!WriteSource()) return;
    const QByteArray shader = shaderFile_.toUtf8();
    EngineThenRead([shader](const EngineEditSessionApi& api)
    {
        api.reloadShaderFile(api.context, shader.constData());
        return QString();
    }, true);
}

void MaterialEditorWindow::ReplaceSource(const QString& text)
{
    const int scroll = source_->verticalScrollBar()->value();
    QTextCursor cursor(source_->document());
    cursor.select(QTextCursor::Document);
    cursor.insertText(text);
    source_->verticalScrollBar()->setValue(scroll);
}

bool MaterialEditorWindow::WriteSource()
{
    QString error;
    if (!EditorCore::GraphFileIo::Write(ProjectRoot(), ContentRoot(), shaderFile_, source_->toPlainText().toUtf8(), &error))
    {
        QMessageBox::warning(this, QStringLiteral("HLSL не сохранён"),
                             QStringLiteral("Не удалось записать %1: %2").arg(shaderFile_, error));
        return false;
    }
    source_->document()->setModified(false);
    return true;
}

bool MaterialEditorWindow::Save(std::function<void(bool)> completion)
{
    if (!loaded_ || saving_) return false;
    if (graphMode_ && (graphApply_->isActive() || graphBackup_.active))
    {
        ApplyGraph(); // перекомпиляция встаёт в очередь раньше сохранения .mat
        if (!WriteGraphFile()) return false;
    }
    const bool sourceChanged = source_->document()->isModified();
    if (sourceChanged && !WriteSource()) return false;
    saving_ = true;
    setEnabled(false);
    const QByteArray material = material_.toUtf8(), shader = shaderFile_.toUtf8();
    EngineThenRead([material, shader, sourceChanged](const EngineEditSessionApi& api)
    {
        // Сначала перекомпиляция (её делает чтение): в .mat попадает новый список параметров.
        if (sourceChanged) api.reloadShaderFile(api.context, shader.constData());
        MaterialEditorState current;
        const QString readError = ReadMaterial(api, material, current);
        if (!readError.isEmpty()) return readError;
        if (!current.modified) return QString();
        char error[4096]{};
        return StatusText(api.saveMaterial(api.context, material.constData(), error, sizeof(error)), error);
    }, sourceChanged, [this, completion](bool saved) {
        saving_ = false;
        setEnabled(true);
        if (saved) {
            SavePublicParams();
            EditorCore::MaterialEvents::Instance().NotifyChanged(material_);
            graphBackup_.active = false; // граф и HLSL на диске теперь сохранённые
        }
        UpdateTitle();
        if (completion) completion(saved);
    });
    statusBar()->showMessage(QStringLiteral("Сохранение…"));
    return true;
}

void MaterialEditorWindow::Revert()
{
    if (!loaded_ || !HasUnsavedChanges()) return;
    if (QMessageBox::question(this, QStringLiteral("Откатить материал"),
                              QStringLiteral("Отменить все несохранённые изменения материала и HLSL?")) != QMessageBox::Yes)
        return;
    const bool restored = graphBackup_.active;
    if (!RestoreGraphFiles()) return;
    LoadShaderText();
    LoadGraph();
    edited_ = false;
    const QByteArray material = material_.toUtf8(), shader = shaderFile_.toUtf8();
    EngineThenRead([material, shader, restored](const EngineEditSessionApi& api)
    {
        if (restored) api.reloadShaderFile(api.context, shader.constData());
        char error[4096]{};
        return StatusText(api.revertMaterial(api.context, material.constData(), error, sizeof(error)), error);
    }, true);
}

void MaterialEditorWindow::Refresh()
{
    if (loaded_ && !shuttingDown_) EngineThenRead({}, true);
}

bool MaterialEditorWindow::ConfirmClose(std::function<void()> afterSave)
{
    if (saving_) return false;
    if (!HasUnsavedChanges()) return true;
    const auto choice = QMessageBox::question(
        this, QStringLiteral("Несохранённый материал"),
        QStringLiteral("Сохранить изменения материала %1?").arg(QFileInfo(material_).fileName()),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (choice == QMessageBox::Save) {
        // Do not destroy the preview or unload the DLL before the queued save succeeds.
        Save([this, afterSave](bool saved) {
            if (saved) { if (afterSave) afterSave(); else close(); }
        });
        return false;
    }
    discard_ = choice == QMessageBox::Discard;
    return discard_;
}

bool MaterialEditorWindow::Shutdown()
{
    if (shuttingDown_) return true;
    graphApply_->stop();
    const bool restored = discard_ && graphBackup_.active;
    if (restored && !RestoreGraphFiles()) return false;
    shuttingDown_ = true;
    const bool revert = discard_ && (edited_ || state_.modified || restored);
    hide(); // нативное окно живо, пока движок не отпустит превью
    QPointer<MaterialEditorWindow> self(this);
    EditorQtHost* host = host_;
    const QByteArray material = material_.toUtf8(), shader = shaderFile_.toUtf8();
    auto engine = engine_;
    Engine([self, host, material, shader, engine, revert, restored](const EngineEditSessionApi& api)
    {
        if (restored) api.reloadShaderFile(api.context, shader.constData());
        if (revert) api.revertMaterial(api.context, material.constData(), nullptr, 0);
        if (engine->preview) api.detachMaterialPreview(api.context, engine->preview);
        engine->preview = 0;
        host->RunOnUi([self] { if (self) self->deleteLater(); });
    });
    return true;
}

void MaterialEditorWindow::closeEvent(QCloseEvent* event)
{
    event->ignore();
    if (!shuttingDown_ && ConfirmClose()) Shutdown();
}
