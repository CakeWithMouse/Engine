#include "EditorHistory.h"
#include <iostream>
#include <stdexcept>
#define CHECK(condition) do { if (!(condition)) throw std::runtime_error("Check failed: " #condition); } while (false)
#include <chrono>
#include <QCoreApplication>
#include "HostRegressionTests.h"
#include "OverlayHostTests.h"

int main(int argc, char** argv)
{
    try {
    QCoreApplication app(argc, argv);
    std::cout << "[Test] Running HostTests (EditorHistory)..." << std::endl;

    int checksPassed = 0;

    // Test 1: Single gesture of 300 updates gives exactly 1 false (the first), and 299 true
    {
        EditorHistory history;
        const uint64_t objId = 42;
        history.BeginGizmoDrag(objId);

        EditorCommand cmd;
        cmd.type = EditorCommand::UpdateObjectTransform;
        cmd.objectId = objId;
        cmd.path = QStringLiteral("gizmo");

        auto now = std::chrono::steady_clock::now();
        int falseCount = 0;
        int trueCount = 0;

        for (int i = 0; i < 300; ++i)
        {
            const bool coalesced = history.ShouldCoalesce(cmd, now);
            if (!coalesced)
            {
                falseCount++;
                history.OnTransformApplied(cmd, now);
            }
            else
            {
                trueCount++;
            }
            now += std::chrono::milliseconds(2);
        }

        history.EndGizmoDrag();

        CHECK(falseCount == 1);
        CHECK(trueCount == 299);
        checksPassed += 2;
    }

    // Test 2: Two consecutive gestures give exactly 2 false (one per gesture)
    {
        EditorHistory history;
        const uint64_t objId = 100;
        int totalFalseCount = 0;

        EditorCommand cmd;
        cmd.type = EditorCommand::UpdateObjectTransform;
        cmd.objectId = objId;
        cmd.path = QStringLiteral("gizmo");

        auto now = std::chrono::steady_clock::now();

        // Gesture 1
        history.BeginGizmoDrag(objId);
        for (int i = 0; i < 150; ++i)
        {
            const bool coalesced = history.ShouldCoalesce(cmd, now);
            if (!coalesced)
            {
                totalFalseCount++;
                history.OnTransformApplied(cmd, now);
            }
            now += std::chrono::milliseconds(2);
        }
        history.EndGizmoDrag();

        // Gesture 2
        history.BeginGizmoDrag(objId);
        for (int i = 0; i < 150; ++i)
        {
            const bool coalesced = history.ShouldCoalesce(cmd, now);
            if (!coalesced)
            {
                totalFalseCount++;
                history.OnTransformApplied(cmd, now);
            }
            now += std::chrono::milliseconds(2);
        }
        history.EndGizmoDrag();

        CHECK(totalFalseCount == 2);
        checksPassed += 1;
    }

    // Test 3: Nudge within 350 ms window coalesces, after 350 ms does not
    {
        EditorHistory history;
        const uint64_t objId = 200;

        EditorCommand cmd;
        cmd.type = EditorCommand::UpdateObjectTransform;
        cmd.objectId = objId;
        cmd.path = QStringLiteral("nudge");

        const auto t0 = std::chrono::steady_clock::now();

        // Initial nudge: should NOT coalesce
        bool coalesced0 = history.ShouldCoalesce(cmd, t0);
        CHECK(!coalesced0);
        history.OnTransformApplied(cmd, t0);
        checksPassed += 1;

        // Nudge within 100 ms (< 350 ms): MUST coalesce
        const auto t1 = t0 + std::chrono::milliseconds(100);
        bool coalesced1 = history.ShouldCoalesce(cmd, t1);
        CHECK(coalesced1);
        history.OnTransformApplied(cmd, t1);
        checksPassed += 1;

        // Nudge after 400 ms (> 350 ms from t1): MUST NOT coalesce
        const auto t2 = t1 + std::chrono::milliseconds(400);
        bool coalesced2 = history.ShouldCoalesce(cmd, t2);
        CHECK(!coalesced2);
        checksPassed += 1;
    }

    checksPassed += HostRegressionTests::Run();
    checksPassed += OverlayHostTests::Run();
    std::cout << "HostTests: " << checksPassed << " checks passed." << std::endl;
    return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << std::endl; return 1; }
}

