#include "ObjectInspectorPanel.h"
#include "QtHost.h"
#include "EditorLocale.h"

#include <QFormLayout>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QCheckBox>
#include <QPushButton>
#include <QToolButton>
#include <QMenu>
#include <QApplication>
#include <QQuaternion>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QLabel>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFrame>
#include <QSizePolicy>
#include <QSlider>
#include <QComboBox>
#include <QTimer>
#include <algorithm>
#include <cmath>

class CollapsibleCard : public QFrame
{
public:
    explicit CollapsibleCard(const QString& title, QWidget* parent = nullptr, bool expanded = true)
        : QFrame(parent), expanded_(expanded), titleText_(title)
    {
        setObjectName(QStringLiteral("CollapsibleCard"));
        setStyleSheet(QStringLiteral(
            "QFrame#CollapsibleCard {"
            "  background-color: #171A21;"
            "  border: 1px solid #282D37;"
            "  border-radius: 6px;"
            "  margin-bottom: 6px;"
            "}"
        ));

        auto* mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);

        headerBtn_ = new QPushButton(this);
        headerBtn_->setObjectName(QStringLiteral("CardHeaderBtn"));
        headerBtn_->setCursor(Qt::PointingHandCursor);
        headerBtn_->setStyleSheet(QStringLiteral(
            "QPushButton#CardHeaderBtn {"
            "  background-color: #1E222B;"
            "  color: #E2E8F0;"
            "  font-weight: 600;"
            "  font-size: 11px;"
            "  text-align: left;"
            "  padding: 6px 10px;"
            "  border: none;"
            "  border-top-left-radius: 5px;"
            "  border-top-right-radius: 5px;"
            "}"
            "QPushButton#CardHeaderBtn:hover {"
            "  background-color: #262C38;"
            "  color: #FFFFFF;"
            "}"
        ));
        updateHeaderText();

        QObject::connect(headerBtn_, &QPushButton::clicked, [this] {
            SetExpanded(!expanded_);
        });
        mainLayout->addWidget(headerBtn_);

        content_ = new QWidget(this);
        contentLayout_ = new QVBoxLayout(content_);
        contentLayout_->setContentsMargins(8, 8, 8, 8);
        contentLayout_->setSpacing(6);
        mainLayout->addWidget(content_);

        content_->setVisible(expanded_);
    }

    void SetTitle(const QString& title)
    {
        titleText_ = title;
        updateHeaderText();
    }

    void SetExpanded(bool exp)
    {
        expanded_ = exp;
        content_->setVisible(expanded_);
        updateHeaderText();
    }

    bool IsExpanded() const { return expanded_; }

    QWidget* Content() const { return content_; }
    QVBoxLayout* ContentLayout() const { return contentLayout_; }

private:
    void updateHeaderText()
    {
        const QString arrow = expanded_ ? QStringLiteral("▾  ") : QStringLiteral("▸  ");
        headerBtn_->setText(arrow + titleText_);
    }

    bool expanded_ = true;
    QString titleText_;
    QPushButton* headerBtn_ = nullptr;
    QWidget* content_ = nullptr;
    QVBoxLayout* contentLayout_ = nullptr;
};

static QWidget* CreateAxisSpinBox(QDoubleSpinBox* spin, const QString& axisLetter, const QString& badgeColor, QWidget* parent)
{
    auto* cell = new QWidget(parent);
    auto* layout = new QHBoxLayout(cell);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* badge = new QLabel(axisLetter, cell);
    badge->setFixedSize(16, 22);
    badge->setAlignment(Qt::AlignCenter);
    badge->setStyleSheet(QStringLiteral(
        "background-color: %1; color: white; font-weight: bold; font-size: 10px; "
        "border-top-left-radius: 4px; border-bottom-left-radius: 4px;"
    ).arg(badgeColor));
    layout->addWidget(badge);

    spin->setParent(cell);
    spin->setFixedHeight(22);
    spin->setStyleSheet(QStringLiteral(
        "QDoubleSpinBox {"
        "  background-color: #1F232B;"
        "  color: #F1F5F9;"
        "  border: 1px solid #333842;"
        "  border-left: none;"
        "  border-top-right-radius: 4px;"
        "  border-bottom-right-radius: 4px;"
        "  font-size: 11px;"
        "  padding: 1px 2px;"
        "}"
        "QDoubleSpinBox:focus {"
        "  border: 1px solid #3B82F6;"
        "}"
    ));
    layout->addWidget(spin, 1);

    return cell;
}

ObjectInspectorPanel::ObjectInspectorPanel(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(80, 60);
    BuildUi();
    langSubId_ = EditorLocale::Instance().SubscribeLanguageChanged([this] {
        RetranslateUi();
    });

    modifierElapsed_.start();
    modifierTimer_ = new QTimer(this);
    connect(modifierTimer_, &QTimer::timeout, this, &ObjectInspectorPanel::TickModifiers);
    modifierTimer_->start(30);
}

ObjectInspectorPanel::~ObjectInspectorPanel()
{
    if (langSubId_) {
        EditorLocale::Instance().UnsubscribeLanguageChanged(langSubId_);
        langSubId_ = 0;
    }
}

void ObjectInspectorPanel::SetMaterialsAvailable(bool available)
{
    if (materialActions_)
    {
        materialActions_->setEnabled(available);
        materialActions_->setToolTip(available ? QString() : Tr("Материалы требуют обновления игрового модуля.",
                                                               "Materials require updating the game module."));
    }
    if (materialAsset_)
        materialAsset_->setEnabled(available);
}

void ObjectInspectorPanel::BuildUi()
{
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setMinimumSize(60, 50);
    scrollArea->setSizeAdjustPolicy(QAbstractScrollArea::AdjustIgnored);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setStyleSheet(QStringLiteral("QScrollArea { background-color: #12141A; }"));

    auto* content = new QWidget(scrollArea);
    content->setStyleSheet(QStringLiteral("background-color: #12141A;"));
    auto* inspectorLayout = new QVBoxLayout(content);
    inspectorLayout->setContentsMargins(6, 6, 6, 6);
    inspectorLayout->setSpacing(6);

    // 1. Шапка объекта (Object Header)
    BuildHeaderCard(inspectorLayout);

    // 2. Секция Transform (Трансформация)
    BuildTransformCard(inspectorLayout);

    // 3. Секция Geometry & Primitive (3ds Max style)
    BuildGeometryCard(inspectorLayout);

    // 4. Секция Material & Surface (Материал и Поверхность)
    BuildMaterialCard(inspectorLayout);

    // 5. Секция Modifiers Stack (Стек Модификаторов)
    BuildModifiersCard(inspectorLayout);

    // 6. Секция Point Light (Точечный свет)
    BuildPointLightCard(inspectorLayout);

    // 7. Секция Character (Персонаж)
    BuildCharacterCard(inspectorLayout);

    inspectorLayout->addStretch(1);

    scrollArea->setWidget(content);
    outerLayout->addWidget(scrollArea);

    setMinimumHeight(0);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
}

void ObjectInspectorPanel::BuildHeaderCard(QVBoxLayout* layout)
{
    cardHeader_ = new CollapsibleCard(Tr("Объект / Шапка", "Object Header"), this, true);
    auto* contentLayout = cardHeader_->ContentLayout();

    // Строка 1: Бейдж типа и ID
    auto* rowType = new QWidget(cardHeader_->Content());
    auto* layoutType = new QHBoxLayout(rowType);
    layoutType->setContentsMargins(0, 0, 0, 0);
    layoutType->setSpacing(6);

    typeBadgeLabel_ = new QLabel(QStringLiteral("None"), rowType);
    typeBadgeLabel_->setStyleSheet(QStringLiteral(
        "background-color: #2D333F; color: #94A3B8; font-weight: bold; font-size: 10px; "
        "padding: 2px 6px; border-radius: 4px; border: 1px solid #3E4554;"
    ));
    layoutType->addWidget(typeBadgeLabel_);

    selectionLabel_ = new QLabel(Tr("Объект не выбран", "No object selected"), rowType);
    selectionLabel_->setObjectName(QStringLiteral("SelectedObjectLabel"));
    selectionLabel_->setWordWrap(true);
    selectionLabel_->setStyleSheet(QStringLiteral("font-weight: bold; color: #38BDF8; font-size: 11px;"));
    layoutType->addWidget(selectionLabel_, 1);
    contentLayout->addWidget(rowType);

    // Строка 2: Имя объекта
    auto* rowName = new QWidget(cardHeader_->Content());
    auto* layoutName = new QHBoxLayout(rowName);
    layoutName->setContentsMargins(0, 0, 0, 0);
    layoutName->setSpacing(6);

    nameRowLabel_ = new QLabel(Tr("Имя", "Name"), rowName);
    nameRowLabel_->setFixedWidth(40);
    nameRowLabel_->setStyleSheet(QStringLiteral("color: #94A3B8; font-size: 11px;"));
    layoutName->addWidget(nameRowLabel_);

    objectName_ = new QLineEdit(rowName);
    objectName_->setMaxLength(120);
    objectName_->setStyleSheet(QStringLiteral(
        "QLineEdit { background: #1F232B; color: #F1F5F9; border: 1px solid #333842; "
        "border-radius: 4px; padding: 2px 6px; font-size: 11px; }"
        "QLineEdit:focus { border: 1px solid #3B82F6; }"
    ));
    connect(objectName_, &QLineEdit::editingFinished, this, [this] {
        if (refreshing_ || selectedId_ == 0) return;
        emit NameEditingFinished(selectedId_, objectName_->text().trimmed());
    });
    layoutName->addWidget(objectName_, 1);
    contentLayout->addWidget(rowName);

    // Строка 3: Быстрые кнопки (Фокус, Дублировать, Удалить)
    auto* rowActions = new QWidget(cardHeader_->Content());
    auto* layoutActions = new QHBoxLayout(rowActions);
    layoutActions->setContentsMargins(0, 2, 0, 0);
    layoutActions->setSpacing(4);

    btnQuickFocus_ = new QPushButton(Tr("Фокус (F)", "Focus (F)"), rowActions);
    btnQuickFocus_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #262B35; color: #CBD5E1; border: 1px solid #3A404D; border-radius: 4px; padding: 3px 6px; font-size: 10px; }"
        "QPushButton:hover { background: #333947; color: #FFFFFF; }"
    ));
    connect(btnQuickFocus_, &QPushButton::clicked, this, [this] {
        if (selectedId_) emit FocusRequested(selectedId_);
    });
    layoutActions->addWidget(btnQuickFocus_);

    btnQuickDuplicate_ = new QPushButton(Tr("Дублировать", "Duplicate"), rowActions);
    btnQuickDuplicate_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #262B35; color: #CBD5E1; border: 1px solid #3A404D; border-radius: 4px; padding: 3px 6px; font-size: 10px; }"
        "QPushButton:hover { background: #333947; color: #FFFFFF; }"
    ));
    connect(btnQuickDuplicate_, &QPushButton::clicked, this, [this] {
        if (selectedId_) emit DuplicateRequested(selectedId_);
    });
    layoutActions->addWidget(btnQuickDuplicate_);

    btnQuickDelete_ = new QPushButton(Tr("Удалить", "Delete"), rowActions);
    btnQuickDelete_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #3A2022; color: #FCA5A5; border: 1px solid #5A2A2D; border-radius: 4px; padding: 3px 6px; font-size: 10px; }"
        "QPushButton:hover { background: #52272A; color: #FFFFFF; }"
    ));
    connect(btnQuickDelete_, &QPushButton::clicked, this, [this] {
        if (selectedId_) emit DeleteRequested(selectedId_);
    });
    layoutActions->addWidget(btnQuickDelete_);

    contentLayout->addWidget(rowActions);
    layout->addWidget(cardHeader_);
}

