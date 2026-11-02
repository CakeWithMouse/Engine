#include "UiTestHelperTests.h"
#include "UiSmokeTests.h"
#include <QApplication>
#include <iostream>

int main(int argc, char* argv[])
{
    int appArgc = argc;
    QApplication app(appArgc, argv);

    try
    {
        UiTestHelperTests::Run();
        UiSmokeTests::Run();
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "UiTests failed: " << ex.what() << std::endl;
        return 1;
    }
}
