#include "MaterialEditorWindow.h"
#include "EditorLocale.h"

#include <QAction>
#include <QCheckBox>
#include <QCloseEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
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
#include <QSaveFile>
#include <QScrollArea>
#include <QSet>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStatusBar>
#include <QSyntaxHighlighter>
#include <QTextBlock>
#include <QToolBar>
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

// Нативное окно, в которое движок рисует превью. Перетаскивание вращает камеру, колесо — приближает.
class MaterialPreviewWidget final : public QWidget
{
public:
    std::function<void(void* hwnd, uint32_t width, uint32_t height)> attach;
    std::function<void(uint32_t width, uint32_t height)> resized;
    std::function<void(float yaw, float pitch, float zoom)> orbit;

    explicit MaterialPreviewWidget(QWidget* parent) : QWidget(parent)
    {
        setAttribute(Qt::WA_NativeWindow, true);
        setAttribute(Qt::WA_PaintOnScreen, true); // сюда Present'ит движок
        setAttribute(Qt::WA_NoSystemBackground, true);
        setMinimumSize(240, 240);
    }

    QPaintEngine* paintEngine() const override { return nullptr; }

protected:
    void paintEvent(QPaintEvent*) override {}

    void showEvent(QShowEvent* event) override
    {
        QWidget::showEvent(event);
        if (attached_ || !attach) return;
        attached_ = true;
        attach(reinterpret_cast<void*>(winId()), PhysW(), PhysH()); // winId создаёт нативное окно
    }

    void resizeEvent(QResizeEvent* event) override
    {
        QWidget::resizeEvent(event);
        if (attached_ && resized) resized(PhysW(), PhysH());
    }

    void mousePressEvent(QMouseEvent* event) override { last_ = event->position(); }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (!(event->buttons() & (Qt::LeftButton | Qt::RightButton)) || !orbit) return;
        const QPointF delta = event->position() - last_;
        last_ = event->position();
        orbit(static_cast<float>(delta.x()) * 0.01f, static_cast<float>(delta.y()) * 0.01f, 0.0f);
    }

    void wheelEvent(QWheelEvent* event) override
    {
        if (orbit) orbit(0.0f, 0.0f, static_cast<float>(event->angleDelta().y()) / 120.0f);
    }

private:
    uint32_t PhysW() const { return static_cast<uint32_t>(std::lround(width() * devicePixelRatioF())); }
    uint32_t PhysH() const { return static_cast<uint32_t>(std::lround(height() * devicePixelRatioF())); }

    bool attached_ = false;
    QPointF last_;
};

MaterialEditorWindow::MaterialEditorWindow(EditorQtHost* host, const QString& material, QWidget* parent)
    : QMainWindow(parent, Qt::Window), host_(host), material_(material)
{
    setAttribute(Qt::WA_DeleteOnClose, false); // удаляет Shutdown, после отключения превью
    setObjectName(QStringLiteral("MaterialEditor"));
    BuildUi();
    UpdateTitle();
    resize(1200, 760);
    langSubId_ = EditorLocale::Instance().SubscribeLanguageChanged([this] {
        RetranslateUi();
    });
}

MaterialEditorWindow::~MaterialEditorWindow()
{
    if (langSubId_) {
        EditorLocale::Instance().UnsubscribeLanguageChanged(langSubId_);
        langSubId_ = 0;
    }
}

