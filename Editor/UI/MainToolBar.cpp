#include "MainToolBar.h"
#include "EditorTheme.h"
#include "EditorLocale.h"
#include <QSizePolicy>

MainToolBar::MainToolBar(QWidget* parent)
    : QToolBar(Tr("Запуск", "Play"), parent)
{
    setObjectName(QStringLiteral("MainToolBar"));
    setMovable(false);

    auto* leftSpacer = new QWidget(this);
    leftSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    addWidget(leftSpacer);

    actPlay_ = addAction(EditorTheme::CreatePlayIcon(), Tr("Play", "Play"));
    actPause_ = addAction(EditorTheme::CreatePauseIcon(), Tr("Pause", "Pause"));
    actStop_ = addAction(EditorTheme::CreateStopIcon(), Tr("Stop", "Stop"));
    actExternal_ = addAction(EditorTheme::CreateExternalIcon(), Tr("Отдельное окно", "External"));

    actPlay_->setObjectName(QStringLiteral("ActPlay"));
    actPause_->setObjectName(QStringLiteral("ActPause"));
    actStop_->setObjectName(QStringLiteral("ActStop"));
    actExternal_->setObjectName(QStringLiteral("ActExternal"));

    connect(actPlay_, &QAction::triggered, this, &MainToolBar::PlayTriggered);
    connect(actPause_, &QAction::triggered, this, &MainToolBar::PauseTriggered);
    connect(actStop_, &QAction::triggered, this, &MainToolBar::StopTriggered);
    connect(actExternal_, &QAction::triggered, this, &MainToolBar::ExternalTriggered);

    addSeparator();

    cameraModeCombo_ = new QComboBox(this);
    cameraModeCombo_->setObjectName(QStringLiteral("PlayCameraMode"));
    cameraModeCombo_->addItem(Tr("Свободная камера", "Free Camera"), 0);
    cameraModeCombo_->addItem(Tr("Камера игрока", "Main Camera"), 1);
    cameraModeCombo_->setToolTip(Tr("Режим камеры во время Play: свободный полёт или следование за персонажем",
                                    "Camera mode during Play: free flight or follow character"));
    connect(cameraModeCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        emit CameraModeChanged(cameraModeCombo_->itemData(index).toInt());
    });
    addWidget(cameraModeCombo_);

    configPlayBtn_ = new QToolButton(this);
    configPlayBtn_->setObjectName(QStringLiteral("ConfigPlayBtn"));
    configPlayBtn_->setText(QStringLiteral("⚙"));
    configPlayBtn_->setToolTip(Tr("Детальные настройки Play…", "Detailed Play Settings…"));
    connect(configPlayBtn_, &QToolButton::clicked, this, &MainToolBar::ConfigPlayTriggered);
    addWidget(configPlayBtn_);

    auto* rightSpacer = new QWidget(this);
    rightSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    addWidget(rightSpacer);

    langButton_ = new QPushButton(this);
    langButton_->setObjectName(QStringLiteral("LangToggleBtn"));
    langButton_->setFixedWidth(40);
    langButton_->setText(EditorLocale::Instance().Language() == EditorLanguage::Ru ? QStringLiteral("RU") : QStringLiteral("EN"));
    langButton_->setToolTip(Tr("Переключить язык интерфейса (RU/EN)", "Toggle UI language (RU/EN)"));
    connect(langButton_, &QPushButton::clicked, this, [this] {
        if (EditorLocale::Instance().Language() == EditorLanguage::Ru) {
            EditorLocale::Instance().SetLanguage(EditorLanguage::En);
        } else {
            EditorLocale::Instance().SetLanguage(EditorLanguage::Ru);
        }
        emit LangToggleTriggered();
    });
    addWidget(langButton_);
}

void MainToolBar::UpdatePlayState(int state, bool sceneBusy)
{
    // state: 0 = Edit, 1 = Play, 2 = Pause
    if (actExternal_) actExternal_->setEnabled(!sceneBusy);
    if (actPlay_) actPlay_->setEnabled(!sceneBusy && (state == 0 || state == 2));
    if (actPause_) actPause_->setEnabled(state == 1);
    if (actStop_) actStop_->setEnabled(state != 0);
}

void MainToolBar::RetranslateUi()
{
    setWindowTitle(Tr("Запуск", "Play"));
    if (actPlay_) actPlay_->setText(Tr("Play", "Play"));
    if (actPause_) actPause_->setText(Tr("Pause", "Pause"));
    if (actStop_) actStop_->setText(Tr("Stop", "Stop"));
    if (actExternal_) actExternal_->setText(Tr("Отдельное окно", "External"));

    if (cameraModeCombo_) {
        const int cur = cameraModeCombo_->currentIndex();
        cameraModeCombo_->blockSignals(true);
        cameraModeCombo_->clear();
        cameraModeCombo_->addItem(Tr("Свободная камера", "Free Camera"), 0);
        cameraModeCombo_->addItem(Tr("Камера игрока", "Main Camera"), 1);
        cameraModeCombo_->setCurrentIndex(cur >= 0 ? cur : 0);
        cameraModeCombo_->setToolTip(Tr("Режим камеры во время Play: свободный полёт или следование за персонажем",
                                        "Camera mode during Play: free flight or follow character"));
        cameraModeCombo_->blockSignals(false);
    }
    if (configPlayBtn_) {
        configPlayBtn_->setToolTip(Tr("Детальные настройки Play…", "Detailed Play Settings…"));
    }
    if (langButton_) {
        langButton_->setText(EditorLocale::Instance().Language() == EditorLanguage::Ru ? QStringLiteral("RU") : QStringLiteral("EN"));
        langButton_->setToolTip(Tr("Переключить язык интерфейса (RU/EN)", "Toggle UI language (RU/EN)"));
    }
}

void MainToolBar::RefreshIcons()
{
    if (actPlay_) actPlay_->setIcon(EditorTheme::CreatePlayIcon());
    if (actPause_) actPause_->setIcon(EditorTheme::CreatePauseIcon());
    if (actStop_) actStop_->setIcon(EditorTheme::CreateStopIcon());
    if (actExternal_) actExternal_->setIcon(EditorTheme::CreateExternalIcon());
}