void ObjectInspectorPanel::BuildTransformCard(QVBoxLayout* layout)
{
    cardTransform_ = new CollapsibleCard(Tr("Трансформация", "Transform"), this, true);
    auto* contentLayout = cardTransform_->ContentLayout();

    const auto makeResetBtn = [this](const QString& toolTip) {
        auto* btn = new QPushButton(QStringLiteral("↺"), cardTransform_->Content());
        btn->setFixedSize(22, 22);
        btn->setToolTip(toolTip);
        btn->setStyleSheet(QStringLiteral(
            "QPushButton { background-color: #242832; color: #94A3B8; border: 1px solid #383E4C; border-radius: 4px; font-size: 12px; font-weight: bold; }"
            "QPushButton:hover { background-color: #313847; color: #FFFFFF; }"
        ));
        return btn;
    };

    auto onTransformSpinFinished = [this] {
        if (refreshing_ || selectedId_ == 0) return;
        emit TransformEditingFinished(selectedId_, CurrentTransformValue());
    };

    // 1. Позиция (X, Y, Z)
    auto* rowPos = new QWidget(cardTransform_->Content());
    auto* layoutPos = new QHBoxLayout(rowPos);
    layoutPos->setContentsMargins(0, 0, 0, 0);
    layoutPos->setSpacing(4);

    labelPosRow_ = new QLabel(Tr("Позиция", "Position"), rowPos);
    labelPosRow_->setFixedWidth(56);
    labelPosRow_->setStyleSheet(QStringLiteral("color: #94A3B8; font-size: 11px;"));
    layoutPos->addWidget(labelPosRow_);

    for (int i = 0; i < 3; ++i)
    {
        auto* spin = transform_[i] = new QDoubleSpinBox(rowPos);
        spin->setObjectName(QStringLiteral("Transform%1").arg(i));
        spin->setRange(-10000.0, 10000.0);
        spin->setDecimals(3);
        spin->setSingleStep(0.25);
        spin->setKeyboardTracking(false);
        connect(spin, &QDoubleSpinBox::editingFinished, this, onTransformSpinFinished);

        const QString letter = (i == 0 ? QStringLiteral("X") : (i == 1 ? QStringLiteral("Y") : QStringLiteral("Z")));
        const QString color = (i == 0 ? QStringLiteral("#EF4444") : (i == 1 ? QStringLiteral("#22C55E") : QStringLiteral("#3B82F6")));
        layoutPos->addWidget(CreateAxisSpinBox(spin, letter, color, rowPos), 1);
    }

    btnResetPos_ = makeResetBtn(Tr("Сброс позиции (0, 0, 0)", "Reset Position (0, 0, 0)"));
    connect(btnResetPos_, &QPushButton::clicked, this, [this, onTransformSpinFinished] {
        transform_[0]->setValue(0.0);
        transform_[1]->setValue(0.0);
        transform_[2]->setValue(0.0);
        onTransformSpinFinished();
    });
    layoutPos->addWidget(btnResetPos_);
    contentLayout->addWidget(rowPos);

    // 2. Поворот (X, Y, Z, °)
    auto* rowRot = new QWidget(cardTransform_->Content());
    auto* layoutRot = new QHBoxLayout(rowRot);
    layoutRot->setContentsMargins(0, 0, 0, 0);
    layoutRot->setSpacing(4);

    labelRotRow_ = new QLabel(Tr("Поворот", "Rotation"), rowRot);
    labelRotRow_->setFixedWidth(56);
    labelRotRow_->setStyleSheet(QStringLiteral("color: #94A3B8; font-size: 11px;"));
    layoutRot->addWidget(labelRotRow_);

    for (int i = 3; i < 6; ++i)
    {
        auto* spin = transform_[i] = new QDoubleSpinBox(rowRot);
        spin->setObjectName(QStringLiteral("Transform%1").arg(i));
        spin->setRange(-10000.0, 10000.0);
        spin->setDecimals(2);
        spin->setSingleStep(15.0);
        spin->setKeyboardTracking(false);
        connect(spin, &QDoubleSpinBox::editingFinished, this, onTransformSpinFinished);

        const QString letter = (i == 3 ? QStringLiteral("X") : (i == 4 ? QStringLiteral("Y") : QStringLiteral("Z")));
        const QString color = (i == 3 ? QStringLiteral("#EF4444") : (i == 4 ? QStringLiteral("#22C55E") : QStringLiteral("#3B82F6")));
        layoutRot->addWidget(CreateAxisSpinBox(spin, letter, color, rowRot), 1);
    }

    btnResetRot_ = makeResetBtn(Tr("Сброс поворота (0, 0, 0)", "Reset Rotation (0, 0, 0)"));
    connect(btnResetRot_, &QPushButton::clicked, this, [this, onTransformSpinFinished] {
        transform_[3]->setValue(0.0);
        transform_[4]->setValue(0.0);
        transform_[5]->setValue(0.0);
        onTransformSpinFinished();
    });
    layoutRot->addWidget(btnResetRot_);
    contentLayout->addWidget(rowRot);

    // 3. Масштаб (X, Y, Z)
    auto* rowScale = new QWidget(cardTransform_->Content());
    auto* layoutScale = new QHBoxLayout(rowScale);
    layoutScale->setContentsMargins(0, 0, 0, 0);
    layoutScale->setSpacing(4);

    labelScaleRow_ = new QLabel(Tr("Масштаб", "Scale"), rowScale);
    labelScaleRow_->setFixedWidth(56);
    labelScaleRow_->setStyleSheet(QStringLiteral("color: #94A3B8; font-size: 11px;"));
    layoutScale->addWidget(labelScaleRow_);

    for (int i = 6; i < 9; ++i)
    {
        auto* spin = transform_[i] = new QDoubleSpinBox(rowScale);
        spin->setObjectName(QStringLiteral("Transform%1").arg(i));
        spin->setRange(0.001, 10000.0);
        spin->setDecimals(3);
        spin->setSingleStep(0.25);
        spin->setKeyboardTracking(false);
        connect(spin, &QDoubleSpinBox::editingFinished, this, onTransformSpinFinished);

        const QString letter = (i == 6 ? QStringLiteral("X") : (i == 7 ? QStringLiteral("Y") : QStringLiteral("Z")));
        const QString color = (i == 6 ? QStringLiteral("#EF4444") : (i == 7 ? QStringLiteral("#22C55E") : QStringLiteral("#3B82F6")));
        layoutScale->addWidget(CreateAxisSpinBox(spin, letter, color, rowScale), 1);
    }

    btnResetScale_ = makeResetBtn(Tr("Сброс масштаба (1, 1, 1)", "Reset Scale (1, 1, 1)"));
    connect(btnResetScale_, &QPushButton::clicked, this, [this, onTransformSpinFinished] {
        transform_[6]->setValue(1.0);
        transform_[7]->setValue(1.0);
        transform_[8]->setValue(1.0);
        onTransformSpinFinished();
    });
    layoutScale->addWidget(btnResetScale_);
    contentLayout->addWidget(rowScale);

    layout->addWidget(cardTransform_);
}

