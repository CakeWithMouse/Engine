#pragma once

class QApplication;

class EditorTheme
{
public:
    static void Apply(QApplication& app);
    static const char* StyleRules();
};
