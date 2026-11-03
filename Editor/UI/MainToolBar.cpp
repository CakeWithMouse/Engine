#include <QTimer>
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

    btnPlay_ = new QToolButton(container);
    btnPlay_->setObjectName(QStringLiteral("BtnPlay"));
    btnPlay_->setFixedSize(28, 28);
    btnPlay_->setAutoRaise(true);
    btnPlay_->setFocusPolicy(Qt::NoFocus);

    btnPause_ = new QToolButton(container);
    btnPause_->setObjectName(QStringLiteral("BtnPause"));
    btnPause_->setFixedSize(28, 28);
    btnPause_->setAutoRaise(true);
    btnPause_->setFocusPolicy(Qt::NoFocus);

    btnStop_ = new QToolButton(container);
    btnStop_->setObjectName(QStringLiteral("BtnStop"));
    btnStop_->setFixedSize(28, 28);
    btnStop_->setAutoRaise(true);
    btnStop_->setFocusPolicy(Qt::NoFocus);

    btnExternal_ = new QToolButton(container);
    btnExternal_->setObjectName(QStringLiteral("BtnExternal"));
    btnExternal_->setFixedSize(28, 28);
    btnExternal_->setAutoRaise(true);
    btnExternal_->setFocusPolicy(Qt::NoFocus);

    auto* btnPlay = btnPlay_;
    auto* btnPause = btnPause_;
    auto* btnStop = btnStop_;
    auto* btnExternal = btnExternal_;

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

    RetranslateUi();
    Apply();

    themeSubId_ = EditorTheme::Instance().SubscribeThemeChanged([this] {
        Apply();
    });

    QTimer::singleShot(0, this, [this] {
        Apply();
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
        "QToolBar#MainToolBar QToolButton#BtnPlay:hover {"
        "  background-color: rgba(34, 197, 94, 0.2);"
        "  border-color: rgba(34, 197, 94, 0.6);"
        "}"
        "QToolBar#MainToolBar QToolButton#BtnPlay[activeState=\"playing\"] {"
        "  background-color: rgba(34, 197, 94, 0.28);"
        "  border: 1px solid #22c55e;"
        "}"
        "QToolBar#MainToolBar QToolButton#BtnPause:hover {"
        "  background-color: rgba(234, 179, 8, 0.2);"
        "  border-color: rgba(234, 179, 8, 0.6);"
        "}"
        "QToolBar#MainToolBar QToolButton#BtnPause[activeState=\"paused\"] {"
        "  background-color: rgba(234, 179, 8, 0.28);"
        "  border: 1px solid #eab308;"
        "}"
        "QToolBar#MainToolBar QToolButton#BtnStop:hover {"
        "  background-color: rgba(239, 68, 68, 0.2);"
        "  border-color: rgba(239, 68, 68, 0.6);"
        "}"
        "QToolBar#MainToolBar QToolButton#BtnStop:pressed {"
        "  background-color: rgba(239, 68, 68, 0.35);"
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

    if (btnPlay_)
    {
        btnPlay_->setProperty("activeState", state == 1 ? "playing" : "");
        btnPlay_->style()->unpolish(btnPlay_);
        btnPlay_->style()->polish(btnPlay_);
    }
    if (btnPause_)
    {
        btnPause_->setProperty("activeState", state == 2 ? "paused" : "");
        btnPause_->style()->unpolish(btnPause_);
        btnPause_->style()->polish(btnPause_);
    }
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

void MainToolBar::Apply()
{
    ApplyTheme();
    RefreshIcons();
    setContentsMargins(0, 0, 0, 0);
    if (layout())
    {
        layout()->setContentsMargins(0, 0, 0, 0);
        layout()->setSpacing(0);
    }
}
