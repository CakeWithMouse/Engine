#pragma once

#include <QProxyStyle>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOption>
#include <cmath>

class SpinBoxArrowStyle : public QProxyStyle
{
public:
    using QProxyStyle::QProxyStyle;

    void drawPrimitive(PrimitiveElement pe, const QStyleOption* opt, QPainter* p, const QWidget* w = nullptr) const override
    {
        if (pe == PE_IndicatorSpinUp || pe == PE_IndicatorSpinDown ||
            pe == PE_IndicatorArrowUp || pe == PE_IndicatorArrowDown)
        {
            if (!opt || !p) return;
            const QRect r = opt->rect;
            if (r.width() < 2 || r.height() < 2) return;

            p->save();
            p->setRenderHint(QPainter::Antialiasing, true);

            QColor arrowColor;
            const QColor btnText = opt->palette.color(QPalette::ButtonText);
            const QColor winText = opt->palette.color(QPalette::WindowText);
            const int textLightness = (btnText.lightness() + winText.lightness()) / 2;

            if (textLightness > 128)
            {
                // Dark theme -> light contrasting arrow
                arrowColor = QColor(0xEC, 0xEF, 0xF4);
            }
            else
            {
                // Light theme -> dark contrasting arrow
                arrowColor = QColor(0x1F, 0x23, 0x28);
            }

            if (opt->state & State_MouseOver)
            {
                arrowColor = opt->palette.color(QPalette::Highlight);
            }
            if (!(opt->state & State_Enabled))
            {
                arrowColor = opt->palette.color(QPalette::Disabled, QPalette::Text);
            }

            p->setPen(QPen(arrowColor, 1.5f, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p->setBrush(Qt::NoBrush);

            const float cx = static_cast<float>(r.center().x());
            const float cy = static_cast<float>(r.center().y());

            QPainterPath path;
            if (pe == PE_IndicatorSpinUp || pe == PE_IndicatorArrowUp)
            {
                path.moveTo(cx - 3.5f, cy + 1.5f);
                path.lineTo(cx, cy - 2.0f);
                path.lineTo(cx + 3.5f, cy + 1.5f);
            }
            else
            {
                path.moveTo(cx - 3.5f, cy - 1.5f);
                path.lineTo(cx, cy + 2.0f);
                path.lineTo(cx + 3.5f, cy - 1.5f);
            }
            p->drawPath(path);
            p->restore();
            return;
        }

        QProxyStyle::drawPrimitive(pe, opt, p, w);
    }
};
