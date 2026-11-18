#include "UIEditorWindow.h"
#include "CollapsibleCard.h"
#include "EditorLocale.h"
#include "InfoButton.h"
#include <Engine/UI/UIMarkup.h>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {
glm::vec4* Color(UIElement* e, int which) {
    if (auto* p = dynamic_cast<UIPanel*>(e)) return which == 0 ? &p->Color : which == 1 ? &p->BorderColor : nullptr;
    if (auto* p = dynamic_cast<UILabel*>(e)) return which == 0 ? &p->Color : nullptr;
    if (auto* p = dynamic_cast<UIBar*>(e)) return which == 0 ? &p->FillColor : which == 1 ? &p->TrackColor : nullptr;
    if (auto* p = dynamic_cast<UIButton*>(e)) {
        glm::vec4* colors[] = {&p->Color, &p->TextColor, &p->HoverColor, &p->PressedColor, &p->BorderColor};
        return which >= 0 && which < 5 ? colors[which] : nullptr;
    }
    return nullptr;
}
}

void UIEditorWindow::BuildInspector(QWidget* props, QScrollArea* scroll) {
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setMinimumWidth(280);
    props->setMinimumWidth(270);

    auto* propsLayout = new QVBoxLayout(props);
    propsLayout->setContentsMargins(4, 4, 4, 4);
    propsLayout->setSpacing(6);

    // Card 1: General
    cardGeneral_ = new CollapsibleCard(Tr("Общие", "General"), props);
    // Content() already owns a QVBoxLayout; a second top-level layout on the same
    // widget is rejected by Qt and leaves every field unmanaged (zero-height card).
    auto* formGeneral = new QFormLayout;
    formGeneral->setContentsMargins(0, 0, 0, 0);
    formGeneral->setSpacing(4);
    formGeneral->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    cardGeneral_->ContentLayout()->addLayout(formGeneral);
    name_ = new QLineEdit(cardGeneral_->Content());
    name_->setObjectName("UIName");
    labelName_ = new QLabel(Tr("Имя", "Name"), cardGeneral_->Content());
    formGeneral->addRow(labelName_, name_);
    text_ = new QLineEdit(cardGeneral_->Content());
    text_->setObjectName("UIText");
    labelText_ = new QLabel(Tr("Текст", "Text"), cardGeneral_->Content());
    formGeneral->addRow(labelText_, text_);
    propsLayout->addWidget(cardGeneral_);

    // Card 2: Layout
    cardLayout_ = new CollapsibleCard(Tr("Макет", "Layout"), props);
    auto* formLayout = new QFormLayout;
    formLayout->setContentsMargins(0, 0, 0, 0);
    formLayout->setSpacing(4);
    formLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    cardLayout_->ContentLayout()->addLayout(formLayout);
    const QString labels[] = {"X", "Y", Tr("Ширина", "Width"), Tr("Высота", "Height")};
    for (int i = 0; i < 4; ++i) {
        rect_[i] = new QDoubleSpinBox(cardLayout_->Content());
        rect_[i]->setObjectName(QString("UIRect%1").arg(i));
        rect_[i]->setRange(i < 2 ? -100000 : 0, 100000);
        rect_[i]->setDecimals(2);
        rect_[i]->setKeyboardTracking(false);
        rect_[i]->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        rect_[i]->setMinimumHeight(22);
        labelRect_[i] = new QLabel(labels[i], cardLayout_->Content());
        formLayout->addRow(labelRect_[i], rect_[i]);
        connect(rect_[i], &QDoubleSpinBox::valueChanged, this, [this] { ApplyInspector(); });
    }
    anchor_ = new QComboBox(cardLayout_->Content());
    anchor_->setObjectName("UIAnchor");
    anchor_->addItems({"TopLeft", "TopCenter", "TopRight", "CenterLeft", "Center", "CenterRight", "BottomLeft", "BottomCenter", "BottomRight"});
    anchor_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    labelAnchor_ = new QLabel(Tr("Привязка", "Anchor"), cardLayout_->Content());
    formLayout->addRow(labelAnchor_, anchor_);

    auto* padRow = new QWidget(cardLayout_->Content());
    auto* padLayout = new QHBoxLayout(padRow);
    padLayout->setContentsMargins(0, 0, 0, 0);
    padLayout->setSpacing(2);
    const QString padNames[] = {Tr("Слева", "Left"), Tr("Сверху", "Top"), Tr("Справа", "Right"), Tr("Снизу", "Bottom")};
    for (int i = 0; i < 4; ++i) {
        padding_[i] = new QDoubleSpinBox(padRow);
        padding_[i]->setObjectName(QString("UIPadding%1").arg(i));
        padding_[i]->setRange(0, 100000);
        padding_[i]->setDecimals(2);
        padding_[i]->setKeyboardTracking(false);
        padding_[i]->setButtonSymbols(QAbstractSpinBox::NoButtons);
        padding_[i]->setAlignment(Qt::AlignCenter);
        padding_[i]->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        padding_[i]->setMinimumHeight(22);
        padding_[i]->setToolTip(padNames[i]);
        padLayout->addWidget(padding_[i]);
        connect(padding_[i], &QDoubleSpinBox::valueChanged, this, [this] { ApplyInspector(); });
    }
    labelPadding_ = new QLabel(Tr("Отступы", "Padding"), cardLayout_->Content());
    formLayout->addRow(labelPadding_, padRow);
    propsLayout->addWidget(cardLayout_);

    // Card 3: Appearance
    cardAppearance_ = new CollapsibleCard(Tr("Вид", "Appearance"), props);
    auto* formAppearance = new QFormLayout;
    formAppearance->setContentsMargins(0, 0, 0, 0);
    formAppearance->setSpacing(4);
    formAppearance->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    cardAppearance_->ContentLayout()->addLayout(formAppearance);
    visible_ = new QCheckBox(cardAppearance_->Content());
    visible_->setObjectName("UIVisible");
    labelVisible_ = new QLabel(Tr("Видимость", "Visible"), cardAppearance_->Content());
    formAppearance->addRow(labelVisible_, visible_);
    colorProperty_ = new QComboBox(cardAppearance_->Content());
    colorProperty_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    labelColorProperty_ = new QLabel(Tr("Свойство цвета", "Color Property"), cardAppearance_->Content());
    formAppearance->addRow(labelColorProperty_, colorProperty_);
    colorBtn_ = new QPushButton(Tr("Цвет и прозрачность…", "Color & Opacity…"), cardAppearance_->Content());
    colorBtn_->setObjectName("UIColor");
    colorBtn_->setFixedHeight(24);
    colorBtn_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    formAppearance->addRow(colorBtn_);

    borderWidth_ = new QDoubleSpinBox(cardAppearance_->Content());
    borderWidth_->setObjectName("UIBorderWidth");
    borderWidth_->setRange(0, 1000);
    borderWidth_->setDecimals(2);
    borderWidth_->setKeyboardTracking(false);
    borderWidth_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    borderWidth_->setMinimumHeight(22);
    labelBorderWidth_ = new QLabel(Tr("Толщина рамки", "Border Width"), cardAppearance_->Content());
    formAppearance->addRow(labelBorderWidth_, borderWidth_);
    connect(borderWidth_, &QDoubleSpinBox::valueChanged, this, [this] { ApplyInspector(); });
    propsLayout->addWidget(cardAppearance_);

    // Card 4: Behavior
    cardBehavior_ = new CollapsibleCard(Tr("Поведение", "Behavior"), props);
    auto* formBehavior = new QFormLayout;
    formBehavior->setContentsMargins(0, 0, 0, 0);
    formBehavior->setSpacing(4);
    formBehavior->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    cardBehavior_->ContentLayout()->addLayout(formBehavior);
    interactive_ = new QCheckBox(cardBehavior_->Content());
    interactive_->setObjectName("UIInteractive");
    labelInteractive_ = new QLabel(Tr("Интерактивность", "Interactive"), cardBehavior_->Content());
    formBehavior->addRow(labelInteractive_, interactive_);
    connect(interactive_, &QCheckBox::toggled, this, [this] { ApplyInspector(); });

    alignRight_ = new QCheckBox(cardBehavior_->Content());
    alignRight_->setObjectName("UIAlignRight");
    labelAlignRight_ = new QLabel(Tr("Выравнивание вправо", "Align Right"), cardBehavior_->Content());
    formBehavior->addRow(labelAlignRight_, alignRight_);
    connect(alignRight_, &QCheckBox::toggled, this, [this] { ApplyInspector(); });

    fill_ = new QDoubleSpinBox(cardBehavior_->Content());
    fill_->setObjectName("UIFill");
    fill_->setRange(0, 1);
    fill_->setDecimals(3);
    fill_->setSingleStep(0.05);
    fill_->setKeyboardTracking(false);
    fill_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    fill_->setMinimumHeight(22);
    labelFill_ = new QLabel(Tr("Заполнение", "Fill"), cardBehavior_->Content());
    formBehavior->addRow(labelFill_, fill_);
    connect(fill_, &QDoubleSpinBox::valueChanged, this, [this] { ApplyInspector(); });
    propsLayout->addWidget(cardBehavior_);

    hintLabel_ = new QLabel(Tr("Размер 0 растягивается до края.\nДобавление — внутрь выбранной панели\nили группы. Кнопки получают логику в C++.", "Size 0 stretches to the edge.\nAdding creates elements inside selected panel\nor group. Buttons receive logic in C++."), props);
    hintLabel_->setWordWrap(true);
    propsLayout->addWidget(hintLabel_);
    propsLayout->addStretch(1);

    connect(name_, &QLineEdit::editingFinished, this, [this] { ApplyInspector(); });
    connect(text_, &QLineEdit::editingFinished, this, [this] { ApplyInspector(); });
    connect(name_, &QLineEdit::textEdited, this, [this] { Changed(); });
    connect(text_, &QLineEdit::textEdited, this, [this] { Changed(); });
    connect(anchor_, &QComboBox::currentIndexChanged, this, [this] { ApplyInspector(); });
    connect(visible_, &QCheckBox::toggled, this, [this] { ApplyInspector(); });
    connect(colorBtn_, &QPushButton::clicked, this, [this] {
        auto* c = Color(selected_, colorProperty_->currentIndex());
        if (!c) return;
        const auto chosen = QColorDialog::getColor(QColor::fromRgbF(c->r, c->g, c->b, c->a), this, Tr("Цвет UI", "UI Color"), QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) {
            Checkpoint();
            *c = glm::vec4(chosen.redF(), chosen.greenF(), chosen.blueF(), chosen.alphaF());
            Changed();
        }
    });
}

