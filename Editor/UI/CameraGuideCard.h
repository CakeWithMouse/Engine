#pragma once
#include "CameraGuide.h"
#include "CollapsibleCard.h"
#include "CommandRegistry.h"
#include "EditorLocale.h"
#include "NoWheelDoubleSpinBox.h"
#include "SettingsService.h"
#include <QCheckBox>
#include <QFormLayout>
#include <QLabel>
#include <QSignalBlocker>

class CameraGuideCard : public CollapsibleCard {
public:
  explicit CameraGuideCard(QWidget *parent = nullptr)
      : CollapsibleCard(Tr("Обзор камеры", "Camera view"), parent) {
    setObjectName("CameraGuideCard");
    enabled_ = new QCheckBox(
        Tr("Показывать камеры во вьюпорте", "Show cameras in viewport"),
        Content());
    enabled_->setObjectName("CameraGuidesEnabled");
    full_ = new QCheckBox(Tr("Схема до дальней границы", "Draw to far clip"),
                          Content());
    full_->setObjectName("CameraGuideFull");
    xray_ = new QCheckBox(Tr("Поверх геометрии", "Draw through geometry"),
                          Content());
    xray_->setObjectName("CameraGuideXray");
    auto *form = new QFormLayout;
    depth_ = new NoWheelDoubleSpinBox(Content());
    depth_->setObjectName("CameraGuideDepth");
    depth_->setRange(.01, 1000000);
    depth_->setDecimals(2);
    depth_->setSuffix(Tr(" м", " m"));
    depth_->setValue(SettingsService::Instance()
                         .GetValue("view/cameraDepth", 10.)
                         .toDouble());
    depth_->setToolTip(Tr("Расстояние, на котором показан размер кадра. Не "
                          "меняет дальнюю границу камеры.",
                          "Distance at which the frame is drawn. Does not "
                          "change the camera far clip."));
    form->addRow(Tr("Глубина схемы", "Guide depth"), depth_);
    ContentLayout()->addWidget(enabled_);
    ContentLayout()->addLayout(form);
    ContentLayout()->addWidget(full_);
    ContentLayout()->addWidget(xray_);
    summary_ = new QLabel(Content());
    summary_->setObjectName("CameraGuideSummary");
    summary_->setWordWrap(true);
    summary_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    ContentLayout()->addWidget(summary_);
    auto *focus =
        new QPushButton(Tr("Найти камеру · F", "Frame camera · F"), Content());
    focus->setObjectName("CameraGuideFocus");
    ContentLayout()->addWidget(focus);
    connect(focus, &QPushButton::clicked,
            [] { CommandRegistry::Instance().Execute("Gizmo.FocusSelected"); });
    auto toggle = [this](QCheckBox *box, const char *key, bool fallback) {
      box->setChecked(
          SettingsService::Instance().GetValue(key, fallback).toBool());
      connect(box, &QCheckBox::toggled, this, [this, key](bool value) {
        SettingsService::Instance().SetValue(key, value);
        Describe();
      });
    };
    toggle(enabled_, "view/cameras", true);
    toggle(full_, "view/cameraFull", false);
    toggle(xray_, "view/cameraXray", false);
    connect(depth_, &QDoubleSpinBox::editingFinished, this, [this] {
      SettingsService::Instance().SetValue("view/cameraDepth", depth_->value());
      Describe();
    });
  }
  void Refresh(const IEditorHost::SceneObject &object,
               const IEditorHost::Snapshot &scene,
               const EngineCameraInfo *editor) {
    view_ = CameraGuide::Read(object, scene,
                              editor && editor->height
                                  ? float(editor->width) / editor->height
                                  : 16.f / 9.f);
    observerDistance_ = -1;
    if (editor)
      observerDistance_ = glm::distance(
          view_.position, glm::vec3(editor->position[0], editor->position[1],
                                    editor->position[2]));
    {
      QSignalBlocker block(enabled_);
      enabled_->setChecked(
          SettingsService::Instance().GetValue("view/cameras", true).toBool());
    }
    Describe();
  }

private:
  void Describe() {
    const float end =
        full_->isChecked()
            ? view_.farClip
            : std::clamp(float(depth_->value()), view_.nearClip, view_.farClip);
    depth_->setEnabled(!full_->isChecked());
    QString text =
        Tr("%1 · формат вьюпорта %2:1\nБлижняя / дальняя: %3 / %4 м\nКадр на "
           "%5 м: %6 × %7 м\nШаг отметок: %8 м",
           "%1 · viewport aspect %2:1\nNear / far: %3 / %4 m\nFrame at %5 m: "
           "%6 × %7 m\nDistance ticks: %8 m")
            .arg(view_.orthographic ? Tr("Ортографическая", "Orthographic")
                                    : Tr("Перспектива", "Perspective"))
            .arg(view_.aspect, 0, 'f', 2)
            .arg(view_.nearClip, 0, 'f', 3)
            .arg(view_.farClip, 0, 'f', 1)
            .arg(end, 0, 'f', 2)
            .arg(view_.Height(end) * view_.aspect, 0, 'f', 2)
            .arg(view_.Height(end), 0, 'f', 2)
            .arg(end / 10, 0, 'f', 2);
    if (observerDistance_ >= 0)
      text += Tr("\nОт вида редактора: %1 м", "\nFrom editor view: %1 m")
                  .arg(observerDistance_, 0, 'f', 2);
    if (view_.forward.y < -.0001f && view_.position.y >= 0)
      text += Tr("\nДо плоскости Y=0 по взгляду: %1 м",
                 "\nAlong view to plane Y=0: %1 m")
                  .arg(-view_.position.y / view_.forward.y, 0, 'f', 2);
    text += Tr(
        "\nЗелёная рамка — Near. Жёлтая — срез схемы. Стрелка — направление "
        "взгляда.",
        "\nGreen frame: near clip. Amber: guide slice. Arrow: view direction.");
    if (view_.plane)
      text += Tr("\nНаправление задано плоскостью; поворот объекта не влияет "
                 "на взгляд.",
                 "\nView plane overrides the object's rotation.");
    summary_->setText(text);
  }
  CameraGuide::View view_;
  float observerDistance_ = -1;
  QCheckBox *enabled_, *full_, *xray_;
  NoWheelDoubleSpinBox *depth_;
  QLabel *summary_;
};
