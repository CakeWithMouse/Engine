#include "UiTestHelperTests.h"
#include <QApplication>

int main(int argc, char* argv[])
{
    int appArgc = argc;
    QApplication app(appArgc, argv);

    try
    {
        UiTestHelperTests::Run();
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "UiTestHelperTests failed: " << ex.what() << std::endl;
        return 1;
    }
}

