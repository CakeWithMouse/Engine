#include "UiTestHelperTests.h"
#include "ImportSettingsDialogTests.h"
#include "EditImportSettingsTests.h"
#include "ImportSettingsV2EditTests.h"
#include "AssetDeleteTests.h"
#include "InspectorRefreshTests.h"
#include "DockRedockTests.h"
#include "ViewportBillboardTests.h"
#include "UiSmokeTests.h"
#include "ContentBrowserTests.h"
#include "ContentBrowserTypeTests.h"
#include "GeometryTextureActionTests.h"
#include "AssetPickerDialogTests.h"
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
        InspectorRefreshTests::Run();
        InspectorRefreshTests::RunMouseRefreshJitter();
        DockRedockTests::Run();
        ViewportBillboardTests::Run();
        ContentBrowserTests::Run();
        ContentBrowserTypeTests::RunStandalone();
        GeometryTextureActionTests::Run();
        AssetPickerDialogTests::Run();
        UiSmokeTests::Run();
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "UiTests failed: " << ex.what() << std::endl;
        return 1;
    }
}
