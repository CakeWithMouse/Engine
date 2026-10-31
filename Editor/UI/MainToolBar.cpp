#include "MainToolBar.h"
#include "EditorLocale.h"
#include "EditorTheme.h"

MainToolBar::MainToolBar(QWidget* parent)
    : QToolBar(parent)
{
    setObjectName(QStringLiteral("MainToolBar"));
    setMovable(false);
    setFloatable(false);
    setFixedHeight(32);
    setIconSize(QSize(16, 16));
    setToolButtonStyle(Qt::ToolButtonIconOnly);

    auto* leftSpacer = new QWidget(this);
    leftSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    addWidget(leftSpacer);

    actPlay_ = addAction(Tr("Play", "Play"));
    actPause_ = addAction(Tr("Пауза", "Pause"));
    actStop_ = addAction(Tr("Стоп", "Stop"));
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
    configPlayBtn_->setIconSize(QSize(16, 16));
    configPlayBtn_->setFixedSize(24, 24);
    configPlayBtn_->setAutoRaise(true);
    configPlayBtn_->setFocusPolicy(Qt::NoFocus);
    connect(configPlayBtn_, &QToolButton::clicked, this, &MainToolBar::ConfigPlayTriggered);
    addWidget(configPlayBtn_);

    auto* rightSpacer = new QWidget(this);
    rightSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    addWidget(rightSpacer);

    ApplyTheme();
    RefreshIcons();
    RetranslateUi();

    themeSubId_ = EditorTheme::Instance().SubscribeThemeChanged([this] {
        ApplyTheme();
        RefreshIcons();
    });
}

MainToolBar::~MainToolBar()
{
    if (themeSubId_)
    {
        EditorTheme::Instance().UnsubscribeThemeChanged(themeSubId_);
        themeSubId_ = 0;
    }
}

void MainToolBar::ApplyTheme()
{
    const auto p = EditorTheme::Instance().Palette();
    setStyleSheet(QStringLiteral(
        "QToolBar#MainToolBar {"
        "  background-color: %1;"
        "  border: none;"
        "  border-bottom: 1px solid %2;"
        "  spacing: 3px;"
        "  padding: 3px 4px;"
        "}"
        "QToolBar#MainToolBar QToolButton {"
        "  background-color: transparent;"
        "  border: 1px solid transparent;"
        "  border-radius: 3px;"
        "  padding: 0px;"
        "  min-width: 24px;"
        "  max-width: 24px;"
        "  min-height: 24px;"
        "  max-height: 24px;"
        "}"
        "QToolBar#MainToolBar QToolButton:hover {"
        "  background-color: %3;"
        "  border-color: %2;"
        "}"
        "QToolBar#MainToolBar QToolButton:pressed {"
        "  background-color: %4;"
        "}"
        "QToolBar#MainToolBar QToolButton:disabled {"
        "  background-color: transparent;"
        "  border-color: transparent;"
        "}"
        "QToolBar#MainToolBar::separator {"
        "  width: 1px;"
        "  background-color: %2;"
        "  margin: 4px 3px;"
        "}"
    ).arg(
        p.bgPanel.name(),
        p.borderLight.name(),
        p.bgButtonHover.name(),
        p.bgButton.name()
    ));
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
    if (actPlay_) {
        actPlay_->setText(Tr("Play", "Play"));
        actPlay_->setToolTip(Tr("Запуск (Play)", "Play"));
    }
    if (actPause_) {
        actPause_->setText(Tr("Пауза", "Pause"));
        actPause_->setToolTip(Tr("Пауза (Pause)", "Pause"));
    }
    if (actStop_) {
        actStop_->setText(Tr("Стоп", "Stop"));
        actStop_->setToolTip(Tr("Остановить (Stop)", "Stop"));
    }
    if (actExternal_) {
        actExternal_->setText(Tr("Отдельное окно", "External"));
        actExternal_->setToolTip(Tr("Запуск в отдельном окне", "Run in External Window"));
    }
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
    if (configPlayBtn_) configPlayBtn_->setIcon(EditorTheme::CreateSettingsIcon());
}
