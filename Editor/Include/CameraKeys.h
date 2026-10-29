#pragma once

// Windows keyboard scan codes identify physical keys independently of layout.
// Keep Qt's logical key for synthetic events and keys outside camera navigation.
inline int CameraNavigationKey(unsigned scanCode, int logicalKey)
{
    switch (scanCode)
    {
    case 0x10: return 'Q';
    case 0x11: return 'W';
    case 0x12: return 'E';
    case 0x13: return 'R';
    case 0x1e: return 'A';
    case 0x1f: return 'S';
    case 0x20: return 'D';
    case 0x22: return 'G';
    case 0x29: return '~';
    case 0x19: return 'P'; // Play pause also follows the physical key on Russian layouts.
    default: return logicalKey;
    }
}