void ObjectInspectorPanel::BuildGeometryCard(QVBoxLayout* layout)
{
    cardGeometry_ = new CollapsibleCard(Tr("Геометрия", "Geometry"), this, true);
    auto* contentLayout = cardGeometry_->ContentLayout();

    // -------------------------------------------------------------
    // А) Блок: Параметры примитива (3ds Max Non-destructive style)
    // -------------------------------------------------------------
    primitiveParamsGroup_ = new QWidget(cardGeometry_->Content());
    auto* primLayout = new QVBoxLayout(primitiveParamsGroup_);
    primLayout->setContentsMargins(0, 0, 0, 0);
    primLayout->setSpacing(4);

    lblPrimParamsTitle_ = new QLabel(Tr("Параметры примитива (3ds Max)", "Primitive Parameters (3ds Max)"), primitiveParamsGroup_);
    lblPrimParamsTitle_->setStyleSheet(QStringLiteral("color: #38BDF8; font-weight: bold; font-size: 10px; margin-bottom: 2px;"));
    primLayout->addWidget(lblPrimParamsTitle_);

    auto makePrimRow = [](const QString& labelText, QLabel*& outLabel, QDoubleSpinBox*& outSpin, QWidget* parent) {
        auto* row = new QWidget(parent);
        auto* l = new QHBoxLayout(row);
        l->setContentsMargins(0, 0, 0, 0);
        l->setSpacing(6);

        outLabel = new QLabel(labelText, row);
        outLabel->setFixedWidth(90);
        outLabel->setStyleSheet(QStringLiteral("color: #94A3B8; font-size: 11px;"));
        l->addWidget(outLabel);

        outSpin = new QDoubleSpinBox(row);
        outSpin->setRange(0.01, 1000.0);
        outSpin->setDecimals(3);
        outSpin->setSingleStep(0.25);
        outSpin->setKeyboardTracking(false);
        outSpin->setStyleSheet(QStringLiteral(
            "QDoubleSpinBox { background: #1F232B; color: #F1F5F9; border: 1px solid #333842; "
            "border-radius: 4px; padding: 2px 4px; font-size: 11px; }"
            "QDoubleSpinBox:focus { border: 1px solid #3B82F6; }"
        ));
        l->addWidget(outSpin, 1);
        return row;
    };

    auto onPrimParamChanged = [this] {
        ApplyPrimitiveParamsToScale(selectedType_);
    };

    rowPrimRadius_ = makePrimRow(Tr("Радиус", "Radius"), lblPrimRadius_, spinPrimRadius_, primitiveParamsGroup_);
    connect(spinPrimRadius_, &QDoubleSpinBox::editingFinished, this, onPrimParamChanged);
    primLayout->addWidget(rowPrimRadius_);

    rowPrimHeight_ = makePrimRow(Tr("Высота", "Height"), lblPrimHeight_, spinPrimHeight_, primitiveParamsGroup_);
    connect(spinPrimHeight_, &QDoubleSpinBox::editingFinished, this, onPrimParamChanged);
    primLayout->addWidget(rowPrimHeight_);

    rowPrimWidth_ = makePrimRow(Tr("Ширина (X)", "Width (X)"), lblPrimWidth_, spinPrimWidth_, primitiveParamsGroup_);
    connect(spinPrimWidth_, &QDoubleSpinBox::editingFinished, this, onPrimParamChanged);
    primLayout->addWidget(rowPrimWidth_);

    rowPrimDepth_ = makePrimRow(Tr("Глубина / Длина (Z)", "Depth / Length (Z)"), lblPrimDepth_, spinPrimDepth_, primitiveParamsGroup_);
    connect(spinPrimDepth_, &QDoubleSpinBox::editingFinished, this, onPrimParamChanged);
    primLayout->addWidget(rowPrimDepth_);

    contentLayout->addWidget(primitiveParamsGroup_);

    // -------------------------------------------------------------
    // Б) Блок: Слот модели / меша (Mesh Asset)
    // -------------------------------------------------------------
    meshParamsGroup_ = new QWidget(cardGeometry_->Content());
    auto* meshLayout = new QVBoxLayout(meshParamsGroup_);
    meshLayout->setContentsMargins(0, 0, 0, 0);
    meshLayout->setSpacing(4);

    modelRowLabel_ = new QLabel(Tr("Меш / Модель", "Mesh / Model Asset"), meshParamsGroup_);
    modelRowLabel_->setStyleSheet(QStringLiteral("color: #94A3B8; font-size: 11px;"));
    meshLayout->addWidget(modelRowLabel_);

    auto* rowModel = new QWidget(meshParamsGroup_);
    auto* layoutModel = new QHBoxLayout(rowModel);
    layoutModel->setContentsMargins(0, 0, 0, 0);
    layoutModel->setSpacing(4);

    modelAsset_ = new QLineEdit(rowModel);
    modelAsset_->setReadOnly(true);
    modelAsset_->setObjectName(QStringLiteral("ModelAsset"));
    modelAsset_->setPlaceholderText(Tr("нет модели", "none"));
    modelAsset_->setStyleSheet(QStringLiteral(
        "QLineEdit { background: #1F232B; color: #CBD5E1; border: 1px solid #333842; "
        "border-radius: 4px; padding: 2px 6px; font-size: 11px; }"
    ));
    layoutModel->addWidget(modelAsset_, 1);

    chooseModel_ = new QPushButton(Tr("Выбрать…", "Choose…"), rowModel);
    chooseModel_->setObjectName(QStringLiteral("ChooseModel"));
    chooseModel_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #262B35; color: #CBD5E1; border: 1px solid #3A404D; border-radius: 4px; padding: 3px 8px; font-size: 11px; }"
        "QPushButton:hover { background: #333947; color: #FFFFFF; }"
    ));
    connect(chooseModel_, &QPushButton::clicked, this, &ObjectInspectorPanel::ChooseModelRequested);
    layoutModel->addWidget(chooseModel_);

    clearModel_ = new QPushButton(Tr("Убрать", "Clear"), rowModel);
    clearModel_->setObjectName(QStringLiteral("ClearModel"));
    clearModel_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #262B35; color: #94A3B8; border: 1px solid #3A404D; border-radius: 4px; padding: 3px 8px; font-size: 11px; }"
        "QPushButton:hover { background: #333947; color: #FFFFFF; }"
    ));
    connect(clearModel_, &QPushButton::clicked, this, [this] {
        if (selectedId_) emit ClearModelRequested(selectedId_);
    });
    layoutModel->addWidget(clearModel_);

    meshLayout->addWidget(rowModel);
    contentLayout->addWidget(meshParamsGroup_);

    // -------------------------------------------------------------
    // В) Блок: Скелет и анимация (Skeletal Mesh & Animation)
    // -------------------------------------------------------------
    skeletalParamsGroup_ = new QWidget(cardGeometry_->Content());
    auto* skelLayout = new QVBoxLayout(skeletalParamsGroup_);
    skelLayout->setContentsMargins(0, 4, 0, 0);
    skelLayout->setSpacing(4);

    lblSkeletonTitle_ = new QLabel(Tr("Скелет и Анимация", "Skeleton & Animation"), skeletalParamsGroup_);
    lblSkeletonTitle_->setStyleSheet(QStringLiteral("color: #A855F7; font-weight: bold; font-size: 10px;"));
    skelLayout->addWidget(lblSkeletonTitle_);

    auto* rowSkelInfo = new QWidget(skeletalParamsGroup_);
    auto* layoutSkelInfo = new QHBoxLayout(rowSkelInfo);
    layoutSkelInfo->setContentsMargins(0, 0, 0, 0);
    layoutSkelInfo->setSpacing(6);

    lblSkeletonStatus_ = new QLabel(Tr("Скелет: Auto / Assimp", "Skeleton: Auto / Assimp"), rowSkelInfo);
    lblSkeletonStatus_->setStyleSheet(QStringLiteral("color: #CBD5E1; font-size: 11px;"));
    layoutSkelInfo->addWidget(lblSkeletonStatus_);
    layoutSkelInfo->addStretch(1);

    chkAnimationLoop_ = new QCheckBox(Tr("Зациклить (Loop)", "Loop"), rowSkelInfo);
    chkAnimationLoop_->setChecked(true);
    chkAnimationLoop_->setStyleSheet(QStringLiteral("color: #CBD5E1; font-size: 11px;"));
    layoutSkelInfo->addWidget(chkAnimationLoop_);
    skelLayout->addWidget(rowSkelInfo);

    auto* rowClip = new QWidget(skeletalParamsGroup_);
    auto* layoutClip = new QHBoxLayout(rowClip);
    layoutClip->setContentsMargins(0, 0, 0, 0);
    layoutClip->setSpacing(6);

    lblAnimClip_ = new QLabel(Tr("Клип", "Clip"), rowClip);
    lblAnimClip_->setFixedWidth(40);
    lblAnimClip_->setStyleSheet(QStringLiteral("color: #94A3B8; font-size: 11px;"));
    layoutClip->addWidget(lblAnimClip_);

    comboAnimationClip_ = new QComboBox(rowClip);
    comboAnimationClip_->addItems({QStringLiteral("Default"), QStringLiteral("Idle"), QStringLiteral("Walk"), QStringLiteral("Run")});
    comboAnimationClip_->setStyleSheet(QStringLiteral(
        "QComboBox { background: #1F232B; color: #F1F5F9; border: 1px solid #333842; border-radius: 4px; padding: 2px 6px; font-size: 11px; }"
    ));
    layoutClip->addWidget(comboAnimationClip_, 1);

    lblAnimSpeed_ = new QLabel(Tr("Скор.", "Speed"), rowClip);
    lblAnimSpeed_->setStyleSheet(QStringLiteral("color: #94A3B8; font-size: 11px;"));
    layoutClip->addWidget(lblAnimSpeed_);

    spinPlaybackSpeed_ = new QDoubleSpinBox(rowClip);
    spinPlaybackSpeed_->setRange(0.0, 10.0);
    spinPlaybackSpeed_->setDecimals(1);
    spinPlaybackSpeed_->setSingleStep(0.2);
    spinPlaybackSpeed_->setValue(1.0);
    spinPlaybackSpeed_->setFixedWidth(54);
    spinPlaybackSpeed_->setStyleSheet(QStringLiteral(
        "QDoubleSpinBox { background: #1F232B; color: #F1F5F9; border: 1px solid #333842; border-radius: 4px; font-size: 11px; }"
    ));
    layoutClip->addWidget(spinPlaybackSpeed_);

    skelLayout->addWidget(rowClip);
    contentLayout->addWidget(skeletalParamsGroup_);

    layout->addWidget(cardGeometry_);
}