void UIEditorWindow::RetranslateUi() {
    setWindowTitle((dirty_ ? QStringLiteral("* ") : QString()) + Tr("Редактор UI — ", "UI Editor — ") + QFileInfo(file_).fileName());
    if (saveAction_) { saveAction_->setText(Tr("Сохранить", "Save")); saveAction_->setToolTip(Tr("Сохранить: Ctrl+S", "Save: Ctrl+S")); }
    if (reloadAction_) { reloadAction_->setText(Tr("Перечитать", "Reload")); reloadAction_->setToolTip(Tr("Перечитать — сброс несохранённых правок", "Reload — discard unsaved changes")); }
    if (deleteAction_) { deleteAction_->setText(Tr("Удалить", "Delete")); deleteAction_->setToolTip(Tr("Удалить выбранный элемент: Delete", "Delete selected element: Delete")); }
    if (undoAction_) { undoAction_->setText(Tr("Отменить", "Undo")); undoAction_->setToolTip(Tr("Отменить: Ctrl+Z", "Undo: Ctrl+Z")); }
    if (redoAction_) { redoAction_->setText(Tr("Повторить", "Redo")); redoAction_->setToolTip(Tr("Повторить: Ctrl+Y", "Redo: Ctrl+Y")); }
    if (gridAction_) { gridAction_->setText(Tr("Сетка", "Grid")); gridAction_->setToolTip(Tr("Включить/выключить сетку", "Toggle Grid")); }
    if (tree_) tree_->setHeaderLabel(Tr("Иерархия", "Hierarchy"));
    if (cardGeneral_) cardGeneral_->SetTitle(Tr("Общие", "General"));
    if (labelName_) labelName_->setText(Tr("Имя", "Name"));
    if (labelText_) labelText_->setText(Tr("Текст", "Text"));
    if (cardLayout_) cardLayout_->SetTitle(Tr("Макет", "Layout"));
    const QString labels[] = {"X", "Y", Tr("Ширина", "Width"), Tr("Высота", "Height")};
    for (int i = 0; i < 4; ++i) {
        if (labelRect_[i]) labelRect_[i]->setText(labels[i]);
    }
    if (labelAnchor_) labelAnchor_->setText(Tr("Привязка", "Anchor"));
    if (cardAppearance_) cardAppearance_->SetTitle(Tr("Вид", "Appearance"));
    if (labelVisible_) labelVisible_->setText(Tr("Видимость", "Visible"));
    if (labelColorProperty_) labelColorProperty_->setText(Tr("Свойство цвета", "Color Property"));
    if (colorBtn_) colorBtn_->setText(Tr("Цвет и прозрачность…", "Color & Opacity…"));
    if (labelPadding_) labelPadding_->setText(Tr("Отступы", "Padding"));
    if (labelInteractive_) labelInteractive_->setText(Tr("Интерактивность", "Interactive"));
    if (labelBorderWidth_) labelBorderWidth_->setText(Tr("Толщина рамки", "Border Width"));
    if (cardBehavior_) cardBehavior_->SetTitle(Tr("Поведение", "Behavior"));
    if (labelAlignRight_) labelAlignRight_->setText(Tr("Выравнивание вправо", "Align Right"));
    if (labelFill_) labelFill_->setText(Tr("Заполнение", "Fill"));
    const QString padNames[] = {Tr("Слева", "Left"), Tr("Сверху", "Top"), Tr("Справа", "Right"), Tr("Снизу", "Bottom")};
    for (int i = 0; i < 4; ++i) if (padding_[i]) padding_[i]->setToolTip(padNames[i]);
    if (hintLabel_) hintLabel_->setText(Tr("Размер 0 растягивается до края.\nДобавление — внутрь выбранной панели\nили группы. Кнопки получают логику в C++.", "Size 0 stretches to the edge.\nAdding creates elements inside selected panel\nor group. Buttons receive logic in C++."));
    if (helpBtn_) {
        helpBtn_->SetInfoText(
            Tr("Редактор интерфейса", "UI Editor"),
            Tr("• СКМ или Пробел+ЛКМ: панорамирование холста\n• Колесо мыши: масштабирование к курсору\n• Перетаскивание из палитры: добавление виджета в точку отпускания\n• Дерево иерархии: переупорядочивание и вложенность элементов",
               "• MMB or Space+LMB: pan canvas\n• Mouse wheel: zoom to cursor\n• Drag from palette: add widget at drop location\n• Hierarchy tree: reorder and nest elements"),
            QString());
    }
    RefreshInspector();
}

