#include "UIEditorWindow.h"
#include "EditorLocale.h"
#include <Engine/UI/UIMarkup.h>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFont>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>

namespace {
QLabel* Section(QFormLayout* form, QWidget* props, const QString& text) {
    auto* label = new QLabel(text, props); QFont font = label->font(); font.setBold(true); label->setFont(font); form->addRow(label); return label;
}
}

// Property editors for the engine UI attributes the inspector previously left hidden:
// padding, interactivity, border width, right alignment and bar fill. UIMarkup already
// serializes all of them, so exposing them here is purely an editor concern.
void UIEditorWindow::BuildPropertyEditors(QFormLayout* form, QWidget* props) {
    auto* padRow = new QWidget(props);
    auto* padLayout = new QHBoxLayout(padRow);
    padLayout->setContentsMargins(0, 0, 0, 0);
    const QString padNames[] = {Tr("Слева", "Left"), Tr("Сверху", "Top"), Tr("Справа", "Right"), Tr("Снизу", "Bottom")};
    for (int i = 0; i < 4; ++i) {
        padding_[i] = new QDoubleSpinBox(padRow);
        padding_[i]->setObjectName(QString("UIPadding%1").arg(i));
        padding_[i]->setRange(0, 100000); padding_[i]->setDecimals(2); padding_[i]->setKeyboardTracking(false);
        padding_[i]->setToolTip(padNames[i]);
        padLayout->addWidget(padding_[i]);
        connect(padding_[i], &QDoubleSpinBox::valueChanged, this, [this] { ApplyInspector(); });
    }
    labelPadding_ = new QLabel(Tr("Отступы", "Padding"), props); form->addRow(labelPadding_, padRow);

    borderWidth_ = new QDoubleSpinBox(props); borderWidth_->setObjectName("UIBorderWidth");
    borderWidth_->setRange(0, 1000); borderWidth_->setDecimals(2); borderWidth_->setKeyboardTracking(false);
    labelBorderWidth_ = new QLabel(Tr("Толщина рамки", "Border Width"), props); form->addRow(labelBorderWidth_, borderWidth_);
    connect(borderWidth_, &QDoubleSpinBox::valueChanged, this, [this] { ApplyInspector(); });

    sectionBehavior_ = Section(form, props, Tr("Поведение", "Behavior"));

    interactive_ = new QCheckBox(props); interactive_->setObjectName("UIInteractive");
    labelInteractive_ = new QLabel(Tr("Интерактивность", "Interactive"), props); form->addRow(labelInteractive_, interactive_);
    connect(interactive_, &QCheckBox::toggled, this, [this] { ApplyInspector(); });

    alignRight_ = new QCheckBox(props); alignRight_->setObjectName("UIAlignRight");
    labelAlignRight_ = new QLabel(Tr("Выравнивание вправо", "Align Right"), props); form->addRow(labelAlignRight_, alignRight_);
    connect(alignRight_, &QCheckBox::toggled, this, [this] { ApplyInspector(); });

    fill_ = new QDoubleSpinBox(props); fill_->setObjectName("UIFill");
    fill_->setRange(0, 1); fill_->setDecimals(3); fill_->setSingleStep(0.05); fill_->setKeyboardTracking(false);
    labelFill_ = new QLabel(Tr("Заполнение", "Fill"), props); form->addRow(labelFill_, fill_);
    connect(fill_, &QDoubleSpinBox::valueChanged, this, [this] { ApplyInspector(); });
}