void ObjectInspectorPanel::BuildMaterialCard(QVBoxLayout* layout)
{
    cardMaterial_ = new CollapsibleCard(Tr("Материал и Поверхность", "Material & Surface"), this, true);
    auto* contentLayout = cardMaterial_->ContentLayout();

    // 1. Слот материала (.mat)
    materialRowLabel_ = new QLabel(Tr("Материал (.mat)", "Material (.mat)"), cardMaterial_->Content());
    materialRowLabel_->setStyleSheet(QStringLiteral("color: #94A3B8; font-size: 11px; font-weight: bold;"));
    contentLayout->addWidget(materialRowLabel_);

    materialAsset_ = new QLineEdit(cardMaterial_->Content());
    materialAsset_->setReadOnly(true);
    materialAsset_->setObjectName(QStringLiteral("MaterialAsset"));
    materialAsset_->setPlaceholderText(Tr("по умолчанию (default)", "default"));
    materialAsset_->setStyleSheet(QStringLiteral(
        "QLineEdit { background: #1F232B; color: #CBD5E1; border: 1px solid #333842; "
        "border-radius: 4px; padding: 2px 6px; font-size: 11px; }"
    ));
    contentLayout->addWidget(materialAsset_);

    auto* rowMatBtns = new QWidget(cardMaterial_->Content());
    materialActions_ = rowMatBtns;
    auto* layoutMatBtns = new QHBoxLayout(rowMatBtns);
    layoutMatBtns->setContentsMargins(0, 0, 0, 0);
    layoutMatBtns->setSpacing(4);

    editMaterialButton_ = new QPushButton(Tr("Редактировать", "Edit Material"), rowMatBtns);
    editMaterialButton_->setObjectName(QStringLiteral("EditMaterial"));
    editMaterialButton_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #2563EB; color: #FFFFFF; font-weight: bold; border: 1px solid #3B82F6; border-radius: 4px; padding: 3px 10px; font-size: 11px; }"
        "QPushButton:hover { background: #1D4ED8; }"
        "QPushButton:disabled { background: #1E293B; color: #64748B; border: 1px solid #334155; }"
    ));
    connect(editMaterialButton_, &QPushButton::clicked, this, [this] {
        emit EditMaterialRequested(materialAsset_->text());
    });
    layoutMatBtns->addWidget(editMaterialButton_);

    chooseMaterialButton_ = new QPushButton(Tr("Выбрать…", "Assign…"), rowMatBtns);
    chooseMaterialButton_->setObjectName(QStringLiteral("ChooseMaterial"));
    chooseMaterialButton_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #262B35; color: #CBD5E1; border: 1px solid #3A404D; border-radius: 4px; padding: 3px 8px; font-size: 11px; }"
        "QPushButton:hover { background: #333947; color: #FFFFFF; }"
    ));
    connect(chooseMaterialButton_, &QPushButton::clicked, this, &ObjectInspectorPanel::ChooseMaterialRequested);
    layoutMatBtns->addWidget(chooseMaterialButton_);

    clearMaterialButton_ = new QPushButton(Tr("Убрать", "Clear"), rowMatBtns);
    clearMaterialButton_->setObjectName(QStringLiteral("ClearMaterial"));
    clearMaterialButton_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #262B35; color: #94A3B8; border: 1px solid #3A404D; border-radius: 4px; padding: 3px 8px; font-size: 11px; }"
        "QPushButton:hover { background: #333947; color: #FFFFFF; }"
    ));
    connect(clearMaterialButton_, &QPushButton::clicked, this, &ObjectInspectorPanel::ClearMaterialRequested);
    layoutMatBtns->addWidget(clearMaterialButton_);

    contentLayout->addWidget(rowMatBtns);

    // 2. Слот базовой текстуры
    textureRowLabel_ = new QLabel(Tr("Текстура (Base Texture)", "Base Texture"), cardMaterial_->Content());
    textureRowLabel_->setStyleSheet(QStringLiteral("color: #94A3B8; font-size: 11px; font-weight: bold; margin-top: 4px;"));
    contentLayout->addWidget(textureRowLabel_);

    textureAsset_ = new QLineEdit(cardMaterial_->Content());
    textureAsset_->setReadOnly(true);
    textureAsset_->setObjectName(QStringLiteral("TextureAsset"));
    textureAsset_->setPlaceholderText(Tr("нет текстуры", "none"));
    textureAsset_->setStyleSheet(QStringLiteral(
        "QLineEdit { background: #1F232B; color: #CBD5E1; border: 1px solid #333842; "
        "border-radius: 4px; padding: 2px 6px; font-size: 11px; }"
    ));
    contentLayout->addWidget(textureAsset_);

    auto* rowTexBtns = new QWidget(cardMaterial_->Content());
    auto* layoutTexBtns = new QHBoxLayout(rowTexBtns);
    layoutTexBtns->setContentsMargins(0, 0, 0, 0);
    layoutTexBtns->setSpacing(4);

    chooseTextureButton_ = new QPushButton(Tr("Выбрать…", "Browse…"), rowTexBtns);
    chooseTextureButton_->setObjectName(QStringLiteral("ChooseTexture"));
    chooseTextureButton_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #262B35; color: #CBD5E1; border: 1px solid #3A404D; border-radius: 4px; padding: 3px 8px; font-size: 11px; }"
        "QPushButton:hover { background: #333947; color: #FFFFFF; }"
    ));
    connect(chooseTextureButton_, &QPushButton::clicked, this, &ObjectInspectorPanel::ChooseTextureRequested);
    layoutTexBtns->addWidget(chooseTextureButton_);

    clearTextureButton_ = new QPushButton(Tr("Убрать", "Clear"), rowTexBtns);
    clearTextureButton_->setObjectName(QStringLiteral("ClearTexture"));
    clearTextureButton_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #262B35; color: #94A3B8; border: 1px solid #3A404D; border-radius: 4px; padding: 3px 8px; font-size: 11px; }"
        "QPushButton:hover { background: #333947; color: #FFFFFF; }"
    ));
    connect(clearTextureButton_, &QPushButton::clicked, this, &ObjectInspectorPanel::ClearTextureRequested);
    layoutTexBtns->addWidget(clearTextureButton_);

    contentLayout->addWidget(rowTexBtns);
    layout->addWidget(cardMaterial_);
}

void ObjectInspectorPanel::BuildModifiersCard(QVBoxLayout* layout)
{
    cardModifiers_ = new CollapsibleCard(Tr("Модификаторы", "Modifiers"), this, true);
    auto* contentLayout = cardModifiers_->ContentLayout();

    // Верхняя строка: Кнопка "+ Add Modifier"
    auto* topRow = new QWidget(cardModifiers_->Content());
    auto* topLayout = new QHBoxLayout(topRow);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(6);

    btnAddModifier_ = new QToolButton(topRow);
    btnAddModifier_->setText(Tr("+ Добавить модификатор", "+ Add Modifier"));
    btnAddModifier_->setPopupMode(QToolButton::InstantPopup);
    btnAddModifier_->setStyleSheet(QStringLiteral(
        "QToolButton { background: #2563EB; color: #FFFFFF; font-weight: bold; border: 1px solid #3B82F6; "
        "border-radius: 4px; padding: 4px 10px; font-size: 11px; }"
        "QToolButton:hover { background: #1D4ED8; }"
    ));

    menuAddModifier_ = new QMenu(btnAddModifier_);
    menuAddModifier_->setStyleSheet(QStringLiteral(
        "QMenu { background: #181A1F; color: #E2E8F0; border: 1px solid #333742; padding: 4px; }"
        "QMenu::item { padding: 4px 16px; border-radius: 3px; }"
        "QMenu::item:selected { background: #252830; color: #FFFFFF; }"
    ));

    auto addModAction = [this](const QString& title, int type) {
        auto* act = menuAddModifier_->addAction(title);
        connect(act, &QAction::triggered, this, [this, type] {
            if (selectedId_ == 0) return;
            ModifierData m;
            m.id = nextModifierId_++;
            m.type = type;
            m.enabled = true;
            m.axis = (type == 0 ? 1 : (type == 1 ? 1 : 0)); // Turntable default Y, Bounce default Y, Mirror default X
            m.speed = 45.0;
            m.amplitude = 0.5;
            m.frequency = 1.0;
            m.basePosInit = false;
            m.arrayCount = 3;
            m.arrayOffsetX = 2.0;
            m.arrayOffsetY = 0.0;
            m.arrayOffsetZ = 0.0;
            m.mirrorAxis = 0;

            objectModifiers_[selectedId_].push_back(m);
            RebuildModifiersList();
        });
    };

    addModAction(Tr("Turntable (Вращение)", "Turntable (Rotation)"), 0);
    addModAction(Tr("Sine Bounce (Покачивание)", "Sine Bounce (Bobbing)"), 1);
    addModAction(Tr("Linear Array (Массив копий)", "Linear Array"), 2);
    addModAction(Tr("Mirror (Отражение)", "Mirror"), 3);
    addModAction(Tr("Snap to Ground (Посадка на землю)", "Snap to Ground"), 4);

    btnAddModifier_->setMenu(menuAddModifier_);
    topLayout->addWidget(btnAddModifier_);
    topLayout->addStretch(1);
    contentLayout->addWidget(topRow);

    // Контейнер списка модификаторов
    modifiersContainer_ = new QWidget(cardModifiers_->Content());
    modifiersLayout_ = new QVBoxLayout(modifiersContainer_);
    modifiersLayout_->setContentsMargins(0, 4, 0, 0);
    modifiersLayout_->setSpacing(6);

    lblNoModifiers_ = new QLabel(Tr("Нет активных модификаторов. Нажмите «+ Добавить модификатор».",
                                   "No active modifiers. Click \"+ Add Modifier\"."), modifiersContainer_);
    lblNoModifiers_->setStyleSheet(QStringLiteral("color: #64748B; font-size: 11px; font-style: italic; padding: 4px 0;"));
    modifiersLayout_->addWidget(lblNoModifiers_);

    contentLayout->addWidget(modifiersContainer_);
    layout->addWidget(cardModifiers_);
}