void MaterialEditorWindow::RetranslateUi()
{
    UpdateTitle();
    if (saveAction_) {
        saveAction_->setText(Tr("Сохранить", "Save"));
        saveAction_->setToolTip(Tr("Записать HLSL и .mat, перекомпилировать (Ctrl+S)", "Write HLSL and .mat, recompile (Ctrl+S)"));
    }
    if (revertAction_) {
        revertAction_->setText(Tr("Откатить", "Revert"));
        revertAction_->setToolTip(Tr("Вернуть материал и HLSL к сохранённым файлам", "Revert material and HLSL to saved files"));
    }
    if (shapeLabel_) shapeLabel_->setText(Tr(" Форма: ", " Shape: "));
    if (shape_) {
        const int cur = shape_->currentIndex();
        shape_->setItemText(0, Tr("Сфера", "Sphere"));
        shape_->setItemText(1, Tr("Куб", "Cube"));
        shape_->setItemText(2, Tr("Плоскость", "Plane"));
        shape_->setCurrentIndex(cur);
    }
    if (blend_) {
        const int cur = blend_->currentIndex();
        blend_->setItemText(0, Tr("Непрозрачный", "Opaque"));
        blend_->setItemText(1, Tr("С вырезом (masked)", "Masked (alpha clip)"));
        blend_->setItemText(2, Tr("Полупрозрачный", "Translucent"));
        blend_->setItemText(3, Tr("Аддитивный", "Additive"));
        blend_->setCurrentIndex(cur);
    }
    if (unlit_) unlit_->setText(Tr("Без освещения", "Unlit"));
    if (twoSided_) twoSided_->setText(Tr("Двусторонний", "Two-sided"));
    if (castShadows_) castShadows_->setText(Tr("Отбрасывает тени", "Cast Shadows"));
    if (blendLabel_) blendLabel_->setText(Tr("Смешивание", "Blend Mode"));
    if (alphaClipLabel_) alphaClipLabel_->setText(Tr("Порог альфы", "Alpha Clip"));
    if (parametersTitle_) parametersTitle_->setText(Tr("<b>Параметры и текстуры</b>", "<b>Parameters and Textures</b>"));
}

QString MaterialEditorWindow::PhysicalPath(const QString& virtualPath) const
{
    if (!virtualPath.startsWith(QStringLiteral("/Game/"))) return {};
    return QDir(QString::fromUtf8(host_->host().contentRoot)).filePath(virtualPath.mid(6));
}

void MaterialEditorWindow::BuildUi()
{
    auto* tools = addToolBar(Tr("Материал", "Material"));
    tools->setMovable(false);
    saveAction_ = tools->addAction(Tr("Сохранить", "Save"), this, [this] { Save(); });
    saveAction_->setShortcut(QKeySequence::Save);
    saveAction_->setToolTip(Tr("Записать HLSL и .mat, перекомпилировать (Ctrl+S)", "Write HLSL and .mat, recompile (Ctrl+S)"));
    revertAction_ = tools->addAction(Tr("Откатить", "Revert"), this, [this] { Revert(); });
    revertAction_->setToolTip(Tr("Вернуть материал и HLSL к сохранённым файлам", "Revert material and HLSL to saved files"));
    tools->addSeparator();
    shapeLabel_ = new QLabel(Tr(" Форма: ", " Shape: "), tools);
    tools->addWidget(shapeLabel_);
    shape_ = new QComboBox(tools);
    shape_->setObjectName(QStringLiteral("MaterialPreviewShape"));
    shape_->addItems({Tr("Сфера", "Sphere"), Tr("Куб", "Cube"), Tr("Плоскость", "Plane")});
    tools->addWidget(shape_);

    preview_ = new MaterialPreviewWidget(this);
    preview_->setObjectName(QStringLiteral("MaterialPreview"));

    auto* settings = new QWidget(this);
    auto* settingsForm = new QFormLayout(settings);
    blend_ = new QComboBox(settings);
    blend_->setObjectName(QStringLiteral("MaterialBlend"));
    blend_->addItems({Tr("Непрозрачный", "Opaque"), Tr("С вырезом (masked)", "Masked (alpha clip)"),
                      Tr("Полупрозрачный", "Translucent"), Tr("Аддитивный", "Additive")});
    alphaClip_ = new QDoubleSpinBox(settings);
    alphaClip_->setObjectName(QStringLiteral("MaterialAlphaClip"));
    alphaClip_->setRange(0.0, 1.0);
    alphaClip_->setSingleStep(0.05);
    alphaClip_->setDecimals(3);
    unlit_ = new QCheckBox(Tr("Без освещения", "Unlit"), settings);
    unlit_->setObjectName(QStringLiteral("MaterialUnlit"));
    twoSided_ = new QCheckBox(Tr("Двусторонний", "Two-sided"), settings);
    twoSided_->setObjectName(QStringLiteral("MaterialTwoSided"));
    castShadows_ = new QCheckBox(Tr("Отбрасывает тени", "Cast Shadows"), settings);
    castShadows_->setObjectName(QStringLiteral("MaterialCastShadows"));
    blendLabel_ = new QLabel(Tr("Смешивание", "Blend Mode"), settings);
    settingsForm->addRow(blendLabel_, blend_);
    alphaClipLabel_ = new QLabel(Tr("Порог альфы", "Alpha Clip"), settings);
    settingsForm->addRow(alphaClipLabel_, alphaClip_);
    settingsForm->addRow(unlit_);
    settingsForm->addRow(twoSided_);
    settingsForm->addRow(castShadows_);

    parameters_ = new QWidget(settings);
    parameterForm_ = new QFormLayout(parameters_);
    parameterForm_->setContentsMargins(0, 0, 0, 0);
    parametersTitle_ = new QLabel(Tr("<b>Параметры и текстуры</b>", "<b>Parameters and Textures</b>"), settings);
    settingsForm->addRow(parametersTitle_);
    settingsForm->addRow(parameters_);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setWidget(settings);
    settings->setEnabled(false); // до первого ответа движка

    source_ = new MaterialSourceEditor(this);
    source_->setObjectName(QStringLiteral("MaterialSource"));
    errors_ = new QListWidget(this);
    errors_->setObjectName(QStringLiteral("MaterialErrors"));

    auto* left = new QSplitter(Qt::Vertical, this);
    left->addWidget(preview_);
    left->addWidget(scroll);
    left->setStretchFactor(0, 3);
    left->setStretchFactor(1, 2);
    auto* right = new QSplitter(Qt::Vertical, this);
    right->addWidget(source_);
    right->addWidget(errors_);
    right->setStretchFactor(0, 4);
    right->setStretchFactor(1, 1);
    auto* main = new QSplitter(Qt::Horizontal, this);
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
        state_ = state;
        loaded_ = true;
        if (!state_.modified) edited_ = false;
        if (first)
        {
            shaderFile_ = PhysicalPath(state_.shader);
            LoadShaderText();
        }
        if (first || rebuild) RebuildSettings();
        ShowErrors();
    }
    else if (!loaded_)
    {
        errors_->clear();
        errors_->addItem(error);
    }

    if (!error.isEmpty()) statusBar()->showMessage(error);
    else statusBar()->showMessage(state_.errors.isEmpty() ? QStringLiteral("Шейдер скомпилирован")
                                                          : QStringLiteral("Ошибки компиляции: см. список под текстом"));
    UpdateTitle();
}

