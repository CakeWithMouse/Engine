#include "MaterialEditorWindow.h"
#include "MaterialGraphWidget.h"
#include "EditorLocale.h"
#include "MainWindow.h"

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
#include <QToolButton>
#include <QTabWidget>
#include <QMenu>
#include <QSlider>
#include <QGroupBox>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
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
        setStyleSheet(QStringLiteral("background: transparent; border: none;"));
        setMinimumSize(220, 200);
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
    resize(1240, 800);
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
        saveAction_->setText(Tr("💾 Сохранить и Скомпилировать", "💾 Save & Compile"));
        saveAction_->setToolTip(Tr("Записать HLSL и .mat, перекомпилировать шейдер (Ctrl+S)", "Write HLSL and .mat, recompile shader (Ctrl+S)"));
    }
    if (revertAction_) {
        revertAction_->setText(Tr("↺ Откатить", "↺ Revert"));
        revertAction_->setToolTip(Tr("Вернуть материал и HLSL к сохранённым файлам", "Revert material and HLSL to saved files"));
    }
    if (btnPresets_) btnPresets_->setText(Tr("⚡ Пресеты PBR / ORM ▾", "⚡ PBR / ORM Presets ▾"));
    if (actPresetPbrOrm_) actPresetPbrOrm_->setText(Tr("Standard PBR + ORM", "Standard PBR + ORM"));
    if (actPresetUnlit_) actPresetUnlit_->setText(Tr("Unlit Emissive (Свечение)", "Unlit Emissive"));
    if (actPresetLava_) actPresetLava_->setText(Tr("Animated Lava / Pulse (Анимация)", "Animated Lava / Pulse"));
    if (actPresetHolo_) actPresetHolo_->setText(Tr("Fresnel Hologram (Голограмма)", "Fresnel Hologram"));
    if (actPresetFoliage_) actPresetFoliage_->setText(Tr("Masked Foliage (Листва)", "Masked Foliage"));
    if (actCreateInstance_) actCreateInstance_->setText(Tr("Создать Material Instance…", "Create Material Instance…"));

    if (shapeLabel_) shapeLabel_->setText(Tr(" Форма: ", " Shape: "));
    if (btnResetView_) btnResetView_->setText(Tr("↺ Ракурс", "↺ Reset View"));
    if (btnResetView_) btnResetView_->setToolTip(Tr("Сбросить положение камеры превью", "Reset preview camera"));

    if (shape_) {
        QSignalBlocker blocker(shape_);
        const int cur = shape_->currentIndex();
        shape_->setItemText(0, Tr("Сфера", "Sphere"));
        shape_->setItemText(1, Tr("Куб", "Cube"));
        shape_->setItemText(2, Tr("Плоскость", "Plane"));
        shape_->setCurrentIndex(cur);
    }

    if (blendLabel_) blendLabel_->setText(Tr("Режим смешивания (Blend Mode)", "Blend Mode"));
    if (blend_) {
        QSignalBlocker blocker(blend_);
        const int cur = blend_->currentIndex();
        blend_->setItemText(0, Tr("Непрозрачный PBR (Opaque)", "Opaque PBR"));
        blend_->setItemText(1, Tr("Маска прозрачности (Masked / Alpha Clip)", "Masked / Alpha Clip"));
        blend_->setItemText(2, Tr("Полупрозрачный (Translucent / Glass)", "Translucent / Glass"));
        blend_->setItemText(3, Tr("Аддитивное свечение (Additive)", "Additive Emissive"));
        blend_->setCurrentIndex(cur);
    }
    if (alphaClipLabel_) alphaClipLabel_->setText(Tr("Порог альфы (Alpha Clip)", "Alpha Clip"));
    if (unlit_) unlit_->setText(Tr("Без освещения (Unlit / Emissive)", "Unlit / Emissive"));
    if (twoSided_) twoSided_->setText(Tr("Двусторонний рендер (Two-Sided)", "Two-Sided"));
    if (castShadows_) castShadows_->setText(Tr("Отбрасывать тени (Cast Shadows)", "Cast Shadows"));

    if (btnInsertOrm_) btnInsertOrm_->setText(Tr("⚡ Вставить сэмплирование ORM в шейдер", "⚡ Insert ORM Sampling into HLSL"));
    if (parametersTitle_) parametersTitle_->setText(Tr("<b>Параметры шейдера (Tweakables)</b>", "<b>Shader Parameters (Tweakables)</b>"));

    if (mainTabs_) {
        mainTabs_->setTabText(0, Tr("Нодовый Граф (Node Graph + AI)", "Node Graph (+ AI)"));
        mainTabs_->setTabText(1, Tr("HLSL Surface Code (Код шейдера)", "HLSL Surface Code"));
    }

    if (graphWidget_) graphWidget_->RetranslateUi();
}