void ObjectInspectorPanel::RebuildModifiersList()
{
    // Очищаем старые виджеты элементов кроме lblNoModifiers_
    QLayoutItem* item;
    while ((item = modifiersLayout_->takeAt(0)) != nullptr)
    {
        if (item->widget() && item->widget() != lblNoModifiers_)
        {
            item->widget()->deleteLater();
        }
        delete item;
    }

    if (selectedId_ == 0 || objectModifiers_.find(selectedId_) == objectModifiers_.end() || objectModifiers_[selectedId_].empty())
    {
        lblNoModifiers_->show();
        modifiersLayout_->addWidget(lblNoModifiers_);
        return;
    }

    lblNoModifiers_->hide();

    auto& list = objectModifiers_[selectedId_];

    for (size_t idx = 0; idx < list.size(); ++idx)
    {
        const int modId = list[idx].id;
        const int modType = list[idx].type;

        auto* modCard = new QFrame(modifiersContainer_);
        modCard->setStyleSheet(QStringLiteral(
            "QFrame { background-color: #1A1D24; border: 1px solid #2D333F; border-radius: 5px; }"
        ));
        auto* modLayout = new QVBoxLayout(modCard);
        modLayout->setContentsMargins(6, 6, 6, 6);
        modLayout->setSpacing(4);

        // Header карточки модификатора
        auto* hRow = new QWidget(modCard);
        auto* hLayout = new QHBoxLayout(hRow);
        hLayout->setContentsMargins(0, 0, 0, 0);
        hLayout->setSpacing(6);

        auto* chk = new QCheckBox(hRow);
        chk->setChecked(list[idx].enabled);
        connect(chk, &QCheckBox::toggled, this, [this, modId](bool checked) {
            auto& mList = objectModifiers_[selectedId_];
            for (auto& m : mList) {
                if (m.id == modId) {
                    m.enabled = checked;
                    m.basePosInit = false; // сброс базы синусоиды при включении
                    break;
                }
            }
        });
        hLayout->addWidget(chk);

        QString title;
        switch (modType)
        {
        case 0: title = Tr("Turntable (Вращение)", "Turntable"); break;
        case 1: title = Tr("Sine Bounce (Покачивание)", "Sine Bounce"); break;
        case 2: title = Tr("Linear Array (Массив)", "Linear Array"); break;
        case 3: title = Tr("Mirror (Отражение)", "Mirror"); break;
        case 4: title = Tr("Snap to Ground (Земля)", "Snap to Ground"); break;
        }

        auto* lblTitle = new QLabel(title, hRow);
        lblTitle->setStyleSheet(QStringLiteral("color: #38BDF8; font-weight: bold; font-size: 11px;"));
        hLayout->addWidget(lblTitle);
        hLayout->addStretch(1);

        auto* btnClose = new QPushButton(QStringLiteral("×"), hRow);
        btnClose->setFixedSize(18, 18);
        btnClose->setStyleSheet(QStringLiteral(
            "QPushButton { background: transparent; color: #94A3B8; font-weight: bold; font-size: 13px; border: none; }"
            "QPushButton:hover { color: #EF4444; }"
        ));
        connect(btnClose, &QPushButton::clicked, this, [this, modId] {
            auto& mList = objectModifiers_[selectedId_];
            mList.erase(std::remove_if(mList.begin(), mList.end(), [modId](const ModifierData& m) {
                return m.id == modId;
            }), mList.end());
            RebuildModifiersList();
        });
        hLayout->addWidget(btnClose);
        modLayout->addWidget(hRow);

        // Специфичные контролы модификатора
        if (modType == 0) // Turntable
        {
            auto* pRow = new QWidget(modCard);
            auto* pLayout = new QHBoxLayout(pRow);
            pLayout->setContentsMargins(0, 0, 0, 0);
            pLayout->setSpacing(4);

            pLayout->addWidget(new QLabel(Tr("Ось:", "Axis:"), pRow));
            auto* comboAxis = new QComboBox(pRow);
            comboAxis->addItems({QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("Z")});
            comboAxis->setCurrentIndex(list[idx].axis);
            comboAxis->setStyleSheet(QStringLiteral("QComboBox { background: #1F232B; color: #F1F5F9; border: 1px solid #333842; font-size: 10px; }"));
            connect(comboAxis, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, modId](int a) {
                for (auto& m : objectModifiers_[selectedId_]) if (m.id == modId) m.axis = a;
            });
            pLayout->addWidget(comboAxis);

            pLayout->addWidget(new QLabel(Tr("Скор. (°/с):", "Speed (°/s):"), pRow));
            auto* spinSpd = new QDoubleSpinBox(pRow);
            spinSpd->setRange(-720.0, 720.0);
            spinSpd->setValue(list[idx].speed);
            spinSpd->setStyleSheet(QStringLiteral("QDoubleSpinBox { background: #1F232B; color: #F1F5F9; border: 1px solid #333842; font-size: 10px; }"));
            connect(spinSpd, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this, modId](double s) {
                for (auto& m : objectModifiers_[selectedId_]) if (m.id == modId) m.speed = s;
            });
            pLayout->addWidget(spinSpd, 1);
            modLayout->addWidget(pRow);
        }
        else if (modType == 1) // Sine Bounce
        {
            auto* pRow = new QWidget(modCard);
            auto* pLayout = new QHBoxLayout(pRow);
            pLayout->setContentsMargins(0, 0, 0, 0);
            pLayout->setSpacing(4);

            pLayout->addWidget(new QLabel(Tr("Ось:", "Axis:"), pRow));
            auto* comboAxis = new QComboBox(pRow);
            comboAxis->addItems({QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("Z")});
            comboAxis->setCurrentIndex(list[idx].axis);
            comboAxis->setStyleSheet(QStringLiteral("QComboBox { background: #1F232B; color: #F1F5F9; border: 1px solid #333842; font-size: 10px; }"));
            connect(comboAxis, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, modId](int a) {
                for (auto& m : objectModifiers_[selectedId_]) {
                    if (m.id == modId) {
                        m.axis = a;
                        m.basePosInit = false;
                    }
                }
            });
            pLayout->addWidget(comboAxis);

            pLayout->addWidget(new QLabel(Tr("Ампл. (м):", "Amp (m):"), pRow));
            auto* spinAmp = new QDoubleSpinBox(pRow);
            spinAmp->setRange(0.01, 20.0);
            spinAmp->setSingleStep(0.2);
            spinAmp->setValue(list[idx].amplitude);
            spinAmp->setStyleSheet(QStringLiteral("QDoubleSpinBox { background: #1F232B; color: #F1F5F9; border: 1px solid #333842; font-size: 10px; }"));
            connect(spinAmp, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this, modId](double a) {
                for (auto& m : objectModifiers_[selectedId_]) if (m.id == modId) m.amplitude = a;
            });
            pLayout->addWidget(spinAmp);

            pLayout->addWidget(new QLabel(Tr("Част. (Гц):", "Freq (Hz):"), pRow));
            auto* spinFreq = new QDoubleSpinBox(pRow);
            spinFreq->setRange(0.1, 10.0);
            spinFreq->setSingleStep(0.2);
            spinFreq->setValue(list[idx].frequency);
            spinFreq->setStyleSheet(QStringLiteral("QDoubleSpinBox { background: #1F232B; color: #F1F5F9; border: 1px solid #333842; font-size: 10px; }"));
            connect(spinFreq, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this, modId](double f) {
                for (auto& m : objectModifiers_[selectedId_]) if (m.id == modId) m.frequency = f;
            });
            pLayout->addWidget(spinFreq);
            modLayout->addWidget(pRow);
        }
        else if (modType == 2) // Linear Array
        {
            auto* pRow1 = new QWidget(modCard);
            auto* pLayout1 = new QHBoxLayout(pRow1);
            pLayout1->setContentsMargins(0, 0, 0, 0);
            pLayout1->setSpacing(4);

            pLayout1->addWidget(new QLabel(Tr("Копий:", "Count:"), pRow1));
            auto* spinCount = new QDoubleSpinBox(pRow1);
            spinCount->setDecimals(0);
            spinCount->setRange(2, 20);
            spinCount->setValue(list[idx].arrayCount);
            spinCount->setStyleSheet(QStringLiteral("QDoubleSpinBox { background: #1F232B; color: #F1F5F9; border: 1px solid #333842; font-size: 10px; }"));
            connect(spinCount, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this, modId](double c) {
                for (auto& m : objectModifiers_[selectedId_]) if (m.id == modId) m.arrayCount = static_cast<int>(c);
            });
            pLayout1->addWidget(spinCount);

            pLayout1->addWidget(new QLabel(Tr("Шаг X, Y, Z:", "Offset:"), pRow1));
            auto* spinX = new QDoubleSpinBox(pRow1);
            spinX->setRange(-50.0, 50.0);
            spinX->setValue(list[idx].arrayOffsetX);
            spinX->setStyleSheet(QStringLiteral("QDoubleSpinBox { background: #1F232B; color: #F1F5F9; border: 1px solid #333842; font-size: 10px; }"));
            connect(spinX, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this, modId](double x) {
                for (auto& m : objectModifiers_[selectedId_]) if (m.id == modId) m.arrayOffsetX = x;
            });
            pLayout1->addWidget(spinX);

            modLayout->addWidget(pRow1);

            auto* btnApplyArray = new QPushButton(Tr("Создать копии в сцене (Apply)", "Apply (Create Clones in Scene)"), modCard);
            btnApplyArray->setStyleSheet(QStringLiteral(
                "QPushButton { background: #2563EB; color: #FFFFFF; font-weight: bold; border: 1px solid #3B82F6; border-radius: 4px; padding: 3px 8px; font-size: 10px; }"
                "QPushButton:hover { background: #1D4ED8; }"
            ));
            connect(btnApplyArray, &QPushButton::clicked, this, [this, modId] {
                for (const auto& m : objectModifiers_[selectedId_]) {
                    if (m.id == modId) {
                        emit CreateClonesRequested(selectedId_, m.arrayCount, m.arrayOffsetX, m.arrayOffsetY, m.arrayOffsetZ);
                        break;
                    }
                }
            });
            modLayout->addWidget(btnApplyArray);
        }
        else if (modType == 3) // Mirror
        {
            auto* pRow = new QWidget(modCard);
            auto* pLayout = new QHBoxLayout(pRow);
            pLayout->setContentsMargins(0, 0, 0, 0);
            pLayout->setSpacing(6);

            pLayout->addWidget(new QLabel(Tr("Ось отражения:", "Mirror Axis:"), pRow));
            auto* comboAxis = new QComboBox(pRow);
            comboAxis->addItems({QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("Z")});
            comboAxis->setCurrentIndex(list[idx].mirrorAxis);
            comboAxis->setStyleSheet(QStringLiteral("QComboBox { background: #1F232B; color: #F1F5F9; border: 1px solid #333842; font-size: 10px; }"));
            connect(comboAxis, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, modId](int a) {
                for (auto& m : objectModifiers_[selectedId_]) if (m.id == modId) m.mirrorAxis = a;
            });
            pLayout->addWidget(comboAxis);

            auto* btnApplyMirror = new QPushButton(Tr("Отразить (Apply)", "Apply Mirror"), pRow);
            btnApplyMirror->setStyleSheet(QStringLiteral(
                "QPushButton { background: #334155; color: #FFFFFF; font-weight: bold; border: 1px solid #475569; border-radius: 4px; padding: 3px 8px; font-size: 10px; }"
                "QPushButton:hover { background: #475569; }"
            ));
            connect(btnApplyMirror, &QPushButton::clicked, this, [this, modId] {
                for (const auto& m : objectModifiers_[selectedId_]) {
                    if (m.id == modId) {
                        const int axisIdx = m.mirrorAxis; // 0: X, 1: Y, 2: Z
                        transform_[axisIdx]->setValue(-transform_[axisIdx]->value());
                        emit TransformEditingFinished(selectedId_, CurrentTransformValue());
                        break;
                    }
                }
            });
            pLayout->addWidget(btnApplyMirror);
            modLayout->addWidget(pRow);
        }
        else if (modType == 4) // Snap to Ground
        {
            auto* pRow = new QWidget(modCard);
            auto* pLayout = new QHBoxLayout(pRow);
            pLayout->setContentsMargins(0, 0, 0, 0);
            pLayout->setSpacing(6);

            auto* lblDesc = new QLabel(Tr("Удерживает основание на Y = 0", "Holds base on Y = 0"), pRow);
            lblDesc->setStyleSheet(QStringLiteral("color: #94A3B8; font-size: 10px; font-style: italic;"));
            pLayout->addWidget(lblDesc);

            auto* btnSnapNow = new QPushButton(Tr("Опустить (Snap)", "Snap Now"), pRow);
            btnSnapNow->setStyleSheet(QStringLiteral(
                "QPushButton { background: #334155; color: #FFFFFF; font-weight: bold; border: 1px solid #475569; border-radius: 4px; padding: 3px 8px; font-size: 10px; }"
                "QPushButton:hover { background: #475569; }"
            ));
            connect(btnSnapNow, &QPushButton::clicked, this, [this] {
                const double halfY = std::abs(transform_[7]->value()) * 0.5;
                transform_[1]->setValue(halfY);
                emit TransformEditingFinished(selectedId_, CurrentTransformValue());
            });
            pLayout->addWidget(btnSnapNow);
            modLayout->addWidget(pRow);
        }

        modifiersLayout_->addWidget(modCard);
    }
}

