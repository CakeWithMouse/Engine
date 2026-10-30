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

    actPlay_ = addAction(QStringLiteral("Play"));
    actPause_ = addAction(QStringLiteral("Pause"));
    actStop_ = addAction(QStringLiteral("Stop"));
    actExternal_ = addAction(QStringLiteral("External"));

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
