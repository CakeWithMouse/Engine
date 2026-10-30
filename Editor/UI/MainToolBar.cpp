#include "MainToolBar.h"
#include "EditorLocale.h"
#include "EditorTheme.h"

MainToolBar::MainToolBar(QWidget* parent)
    : QToolBar(parent)
{
    setObjectName(QStringLiteral("MainToolBar"));
    setMovable(false);
    setFloatable(false);

    auto* leftSpacer = new QWidget(this);
    leftSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    addWidget(leftSpacer);

    actPlay_ = addAction(Tr("Play", "Play"));
    actPause_ = addAction(Tr("Pause", "Pause"));
    actStop_ = addAction(Tr("Stop", "Stop"));
    actExternal_ = addAction(Tr("Отдельное окно", "External"));

    actPlay_->setObjectName(QStringLiteral("ActPlay"));
    actPause_->setObjectName(QStringLiteral("ActPause"));
    actStop_->setObjectName(QStringLiteral("ActStop"));
    actExternal_->setObjectName(QStringLiteral("ActExternal"));

    connect(actPlay_, &QAction::triggered, this, &MainToolBar::PlayTriggered);
    connect(actPause_, &QAction::triggered, this, &MainToolBar::PauseTriggered);
    connect(actStop_, &QAction::triggered, this, &MainToolBar::StopTriggered);
    connect(actExternal_, &QAction::triggered, this, &MainToolBar::ExternalTriggered);

    addSeparator();

    configPlayBtn_ = new QToolButton(this);
    configPlayBtn_->setObjectName(QStringLiteral("ConfigPlayBtn"));
    configPlayBtn_->setText(QStringLiteral("⚙"));
    configPlayBtn_->setToolTip(Tr("Детальные настройки Play…", "Detailed Play Settings…"));
    connect(configPlayBtn_, &QToolButton::clicked, this, &MainToolBar::ConfigPlayTriggered);
    addWidget(configPlayBtn_);

    auto* rightSpacer = new QWidget(this);
    rightSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    addWidget(rightSpacer);
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

    if (configPlayBtn_) {
        configPlayBtn_->setToolTip(Tr("Детальные настройки Play…", "Detailed Play Settings…"));
    }
}

void MainToolBar::RefreshIcons()
{
    if (actPlay_) actPlay_->setIcon(EditorTheme::CreatePlayIcon());
    if (actPause_) actPause_->setIcon(EditorTheme::CreatePauseIcon());
    if (actStop_) actStop_->setIcon(EditorTheme::CreateStopIcon());
    if (actExternal_) actExternal_->setIcon(EditorTheme::CreateExternalIcon());
}