void ObjectInspectorPanel::TickModifiers()
{
    if (selectedId_ == 0 || refreshing_)
        return;

    auto it = objectModifiers_.find(selectedId_);
    if (it == objectModifiers_.end() || it->second.empty())
        return;

    const double dt = std::min(0.1, static_cast<double>(modifierElapsed_.restart()) / 1000.0);
    bool hasChange = false;

    for (auto& mod : it->second)
    {
        if (!mod.enabled)
            continue;

        if (mod.type == 0) // Turntable
        {
            const int rotIdx = 3 + mod.axis; // 3: X, 4: Y, 5: Z
            const double currentRot = transform_[rotIdx]->value();
            double nextRot = currentRot + mod.speed * dt;
            if (nextRot > 360.0) nextRot -= 360.0;
            else if (nextRot < -360.0) nextRot += 360.0;

            transform_[rotIdx]->setValue(nextRot);
            hasChange = true;
        }
        else if (mod.type == 1) // Sine Bounce
        {
            const int posIdx = mod.axis; // 0: X, 1: Y, 2: Z
            if (!mod.basePosInit)
            {
                mod.basePos = transform_[posIdx]->value();
                mod.basePosInit = true;
            }

            bouncePhase_ += 2.0 * 3.14159265358979323846 * mod.frequency * dt;
            if (bouncePhase_ > 628.31853) bouncePhase_ -= 628.31853;

            const double offset = std::sin(bouncePhase_) * mod.amplitude;
            transform_[posIdx]->setValue(mod.basePos + offset);
            hasChange = true;
        }
        else if (mod.type == 4) // Snap to Ground (держит основание на Y = 0)
        {
            const double targetY = std::abs(transform_[7]->value()) * 0.5;
            if (std::abs(transform_[1]->value() - targetY) > 0.001)
            {
                transform_[1]->setValue(targetY);
                hasChange = true;
            }
        }
    }

    if (hasChange)
    {
        emit TransformEditingFinished(selectedId_, CurrentTransformValue());
    }
}

void ObjectInspectorPanel::BuildPointLightCard(QVBoxLayout* layout)
{
    cardLight_ = new CollapsibleCard(Tr("Точечный свет", "Point Light"), this, true);
    lightInspector_ = cardLight_;
    auto* contentLayout = cardLight_->ContentLayout();

    auto emitPointLightChanged = [this] {
        if (refreshing_ || selectedId_ == 0) return;
        emit PointLightEditingFinished(selectedId_, CurrentPointLightValue());
    };

    // Чекбокс активности и кнопка цвета
    auto* rowTop = new QWidget(cardLight_->Content());
    auto* layoutTop = new QHBoxLayout(rowTop);
    layoutTop->setContentsMargins(0, 0, 0, 0);
    layoutTop->setSpacing(6);

    lightEnabled_ = new QCheckBox(Tr("Включён", "Enabled"), rowTop);
    lightEnabled_->setObjectName(QStringLiteral("LightEnabled"));
    lightEnabled_->setStyleSheet(QStringLiteral("color: #E2E8F0; font-size: 11px; font-weight: bold;"));
    connect(lightEnabled_, &QCheckBox::clicked, this, emitPointLightChanged);
    layoutTop->addWidget(lightEnabled_);

    lightColor_ = new QPushButton(Tr("Выбрать цвет…", "Choose Color…"), rowTop);
    lightColor_->setObjectName(QStringLiteral("LightColor"));
    lightColor_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #333842; color: #FFFFFF; font-weight: bold; border: 1px solid #4B5263; border-radius: 4px; padding: 3px 8px; font-size: 11px; }"
    ));
    connect(lightColor_, &QPushButton::clicked, this, [this] {
        if (refreshing_ || selectedId_ == 0) return;
        const QColor current = QColor::fromRgbF(
            lightValues_[0]->value(),
            lightValues_[1]->value(),
            lightValues_[2]->value()
        );
        emit LightColorRequested(selectedId_, current);
    });
    layoutTop->addWidget(lightColor_, 1);
    contentLayout->addWidget(rowTop);

    // RGB спинбоксы
    auto* rowRgb = new QWidget(cardLight_->Content());
    auto* layoutRgb = new QHBoxLayout(rowRgb);
    layoutRgb->setContentsMargins(0, 0, 0, 0);
    layoutRgb->setSpacing(4);

    for (int i = 0; i < 3; ++i)
    {
        auto* spin = lightValues_[i] = new QDoubleSpinBox(rowRgb);
        spin->setObjectName(QStringLiteral("LightValue%1").arg(i));
        spin->setRange(0.0, 1.0);
        spin->setDecimals(2);
        spin->setSingleStep(0.05);
        spin->setKeyboardTracking(false);
        connect(spin, &QDoubleSpinBox::editingFinished, this, emitPointLightChanged);

        const QString letter = (i == 0 ? QStringLiteral("R") : (i == 1 ? QStringLiteral("G") : QStringLiteral("B")));
        const QString color = (i == 0 ? QStringLiteral("#EF4444") : (i == 1 ? QStringLiteral("#22C55E") : QStringLiteral("#3B82F6")));
        layoutRgb->addWidget(CreateAxisSpinBox(spin, letter, color, rowRgb), 1);
    }
    contentLayout->addWidget(rowRgb);

    // Интенсивность (Intensity) со слайдером
    auto* rowInt = new QWidget(cardLight_->Content());
    auto* layoutInt = new QHBoxLayout(rowInt);
    layoutInt->setContentsMargins(0, 0, 0, 0);
    layoutInt->setSpacing(6);

    lightRowLabels_[3] = new QLabel(Tr("Интенсивность", "Intensity"), rowInt);
    lightRowLabels_[3]->setFixedWidth(80);
    lightRowLabels_[3]->setStyleSheet(QStringLiteral("color: #94A3B8; font-size: 11px;"));
    layoutInt->addWidget(lightRowLabels_[3]);

    lightValues_[3] = new QDoubleSpinBox(rowInt);
    lightValues_[3]->setRange(0.0, 10000.0);
    lightValues_[3]->setDecimals(2);
    lightValues_[3]->setSingleStep(0.5);
    lightValues_[3]->setFixedWidth(64);
    lightValues_[3]->setKeyboardTracking(false);
    lightValues_[3]->setStyleSheet(QStringLiteral(
        "QDoubleSpinBox { background: #1F232B; color: #F1F5F9; border: 1px solid #333842; border-radius: 4px; font-size: 11px; }"
    ));
    layoutInt->addWidget(lightValues_[3]);

    lightIntensitySlider_ = new QSlider(Qt::Horizontal, rowInt);
    lightIntensitySlider_->setRange(0, 1000); // 0.0 to 50.0
    layoutInt->addWidget(lightIntensitySlider_, 1);
    contentLayout->addWidget(rowInt);

    connect(lightValues_[3], qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double val) {
        if (lightIntensitySlider_) {
            lightIntensitySlider_->blockSignals(true);
            lightIntensitySlider_->setValue(std::min(1000, static_cast<int>(val * 20.0)));
            lightIntensitySlider_->blockSignals(false);
        }
    });
    connect(lightValues_[3], &QDoubleSpinBox::editingFinished, this, emitPointLightChanged);
    connect(lightIntensitySlider_, &QSlider::valueChanged, this, [this, emitPointLightChanged](int val) {
        lightValues_[3]->blockSignals(true);
        lightValues_[3]->setValue(static_cast<double>(val) / 20.0);
        lightValues_[3]->blockSignals(false);
        emitPointLightChanged();
    });

    // Радиус (Radius) со слайдером
    auto* rowRad = new QWidget(cardLight_->Content());
    auto* layoutRad = new QHBoxLayout(rowRad);
    layoutRad->setContentsMargins(0, 0, 0, 0);
    layoutRad->setSpacing(6);

    lightRowLabels_[4] = new QLabel(Tr("Радиус", "Radius"), rowRad);
    lightRowLabels_[4]->setFixedWidth(80);
    lightRowLabels_[4]->setStyleSheet(QStringLiteral("color: #94A3B8; font-size: 11px;"));
    layoutRad->addWidget(lightRowLabels_[4]);

    lightValues_[4] = new QDoubleSpinBox(rowRad);
    lightValues_[4]->setRange(0.01, 10000.0);
    lightValues_[4]->setDecimals(2);
    lightValues_[4]->setSingleStep(0.5);
    lightValues_[4]->setFixedWidth(64);
    lightValues_[4]->setKeyboardTracking(false);
    lightValues_[4]->setStyleSheet(QStringLiteral(
        "QDoubleSpinBox { background: #1F232B; color: #F1F5F9; border: 1px solid #333842; border-radius: 4px; font-size: 11px; }"
    ));
    layoutRad->addWidget(lightValues_[4]);

    lightRadiusSlider_ = new QSlider(Qt::Horizontal, rowRad);
    lightRadiusSlider_->setRange(1, 1000); // 0.1 to 25.0
    layoutRad->addWidget(lightRadiusSlider_, 1);
    contentLayout->addWidget(rowRad);

    connect(lightValues_[4], qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double val) {
        if (lightRadiusSlider_) {
            lightRadiusSlider_->blockSignals(true);
            lightRadiusSlider_->setValue(std::min(1000, static_cast<int>(val * 40.0)));
            lightRadiusSlider_->blockSignals(false);
        }
    });
    connect(lightValues_[4], &QDoubleSpinBox::editingFinished, this, emitPointLightChanged);
    connect(lightRadiusSlider_, &QSlider::valueChanged, this, [this, emitPointLightChanged](int val) {
        lightValues_[4]->blockSignals(true);
        lightValues_[4]->setValue(std::max(0.01, static_cast<double>(val) / 40.0));
        lightValues_[4]->blockSignals(false);
        emitPointLightChanged();
    });

    cardLight_->hide();
    layout->addWidget(cardLight_);
}