void UIEditorWindow::RefreshInspector() {
    refreshing_ = true; const bool valid = selected_ != nullptr;
    for (auto* field : rect_) field->setEnabled(valid); name_->setEnabled(valid); anchor_->setEnabled(valid); visible_->setEnabled(valid); text_->setEnabled(false); colorProperty_->clear();
    for (auto* field : padding_) field->setEnabled(valid); interactive_->setEnabled(valid);
    borderWidth_->setEnabled(false); alignRight_->setEnabled(false); fill_->setEnabled(false);
    if (valid) {
        name_->setText(QString::fromStdString(selected_->Name)); const auto& r = selected_->Rect; const float values[] = {r.X, r.Y, r.Width, r.Height}; for (int i = 0; i < 4; ++i) rect_[i]->setValue(values[i]); anchor_->setCurrentIndex(int(r.Anchor)); visible_->setChecked(selected_->bVisible); text_->clear();
        const auto& pad = selected_->Padding; const float pads[] = {pad.Left, pad.Top, pad.Right, pad.Bottom}; for (int i = 0; i < 4; ++i) padding_[i]->setValue(pads[i]);
        interactive_->setChecked(selected_->bInteractive); borderWidth_->setValue(0); alignRight_->setChecked(false); fill_->setValue(0);
        if (auto* p = dynamic_cast<UILabel*>(selected_)) { text_->setEnabled(true); text_->setText(QString::fromStdString(p->GetText())); colorProperty_->addItems({Tr("Текст", "Text")}); alignRight_->setEnabled(true); alignRight_->setChecked(p->bAlignRight); }
        if (auto* p = dynamic_cast<UIButton*>(selected_)) { text_->setEnabled(true); text_->setText(QString::fromStdString(p->GetText())); colorProperty_->addItems({Tr("Фон", "Background"), Tr("Текст", "Text"), Tr("Наведение", "Hover"), Tr("Нажатие", "Pressed"), Tr("Рамка", "Border")}); borderWidth_->setEnabled(true); borderWidth_->setValue(p->BorderWidth); }
        if (auto* p = dynamic_cast<UIPanel*>(selected_)) { colorProperty_->addItems({Tr("Фон", "Background"), Tr("Рамка", "Border")}); borderWidth_->setEnabled(true); borderWidth_->setValue(p->BorderWidth); }
        if (auto* p = dynamic_cast<UIBar*>(selected_)) { colorProperty_->addItems({Tr("Заполнение", "Fill"), Tr("Фон", "Background")}); fill_->setEnabled(true); fill_->setValue(p->Fill); }
    }
    refreshing_ = false;
}

bool UIEditorWindow::ApplyInspector() {
    if (refreshing_ || !selected_) return true;
    const auto name = name_->text().toUtf8().toStdString(); auto* existing = root_->Find(name);
    if (!name.empty() && existing && existing != selected_) { status_->setText(Tr("Имя уже используется: ", "Name already in use: ") + name_->text()); return false; }
    const auto rootBefore = UIMarkup::Serialize(*root_);
    const auto before = UIMarkup::Serialize(*selected_);
    selected_->Name = name; selected_->Rect = {UIAnchor(anchor_->currentIndex()), float(rect_[0]->value()), float(rect_[1]->value()), float(rect_[2]->value()), float(rect_[3]->value())}; selected_->bVisible = visible_->isChecked();
    selected_->Padding = {float(padding_[0]->value()), float(padding_[1]->value()), float(padding_[2]->value()), float(padding_[3]->value())}; selected_->bInteractive = interactive_->isChecked();
    if (auto* p = dynamic_cast<UILabel*>(selected_)) { p->SetText(text_->text().toUtf8().toStdString()); p->bAlignRight = alignRight_->isChecked(); }
    if (auto* p = dynamic_cast<UIButton*>(selected_)) { p->SetText(text_->text().toUtf8().toStdString()); p->BorderWidth = float(borderWidth_->value()); }
    if (auto* p = dynamic_cast<UIPanel*>(selected_)) p->BorderWidth = float(borderWidth_->value());
    if (auto* p = dynamic_cast<UIBar*>(selected_)) p->Fill = float(fill_->value());
    if (before != UIMarkup::Serialize(*selected_)) { PushUndo(rootBefore); Changed(); RebuildTree(); }
    return true;
}