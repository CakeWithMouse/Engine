#include "../Core/IUserPrompt.h"
#include "../Core/Logger.h"
#include "../Core/CommandRegistry.h"
#include "../Core/FileService.h"

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
} // namespace

#include "EditorLocale.h"
#include "GizmoMath.h"
#include "SettingsService.h"

static void TestGizmoMath()
{
    // 1. Translation snap
    Check(std::abs(GizmoMath::SnapTranslation(1.35, 0.5) - 1.5) < 1e-6, "SnapTranslation 1.35 to step 0.5 is 1.5");
    Check(std::abs(GizmoMath::SnapTranslation(1.10, 0.5) - 1.0) < 1e-6, "SnapTranslation 1.10 to step 0.5 is 1.0");
    Check(std::abs(GizmoMath::SnapTranslation(2.75, 0.0) - 2.75) < 1e-6, "SnapTranslation with step 0 is unchanged");

    // 2. Rotation snap (degrees step, radians input)
    constexpr double kPi = 3.14159265358979323846;
    const double rad32 = 32.0 * (kPi / 180.0);
    const double snappedRad = GizmoMath::SnapRotation(rad32, 15.0);
    const double degSnapped = snappedRad * (180.0 / kPi);
    Check(std::abs(degSnapped - 30.0) < 1e-4, "SnapRotation 32 deg to step 15 deg is 30 deg");

    // 3. Scale snap
    Check(std::abs(GizmoMath::SnapScale(1.18, 0.25) - 1.25) < 1e-6, "SnapScale 1.18 to step 0.25 is 1.25");

    // 4. TransformDelta World vs Local
    const double delta[3] = {1.0, 2.0, 3.0};
    const double qIdent[4] = {0.0, 0.0, 0.0, 1.0};
    double outWorld[3]{};
    GizmoMath::TransformDelta(delta, qIdent, false, outWorld);
    Check(std::abs(outWorld[0] - 1.0) < 1e-6 && std::abs(outWorld[1] - 2.0) < 1e-6 && std::abs(outWorld[2] - 3.0) < 1e-6,
          "TransformDelta World returns unchanged delta");

    // 90 deg around Z: turns (1, 0, 0) into (0, 1, 0)
    double qZ90[4]{};
    GizmoMath::AxisAngleQuat(0.0, 0.0, 1.0, kPi * 0.5, qZ90);
    const double unitX[3] = {1.0, 0.0, 0.0};
    double outLocal[3]{};
    GizmoMath::TransformDelta(unitX, qZ90, true, outLocal);
    Check(std::abs(outLocal[0]) < 1e-5 && std::abs(outLocal[1] - 1.0) < 1e-5 && std::abs(outLocal[2]) < 1e-5,
          "TransformDelta Local rotates vector by quaternion");

    // 5. Accumulated translation snapping
    // Small tick deltas (e.g. 0.05) would round to 0 if snapped per-tick with step 1.0.
    const double smallDelta = 0.05;
    double perTickAccum = 0.0;
    for (int i = 0; i < 15; ++i)
        perTickAccum += GizmoMath::SnapTranslation(smallDelta, 1.0);
    Check(perTickAccum == 0.0, "Per-tick snapping of small delta rounds to zero every tick");

    // Snapping the accumulated gesture offset (startPos + accumulatedDelta) preserves movement and jumps cleanly to 1.0.
    const double startPos = 0.0;
    double accumulatedDelta = 0.0;
    double lastSnapped = startPos;
    for (int tick = 0; tick < 12; ++tick)
    {
        accumulatedDelta += 0.05;
        lastSnapped = GizmoMath::SnapTranslation(startPos + accumulatedDelta, 1.0);
    }
    Check(lastSnapped == 1.0, "Accumulated offset reaches threshold and snaps to 1.0");
}

static void TestEditorLocale()
{
    auto& loc = EditorLocale::Instance();
    loc.SetLanguage(EditorLanguage::Ru);
    Check(loc.Language() == EditorLanguage::Ru, "Locale default Ru");
    Check(Tr("Файл", "File") == QStringLiteral("Файл"), "Translate Ru");

    bool notified = false;
    int sub = loc.SubscribeLanguageChanged([&] { notified = true; });
    loc.SetLanguage(EditorLanguage::En);
    Check(notified == true, "Locale subscriber notified on change");
    Check(loc.Language() == EditorLanguage::En, "Locale changed to En");
    Check(Tr("Файл", "File") == QStringLiteral("File"), "Translate En");

    loc.UnsubscribeLanguageChanged(sub);
    notified = false;
    loc.SetLanguage(EditorLanguage::Ru);
    Check(notified == false, "Unsubscribed callback not notified");
    Check(Tr("Файл", "File") == QStringLiteral("Файл"), "Translate Ru again");
}

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

    std::printf("[Test] TestEditorLocale...\n");
    std::fflush(stdout);
    TestEditorLocale();

    std::printf("[Test] TestGizmoMath...\n");
    std::fflush(stdout);
    TestGizmoMath();

    std::printf("[Test] TestSettingsService...\n");
    std::fflush(stdout);
    {
        const QString k1 = SettingsService::ComputeProjectKey("C:/Proj1/p.painproj");
        const QString k2 = SettingsService::ComputeProjectKey("C:/Proj2/p.painproj");
        const QString kEmpty = SettingsService::ComputeProjectKey("");
        Check(!k1.isEmpty(), "k1 not empty");
        Check(!k2.isEmpty(), "k2 not empty");
        Check(k1 != k2, "k1 != k2");
        Check(kEmpty == QStringLiteral("default"), "kEmpty is default");

        SettingsService::Instance().SaveProjectLayout("C:/Proj1/p.painproj", QByteArray("STATE1"), QByteArray("GEOM1"));
        QByteArray s, g;
        Check(SettingsService::Instance().RestoreProjectLayout("C:/Proj1/p.painproj", s, g), "restore layout proj1");
        Check(s == QByteArray("STATE1"), "state proj1");
        Check(g == QByteArray("GEOM1"), "geom proj1");

        QByteArray s2, g2;
        Check(!SettingsService::Instance().RestoreProjectLayout("C:/ProjNew/never.painproj", s2, g2), "new project gets no saved layout");

        SettingsService::Instance().ResetProjectLayout("C:/Proj1/p.painproj");
        QByteArray s3, g3;
        Check(!SettingsService::Instance().RestoreProjectLayout("C:/Proj1/p.painproj", s3, g3), "reset layout proj1");
    }

    std::printf("CoreTests: %d checks passed.\n", gChecks);
    std::fflush(stdout);
    return 0;
}