void ObjectInspectorPanel::BuildCharacterCard(QVBoxLayout* layout)
{
    cardCharacter_ = new CollapsibleCard(Tr("Персонаж", "Character"), this, true);
    characterInspector_ = cardCharacter_;
    auto* contentLayout = cardCharacter_->ContentLayout();

    auto* rowSpeed = new QWidget(cardCharacter_->Content());
    auto* layoutSpeed = new QHBoxLayout(rowSpeed);
    layoutSpeed->setContentsMargins(0, 0, 0, 0);
    layoutSpeed->setSpacing(6);

    characterSpeedLabel_ = new QLabel(Tr("Скорость (ед/с)", "Speed (units/s)"), rowSpeed);
    characterSpeedLabel_->setFixedWidth(90);
    characterSpeedLabel_->setStyleSheet(QStringLiteral("color: #94A3B8; font-size: 11px;"));
    layoutSpeed->addWidget(characterSpeedLabel_);

    characterSpeed_ = new QDoubleSpinBox(rowSpeed);
    characterSpeed_->setObjectName(QStringLiteral("CharacterSpeed"));
    characterSpeed_->setRange(0, 10000);
    characterSpeed_->setDecimals(1);
    characterSpeed_->setSingleStep(0.5);
    characterSpeed_->setFixedWidth(64);
    characterSpeed_->setKeyboardTracking(false);
    characterSpeed_->setStyleSheet(QStringLiteral(
        "QDoubleSpinBox { background: #1F232B; color: #F1F5F9; border: 1px solid #333842; border-radius: 4px; font-size: 11px; }"
    ));
    layoutSpeed->addWidget(characterSpeed_);

    characterSpeedSlider_ = new QSlider(Qt::Horizontal, rowSpeed);
    characterSpeedSlider_->setRange(0, 200); // 0 to 20.0 m/s
    layoutSpeed->addWidget(characterSpeedSlider_, 1);
    contentLayout->addWidget(rowSpeed);

    connect(characterSpeed_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double val) {
        if (characterSpeedSlider_) {
            characterSpeedSlider_->blockSignals(true);
            characterSpeedSlider_->setValue(std::min(200, static_cast<int>(val * 10.0)));
            characterSpeedSlider_->blockSignals(false);
        }
    });
    connect(characterSpeed_, &QDoubleSpinBox::editingFinished, this, [this] {
        if (!refreshing_ && selectedId_)
            emit CharacterSpeedEditingFinished(selectedId_, characterSpeed_->value());
    });
    connect(characterSpeedSlider_, &QSlider::valueChanged, this, [this](int val) {
        characterSpeed_->blockSignals(true);
        characterSpeed_->setValue(static_cast<double>(val) / 10.0);
        characterSpeed_->blockSignals(false);
        if (!refreshing_ && selectedId_)
            emit CharacterSpeedEditingFinished(selectedId_, characterSpeed_->value());
    });

    cardCharacter_->hide();
    layout->addWidget(cardCharacter_);
}

void ObjectInspectorPanel::UpdatePrimitiveParamsFromScale(const std::string& type, const float scale[3])
{
    primitiveUpdating_ = true;
    const QString t = QString::fromStdString(type);

    if (t == QStringLiteral("Primitive.Sphere"))
    {
        rowPrimRadius_->show();
        rowPrimHeight_->hide();
        rowPrimWidth_->hide();
        rowPrimDepth_->hide();
        spinPrimRadius_->setValue(std::max(0.01, static_cast<double>(scale[0]) * 0.5));
    }
    else if (t == QStringLiteral("Primitive.Cylinder") || t == QStringLiteral("Primitive.Cone"))
    {
        rowPrimRadius_->show();
        rowPrimHeight_->show();
        rowPrimWidth_->hide();
        rowPrimDepth_->hide();
        spinPrimRadius_->setValue(std::max(0.01, static_cast<double>(scale[0]) * 0.5));
        spinPrimHeight_->setValue(std::max(0.01, static_cast<double>(scale[1])));
    }
    else if (t == QStringLiteral("Primitive.Cube") || t == QStringLiteral("Primitive.Pyramid"))
    {
        rowPrimRadius_->hide();
        rowPrimHeight_->show();
        rowPrimWidth_->show();
        rowPrimDepth_->show();
        spinPrimWidth_->setValue(std::max(0.01, static_cast<double>(scale[0])));
        spinPrimHeight_->setValue(std::max(0.01, static_cast<double>(scale[1])));
        spinPrimDepth_->setValue(std::max(0.01, static_cast<double>(scale[2])));
    }
    else if (t == QStringLiteral("Primitive.Plane"))
    {
        rowPrimRadius_->hide();
        rowPrimHeight_->hide();
        rowPrimWidth_->show();
        rowPrimDepth_->show();
        spinPrimWidth_->setValue(std::max(0.01, static_cast<double>(scale[0])));
        spinPrimDepth_->setValue(std::max(0.01, static_cast<double>(scale[2])));
    }
    primitiveUpdating_ = false;
}

void ObjectInspectorPanel::ApplyPrimitiveParamsToScale(const std::string& type)
{
    if (primitiveUpdating_ || selectedId_ == 0 || refreshing_)
        return;

    const QString t = QString::fromStdString(type);

    if (t == QStringLiteral("Primitive.Sphere"))
    {
        const double r = spinPrimRadius_->value();
        const double s = r * 2.0;
        transform_[6]->setValue(s);
        transform_[7]->setValue(s);
        transform_[8]->setValue(s);
    }
    else if (t == QStringLiteral("Primitive.Cylinder") || t == QStringLiteral("Primitive.Cone"))
    {
        const double r = spinPrimRadius_->value();
        const double s = r * 2.0;
        const double h = spinPrimHeight_->value();
        transform_[6]->setValue(s);
        transform_[7]->setValue(h);
        transform_[8]->setValue(s);
    }
    else if (t == QStringLiteral("Primitive.Cube") || t == QStringLiteral("Primitive.Pyramid"))
    {
        transform_[6]->setValue(spinPrimWidth_->value());
        transform_[7]->setValue(spinPrimHeight_->value());
        transform_[8]->setValue(spinPrimDepth_->value());
    }
    else if (t == QStringLiteral("Primitive.Plane"))
    {
        transform_[6]->setValue(spinPrimWidth_->value());
        transform_[8]->setValue(spinPrimDepth_->value());
    }

    emit TransformEditingFinished(selectedId_, CurrentTransformValue());
}

void ObjectInspectorPanel::RetranslateUi()
{
    if (cardHeader_) cardHeader_->SetTitle(Tr("Объект / Шапка", "Object Header"));
    if (cardTransform_) cardTransform_->SetTitle(Tr("Трансформация", "Transform"));
    if (cardGeometry_) cardGeometry_->SetTitle(Tr("Геометрия", "Geometry"));
    if (cardMaterial_) cardMaterial_->SetTitle(Tr("Материал и Поверхность", "Material & Surface"));
    if (cardModifiers_) cardModifiers_->SetTitle(Tr("Модификаторы", "Modifiers"));
    if (cardLight_) cardLight_->SetTitle(Tr("Точечный свет", "Point Light"));
    if (cardCharacter_) cardCharacter_->SetTitle(Tr("Персонаж", "Character"));

    if (btnAddModifier_) btnAddModifier_->setText(Tr("+ Добавить модификатор", "+ Add Modifier"));
    if (lblNoModifiers_) lblNoModifiers_->setText(Tr("Нет активных модификаторов. Нажмите «+ Добавить модификатор».",
                                                   "No active modifiers. Click \"+ Add Modifier\"."));

    if (nameRowLabel_) nameRowLabel_->setText(Tr("Имя", "Name"));
    if (btnQuickFocus_) btnQuickFocus_->setText(Tr("Фокус (F)", "Focus (F)"));
    if (btnQuickDuplicate_) btnQuickDuplicate_->setText(Tr("Дублировать", "Duplicate"));
    if (btnQuickDelete_) btnQuickDelete_->setText(Tr("Удалить", "Delete"));

    if (labelPosRow_) labelPosRow_->setText(Tr("Позиция", "Position"));
    if (labelRotRow_) labelRotRow_->setText(Tr("Поворот", "Rotation"));
    if (labelScaleRow_) labelScaleRow_->setText(Tr("Масштаб", "Scale"));
    if (btnResetPos_) btnResetPos_->setToolTip(Tr("Сброс позиции (0, 0, 0)", "Reset Position (0, 0, 0)"));
    if (btnResetRot_) btnResetRot_->setToolTip(Tr("Сброс поворота (0, 0, 0)", "Reset Rotation (0, 0, 0)"));
    if (btnResetScale_) btnResetScale_->setToolTip(Tr("Сброс масштаба (1, 1, 1)", "Reset Scale (1, 1, 1)"));

    if (lblPrimParamsTitle_) lblPrimParamsTitle_->setText(Tr("Параметры примитива (3ds Max)", "Primitive Parameters (3ds Max)"));
    if (lblPrimRadius_) lblPrimRadius_->setText(Tr("Радиус", "Radius"));
    if (lblPrimHeight_) lblPrimHeight_->setText(Tr("Высота", "Height"));
    if (lblPrimWidth_) lblPrimWidth_->setText(Tr("Ширина (X)", "Width (X)"));
    if (lblPrimDepth_) lblPrimDepth_->setText(Tr("Глубина / Длина (Z)", "Depth / Length (Z)"));

    if (modelRowLabel_) modelRowLabel_->setText(Tr("Меш / Модель", "Mesh / Model Asset"));
    if (chooseModel_) chooseModel_->setText(Tr("Выбрать…", "Choose…"));
    if (clearModel_) clearModel_->setText(Tr("Убрать", "Clear"));

    if (lblSkeletonTitle_) lblSkeletonTitle_->setText(Tr("Скелет и Анимация", "Skeleton & Animation"));
    if (lblSkeletonStatus_) lblSkeletonStatus_->setText(Tr("Скелет: Auto / Assimp", "Skeleton: Auto / Assimp"));
    if (lblAnimClip_) lblAnimClip_->setText(Tr("Клип", "Clip"));
    if (lblAnimSpeed_) lblAnimSpeed_->setText(Tr("Скор.", "Speed"));
    if (chkAnimationLoop_) chkAnimationLoop_->setText(Tr("Зациклить (Loop)", "Loop"));

    if (materialRowLabel_) materialRowLabel_->setText(Tr("Материал (.mat)", "Material (.mat)"));
    if (editMaterialButton_) editMaterialButton_->setText(Tr("Редактировать", "Edit Material"));
    if (chooseMaterialButton_) chooseMaterialButton_->setText(Tr("Выбрать…", "Assign…"));
    if (clearMaterialButton_) clearMaterialButton_->setText(Tr("Убрать", "Clear"));

    if (textureRowLabel_) textureRowLabel_->setText(Tr("Текстура (Base Texture)", "Base Texture"));
    if (chooseTextureButton_) chooseTextureButton_->setText(Tr("Выбрать…", "Browse…"));
    if (clearTextureButton_) clearTextureButton_->setText(Tr("Убрать", "Clear"));

    if (lightColor_) lightColor_->setText(Tr("Выбрать цвет…", "Choose Color…"));
    if (lightEnabled_) lightEnabled_->setText(Tr("Включён", "Enabled"));
    if (lightRowLabels_[3]) lightRowLabels_[3]->setText(Tr("Интенсивность", "Intensity"));
    if (lightRowLabels_[4]) lightRowLabels_[4]->setText(Tr("Радиус", "Radius"));

    if (characterSpeedLabel_) characterSpeedLabel_->setText(Tr("Скорость (ед/с)", "Speed (units/s)"));

    if (selectedId_ == 0 && selectionLabel_)
    {
        selectionLabel_->setText(Tr("Объект не выбран", "No object selected"));
    }
}

