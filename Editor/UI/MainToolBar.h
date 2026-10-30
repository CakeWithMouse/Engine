#pragma once

#include <QToolBar>
#include <QAction>
#include <QComboBox>
#include <QPushButton>
#include <QToolButton>

class MainToolBar : public QToolBar
{
    Q_OBJECT

public:
    explicit MainToolBar(QWidget* parent = nullptr);

    QAction* ActPlay() const { return actPlay_; }
    QAction* ActPause() const { return actPause_; }
    QAction* ActStop() const { return actStop_; }
    QAction* ActExternal() const { return actExternal_; }
    QComboBox* CameraModeCombo() const { return cameraModeCombo_; }

    void UpdatePlayState(int state, bool sceneBusy);
    void RetranslateUi();
    void RefreshIcons();

signals:
    void PlayTriggered();
    void PauseTriggered();
    void StopTriggered();
    void ExternalTriggered();
    void ConfigPlayTriggered();
    void CameraModeChanged(int mode);
    void LangToggleTriggered();

private:
    QAction* actPlay_ = nullptr;
    QAction* actPause_ = nullptr;
    QAction* actStop_ = nullptr;
    QAction* actExternal_ = nullptr;
    QComboBox* cameraModeCombo_ = nullptr;
    QToolButton* configPlayBtn_ = nullptr;
    QPushButton* langButton_ = nullptr;
};

