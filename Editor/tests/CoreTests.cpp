#include "../Core/IUserPrompt.h"
#include "../Core/Logger.h"
#include "../Core/CommandRegistry.h"
#include "../Core/FileService.h"
#include "../Core/GizmoMath.cpp"
#include "core/GizmoMathTests.h"
#include "../Core/MaterialGraphModel.cpp"
#include "../Core/HlslGraphCompiler.cpp"
#include "core/MaterialGraphTests.h"
#include "../Core/SceneFile.cpp"
#include "core/SceneFileTests.h"
#include "../Core/Terminal.cpp"
#include "core/TerminalTests.h"

#include <QCoreApplication>
#include <QTemporaryDir>
#include <cassert>
#include <cstdio>
#ifdef _DEBUG
#include <crtdbg.h>
#endif
#include <thread>
#include <vector>

namespace
{
int gChecks = 0;
void Check(bool condition, const char* message)
{
    if (!condition)
    {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
    ++gChecks;
}

class MockUserPrompt : public IUserPrompt
{
public:
    PromptResult AskYesNoCancel(const QString&, const QString&) override { return PromptResult::Yes; }
    bool AskConfirmation(const QString&, const QString&) override { return true; }
    void ShowMessage(const QString&, const QString&, bool) override { ++messagesShown; }
    QString AskOpenFileName(const QString&, const QString&) override { return QStringLiteral("selected.file"); }
    QStringList AskOpenFileNames(const QString&, const QString&) override { return {QStringLiteral("f1"), QStringLiteral("f2")}; }
    QString AskSaveFileName(const QString&, const QString&) override { return QStringLiteral("saved.file"); }

    int messagesShown = 0;
};

void TestUserPrompt()
{
    MockUserPrompt prompt;
    IUserPrompt* iface = &prompt;
    Check(iface->AskYesNoCancel("Title", "Msg") == PromptResult::Yes, "Prompt YesNoCancel");
    Check(iface->AskConfirmation("Title", "Msg") == true, "Prompt Confirmation");
    iface->ShowMessage("Title", "Msg");
    Check(prompt.messagesShown == 1, "Prompt ShowMessage");
    Check(iface->AskOpenFileName("Title", "*.txt") == "selected.file", "Prompt OpenFileName");
    Check(iface->AskOpenFileNames("Title", "*.txt").size() == 2, "Prompt OpenFileNames");
    Check(iface->AskSaveFileName("Title", "*.txt") == "saved.file", "Prompt SaveFileName");
}

void TestLogger()
{
    Logger logger;
    int sinkCalls = 0;
    LogLevel lastLevel = LogLevel::Info;
    QString lastMessage;

    logger.AddSink([&](const LogEntry& e) {
        ++sinkCalls;
        lastLevel = e.level;
        lastMessage = e.message;
    });

    logger.Info("Hello Info");
    Check(sinkCalls == 1, "Logger sink called on Info");
    Check(lastLevel == LogLevel::Info, "Logger Info level");
    Check(lastMessage == "Hello Info", "Logger message match");

    logger.Warning("Careful!");
    Check(sinkCalls == 2, "Logger sink called on Warning");
    Check(lastLevel == LogLevel::Warning, "Logger Warning level");

    logger.Error("Critical error");
    Check(sinkCalls == 3, "Logger sink called on Error");
    Check(lastLevel == LogLevel::Error, "Logger Error level");

    auto recent = logger.GetRecentEntries(10);
    Check(recent.size() == 3, "Logger recent entries count");

    logger.ClearSinks();

    // Thread-safety check
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t)
    {
        threads.emplace_back([&logger, t] {
            for (int i = 0; i < 50; ++i)
            {
                logger.Debug(QStringLiteral("Thread %1 Msg %2").arg(t).arg(i));
            }
        });
    }
    for (auto& th : threads)
    {
        th.join();
    }
    Check(logger.GetRecentEntries(500).size() == 203, "Concurrent logging completed without corruption");
}

void TestCommandRegistry()
{
    CommandRegistry registry;

    Check(!registry.RegisterCommand({"", "Empty ID", "", "", [](const CommandContext&) { return true; }, nullptr}),
          "Reject empty command ID");

    bool executed = false;
    QVariantMap receivedParams;
    CommandDef testCmd;
    testCmd.id = "Edit.Test";
    testCmd.title = "Test Command";
    testCmd.handler = [&](const CommandContext& ctx) {
        executed = true;
        receivedParams = ctx.parameters;
        return true;
    };

    Check(registry.RegisterCommand(testCmd), "Register command success");
    Check(!registry.RegisterCommand(testCmd), "Duplicate command registration rejected");
    Check(registry.HasCommand("Edit.Test"), "HasCommand true");
    Check(registry.CanExecute("Edit.Test"), "CanExecute true by default");

    QVariantMap params;
    params["count"] = 42;
    params["mode"] = "fast";
    Check(registry.Execute("Edit.Test", params), "Execute success");
    Check(executed, "Command handler executed");
    Check(receivedParams["count"].toInt() == 42, "Parameter count received");
    Check(receivedParams["mode"].toString() == "fast", "Parameter mode received");

    // CanExecute disabled test
    CommandDef disabledCmd;
    disabledCmd.id = "Edit.Disabled";
    disabledCmd.title = "Disabled";
    disabledCmd.canExecute = [] { return false; };
    disabledCmd.handler = [](const CommandContext&) { return true; };
    registry.RegisterCommand(disabledCmd);

    Check(!registry.CanExecute("Edit.Disabled"), "CanExecute returns false");
    Check(!registry.Execute("Edit.Disabled"), "Execute fails when CanExecute is false");

    Check(registry.UnregisterCommand("Edit.Test"), "Unregister command success");
    Check(!registry.HasCommand("Edit.Test"), "HasCommand false after unregister");
    Check(!registry.Execute("Edit.Test"), "Execute fails for unregistered command");
}

void TestFileService()
{
    QTemporaryDir tempDir;
    Check(tempDir.isValid(), "Temporary directory created");

    Logger logger;
    FileService fs(&logger);
    fs.SetProjectRoot(tempDir.path());

    Check(fs.IsInSandbox(tempDir.path() + "/Content/scene.json"), "Path in sandbox");
    Check(!fs.IsInSandbox("C:/Windows/System32/calc.exe"), "External path not in sandbox");
    Check(!fs.IsInSandbox(tempDir.path() + "/../outside.txt"), "Parent directory escape not in sandbox");

    // Write file
    const QByteArray data = "Hello, Engine FileService!";
    QString error;
    Check(fs.WriteFileAtomic("SubDir/test.txt", data, &error), "WriteFileAtomic relative success");
    Check(error.isEmpty(), "No error on valid write");

    // Read file
    QByteArray readBack;
    Check(fs.ReadFile("SubDir/test.txt", readBack, &error), "ReadFile success");
    Check(readBack == data, "ReadFile data matches written");

    // Overwrite atomically
    const QByteArray updated = "Updated content v2";
    Check(fs.WriteFileAtomic("SubDir/test.txt", updated, &error), "WriteFileAtomic overwrite success");
    Check(fs.ReadFile("SubDir/test.txt", readBack), "ReadFile second time success");
    Check(readBack == updated, "ReadFile matches updated data");

    // List files
    QStringList files = fs.ListFiles("SubDir");
    Check(files.contains("test.txt"), "ListFiles found created file");

    // Delete file
    Check(fs.DeleteFile("SubDir/test.txt", &error), "DeleteFile success");
    Check(!fs.ReadFile("SubDir/test.txt", readBack), "ReadFile fails after delete");

    // Sandbox violations rejected
    Check(!fs.WriteFileAtomic("../forbidden.txt", "evil", &error), "Write outside sandbox rejected");
    Check(!error.isEmpty(), "Error populated on sandbox violation");
    Check(!fs.ReadFile("../forbidden.txt", readBack, &error), "Read outside sandbox rejected");
    Check(!fs.DeleteFile("../forbidden.txt", &error), "Delete outside sandbox rejected");
}

void TestGizmoMath()
{
    RunGizmoMathTests();
    // 11 distinct checks in RunGizmoMathTests
    gChecks += 11;
}

void TestMaterialGraph()
{
    RunMaterialGraphTests();
    // 18 distinct checks in RunMaterialGraphTests
    gChecks += 18;
}

void TestSceneFile()
{
    RunSceneFileTests();
    // 21 distinct checks in RunSceneFileTests
    gChecks += 21;
}

void TestTerminal()
{
    RunTerminalTests();
    // 28 distinct checks in RunTerminalTests
    gChecks += 28;
}

} // namespace

int main(int argc, char* argv[])
{
#ifdef _DEBUG
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
#endif

    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::setvbuf(stderr, nullptr, _IONBF, 0);
    QCoreApplication app(argc, argv);

    std::printf("Running CoreTests...\n");
    std::fflush(stdout);

    std::printf("[Test] TestUserPrompt...\n");
    std::fflush(stdout);
    TestUserPrompt();

    std::printf("[Test] TestLogger...\n");
    std::fflush(stdout);
    TestLogger();

    std::printf("[Test] TestCommandRegistry...\n");
    std::fflush(stdout);
    TestCommandRegistry();

    std::printf("[Test] TestFileService...\n");
    std::fflush(stdout);
    TestFileService();

    std::printf("[Test] TestGizmoMath...\n");
    std::fflush(stdout);
    TestGizmoMath();

    std::printf("[Test] TestMaterialGraph...\n");
    std::fflush(stdout);
    TestMaterialGraph();

    std::printf("[Test] TestSceneFile...\n");
    std::fflush(stdout);
    TestSceneFile();

    std::printf("[Test] TestTerminal...\n");
    std::fflush(stdout);
    TestTerminal();

    std::printf("CoreTests: %d checks passed.\n", gChecks);
    std::fflush(stdout);
    return 0;
}
