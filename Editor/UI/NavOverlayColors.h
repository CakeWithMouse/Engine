#pragma once

#include <QColor>

namespace NavOverlayColors
{
    // Navigation overlay colors matching engine D3D11 overlay rasterization:
    // Walkable: 0x6657b866 (green)
    inline QColor Walkable() { return QColor(87, 184, 102); }

    // Obstacle: 0x994646ef (red)
    inline QColor Obstacle() { return QColor(239, 70, 70); }

    // Cost: 0x995000f0 (warm gradient, amber/yellow)
    inline QColor Cost() { return QColor(240, 180, 50); }

    // Route A/B: start/goal endpoints (0xffdb78f5, magenta/purple)
    inline QColor RouteAB() { return QColor(219, 120, 245); }

    // Direction field: 0xffffdb60 (cyan/light blue)
    inline QColor Direction() { return QColor(96, 219, 255); }
}
