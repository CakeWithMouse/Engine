#include "UiTestHelperTests.h"
#include "HighDpiIconTests.h"
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
#include "InspectorCardStyleTests.h"
#include "EditorSeparatorTests.h"
#include "WheelGuardTests.h"
#include "EditorTheme.h"
#include <QApplication>
#include <iostream>

int main(int argc, char* argv[])
{
    int appArgc = argc;
    QApplication app(appArgc, argv);
    EditorTheme::Apply(app);

    int failures = 0;
    auto runSuite = [&](auto func, const char* name) {
        try
        {
            func();
        }
        catch (const std::exception& ex)
        {
            std::cerr << name << " failed: " << ex.what() << std::endl;
            ++failures;
        }
    };

    runSuite(UiTestHelperTests::Run, "UiTestHelperTests");
    runSuite(HighDpiIconTests::Run, "HighDpiIconTests");
    runSuite(ImportSettingsDialogTests::Run, "ImportSettingsDialogTests");
    runSuite(EditImportSettingsTests::Run, "EditImportSettingsTests");
    runSuite(ImportSettingsV2EditTests::Run, "ImportSettingsV2EditTests");
    runSuite(AssetDeleteTests::Run, "AssetDeleteTests");
    runSuite(InspectorRefreshTests::Run, "InspectorRefreshTests");
    runSuite(InspectorRefreshTests::RunMouseRefreshJitter, "InspectorRefreshTests::RunMouseRefreshJitter");
    runSuite(InspectorRefreshTests::RunModelReplaceButtonRegression, "InspectorRefreshTests::RunModelReplaceButtonRegression");
    runSuite(InspectorCardStyleTests::Run, "InspectorCardStyleTests");
    runSuite(EditorSeparatorTests::Run, "EditorSeparatorTests");
    runSuite(WheelGuardTests::Run, "WheelGuardTests");
    runSuite(DockRedockTests::Run, "DockRedockTests");
    runSuite(GeometryTextureActionTests::Run, "GeometryTextureActionTests");
    runSuite(AssetPickerDialogTests::Run, "AssetPickerDialogTests");
    runSuite(ViewportBillboardTests::Run, "ViewportBillboardTests");
    runSuite(ContentBrowserTests::Run, "ContentBrowserTests");
    runSuite(ContentBrowserTypeTests::RunStandalone, "ContentBrowserTypeTests");
    runSuite(UiSmokeTests::Run, "UiSmokeTests");

    if (failures > 0)
    {
        std::cerr << "UiTests finished with " << failures << " failed suite(s)." << std::endl;
        return 1;
    }
    return 0;
}