void MaterialEditorWindow::LoadShaderText()
{
    QFile file(shaderFile_);
    if (shaderFile_.isEmpty() || !file.open(QIODevice::ReadOnly))
    {
        source_->setPlainText({});
        source_->setReadOnly(true);
        statusBar()->showMessage(QStringLiteral("Не удалось открыть HLSL: %1").arg(shaderFile_));
        return;
    }
    source_->setReadOnly(false);
    source_->setPlainText(QString::fromUtf8(file.readAll()));
    source_->document()->setModified(false);
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

    while (parameterForm_->rowCount()) parameterForm_->removeRow(0);

    for (int index = 0; index < state_.parameters.size(); ++index)
    {
        const auto& parameter = state_.parameters[index];
        auto* row = new QWidget(parameters_);
        auto* layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        if (!parameter.components)
        {
            layout->addWidget(new QLabel(QStringLiteral("тип не поддерживается редактором"), row));
            parameterForm_->addRow(parameter.name, row);
            continue;
        }

        std::vector<QDoubleSpinBox*> spins;
        for (uint32_t i = 0; i < parameter.components; ++i)
        {
            auto* spin = new QDoubleSpinBox(row);
            spin->setObjectName(QStringLiteral("MaterialParam_%1_%2").arg(parameter.name).arg(i));
            spin->setRange(-100000.0, 100000.0);
            spin->setDecimals(3);
            spin->setSingleStep(0.05);
            spin->setKeyboardTracking(false);
            spin->setValue(parameter.value[i]);
            spin->setMinimumWidth(64);
            layout->addWidget(spin, 1);
            spins.push_back(spin);
        }
        auto values = [spins] {
            std::array<float, 4> value{};
            for (size_t i = 0; i < spins.size(); ++i) value[i] = static_cast<float>(spins[i]->value());
            return value;
        };
        auto setValues = [spins](const std::array<float, 4>& value) {
            for (size_t i = 0; i < spins.size(); ++i)
            {
                const QSignalBlocker block(spins[i]);
                spins[i]->setValue(value[i]);
            }
        };

        QPushButton* color = nullptr;
        if (parameter.components >= 3)
        {
            color = new QPushButton(row);
            color->setObjectName(QStringLiteral("MaterialColor_%1").arg(parameter.name));
            color->setFixedSize(28, 22);
            color->setToolTip(QStringLiteral("Выбрать цвет"));
            layout->addWidget(color);
        }
        const uint32_t components = parameter.components;
        auto paint = [color, components](const std::array<float, 4>& value) {
            if (color) color->setStyleSheet(QStringLiteral("background-color: %1;").arg(ToColor(value, components).name()));
        };
        paint(parameter.value);

        for (auto* spin : spins)
            connect(spin, &QDoubleSpinBox::valueChanged, this, [this, index, values, paint]
            {
                paint(values());
                SetParameter(index, values());
            });
        if (color)
            connect(color, &QPushButton::clicked, this, [this, index, values, setValues, paint, components]
            {
                QColorDialog::ColorDialogOptions options;
                if (components == 4) options |= QColorDialog::ShowAlphaChannel;
                const QColor picked = QColorDialog::getColor(ToColor(values(), components), this,
                                                             state_.parameters[index].name, options);
                if (!picked.isValid()) return;
                auto value = values();
                value[0] = static_cast<float>(picked.redF());
                value[1] = static_cast<float>(picked.greenF());
                value[2] = static_cast<float>(picked.blueF());
                if (components == 4) value[3] = static_cast<float>(picked.alphaF());
                setValues(value);
                paint(value);
                SetParameter(index, value);
            });

        auto* reset = new QPushButton(QStringLiteral("↺"), row);
        reset->setFixedWidth(28);
        reset->setToolTip(QStringLiteral("Значение из HLSL"));
        layout->addWidget(reset);
        connect(reset, &QPushButton::clicked, this, [this, index, setValues, paint]
        {
            const auto value = state_.parameters[index].defaultValue;
            setValues(value);
            paint(value);
            SetParameter(index, value);
        });
        parameterForm_->addRow(parameter.name, row);
    }

    for (int index = 0; index < state_.textures.size(); ++index)
    {
        const auto& texture = state_.textures[index];
        auto* row = new QWidget(parameters_);
        auto* layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        auto* path = new QLineEdit(texture.path, row);
        path->setObjectName(QStringLiteral("MaterialTexture_%1").arg(texture.name));
        path->setReadOnly(true);
        path->setPlaceholderText(QStringLiteral("белая"));
        auto* choose = new QPushButton(QStringLiteral("…"), row);
        choose->setFixedWidth(28);
        choose->setToolTip(QStringLiteral("Выбрать текстуру проекта"));
        auto* clear = new QPushButton(QStringLiteral("×"), row);
        clear->setFixedWidth(28);
        clear->setToolTip(QStringLiteral("Белая текстура"));
        layout->addWidget(path, 1);
        layout->addWidget(choose);
        layout->addWidget(clear);
        connect(choose, &QPushButton::clicked, this, [this, index] { ChooseTexture(index, false); });
        connect(clear, &QPushButton::clicked, this, [this, index] { ChooseTexture(index, true); });
        parameterForm_->addRow(texture.name, row);
    }

    if (state_.parameters.isEmpty() && state_.textures.isEmpty())
        parameterForm_->addRow(new QLabel(QStringLiteral("В HLSL нет cbuffer MaterialParams и текстур t6+"), parameters_));
    parameters_->parentWidget()->setEnabled(true);
}

