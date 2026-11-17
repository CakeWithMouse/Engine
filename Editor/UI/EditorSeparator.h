#pragma once

#include "EditorTheme.h"
#include <QWidget>
#include <QPainter>
#include <algorithm>
#include <cmath>

class EditorSeparator : public QWidget
{
public:
    explicit EditorSeparator(Qt::Orientation orientation = Qt::Vertical, QWidget* parent = nullptr)
        : QWidget(parent)
        , orientation_(orientation)
    {
        setAttribute(Qt::WA_NoSystemBackground, true);
        if (orientation_ == Qt::Vertical)
        {
            setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
        }
        else
        {
            setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        }

        themeSubId_ = EditorTheme::Instance().SubscribeThemeChanged([this] {
            update();
        });
    }

    explicit EditorSeparator(QWidget* parent)
        : EditorSeparator(Qt::Vertical, parent)
    {
    }

    ~EditorSeparator() override
    {
        if (themeSubId_)
        {
            EditorTheme::Instance().UnsubscribeThemeChanged(themeSubId_);
            themeSubId_ = 0;
        }
    }

    Qt::Orientation Orientation() const { return orientation_; }
    void SetOrientation(Qt::Orientation orientation)
    {
        if (orientation_ != orientation)
        {
            orientation_ = orientation;
            updateGeometry();
            update();
        }
    }

    QSize sizeHint() const override
    {
        return orientation_ == Qt::Vertical ? QSize(6, 16) : QSize(16, 6);
    }

    QSize minimumSizeHint() const override
    {
        return QSize(1, 1);
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);
        if (width() <= 0 || height() <= 0)
        {
            return;
        }

        QPainter painter(this);
        const qreal dpr = devicePixelRatioF() > 0.0 ? devicePixelRatioF() : 1.0;
        const int physW = std::max(1, qRound(width() * dpr));
        const int physH = std::max(1, qRound(height() * dpr));
        const QColor color = EditorTheme::Instance().Palette().borderLight;

        painter.setRenderHint(QPainter::Antialiasing, false);

        if (orientation_ == Qt::Vertical)
        {
            const int physThickness = std::max(1, std::min(physW, qRound(1.0 * dpr)));
            const int physX = std::max(0, (physW - physThickness) / 2);
            const qreal logicalX = static_cast<qreal>(physX) / dpr;
            const qreal logicalThickness = static_cast<qreal>(physThickness) / dpr;
            painter.fillRect(QRectF(logicalX, 0.0, logicalThickness, static_cast<qreal>(height())), color);
        }
        else
        {
            const int physThickness = std::max(1, std::min(physH, qRound(1.0 * dpr)));
            const int physY = std::max(0, (physH - physThickness) / 2);
            const qreal logicalY = static_cast<qreal>(physY) / dpr;
            const qreal logicalThickness = static_cast<qreal>(physThickness) / dpr;
            painter.fillRect(QRectF(0.0, logicalY, static_cast<qreal>(width()), logicalThickness), color);
        }
    }

private:
    Qt::Orientation orientation_;
    uint32_t themeSubId_{0};
};
