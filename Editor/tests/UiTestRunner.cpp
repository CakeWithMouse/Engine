#include "UiTestHelperTests.h"
#include "ImportSettingsDialogTests.h"
#include "EditImportSettingsTests.h"
#include "ImportSettingsV2EditTests.h"
#include "AssetDeleteTests.h"
#include "UiSmokeTests.h"
#include "EditorTheme.h"
#include <QApplication>
#include <iostream>

int main(int argc, char* argv[])
{
    int appArgc = argc;
    QApplication app(appArgc, argv);
    EditorTheme::Apply(app);

    try
    {
        UiTestHelperTests::Run();
        ImportSettingsDialogTests::Run();
        EditImportSettingsTests::Run();
        ImportSettingsV2EditTests::Run();
        AssetDeleteTests::Run();
        UiSmokeTests::Run();
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "UiTests failed: " << ex.what() << std::endl;
        return 1;
    }
}