void MaterialEditorWindow::ShowErrors()
{
    errors_->clear();
    QSet<int> lines;
    const QString surface = QFileInfo(shaderFile_).absoluteFilePath();
    static const QRegularExpression location(QStringLiteral(R"(^(.*)\((\d+),(\d+)(?:-\d+)?\):)"));
    for (const QString& text : state_.errors.split(QLatin1Char('\n'), Qt::SkipEmptyParts))
    {
        auto* item = new QListWidgetItem(text.trimmed(), errors_);
        const auto match = location.match(text);
        if (!match.hasMatch() || surface.isEmpty()) continue;
        const QString file = QFileInfo(match.captured(1).trimmed()).absoluteFilePath();
        if (file.compare(surface, Qt::CaseInsensitive) != 0) continue;
        const int line = match.captured(2).toInt();
        item->setData(Qt::UserRole, line);
        item->setData(Qt::UserRole + 1, match.captured(3).toInt());
        item->setToolTip(QStringLiteral("Двойной щелчок — перейти к строке %1").arg(line));
        lines.insert(line);
    }
    source_->SetErrorLines(lines);
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
    setWindowTitle(QStringLiteral("%1[*] — %2").arg(QFileInfo(material_).fileName(), Tr("Материал", "Material")));
    setWindowModified(HasUnsavedChanges());
}