void UIEditorWindow::RefreshInspector() {
    refreshing_ = true;
    const bool valid = (selected_ != nullptr);
    for (auto* field : rect_) field->setEnabled(valid);
    name_->setEnabled(valid);
    anchor_->setEnabled(valid);
    visible_->setEnabled(valid);
    text_->setEnabled(false);
    colorProperty_->clear();
    for (auto* field : padding_) field->setEnabled(valid);
    interactive_->setEnabled(valid);
    borderWidth_->setEnabled(false);
    alignRight_->setEnabled(false);
    fill_->setEnabled(false);

    if (valid) {
        name_->setText(QString::fromStdString(selected_->Name));
        const auto& r = selected_->Rect;
        const float values[] = {r.X, r.Y, r.Width, r.Height};
        for (int i = 0; i < 4; ++i) rect_[i]->setValue(values[i]);
        anchor_->setCurrentIndex(int(r.Anchor));
        visible_->setChecked(selected_->bVisible);
        text_->clear();
        const auto& pad = selected_->Padding;
        const float pads[] = {pad.Left, pad.Top, pad.Right, pad.Bottom};
        for (int i = 0; i < 4; ++i) padding_[i]->setValue(pads[i]);
        interactive_->setChecked(selected_->bInteractive);
        borderWidth_->setValue(0);
        alignRight_->setChecked(false);
        fill_->setValue(0);

        if (auto* p = dynamic_cast<UILabel*>(selected_)) {
            text_->setEnabled(true);
            text_->setText(QString::fromStdString(p->GetText()));
            colorProperty_->addItems({Tr("Текст", "Text")});
            alignRight_->setEnabled(true);
            alignRight_->setChecked(p->bAlignRight);
        }
        if (auto* p = dynamic_cast<UIButton*>(selected_)) {
            text_->setEnabled(true);
            text_->setText(QString::fromStdString(p->GetText()));
            colorProperty_->addItems({Tr("Фон", "Background"), Tr("Текст", "Text"), Tr("Наведение", "Hover"), Tr("Нажатие", "Pressed"), Tr("Рамка", "Border")});
            borderWidth_->setEnabled(true);
            borderWidth_->setValue(p->BorderWidth);
        }
        if (auto* p = dynamic_cast<UIPanel*>(selected_)) {
            colorProperty_->addItems({Tr("Фон", "Background"), Tr("Рамка", "Border")});
            borderWidth_->setEnabled(true);
            borderWidth_->setValue(p->BorderWidth);
        }
        if (auto* p = dynamic_cast<UIBar*>(selected_)) {
            colorProperty_->addItems({Tr("Заполнение", "Fill"), Tr("Фон", "Background")});
            fill_->setEnabled(true);
            fill_->setValue(p->Fill);
        }
    }
    refreshing_ = false;
}

