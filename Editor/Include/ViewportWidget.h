#pragma once
#include <QWidget>
#include <QSet>
#include <QPointF>

class EditorQtHost;

class ViewportWidget : public QWidget
{
public:
    explicit ViewportWidget(EditorQtHost* host, QWidget* parent = nullptr);

protected:
    bool event(QEvent* event) override;
    QSet<int> keys_;
    QPointF lastMouse_;
    bool looking_ = false;
    QPaintEngine* paintEngine() const override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    void paintEvent(QPaintEvent* e) override;

private:
    void SendAttach();
    void SendResize();
    uint32_t PhysW() const;
    uint32_t PhysH() const;

    EditorQtHost* host_ = nullptr;
    bool attached_ = false;
};