void ObjectInspectorPanel::SetEnabled(bool enabled)
{
    setEnabled(enabled);
}

void ObjectInspectorPanel::ClearFocusIfInside()
{
    if (auto* focus = QApplication::focusWidget(); focus && isAncestorOf(focus))
        focus->clearFocus();
}

bool ObjectInspectorPanel::HasFocusedChild() const
{
    const auto* focus = QApplication::focusWidget();
    return focus && isAncestorOf(focus);
}

InspectorTransformValue ObjectInspectorPanel::CurrentTransformValue() const
{
    InspectorTransformValue value;

    for (int i = 0; i < 3; ++i)
    {
        value.position[i] = transform_[i]->value();
        value.euler[i] = transform_[i + 3]->value();
        value.scale[i] = transform_[i + 6]->value();
    }

    return value;
}

InspectorPointLightValue ObjectInspectorPanel::CurrentPointLightValue() const
{
    InspectorPointLightValue value;

    for (int i = 0; i < 3; ++i)
        value.color[i] = lightValues_[i]->value();

    value.intensity = lightValues_[3]->value();
    value.radius = lightValues_[4]->value();
    value.enabled = lightEnabled_->isChecked();

    return value;
}

void ObjectInspectorPanel::UpdateLightColorButton(const float color[3])
{
    const QColor c = QColor::fromRgbF(color[0], color[1], color[2]);
    lightColor_->setStyleSheet(QStringLiteral(
        "background-color: %1; color: %2; font-weight: bold; border: 1px solid #4B5263; border-radius: 4px; padding: 3px 8px;"
    ).arg(c.name(), c.lightnessF() > 0.5 ? QStringLiteral("black") : QStringLiteral("white")));
}

void ObjectInspectorPanel::Refresh(
    const EditorQtHost::Snapshot& view,
    quint64 selectedId,
    bool selectionChanged
)
{
    refreshing_ = true;

    selectedId_ = selectedId;

    if (selectedId_ == 0)
    {
        selectedType_.clear();
        selectionLabel_->setText(Tr("Объект не выбран", "No object selected"));
        typeBadgeLabel_->setText(QStringLiteral("None"));
        objectName_->clear();

        for (auto* spin : transform_)
            spin->setValue(0.0);

        modelAsset_->clear();
        textureAsset_->clear();
        materialAsset_->clear();
        editMaterialButton_->setEnabled(false);

        if (cardLight_) cardLight_->hide();
        if (cardCharacter_) cardCharacter_->hide();

        RebuildModifiersList();
        refreshing_ = false;
        return;
    }

    const auto it = std::find_if(
        view.objects.begin(),
        view.objects.end(),
        [selectedId](const auto& o) { return o.id == selectedId; }
    );

    if (it == view.objects.end())
    {
        selectedType_.clear();
        selectionLabel_->setText(Tr("Выбран несуществующий объект", "Selected object does not exist"));
        typeBadgeLabel_->setText(QStringLiteral("None"));
        RebuildModifiersList();
        refreshing_ = false;
        return;
    }

    const auto& object = *it;
    selectedType_ = object.type;

    typeBadgeLabel_->setText(QString::fromStdString(object.type.empty() ? "Object" : object.type));
    selectionLabel_->setText(
        QStringLiteral("%1  [#%2]")
            .arg(QString::fromStdString(object.name))
            .arg(object.id)
    );

    if (selectionChanged || !objectName_->hasFocus())
        objectName_->setText(QString::fromStdString(object.name));

    const auto& t = object.transform;

    const QQuaternion q(
        t.rotation[3],
        t.rotation[0],
        t.rotation[1],
        t.rotation[2]
    );

    const auto euler = q.toEulerAngles();

    const double values[] =
    {
        t.position[0],
        t.position[1],
        t.position[2],
        euler.x(),
        euler.y(),
        euler.z(),
        t.scale[0],
        t.scale[1],
        t.scale[2]
    };

    for (int i = 0; i < 9; ++i)
    {
        const bool focused = transform_[i]->hasFocus() || transform_[i]->isAncestorOf(QApplication::focusWidget());
        if (selectionChanged || !focused)
            transform_[i]->setValue(values[i]);
    }

    // Геометрия: Примитив vs Меш
    const QString qType = QString::fromStdString(object.type);
    const bool isPrimitive = qType.startsWith(QStringLiteral("Primitive."));

    if (isPrimitive)
    {
        primitiveParamsGroup_->show();
        meshParamsGroup_->hide();
        skeletalParamsGroup_->hide();
        UpdatePrimitiveParamsFromScale(object.type, object.transform.scale);
    }
    else
    {
        primitiveParamsGroup_->hide();
        meshParamsGroup_->show();
        skeletalParamsGroup_->show();
    }

    const QString model = QString::fromStdString(object.modelAsset);
    const QString texture = QString::fromStdString(object.textureAsset);
    const QString material = QString::fromStdString(object.materialAsset);

    if (selectionChanged || !modelAsset_->hasFocus())
        modelAsset_->setText(model);

    if (selectionChanged || !textureAsset_->hasFocus())
        textureAsset_->setText(texture);

    if (selectionChanged || !materialAsset_->hasFocus())
        materialAsset_->setText(material);

    editMaterialButton_->setEnabled(!object.materialAsset.empty() || !object.hasPointLight);
    editMaterialButton_->setToolTip(object.materialAsset.empty()
        ? Tr("Создать материал в Content/Materials, назначить его объекту и открыть редактор",
             "Create material in Content/Materials, assign to object, and open editor")
        : QString());

    const bool supportsModel = object.type == "Engine.Model" || isPrimitive;
    chooseModel_->setEnabled(supportsModel);

    // Модификаторы
    if (selectionChanged)
    {
        RebuildModifiersList();
    }

    // Точечный свет (Point Light)
    if (object.hasPointLight)
    {
        cardLight_->show();

        const auto& light = object.pointLight;

        const double lv[] =
        {
            light.color[0],
            light.color[1],
            light.color[2],
            light.intensity,
            light.radius
        };

        for (int i = 0; i < 5; ++i)
        {
            const bool focused = lightValues_[i]->hasFocus() || lightValues_[i]->isAncestorOf(QApplication::focusWidget());
            if (selectionChanged || !focused)
                lightValues_[i]->setValue(lv[i]);
        }

        if (lightIntensitySlider_) {
            lightIntensitySlider_->blockSignals(true);
            lightIntensitySlider_->setValue(std::min(1000, static_cast<int>(light.intensity * 20.0)));
            lightIntensitySlider_->blockSignals(false);
        }

        if (lightRadiusSlider_) {
            lightRadiusSlider_->blockSignals(true);
            lightRadiusSlider_->setValue(std::min(1000, static_cast<int>(light.radius * 40.0)));
            lightRadiusSlider_->blockSignals(false);
        }

        lightEnabled_->setChecked(light.enabled != 0);
        UpdateLightColorButton(light.color);
    }
    else
    {
        cardLight_->hide();
    }

    // Персонаж (Character)
    if (object.gameplayKind == 1)
    {
        cardCharacter_->show();
        double speed = 5.0;
        if (!object.projectData.empty()) {
            const auto json = QJsonDocument::fromJson(QByteArray::fromStdString(object.projectData)).object();
            if (json.contains("moveSpeed")) speed = json["moveSpeed"].toDouble();
            else if (json.contains("speed")) speed = json["speed"].toDouble();
        }
        const bool speedFocused = characterSpeed_->hasFocus() || characterSpeed_->isAncestorOf(QApplication::focusWidget());
        if (selectionChanged || !speedFocused) {
            characterSpeed_->setValue(speed);
            if (characterSpeedSlider_) {
                characterSpeedSlider_->blockSignals(true);
                characterSpeedSlider_->setValue(std::min(200, static_cast<int>(speed * 10.0)));
                characterSpeedSlider_->blockSignals(false);
            }
        }
    }
    else
    {
        cardCharacter_->hide();
    }

    refreshing_ = false;
}