QString MaterialEditorWindow::PhysicalPath(const QString& virtualPath) const
{
    if (!virtualPath.startsWith(QStringLiteral("/Game/"))) return {};
    return QDir(QString::fromUtf8(host_->host().contentRoot)).filePath(virtualPath.mid(6));
}

void MaterialEditorWindow::BuildUi()
{
    // ==========================================
    // 1. Верхний тулбар окна материала (UE5 style)
    // ==========================================
    auto* tools = addToolBar(Tr("Материал", "Material"));
    tools->setMovable(false);
    tools->setStyleSheet(QStringLiteral(
        "QToolBar { background-color: #16181D; border-bottom: 1px solid #282C34; spacing: 4px; padding: 3px; }"
    ));

    saveAction_ = tools->addAction(Tr("💾 Сохранить и Скомпилировать", "💾 Save & Compile"), this, [this] { Save(); });
    saveAction_->setShortcut(QKeySequence::Save);
    saveAction_->setToolTip(Tr("Записать HLSL и .mat, перекомпилировать шейдер (Ctrl+S)", "Write HLSL and .mat, recompile shader (Ctrl+S)"));

    revertAction_ = tools->addAction(Tr("↺ Откатить", "↺ Revert"), this, [this] { Revert(); });
    revertAction_->setToolTip(Tr("Вернуть материал и HLSL к сохранённым файлам", "Revert material and HLSL to saved files"));

    tools->addSeparator();

    // Пресеты PBR / ORM
    btnPresets_ = new QToolButton(tools);
    btnPresets_->setText(Tr("⚡ Пресеты PBR / ORM ▾", "⚡ PBR / ORM Presets ▾"));
    btnPresets_->setPopupMode(QToolButton::InstantPopup);
    btnPresets_->setStyleSheet(QStringLiteral(
        "QToolButton { background: #1E293B; color: #38BDF8; font-weight: bold; border: 1px solid #0284C7; border-radius: 4px; padding: 3px 8px; font-size: 11px; }"
        "QToolButton:hover { background: #0369A1; color: white; }"
    ));
    auto* menuPresets = new QMenu(btnPresets_);
    actPresetPbrOrm_ = menuPresets->addAction(Tr("Standard PBR + ORM", "Standard PBR + ORM"), this, [this] { ApplyPbrOrmPreset(0); });
    actPresetUnlit_ = menuPresets->addAction(Tr("Unlit Emissive (Свечение)", "Unlit Emissive"), this, [this] { ApplyPbrOrmPreset(1); });
    actPresetLava_ = menuPresets->addAction(Tr("Animated Lava / Pulse (Анимация)", "Animated Lava / Pulse"), this, [this] { ApplyPbrOrmPreset(2); });
    actPresetHolo_ = menuPresets->addAction(Tr("Fresnel Hologram (Голограмма)", "Fresnel Hologram"), this, [this] { ApplyPbrOrmPreset(3); });
    actPresetFoliage_ = menuPresets->addAction(Tr("Masked Foliage (Листва)", "Masked Foliage"), this, [this] { ApplyPbrOrmPreset(4); });
    btnPresets_->setMenu(menuPresets);
    tools->addWidget(btnPresets_);

    // Кнопка создания Material Instance
    auto* btnInst = new QToolButton(tools);
    btnInst->setText(Tr("✨ Material Instance…", "✨ Material Instance…"));
    btnInst->setStyleSheet(QStringLiteral(
        "QToolButton { background: #1E293B; color: #A78BFA; border: 1px solid #6D28D9; border-radius: 4px; padding: 3px 8px; font-size: 11px; }"
        "QToolButton:hover { background: #5B21B6; color: white; }"
    ));
    connect(btnInst, &QToolButton::clicked, this, [this] { CreateMaterialInstance(); });
    tools->addWidget(btnInst);

    tools->addSeparator();

    shapeLabel_ = new QLabel(Tr(" Форма: ", " Shape: "), tools);
    tools->addWidget(shapeLabel_);
    shape_ = new QComboBox(tools);
    shape_->setObjectName(QStringLiteral("MaterialPreviewShape"));
    shape_->addItems({Tr("Сфера", "Sphere"), Tr("Куб", "Cube"), Tr("Плоскость", "Plane")});
    tools->addWidget(shape_);

    btnResetView_ = new QPushButton(Tr("↺ Ракурс", "↺ Reset View"), tools);
    btnResetView_->setToolTip(Tr("Сбросить положение камеры превью", "Reset preview camera"));
    btnResetView_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #1F2430; color: #CBD5E1; border: 1px solid #333844; border-radius: 3px; padding: 2px 6px; font-size: 10px; }"
        "QPushButton:hover { background: #2A3040; color: white; }"
    ));
    tools->addWidget(btnResetView_);

    // ==========================================
    // 2. Левая колонка (Preview + Свойства)
    // ==========================================
    preview_ = new MaterialPreviewWidget(this);
    preview_->setObjectName(QStringLiteral("MaterialPreview"));

    auto* leftScroll = new QScrollArea(this);
    leftScroll->setWidgetResizable(true);
    leftScroll->setStyleSheet(QStringLiteral("QScrollArea { border: none; background: #181A20; }"));

    auto* settings = new QWidget(leftScroll);
    settings->setObjectName(QStringLiteral("MaterialSettingsPanel"));
    auto* settingsLayout = new QVBoxLayout(settings);
    settingsLayout->setContentsMargins(8, 8, 8, 8);
    settingsLayout->setSpacing(8);

    // Секция 1: Surface & Blend State
    auto* grpSurface = new QGroupBox(Tr("Свойства поверхности (Surface & Blend)", "Surface & Blend"), settings);
    grpSurface->setStyleSheet(QStringLiteral(
        "QGroupBox { font-weight: bold; color: #60A5FA; border: 1px solid #2B303C; border-radius: 6px; margin-top: 6px; padding-top: 10px; background: #1C1F26; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; }"
    ));
    auto* formSurface = new QFormLayout(grpSurface);
    formSurface->setContentsMargins(6, 6, 6, 6);
    formSurface->setSpacing(6);

    blend_ = new QComboBox(grpSurface);
    blend_->setObjectName(QStringLiteral("MaterialBlend"));
    blend_->addItems({
        Tr("Непрозрачный PBR (Opaque)", "Opaque PBR"),
        Tr("Маска прозрачности (Masked / Alpha Clip)", "Masked / Alpha Clip"),
        Tr("Полупрозрачный (Translucent / Glass)", "Translucent / Glass"),
        Tr("Аддитивное свечение (Additive)", "Additive Emissive")
    });
    blendLabel_ = new QLabel(Tr("Режим смешивания (Blend Mode)", "Blend Mode"), grpSurface);
    formSurface->addRow(blendLabel_, blend_);

    alphaClip_ = new QDoubleSpinBox(grpSurface);
    alphaClip_->setObjectName(QStringLiteral("MaterialAlphaClip"));
    alphaClip_->setRange(0.0, 1.0);
    alphaClip_->setSingleStep(0.05);
    alphaClip_->setDecimals(3);
    alphaClipLabel_ = new QLabel(Tr("Порог альфы (Alpha Clip)", "Alpha Clip"), grpSurface);
    formSurface->addRow(alphaClipLabel_, alphaClip_);

    unlit_ = new QCheckBox(Tr("Без освещения (Unlit / Emissive)", "Unlit / Emissive"), grpSurface);
    unlit_->setObjectName(QStringLiteral("MaterialUnlit"));
    formSurface->addRow(unlit_);

    twoSided_ = new QCheckBox(Tr("Двусторонний рендер (Two-Sided)", "Two-Sided"), grpSurface);
    twoSided_->setObjectName(QStringLiteral("MaterialTwoSided"));
    formSurface->addRow(twoSided_);

    castShadows_ = new QCheckBox(Tr("Отбрасывать тени (Cast Shadows)", "Cast Shadows"), grpSurface);
    castShadows_->setObjectName(QStringLiteral("MaterialCastShadows"));
    formSurface->addRow(castShadows_);

    settingsLayout->addWidget(grpSurface);

    // Секция 2: PBR & ORM Texture Channels
    auto* grpPbr = new QGroupBox(Tr("PBR & ORM Текстуры", "PBR & ORM Textures"), settings);
    grpPbr->setStyleSheet(QStringLiteral(
        "QGroupBox { font-weight: bold; color: #34D399; border: 1px solid #2B303C; border-radius: 6px; margin-top: 6px; padding-top: 10px; background: #1C1F26; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; }"
    ));
    auto* layoutPbr = new QVBoxLayout(grpPbr);
    layoutPbr->setContentsMargins(6, 6, 6, 6);
    layoutPbr->setSpacing(4);

    btnInsertOrm_ = new QPushButton(Tr("⚡ Вставить сэмплирование ORM в шейдер", "⚡ Insert ORM Sampling into HLSL"), grpPbr);
    btnInsertOrm_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #065F46; color: #6EE7B7; border: 1px solid #059669; border-radius: 4px; padding: 3px 6px; font-size: 11px; font-weight: bold; }"
        "QPushButton:hover { background: #047857; color: white; }"
    ));
    connect(btnInsertOrm_, &QPushButton::clicked, this, [this] { InsertOrmSamplingIntoHlsl(); });
    layoutPbr->addWidget(btnInsertOrm_);

    pbrChannelsCard_ = grpPbr;
    settingsLayout->addWidget(grpPbr);

    // Секция 3: Shader Parameters (Tweakables)
    auto* grpParams = new QGroupBox(Tr("Параметры шейдера (Tweakables)", "Shader Parameters (Tweakables)"), settings);
    grpParams->setStyleSheet(QStringLiteral(
        "QGroupBox { font-weight: bold; color: #FBBF24; border: 1px solid #2B303C; border-radius: 6px; margin-top: 6px; padding-top: 10px; background: #1C1F26; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; }"
    ));
    auto* layoutParams = new QVBoxLayout(grpParams);
    layoutParams->setContentsMargins(6, 6, 6, 6);
    layoutParams->setSpacing(4);

    parameters_ = new QWidget(grpParams);
    parameterForm_ = new QFormLayout(parameters_);
    parameterForm_->setContentsMargins(0, 0, 0, 0);
    parameterForm_->setSpacing(6);
    layoutParams->addWidget(parameters_);

    settingsLayout->addWidget(grpParams);
    settingsLayout->addStretch(1);

    leftScroll->setWidget(settings);
    settings->setEnabled(false); // до первого ответа движка

    auto* leftSplitter = new QSplitter(Qt::Vertical, this);
    leftSplitter->addWidget(preview_);
    leftSplitter->addWidget(leftScroll);
    leftSplitter->setStretchFactor(0, 3);
    leftSplitter->setStretchFactor(1, 4);

    // ==========================================
    // 3. Правая колонка (Табы: Graph + Source) + Диагностика
    // ==========================================
    mainTabs_ = new QTabWidget(this);
    mainTabs_->setStyleSheet(QStringLiteral(
        "QTabWidget::pane { border: 1px solid #282C34; background: #14161C; }"
        "QTabBar::tab { background: #181A20; color: #94A3B8; border: 1px solid #282C34; padding: 6px 14px; font-weight: bold; border-top-left-radius: 4px; border-top-right-radius: 4px; }"
        "QTabBar::tab:selected { background: #2563EB; color: #FFFFFF; border-color: #3B82F6; }"
    ));

    // Вкладка 0: Визуальный нодовый граф + AI
    graphWidget_ = new MaterialGraphWidget(mainTabs_);
    graphWidget_->SetOnHlslGeneratedCallback([this](const QString& hlsl) {
        if (source_) {
            source_->setPlainText(hlsl);
            source_->document()->setModified(true);
            UpdateTitle();
            Save();
        }
    });
    mainTabs_->addTab(graphWidget_, Tr("Нодовый Граф (Node Graph + AI)", "Node Graph (+ AI)"));

    // Вкладка 1: HLSL редактор кода
    source_ = new MaterialSourceEditor(mainTabs_);
    source_->setObjectName(QStringLiteral("MaterialSource"));
    mainTabs_->addTab(source_, Tr("HLSL Surface Code (Код шейдера)", "HLSL Surface Code"));

    // Панель диагностики компилятора снизу
    auto* diagWidget = new QWidget(this);
    diagWidget->setStyleSheet(QStringLiteral("background: #121418; border-top: 1px solid #242832;"));
    auto* diagLayout = new QVBoxLayout(diagWidget);
    diagLayout->setContentsMargins(4, 4, 4, 4);
    diagLayout->setSpacing(2);

    lblCompileStatus_ = new QLabel(diagWidget);
    lblCompileStatus_->setStyleSheet(QStringLiteral(
        "background: #064E3B; color: #34D399; font-weight: bold; font-size: 11px; padding: 3px 8px; border-radius: 3px; border: 1px solid #059669;"
    ));
    lblCompileStatus_->setText(Tr("✓ Шейдер скомпилирован успешно (Ready)", "✓ Shader Compiled Successfully (Ready)"));
    diagLayout->addWidget(lblCompileStatus_);

    errors_ = new QListWidget(diagWidget);
    errors_->setObjectName(QStringLiteral("MaterialErrors"));
    errors_->setStyleSheet(QStringLiteral(
        "QListWidget#MaterialErrors { background: #181A20; color: #F87171; border: 1px solid #2D3340; border-radius: 4px; font-family: Consolas, monospace; font-size: 11px; }"
    ));
    diagLayout->addWidget(errors_, 1);

    auto* rightSplitter = new QSplitter(Qt::Vertical, this);
    rightSplitter->addWidget(mainTabs_);
    rightSplitter->addWidget(diagWidget);
    rightSplitter->setStretchFactor(0, 5);
    rightSplitter->setStretchFactor(1, 1);
    rightSplitter->setSizes({ 560, 140 });

    auto* mainSplitter = new QSplitter(Qt::Horizontal, this);
    mainSplitter->addWidget(leftSplitter);
    mainSplitter->addWidget(rightSplitter);
    mainSplitter->setStretchFactor(0, 2);
    mainSplitter->setStretchFactor(1, 3);
    mainSplitter->setSizes({ 360, 840 });

    setCentralWidget(mainSplitter);
    statusBar();

    // ==========================================
    // 4. Подключение событий движка
    // ==========================================
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

    connect(btnResetView_, &QPushButton::clicked, this, [this, engine] {
        Engine([engine](const EngineEditSessionApi& api) {
            if (engine->preview) api.materialPreviewCamera(api.context, engine->preview, 0.0f, 0.0f, 0.0f);
        });
    });

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
            if (graphWidget_)
            {
                graphWidget_->SetMaterialPath(PhysicalPath(material_), shaderFile_);
            }
        }
        if (first || rebuild) RebuildSettings();
        ShowErrors();
    }
    else if (!loaded_)
    {
        errors_->clear();
        errors_->addItem(error);
        UpdateCompileStatus(false, 1);
    }

    if (!error.isEmpty())
    {
        statusBar()->showMessage(error);
        UpdateCompileStatus(false, 1);
    }
    else
    {
        statusBar()->showMessage(state_.errors.isEmpty() ? Tr("Шейдер скомпилирован", "Shader compiled")
                                                              : Tr("Ошибки компиляции: см. список под текстом", "Compilation errors: see list"));
        UpdateCompileStatus(state_.errors.isEmpty(), state_.errors.isEmpty() ? 0 : 1);
    }
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
        layout->setSpacing(4);

        if (!parameter.components)
        {
            layout->addWidget(new QLabel(QStringLiteral("тип не поддерживается редактором"), row));
            parameterForm_->addRow(parameter.name, row);
            continue;
        }

        std::vector<QDoubleSpinBox*> spins;
        QSlider* syncSlider = nullptr;

        // Если скаляр 0..1 (Metallic, Roughness, Gloss, Specular) — добавляем синхронный ползунок
        if (parameter.components == 1)
        {
            auto* spin = new QDoubleSpinBox(row);
            spin->setObjectName(QStringLiteral("MaterialParam_%1_0").arg(parameter.name));
            spin->setRange(-10000.0, 10000.0);
            spin->setDecimals(3);
            spin->setSingleStep(0.05);
            spin->setValue(parameter.value[0]);
            spin->setMinimumWidth(55);
            layout->addWidget(spin);
            spins.push_back(spin);

            syncSlider = new QSlider(Qt::Horizontal, row);
            syncSlider->setRange(0, 100);
            syncSlider->setValue(qBound(0, static_cast<int>(parameter.value[0] * 100.0), 100));
            syncSlider->setFixedWidth(70);
            layout->addWidget(syncSlider);

            connect(syncSlider, &QSlider::valueChanged, this, [spin](int v) {
                spin->setValue(v / 100.0);
            });
            connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [syncSlider](double v) {
                QSignalBlocker b(syncSlider);
                syncSlider->setValue(qBound(0, static_cast<int>(v * 100.0), 100));
            });
        }
        else
        {
            for (uint32_t i = 0; i < parameter.components; ++i)
            {
                auto* spin = new QDoubleSpinBox(row);
                spin->setObjectName(QStringLiteral("MaterialParam_%1_%2").arg(parameter.name).arg(i));
                spin->setRange(-100000.0, 100000.0);
                spin->setDecimals(3);
                spin->setSingleStep(0.05);
                spin->setKeyboardTracking(false);
                spin->setValue(parameter.value[i]);
                spin->setMinimumWidth(50);
                layout->addWidget(spin, 1);
                spins.push_back(spin);
            }
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
            color->setFixedSize(26, 20);
            color->setToolTip(Tr("Выбрать цвет", "Choose Color"));
            layout->addWidget(color);
        }
        const uint32_t components = parameter.components;
        auto paint = [color, components](const std::array<float, 4>& value) {
            if (color) color->setStyleSheet(QStringLiteral("background-color: %1; border: 1px solid #444; border-radius: 3px;").arg(ToColor(value, components).name()));
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
        reset->setFixedWidth(24);
        reset->setToolTip(Tr("Сбросить к исходному из HLSL", "Reset to default from HLSL"));
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

    // Текстурные слоты
    for (int index = 0; index < state_.textures.size(); ++index)
    {
        const auto& texture = state_.textures[index];
        auto* row = new QWidget(parameters_);
        auto* layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(4);

        auto* path = new QLineEdit(texture.path, row);
        path->setObjectName(QStringLiteral("MaterialTexture_%1").arg(texture.name));
        path->setReadOnly(true);
        path->setPlaceholderText(Tr("белая текстура", "default white"));
        path->setStyleSheet(QStringLiteral("background: #1C1F26; color: #CBD5E1; border: 1px solid #333844; border-radius: 3px; font-size: 10px;"));

        auto* choose = new QPushButton(QStringLiteral("…"), row);
        choose->setFixedWidth(24);
        choose->setToolTip(Tr("Выбрать текстуру проекта", "Choose project texture"));

        auto* clear = new QPushButton(QStringLiteral("×"), row);
        clear->setFixedWidth(24);
        clear->setToolTip(Tr("Очистить текстуру", "Clear texture"));

        layout->addWidget(path, 1);
        layout->addWidget(choose);
        layout->addWidget(clear);

        connect(choose, &QPushButton::clicked, this, [this, index] { ChooseTexture(index, false); });
        connect(clear, &QPushButton::clicked, this, [this, index] { ChooseTexture(index, true); });

        parameterForm_->addRow(texture.name, row);
    }

    if (state_.parameters.isEmpty() && state_.textures.isEmpty())
        parameterForm_->addRow(new QLabel(Tr("В HLSL нет cbuffer MaterialParams и текстур t6+", "No cbuffer MaterialParams or textures in HLSL"), parameters_));

    parameters_->parentWidget()->setEnabled(true);
}

