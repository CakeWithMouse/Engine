#include "MainToolBar.h"
#include "EditorLocale.h"
#include "EditorTheme.h"
#include <QHBoxLayout>
#include <QFrame>
#include <QStyle>
#include <cstdio>
#include <cmath>

MainToolBar::MainToolBar(QWidget* parent)
    : QToolBar(parent)
{
    setObjectName(QStringLiteral("MainToolBar"));
    setMovable(false);
    setFloatable(false);
    setFixedHeight(36);
    setIconSize(QSize(16, 16));
    setToolButtonStyle(Qt::ToolButtonIconOnly);

    const int pmFrameWidth = style() ? style()->pixelMetric(QStyle::PM_ToolBarFrameWidth, nullptr, this) : 0;
    const int pmItemMargin = style() ? style()->pixelMetric(QStyle::PM_ToolBarItemMargin, nullptr, this) : 0;
    const int pmItemSpacing = style() ? style()->pixelMetric(QStyle::PM_ToolBarItemSpacing, nullptr, this) : 0;
    const auto cm = contentsMargins();
    const auto lm = layout() ? layout()->contentsMargins() : QMargins{};
    std::printf("[ToolBarFacts] initial tb geom=%d,%d %dx%d, margins=(%d,%d,%d,%d), layoutMargins=(%d,%d,%d,%d), PM_ToolBarFrameWidth=%d, PM_ItemMargin=%d, PM_ItemSpacing=%d\n",
        geometry().x(), geometry().y(), geometry().width(), geometry().height(),
        cm.left(), cm.top(), cm.right(), cm.bottom(),
        lm.left(), lm.top(), lm.right(), lm.bottom(),
        pmFrameWidth, pmItemMargin, pmItemSpacing);

    if (layout())
    {
        layout()->setContentsMargins(0, 0, 0, 0);
        layout()->setSpacing(0);
    }

    auto* container = new QFrame(this);
    container->setObjectName(QStringLiteral("ToolBarContainer"));
    container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    container->setFixedHeight(36);

    auto* rootLayout = new QHBoxLayout(container);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(4);
    rootLayout->setAlignment(Qt::AlignCenter);

    auto* btnPlay = new QToolButton(container);
    btnPlay->setObjectName(QStringLiteral("BtnPlay"));
    btnPlay->setFixedSize(28, 28);
    btnPlay->setAutoRaise(true);
    btnPlay->setFocusPolicy(Qt::NoFocus);

    auto* btnPause = new QToolButton(container);
    btnPause->setObjectName(QStringLiteral("BtnPause"));
    btnPause->setFixedSize(28, 28);
    btnPause->setAutoRaise(true);
    btnPause->setFocusPolicy(Qt::NoFocus);

    auto* btnStop = new QToolButton(container);
    btnStop->setObjectName(QStringLiteral("BtnStop"));
    btnStop->setFixedSize(28, 28);
    btnStop->setAutoRaise(true);
    btnStop->setFocusPolicy(Qt::NoFocus);

    auto* btnExternal = new QToolButton(container);
    btnExternal->setObjectName(QStringLiteral("BtnExternal"));
    btnExternal->setFixedSize(28, 28);
    btnExternal->setAutoRaise(true);
    btnExternal->setFocusPolicy(Qt::NoFocus);

    actPlay_ = new QAction(Tr("Play", "Play"), this);
    actPlay_->setObjectName(QStringLiteral("ActPlay"));
    btnPlay->setDefaultAction(actPlay_);

    actPause_ = new QAction(Tr("Пауза", "Pause"), this);
    actPause_->setObjectName(QStringLiteral("ActPause"));
    btnPause->setDefaultAction(actPause_);

    actStop_ = new QAction(Tr("Стоп", "Stop"), this);
    actStop_->setObjectName(QStringLiteral("ActStop"));
    btnStop->setDefaultAction(actStop_);

    actExternal_ = new QAction(Tr("Отдельное окно", "External"), this);
    actExternal_->setObjectName(QStringLiteral("ActExternal"));
    btnExternal->setDefaultAction(actExternal_);

    connect(actPlay_, &QAction::triggered, this, &MainToolBar::PlayTriggered);
    connect(actPause_, &QAction::triggered, this, &MainToolBar::PauseTriggered);
    connect(actStop_, &QAction::triggered, this, &MainToolBar::StopTriggered);
    connect(actExternal_, &QAction::triggered, this, &MainToolBar::ExternalTriggered);

    rootLayout->addWidget(btnPlay);
    rootLayout->addWidget(btnPause);
    rootLayout->addWidget(btnStop);
    rootLayout->addWidget(btnExternal);

    addWidget(container);

    ApplyTheme();
    RefreshIcons();
    RetranslateUi();

    themeSubId_ = EditorTheme::Instance().SubscribeThemeChanged([this] {
        ApplyTheme();
        RefreshIcons();
    });

    std::printf("[ToolBarFacts] configured tb height=36, btn height=28, topMargin=%d, bottomMargin=%d\n",
        (36 - 28) / 2, 36 - 28 - (36 - 28) / 2);
    std::fflush(stdout);
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
        "  margin: 0px;"
        "  padding: 0px;"
        "}"
        "QFrame#ToolBarContainer {"
        "  background: transparent;"
        "  border: none;"
        "  margin: 0px;"
        "  padding: 0px;"
        "}"
        "QToolBar#MainToolBar QToolButton {"
        "  background-color: transparent;"
        "  border: 1px solid transparent;"
        "  border-radius: 4px;"
        "  padding: 0px;"
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
        "QFrame#ToolBarSep {"
        "  background-color: %2;"
        "  border: none;"
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
}

void MainToolBar::RefreshIcons()
{
    if (actPlay_) actPlay_->setIcon(EditorTheme::CreatePlayIcon());
    if (actPause_) actPause_->setIcon(EditorTheme::CreatePauseIcon());
    if (actStop_) actStop_->setIcon(EditorTheme::CreateStopIcon());
    if (actExternal_) actExternal_->setIcon(EditorTheme::CreateExternalIcon());
}