bool UIEditorWindow::ApplyInspector() {
    if (refreshing_ || !selected_) return true;
    const auto name = name_->text().toUtf8().toStdString();
    auto* existing = root_->Find(name);
    if (!name.empty() && existing && existing != selected_) {
        status_->setText(Tr("Имя уже используется: ", "Name already in use: ") + name_->text());
        return false;
    }
    const auto rootBefore = UIMarkup::Serialize(*root_);
    const auto before = UIMarkup::Serialize(*selected_);
    selected_->Name = name;
    selected_->Rect = {UIAnchor(anchor_->currentIndex()), float(rect_[0]->value()), float(rect_[1]->value()), float(rect_[2]->value()), float(rect_[3]->value())};
    selected_->bVisible = visible_->isChecked();
    selected_->Padding = {float(padding_[0]->value()), float(padding_[1]->value()), float(padding_[2]->value()), float(padding_[3]->value())};
    selected_->bInteractive = interactive_->isChecked();
    if (auto* p = dynamic_cast<UILabel*>(selected_)) { p->SetText(text_->text().toUtf8().toStdString()); p->bAlignRight = alignRight_->isChecked(); }
    if (auto* p = dynamic_cast<UIButton*>(selected_)) { p->SetText(text_->text().toUtf8().toStdString()); p->BorderWidth = float(borderWidth_->value()); }
    if (auto* p = dynamic_cast<UIPanel*>(selected_)) p->BorderWidth = float(borderWidth_->value());
    if (auto* p = dynamic_cast<UIBar*>(selected_)) p->Fill = float(fill_->value());
    if (before != UIMarkup::Serialize(*selected_)) { PushUndo(rootBefore); Changed(); RebuildTree(); }
    return true;
}