void MaterialEditorWindow::UpdateCompileStatus(bool success, int errorCount)
{
    if (!lblCompileStatus_) return;
    if (success && errorCount == 0)
    {
        lblCompileStatus_->setText(Tr("✓ Шейдер скомпилирован успешно (Ready)", "✓ Shader Compiled Successfully (Ready)"));
        lblCompileStatus_->setStyleSheet(QStringLiteral(
            "background: #064E3B; color: #34D399; font-weight: bold; font-size: 11px; padding: 3px 8px; border-radius: 3px; border: 1px solid #059669;"
        ));
    }
    else
    {
        lblCompileStatus_->setText(Tr("⚠ Ошибки компиляции (%1) — см. список ниже", "⚠ Compilation Errors (%1) — see list below").arg(std::max(1, errorCount)));
        lblCompileStatus_->setStyleSheet(QStringLiteral(
            "background: #450A0A; color: #F87171; font-weight: bold; font-size: 11px; padding: 3px 8px; border-radius: 3px; border: 1px solid #DC2626;"
        ));
    }
}

void MaterialEditorWindow::ShowErrors()
{
    errors_->clear();
    QSet<int> lines;
    const QString surface = QFileInfo(shaderFile_).absoluteFilePath();
    static const QRegularExpression location(QStringLiteral(R"(^(.*)\((\d+),(\d+)(?:-\d+)?\):)"));

    int errCount = 0;
    for (const QString& text : state_.errors.split(QLatin1Char('\n'), Qt::SkipEmptyParts))
    {
        auto* item = new QListWidgetItem(text.trimmed(), errors_);
        errCount++;
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
    UpdateCompileStatus(errCount == 0, errCount);
}

void MaterialEditorWindow::GoToError(int line, int column)
{
    if (mainTabs_) mainTabs_->setCurrentIndex(1); // Переключить на вкладку HLSL
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

void MaterialEditorWindow::ApplyPbrOrmPreset(int presetId)
{
    QString code;
    if (presetId == 0) // Standard PBR + ORM
    {
        code = QStringLiteral(
            "// Standard PBR + ORM Material\n"
            "cbuffer MaterialParams : register(b1)\n"
            "{\n"
            "    float4 BaseColor = float4(0.85f, 0.85f, 0.85f, 1.0f);\n"
            "    float Metallic = 0.0f;\n"
            "    float Roughness = 0.5f;\n"
            "    float AmbientOcclusion = 1.0f;\n"
            "};\n\n"
            "Texture2D BaseMap : register(t6);\n"
            "Texture2D NormalMap : register(t7);\n"
            "Texture2D OrmMap : register(t8);\n\n"
            "void Surface(MaterialInput i, inout MaterialOutput o)\n"
            "{\n"
            "    float4 baseTex = BaseMap.Sample(MaterialSampler, i.UV);\n"
            "    float3 ormTex = OrmMap.Sample(MaterialSampler, i.UV).rgb; // R: AO, G: Roughness, B: Metallic\n\n"
            "    float3 finalColor = BaseColor.rgb * baseTex.rgb;\n"
            "    float finalRoughness = Roughness * ormTex.g;\n"
            "    float finalMetallic = Metallic * ormTex.b;\n"
            "    float finalAO = AmbientOcclusion * ormTex.r;\n\n"
            "    o.Albedo = saturate(finalColor);\n"
            "    o.SpecularWeight = saturate(1.0f - finalRoughness);\n"
            "    o.AmbientModel = saturate(finalAO);\n"
            "    o.Alpha = BaseColor.a * baseTex.a;\n"
            "}\n"
        );
        blend_->setCurrentIndex(0); // Opaque
    }
    else if (presetId == 1) // Unlit Emissive
    {
        code = QStringLiteral(
            "// Unlit Emissive Material\n"
            "cbuffer MaterialParams : register(b1)\n"
            "{\n"
            "    float4 EmissiveColor = float4(0.0f, 0.85f, 1.0f, 1.0f);\n"
            "    float Intensity = 2.5f;\n"
            "};\n\n"
            "Texture2D BaseMap : register(t6);\n\n"
            "void Surface(MaterialInput i, inout MaterialOutput o)\n"
            "{\n"
            "    float4 baseTex = BaseMap.Sample(MaterialSampler, i.UV);\n"
            "    o.Albedo = saturate(EmissiveColor.rgb * baseTex.rgb * Intensity);\n"
            "    o.SpecularWeight = 0.0f;\n"
            "    o.AmbientModel = 1.0f;\n"
            "    o.Alpha = EmissiveColor.a * baseTex.a;\n"
            "}\n"
        );
        unlit_->setChecked(true);
    }
    else if (presetId == 2) // Animated Lava / Pulse
    {
        code = QStringLiteral(
            "// Animated Lava / Pulse\n"
            "#define MATERIAL_EMISSIVE 1\n\n"
            "cbuffer MaterialParams : register(b1)\n"
            "{\n"
            "    float4 LavaColor = float4(1.0f, 0.25f, 0.05f, 1.0f);\n"
            "    float4 CrustColor = float4(0.12f, 0.08f, 0.06f, 1.0f);\n"
            "    float PulseSpeed = 2.5f;\n"
            "    float GlowIntensity = 3.0f;\n"
            "};\n\n"
            "void Surface(MaterialInput i, inout MaterialOutput o)\n"
            "{\n"
            "    float wave = 0.5f + 0.5f * sin(i.Time * PulseSpeed + (i.WorldPos.x + i.WorldPos.z) * 2.0f);\n"
            "    float3 col = lerp(CrustColor.rgb, LavaColor.rgb, wave);\n\n"
            "    o.Albedo = col;\n"
            "    o.Emissive = LavaColor.rgb * (wave * GlowIntensity);\n"
            "    o.SpecularWeight = 0.2f * (1.0f - wave);\n"
            "    o.AmbientModel = 0.5f;\n"
            "}\n"
        );
    }
    else if (presetId == 3) // Fresnel Hologram
    {
        code = QStringLiteral(
            "// Fresnel Hologram\n"
            "#define MATERIAL_EMISSIVE 1\n\n"
            "cbuffer MaterialParams : register(b1)\n"
            "{\n"
            "    float4 HoloColor = float4(0.1f, 0.9f, 1.0f, 1.0f);\n"
            "    float FresnelPower = 3.0f;\n"
            "    float ScanlineSpeed = 4.0f;\n"
            "    float ScanlineDensity = 40.0f;\n"
            "};\n\n"
            "void Surface(MaterialInput i, inout MaterialOutput o)\n"
            "{\n"
            "    float fresnel = pow(1.0f - saturate(dot(i.Normal, i.ViewDir)), FresnelPower);\n"
            "    float scanline = 0.7f + 0.3f * sin(i.WorldPos.y * ScanlineDensity + i.Time * ScanlineSpeed);\n"
            "    float3 glow = HoloColor.rgb * fresnel * scanline * 2.0f;\n\n"
            "    o.Albedo = glow;\n"
            "    o.Emissive = glow;\n"
            "    o.SpecularWeight = 0.9f;\n"
            "    o.AmbientModel = 1.0f;\n"
            "    o.Alpha = saturate(fresnel * scanline + 0.15f);\n"
            "}\n"
        );
        blend_->setCurrentIndex(2); // Translucent
    }
    else if (presetId == 4) // Masked Foliage
    {
        code = QStringLiteral(
            "// Masked Foliage\n"
            "cbuffer MaterialParams : register(b1)\n"
            "{\n"
            "    float4 FoliageTint = float4(0.28f, 0.65f, 0.22f, 1.0f);\n"
            "    float WindSpeed = 1.5f;\n"
            "    float AlphaCutoff = 0.5f;\n"
            "};\n\n"
            "Texture2D BaseMap : register(t6);\n\n"
            "void Surface(MaterialInput i, inout MaterialOutput o)\n"
            "{\n"
            "    float4 tex = BaseMap.Sample(MaterialSampler, i.UV);\n"
            "    float3 col = FoliageTint.rgb * tex.rgb;\n\n"
            "    o.Albedo = saturate(col);\n"
            "    o.SpecularWeight = 0.15f;\n"
            "    o.AmbientModel = 0.85f;\n"
            "    o.Alpha = tex.a;\n"
            "}\n"
        );
        blend_->setCurrentIndex(1); // Masked
        twoSided_->setChecked(true);
    }

    if (source_)
    {
        source_->setPlainText(code);
        source_->document()->setModified(true);
        UpdateTitle();
        Save();
    }
}

void MaterialEditorWindow::CreateMaterialInstance()
{
    QFileInfo fi(PhysicalPath(material_));
    QString defaultName = fi.baseName() + QStringLiteral("_Inst");

    bool ok = false;
    QString name = QInputDialog::getText(this, Tr("Новый Material Instance", "New Material Instance"),
        Tr("Имя дочернего материала:", "Child Material Name:"), QLineEdit::Normal, defaultName, &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    QString fileName = name.trimmed();
    if (!fileName.endsWith(QStringLiteral(".mat"), Qt::CaseInsensitive))
        fileName += QStringLiteral(".mat");

    QString dirPath = fi.dir().absolutePath();
    QString fullPath = QDir(dirPath).filePath(fileName);

    QJsonObject root;
    root[QStringLiteral("version")] = 1;
    root[QStringLiteral("name")] = QFileInfo(fullPath).baseName();
    root[QStringLiteral("parent")] = material_;
    root[QStringLiteral("isInstance")] = true;

    QFile file(fullPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        file.close();

        // Открыть новый инстанс материала
        if (auto* editor = qobject_cast<EditorMainWindow*>(parentWidget()))
        {
            editor->OpenMaterialEditor(fullPath);
        }
    }
}

void MaterialEditorWindow::InsertOrmSamplingIntoHlsl()
{
    if (!source_) return;
    QString text = source_->toPlainText();

    if (!text.contains(QStringLiteral("OrmMap"), Qt::CaseInsensitive))
    {
        // Вставить Texture2D OrmMap : register(t8);
        const int insertPos = text.indexOf(QStringLiteral("void Surface"));
        if (insertPos >= 0)
        {
            text.insert(insertPos, QStringLiteral("Texture2D OrmMap : register(t8);\n\n"));
        }
        else
        {
            text += QStringLiteral("\nTexture2D OrmMap : register(t8);\n");
        }
    }

    if (!text.contains(QStringLiteral("OrmMap.Sample")))
    {
        const int surfaceStart = text.indexOf(QStringLiteral("void Surface"));
        if (surfaceStart >= 0)
        {
            const int braceOpen = text.indexOf(QLatin1Char('{'), surfaceStart);
            if (braceOpen >= 0)
            {
                QString injection = QStringLiteral("\n    // ORM Map unpacking (R: Ambient Occlusion, G: Roughness, B: Metallic)\n"
                                                   "    float3 orm = OrmMap.Sample(MaterialSampler, i.UV).rgb;\n"
                                                   "    o.SpecularWeight = saturate(1.0f - orm.g);\n"
                                                   "    o.AmbientModel = saturate(orm.r);\n");
                text.insert(braceOpen + 1, injection);
            }
        }
    }

    source_->setPlainText(text);
    source_->document()->setModified(true);
    UpdateTitle();
    Save();
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
    hide();
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