bool MaterialEditorWindow::HasUnsavedChanges() const
{
    return edited_ || state_.modified || (source_ && source_->document()->isModified());
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
    edited_ = true;
    UpdateTitle();
    const QByteArray material = material_.toUtf8(), name = state_.parameters[index].name.toUtf8();
    EngineThenRead([material, name, value](const EngineEditSessionApi& api)
    {
        return StatusText(api.setMaterialParameter(api.context, material.constData(), name.constData(), value.data()), nullptr);
    }, false);
}

void MaterialEditorWindow::ChooseTexture(int index, bool clear)
{
    QString texture;
    if (!clear)
    {
        const QString content = QString::fromUtf8(host_->host().contentRoot);
        const QString file = QFileDialog::getOpenFileName(this, QStringLiteral("Текстура для %1").arg(state_.textures[index].name),
                                                          content, QStringLiteral("Текстуры (*.png *.jpg *.jpeg *.bmp *.tga)"));
        if (file.isEmpty()) return;
        const QString relative = QDir(content).relativeFilePath(QFileInfo(file).absoluteFilePath());
        if (relative.startsWith(QStringLiteral("..")) || QDir::isAbsolutePath(relative))
        {
            QMessageBox::warning(this, QStringLiteral("Текстура вне проекта"),
                                 QStringLiteral("Сначала импортируйте текстуру в проект кнопкой «Импорт текстуры»."));
            return;
        }
        texture = QStringLiteral("/Game/") + QDir::fromNativeSeparators(relative);
    }
    edited_ = true;
    UpdateTitle();
    const QByteArray material = material_.toUtf8(), name = state_.textures[index].name.toUtf8(), path = texture.toUtf8();
    EngineThenRead([material, name, path](const EngineEditSessionApi& api)
    {
        char error[4096]{};
        return StatusText(api.setMaterialTexture(api.context, material.constData(), name.constData(), path.constData(),
                                                 error, sizeof(error)), error);
    }, true);
}

bool MaterialEditorWindow::Save(std::function<void(bool)> completion)
{
    if (!loaded_ || saving_) return false;
    const bool sourceChanged = source_->document()->isModified();
    if (sourceChanged)
    {
        QSaveFile file(shaderFile_);
        if (!file.open(QIODevice::WriteOnly) || file.write(source_->toPlainText().toUtf8()) < 0 || !file.commit())
        {
            QMessageBox::warning(this, QStringLiteral("HLSL не сохранён"),
                                 QStringLiteral("Не удалось записать %1: %2").arg(shaderFile_, file.errorString()));
            return false;
        }
        source_->document()->setModified(false);
    }
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
    LoadShaderText();
    edited_ = false;
    const QByteArray material = material_.toUtf8();
    EngineThenRead([material](const EngineEditSessionApi& api)
    {
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
        this, Tr("Несохранённый материал", "Unsaved Material"),
        Tr("Сохранить изменения материала %1?", "Save changes to material %1?").arg(QFileInfo(material_).fileName()),
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

void MaterialEditorWindow::Shutdown()
{
    if (shuttingDown_) return;
    shuttingDown_ = true;
    const bool revert = discard_ && (edited_ || state_.modified);
    hide(); // нативное окно живо, пока движок не отпустит превью
    QPointer<MaterialEditorWindow> self(this);
    EditorQtHost* host = host_;
    const QByteArray material = material_.toUtf8();
    auto engine = engine_;
    Engine([self, host, material, engine, revert](const EngineEditSessionApi& api)
    {
        if (revert) api.revertMaterial(api.context, material.constData(), nullptr, 0);
        if (engine->preview) api.detachMaterialPreview(api.context, engine->preview);
        engine->preview = 0;
        host->RunOnUi([self] { if (self) self->deleteLater(); });
    });
}

void MaterialEditorWindow::closeEvent(QCloseEvent* event)
{
    event->ignore();
    if (!shuttingDown_ && ConfirmClose()) Shutdown();
}
