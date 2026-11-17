#include "MaterialEditorWindow.h"

#include "MaterialGraphPresets.h"
#include "MaterialGraphWidget.h"
#include "EditorLocale.h"

#include <QMenu>
#include <QMessageBox>
#include <QToolBar>
#include <QToolButton>

// Шаблоны материала (Simple/ORM) и команды графа, вынесенные из MaterialEditorWindow.cpp:
// работают с graph_ через публичный API, не меняя размер самого окна.

void MaterialEditorWindow::BuildGraphTools(QToolBar* tools)
{
    templates_ = new QToolButton(this);
    templates_->setObjectName(QStringLiteral("MaterialTemplates"));
    templates_->setText(Tr("Шаблоны", "Templates"));
    templates_->setToolTip(Tr("Создать материал из готового графа: Simple — плоский цвет, ORM — упакованная текстура",
                              "Create a material from a preset graph: Simple — flat colour, ORM — packed texture"));
    templates_->setPopupMode(QToolButton::InstantPopup);
    templates_->setEnabled(false); // до загрузки HLSL

    auto* menu = new QMenu(templates_);
    menu->setObjectName(QStringLiteral("MaterialTemplateMenu"));
    auto apply = [this](int kind) {
        if (QMessageBox::question(this, Tr("Шаблон материала", "Material template"),
                Tr("Заменить текущий материал графом шаблона? До сохранения это отменяется кнопкой «Откатить».",
                   "Replace the current material with the template graph? Revert undoes this before saving.")) == QMessageBox::Yes)
            ApplyTemplate(kind);
    };
    menu->addAction(Tr("Simple — плоский цвет", "Simple — flat colour"), this, [this, apply] { apply(0); });
    menu->addAction(Tr("ORM — упакованная текстура AO/Roughness/Metallic", "ORM — packed texture AO/Roughness/Metallic"),
                    this, [this, apply] { apply(1); });
    templates_->setMenu(menu);
    tools->addWidget(templates_);
}

void MaterialEditorWindow::ApplyTemplate(int kind)
{
    if (!loaded_ || !sourceLoaded_ || saving_) return;
    BackupGraphFiles();
    graph_->SetModel(kind == 0 ? EditorCore::MaterialGraphPresets::BuildSimple()
                               : EditorCore::MaterialGraphPresets::BuildOrm());
    if (!graphMode_) SetGraphMode(true);
    ApplyGraph();
}