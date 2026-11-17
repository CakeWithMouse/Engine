#pragma once

#include "InfoButton.h"
#include "IconProvider.h"
#include <QApplication>
#include <QKeyEvent>
#include <QPixmap>
#include <iostream>
#include <stdexcept>

namespace InfoButtonTests
{

inline void Check(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(std::string("InfoButton test failed: ") + message);
}

inline void Run()
{
    int checks = 0;
    std::cout << "[Test] Running InfoButtonTests..." << std::endl;

    // 1. Icon rendering across 4 DPRs (1.0, 1.25, 1.5, 2.0)
    const qreal testDprs[] = {1.0, 1.25, 1.5, 2.0};
    for (qreal dpr : testDprs)
    {
        QPixmap pixHelp = IconProvider::GetPixmap(QStringLiteral("help"), QColor(255, 255, 255), 16, dpr);
        Check(!pixHelp.isNull(), "Icon 'help' produces non-null pixmap");
        Check(pixHelp.width() == qRound(16 * dpr), "Physical width matches 16 * dpr");
        Check(pixHelp.height() == qRound(16 * dpr), "Physical height matches 16 * dpr");
        Check(qFuzzyCompare(pixHelp.devicePixelRatio(), dpr), "devicePixelRatio matches requested DPR");

        QPixmap pixCircle = IconProvider::GetPixmap(QStringLiteral("help-circle"), QColor(255, 255, 255), 16, dpr);
        Check(!pixCircle.isNull(), "Icon 'help-circle' produces non-null pixmap");
        Check(pixCircle.width() == qRound(16 * dpr), "help-circle physical width matches 16 * dpr");
        ++checks;
    }

    // 2. Pure geometry calculation: primary monitor (0, 0, 1920, 1080)
    const QRect primaryScreen(0, 0, 1920, 1080);
    const QSize popupSize(300, 120);

    // Case 2a: Button at right edge
    {
        QRect btnRect(1900, 200, 18, 18);
        QRect geom = InfoButtonGeometry::CalculatePopupGeometry(btnRect, popupSize, primaryScreen);
        Check(primaryScreen.contains(geom), "Button at right edge: popup is strictly inside screen");
        Check(geom.right() == btnRect.right(), "Popup right edge aligns with button right edge");
        Check(geom.top() == btnRect.bottom() + 2, "Popup opens below button");
        ++checks;
    }

    // Case 2b: Button at bottom edge
    {
        QRect btnRect(500, 1060, 18, 18);
        QRect geom = InfoButtonGeometry::CalculatePopupGeometry(btnRect, popupSize, primaryScreen);
        Check(primaryScreen.contains(geom), "Button at bottom edge: popup is strictly inside screen");
        Check(geom.bottom() < btnRect.top(), "Popup opens above button when overflowing bottom");
        ++checks;
    }

    // Case 2c: Button at bottom-right corner
    {
        QRect btnRect(1900, 1060, 18, 18);
        QRect geom = InfoButtonGeometry::CalculatePopupGeometry(btnRect, popupSize, primaryScreen);
        Check(primaryScreen.contains(geom), "Button at bottom-right corner: popup is strictly inside screen");
        Check(geom.right() <= primaryScreen.right(), "Does not exceed screen right edge");
        Check(geom.bottom() < btnRect.top(), "Opens above button");
        ++checks;
    }

    // Case 2d: Button at top-left corner
    {
        QRect btnRect(5, 5, 18, 18);
        QRect geom = InfoButtonGeometry::CalculatePopupGeometry(btnRect, popupSize, primaryScreen);
        Check(primaryScreen.contains(geom), "Button at top-left corner: popup is strictly inside screen");
        Check(geom.left() >= primaryScreen.left(), "Does not exceed screen left edge");
        Check(geom.top() == btnRect.bottom() + 2, "Opens below button");
        ++checks;
    }

    // 3. Multi-monitor support: second monitor with offset coordinates (1920, 0, 1920, 1080)
    const QRect secondScreen(1920, 0, 1920, 1080);
    {
        QRect btnRect(3820, 1060, 18, 18);
        QRect geom = InfoButtonGeometry::CalculatePopupGeometry(btnRect, popupSize, secondScreen);
        Check(secondScreen.contains(geom), "Second monitor corner: popup strictly inside second monitor bounds");
        Check(geom.left() >= 1920, "Popup left edge is within second screen");
        Check(geom.right() <= 3840, "Popup right edge is within second screen");
        ++checks;
    }

    // 4. Live widget properties and interactions
    {
        InfoButton btn(QStringLiteral("Русский текст"), QStringLiteral("English text"));
        Check(btn.width() == 18 && btn.height() == 18, "InfoButton size is 18x18");
        Check(btn.cursor().shape() == Qt::PointingHandCursor, "InfoButton cursor is PointingHandCursor");
        Check(!btn.toolTip().isEmpty(), "InfoButton has non-empty tooltip");
        Check(btn.toolTip() == QStringLiteral("Справка") || btn.toolTip() == QStringLiteral("Help"),
              "Tooltip is 'Справка' or 'Help'");

        // Click to open popup
        btn.click();
        QFrame* popup = btn.Popup();
        Check(popup != nullptr, "Popup window created");
        Check(popup->isVisible(), "Popup is visible after click");

        // Esc key closes popup
        QKeyEvent escEvent(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
        QApplication::sendEvent(popup, &escEvent);
        Check(!popup->isVisible(), "Popup closed by Escape key");
        ++checks;
    }

    std::cout << "InfoButton tests: " << checks << " checks passed" << std::endl;
}

} // namespace InfoButtonTests
