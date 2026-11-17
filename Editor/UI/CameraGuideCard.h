#pragma once
#include "CameraGuide.h"
#include "CollapsibleCard.h"
#include "CommandRegistry.h"
#include "EditorLocale.h"
#include "InfoButton.h"
#include "InspectorCardStyle.h"
#include "NoWheelDoubleSpinBox.h"
#include "SettingsService.h"
#include <QCheckBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>

class CameraGuideCard : public CollapsibleCard {
public:
  explicit CameraGuideCard(QWidget *parent = nullptr)
      : CollapsibleCard(Tr("Обзор камеры", "Camera view"), parent) {
    setObjectName("CameraGuideCard");

    SetInfo(
        Tr("Обзор камеры", "Camera view"),
        Tr("Справка по схеме камеры:\n• Зелёная рамка — ближняя плоскость Near clip.\n• Жёлтая рамка — срез схемы Guide slice.\n• Стрелка — направление взгляда.\n• Ортографический размер — размер вьюпорта.\n\nЕсли направление задано плоскостью, вращение объекта не влияет на взгляд.",
           "Camera guide help:\n• Green frame — near clip.\n• Amber frame — guide slice.\n• Arrow — view direction.\n• Orthographic size — viewport dimensions.\n\nWhen view plane overrides direction, object rotation does not affect view.")
    );
    if (auto* btn = InfoBtn()) {
      btn->setObjectName(QStringLiteral("CameraGuideInfo"));
    }

    enabled_ = new QCheckBox(
        Tr("Показывать камеры во вьюпорте", "Show cameras in viewport"),
        Content());
    enabled_->setObjectName("CameraGuidesEnabled");

    auto *form = new QFormLayout;
    InspectorCardStyle::ApplyRowLayout(form);
    depth_ = new NoWheelDoubleSpinBox(Content());
    depth_->setObjectName("CameraGuideDepth");
    depth_->setFixedHeight(InspectorCardStyle::kFieldHeight);
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
    depthLabel_ = new QLabel(Tr("Глубина схемы", "Guide depth"), Content());
    InspectorCardStyle::ApplyLabelStyle(depthLabel_);
    form->addRow(depthLabel_, depth_);

    full_ = new QCheckBox(Tr("Схема до дальней границы", "Draw to far clip"),
                          Content());
    full_->setObjectName("CameraGuideFull");
    xray_ = new QCheckBox(Tr("Поверх геометрии", "Draw through geometry"),
                          Content());
    xray_->setObjectName("CameraGuideXray");

    ContentLayout()->addWidget(enabled_);
    ContentLayout()->addLayout(form);
    ContentLayout()->addWidget(full_);
    ContentLayout()->addWidget(xray_);

    // Collapsible metrics block (collapsed by default)
    metricsCard_ = new CollapsibleCard(Tr("Параметры кадра", "Frame parameters"), Content(), false);
    metricsCard_->setObjectName(QStringLiteral("CameraGuideMetricsCard"));

    auto addMetricRow = [this](QLabel*& lbl, QLabel*& val, const QString& name, const QString& text) {
      auto* row = new QWidget(metricsCard_->Content());
      auto* l = new QHBoxLayout(row);
      InspectorCardStyle::ApplyRowLayout(l);
      lbl = new QLabel(text, row);
      InspectorCardStyle::ApplyLabelStyle(lbl);
      l->addWidget(lbl);
      val = new QLabel(QStringLiteral("—"), row);
      val->setObjectName(name);
      val->setTextInteractionFlags(Qt::TextSelectableByMouse);
      l->addWidget(val, 1);
      metricsCard_->ContentLayout()->addWidget(row);
    };

    addMetricRow(lblProjection_, valProjection_, QStringLiteral("CameraGuideProjection"), Tr("Проекция", "Projection"));
    addMetricRow(lblNearFar_, valNearFar_, QStringLiteral("CameraGuideNearFar"), QStringLiteral("Near / Far"));
    addMetricRow(lblFrame_, valFrame_, QStringLiteral("CameraGuideFrame"), Tr("Кадр", "Frame"));
    addMetricRow(lblTicks_, valTicks_, QStringLiteral("CameraGuideTicks"), Tr("Шаг отметок", "Ticks"));
    addMetricRow(lblObserver_, valObserver_, QStringLiteral("CameraGuideObserver"), Tr("От вида", "From view"));
    addMetricRow(lblPlane_, valPlane_, QStringLiteral("CameraGuidePlane"), Tr("До Y=0", "To Y=0"));
    if (lblPlane_ && lblPlane_->parentWidget()) lblPlane_->parentWidget()->hide();

    ContentLayout()->addWidget(metricsCard_);

    focus_ =
        new QPushButton(Tr("Найти камеру · F", "Frame camera · F"), Content());
    focus_->setObjectName("CameraGuideFocus");
    focus_->setFixedHeight(InspectorCardStyle::kButtonHeight);
    ContentLayout()->addWidget(focus_);
    connect(focus_, &QPushButton::clicked,
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
    langSubId_ = EditorLocale::Instance().SubscribeLanguageChanged([this] {
      RetranslateUi();
    });
  }

  ~CameraGuideCard() override {
    if (langSubId_) EditorLocale::Instance().UnsubscribeLanguageChanged(langSubId_);
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

  void RetranslateUi() {
    SetTitle(Tr("Обзор камеры", "Camera view"));
    SetInfo(
        Tr("Обзор камеры", "Camera view"),
        Tr("Справка по схеме камеры:\n• Зелёная рамка — ближняя плоскость Near clip.\n• Жёлтая рамка — срез схемы Guide slice.\n• Стрелка — направление взгляда.\n• Ортографический размер — размер вьюпорта.\n\nЕсли направление задано плоскостью, вращение объекта не влияет на взгляд.",
           "Camera guide help:\n• Green frame — near clip.\n• Amber frame — guide slice.\n• Arrow — view direction.\n• Orthographic size — viewport dimensions.\n\nWhen view plane overrides direction, object rotation does not affect view.")
    );
    if (auto* btn = InfoBtn()) {
      btn->setObjectName(QStringLiteral("CameraGuideInfo"));
    }
    if (enabled_)
      enabled_->setText(
          Tr("Показывать камеры во вьюпорте", "Show cameras in viewport"));
    if (full_)
      full_->setText(Tr("Схема до дальней границы", "Draw to far clip"));
    if (xray_)
      xray_->setText(Tr("Поверх геометрии", "Draw through geometry"));
    if (depth_) {
      depth_->setSuffix(Tr(" м", " m"));
      depth_->setToolTip(
          Tr("Расстояние, на котором показан размер кадра. Не "
             "меняет дальнюю границу камеры.",
             "Distance at which the frame is drawn. Does not "
             "change the camera far clip."));
    }
    if (depthLabel_)
      depthLabel_->setText(Tr("Глубина схемы", "Guide depth"));
    if (metricsCard_)
      metricsCard_->SetTitle(Tr("Параметры кадра", "Frame parameters"));
    if (lblProjection_) lblProjection_->setText(Tr("Проекция", "Projection"));
    if (lblNearFar_) lblNearFar_->setText(QStringLiteral("Near / Far"));
    if (lblTicks_) lblTicks_->setText(Tr("Шаг отметок", "Distance ticks"));
    if (lblObserver_) lblObserver_->setText(Tr("От вида", "From view"));
    if (lblPlane_) lblPlane_->setText(Tr("До Y=0", "To Y=0"));
    if (focus_)
      focus_->setText(Tr("Найти камеру · F", "Frame camera · F"));
    Describe();
  }

  CollapsibleCard* MetricsCard() const { return metricsCard_; }

private:
  void Describe() {
    const float end =
        full_->isChecked()
            ? view_.farClip
            : std::clamp(float(depth_->value()), view_.nearClip, view_.farClip);
    depth_->setEnabled(!full_->isChecked());

    if (valProjection_) {
      valProjection_->setText(QStringLiteral("%1, %2:1")
          .arg(view_.orthographic ? Tr("Ортографическая", "Orthographic") : Tr("Перспектива", "Perspective"))
          .arg(view_.aspect, 0, 'f', 2));
    }
    if (valNearFar_) {
      valNearFar_->setText(Tr("%1 / %2 м", "%1 / %2 m")
          .arg(view_.nearClip, 0, 'f', 3)
          .arg(view_.farClip, 0, 'f', 1));
    }
    if (lblFrame_) {
      lblFrame_->setText(Tr("Кадр на %1 м", "Frame at %1 m").arg(end, 0, 'f', 2));
    }
    if (valFrame_) {
      valFrame_->setText(Tr("%1 × %2 м", "%1 × %2 m")
          .arg(view_.Height(end) * view_.aspect, 0, 'f', 2)
          .arg(view_.Height(end), 0, 'f', 2));
    }
    if (valTicks_) {
      valTicks_->setText(Tr("%1 м", "%1 m").arg(end / 10, 0, 'f', 2));
    }
    if (valObserver_) {
      if (observerDistance_ >= 0) {
        valObserver_->setText(Tr("%1 м", "%1 m").arg(observerDistance_, 0, 'f', 2));
        if (lblObserver_ && lblObserver_->parentWidget()) lblObserver_->parentWidget()->show();
      } else {
        if (lblObserver_ && lblObserver_->parentWidget()) lblObserver_->parentWidget()->hide();
      }
    }
    if (valPlane_) {
      if (view_.forward.y < -.0001f && view_.position.y >= 0) {
        valPlane_->setText(Tr("%1 м", "%1 m").arg(-view_.position.y / view_.forward.y, 0, 'f', 2));
        if (lblPlane_ && lblPlane_->parentWidget()) lblPlane_->parentWidget()->show();
      } else {
        if (lblPlane_ && lblPlane_->parentWidget()) lblPlane_->parentWidget()->hide();
      }
    }
  }

  CameraGuide::View view_;
  float observerDistance_ = -1;
  QCheckBox *enabled_{nullptr}, *full_{nullptr}, *xray_{nullptr};
  NoWheelDoubleSpinBox *depth_{nullptr};
  QLabel *depthLabel_{nullptr};
  QPushButton *focus_{nullptr};

  CollapsibleCard *metricsCard_{nullptr};
  QLabel *lblProjection_{nullptr}, *valProjection_{nullptr};
  QLabel *lblNearFar_{nullptr}, *valNearFar_{nullptr};
  QLabel *lblFrame_{nullptr}, *valFrame_{nullptr};
  QLabel *lblTicks_{nullptr}, *valTicks_{nullptr};
  QLabel *lblObserver_{nullptr}, *valObserver_{nullptr};
  QLabel *lblPlane_{nullptr}, *valPlane_{nullptr};

  uint32_t langSubId_{0};
};
