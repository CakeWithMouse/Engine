#pragma once

#include <QToolBar>
#include <QAction>
#include <QPushButton>
#include <QToolButton>

class MainToolBar : public QToolBar
{
    Q_OBJECT

public:
    explicit MainToolBar(QWidget* parent = nullptr);
    ~MainToolBar() override;

    QAction* ActPlay() const { return actPlay_; }
    QAction* ActPause() const { return actPause_; }
    QAction* ActStop() const { return actStop_; }
    QAction* ActExternal() const { return actExternal_; }

    void UpdatePlayState(int state, bool sceneBusy);
    void RetranslateUi();
    void RefreshIcons();
    void ApplyTheme();
    void Apply();

signals:
    void PlayTriggered();
    void PauseTriggered();
    void StopTriggered();
    void ExternalTriggered();
    void ConfigPlayTriggered();

private:
    QAction* actPlay_ = nullptr;
    QAction* actPause_ = nullptr;
    QAction* actStop_ = nullptr;
    QAction* actExternal_ = nullptr;
    QToolButton* btnPlay_ = nullptr;
    QToolButton* btnPause_ = nullptr;
    QToolButton* btnStop_ = nullptr;
    QToolButton* btnExternal_ = nullptr;
    QToolButton* configPlayBtn_ = nullptr;
    int themeSubId_ = 0;
};
