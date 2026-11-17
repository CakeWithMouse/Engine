#include "MaterialEditorWindow.h"

#include "MaterialGraphPresets.h"
#include "MaterialGraphTransform.h"
#include "MaterialGraphWidget.h"
#include "EditorLocale.h"

#include <QMenu>
#include <QMessageBox>
#include <QToolBar>
#include <QToolButton>
#include <QAction>
#include <QStatusBar>

// Шаблоны материала (Simple/ORM) и команды графа, вынесенные из MaterialEditorWindow.cpp:
// работают с graph_ через публичный API, не меняя размер самого окна.

void MaterialEditorWindow::BuildGraphTools(QToolBar* tools)
{
    templates_ = new QToolButton(this);
    templates_->setObjectName(QStringLiteral("MaterialTemplates"));
    templates_->setIcon(IconProvider::TemplateIcon());
    templates_->setIconSize(QSize(16, 16));
    templates_->setToolTip(Tr("Шаблоны материала: Simple, ORM, базовые эффекты",
                              "Material templates: Simple, ORM, base effects"));
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
    menu->addAction(Tr("Simple — слоты текстур", "Simple — texture slots"), this, [this, apply] { apply(0); });
    menu->addAction(Tr("ORM — упакованная текстура AO/Roughness/Metallic", "ORM — packed texture AO/Roughness/Metallic"),
                    this, [this, apply] { apply(1); });
    menu->addSeparator();
    menu->addAction(Tr("Textured Color — базовая текстура", "Textured Color — base texture"), this, [this, apply] { apply(2); });
    menu->addAction(Tr("Animated Neon — неоновая пульсация", "Animated Neon — pulsing neon"), this, [this, apply] { apply(3); });
    menu->addAction(Tr("Gold — золотой металл с бликом", "Gold — golden metal with rim"), this, [this, apply] { apply(4); });
    menu->addAction(Tr("Scrolling Energy — бегущая текстура", "Scrolling Energy — scrolling energy"), this, [this, apply] { apply(5); });
    templates_->setMenu(menu);
    tools->addWidget(templates_);

    exposeAction_ = tools->addAction(IconProvider::VariableIcon(), QString(), this,
                                     [this] { ExposeSelectedAsParameter(); });
    exposeAction_->setObjectName(QStringLiteral("MaterialExposeParam"));
    exposeAction_->setToolTip(Tr("Превратить выбранную ноду Constant в параметр материала без перекомпиляции",
                                 "Turn the selected Constant node into a material parameter without recompiling"));
    exposeAction_->setEnabled(false); // до загрузки HLSL
}

void MaterialEditorWindow::ApplyTemplate(int kind)
{
    if (!loaded_ || !sourceLoaded_ || saving_) return;
    BackupGraphFiles();
    if (kind == 0)
        graph_->SetModel(EditorCore::MaterialGraphPresets::BuildSimple());
    else if (kind == 1)
        graph_->SetModel(EditorCore::MaterialGraphPresets::BuildOrm());
    else
        graph_->LoadPreset(kind - 2);
    if (!graphMode_) SetGraphMode(true);
    ApplyGraph();
}

void MaterialEditorWindow::ExposeSelectedAsParameter()
{
    if (!loaded_ || !sourceLoaded_ || saving_) return;
    if (!graphMode_)
    {
        statusBar()->showMessage(Tr("Сначала перейдите в нодовый граф", "Switch to the node graph first"));
        return;
    }
    const QList<int> ids = graph_->SelectedNodes();
    if (!EditorCore::MaterialGraphTransform::CanExpose(graph_->Model(), ids))
    {
        statusBar()->showMessage(Tr("Выделите ноду Constant, чтобы выставить её параметром",
                                    "Select a Constant node to expose it as a parameter"));
        return;
    }
    BackupGraphFiles();
    graph_->SetModel(EditorCore::MaterialGraphTransform::ExposeConstants(graph_->Model(), ids));
    ApplyGraph();
    statusBar()->showMessage(Tr("Константа стала параметром: правится без перекомпиляции",
                                "Constant exposed as a parameter: editable without recompile"));
}