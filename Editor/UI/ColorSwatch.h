#pragma once

#include <QWidget>
#include <QColor>
#include <QPainter>
#include <QPen>
#include "EditorTheme.h"

class ColorSwatch : public QWidget
{
public:
    static constexpr int kSwatchSize = 10;

    explicit ColorSwatch(const QColor& color, QWidget* parent = nullptr)
        : QWidget(parent), color_(color)
    {
        setFixedSize(kSwatchSize, kSwatchSize);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }

    void SetColor(const QColor& color)
    {
        if (color_ != color)
        {
            color_ = color;
            update();
        }
    }

    const QColor& Color() const { return color_; }

    QSize sizeHint() const override { return QSize(kSwatchSize, kSwatchSize); }
    QSize minimumSizeHint() const override { return QSize(kSwatchSize, kSwatchSize); }

protected:
    void paintEvent(QPaintEvent* /*event*/) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const qreal dpr = devicePixelRatioF();
        QRectF r(0.5, 0.5, width() - 1.0, height() - 1.0);

        p.setBrush(color_);
        const bool isDark = EditorTheme::Instance().Mode() == ThemeMode::Dark;
        const QColor strokeColor = isDark ? QColor(255, 255, 255, 90) : QColor(0, 0, 0, 90);
        p.setPen(QPen(strokeColor, 1.0 / (dpr > 0.0 ? dpr : 1.0)));
        p.drawEllipse(r);
    }

private:
    QColor color_;
};
