#pragma once

#include "ViewportWidget.h"
#include <QApplication>
#include <QElapsedTimer>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QTemporaryDir>
#include <QThread>
#include <QSettings>
#include <stdexcept>
#include <cstdio>

namespace ViewportDpiTests
{
inline void Check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

// Exercise the real widget input/resize paths without a GPU or game module.
class Host final : public EditorQtHost
{
public:
    Host() : EditorQtHost(EngineModuleHost{})
    {
        camera.position[2] = -10;
        camera.fov = 1.04719755f;
        camera.viewMatrix[0] = camera.viewMatrix[5] = camera.viewMatrix[10] = camera.viewMatrix[15] = 1;
        camera.viewMatrix[14] = 10;
        camera.projMatrix[0] = 1.3f;
        camera.projMatrix[5] = 1.73f;
        camera.projMatrix[10] = camera.projMatrix[11] = 1;
        SceneObject object{};
        object.id = 1;
        object.type = "Primitive.Cube";
        object.transform = EngineSceneTransform{{1, 0.5f, 0}, {0, 0, 0, 1}, {1, 1, 1}};
        snapshot.objects.push_back(object);
        snapshot.selected = 1;
    }
    bool HasScene() const override { return true; }
    bool HasViewport() const override { return true; }
    bool HasSelection() const override { return snapshot.selected != 0; }
    Snapshot ReadSnapshot() override { return snapshot; }
    bool GetCamera(EngineCameraInfo& result) const override { result = camera; return true; }
    void PostCommand(const EditorCommand& command) override
    {
        commands.push_back(command);
        if (command.type == EditorCommand::ViewportAttach ||
            (command.type == EditorCommand::ViewportResize && applyResize))
        {
            camera.width = command.w;
            camera.height = command.h;
        }
    }
    const EditorCommand* Last(decltype(EditorCommand::type) type) const
    {
        for (auto i = commands.rbegin(); i != commands.rend(); ++i)
            if (i->type == type) return &*i;
        return nullptr;
    }
    EngineCameraInfo camera{};
    Snapshot snapshot;
    std::vector<EditorCommand> commands;
    bool applyResize = true;
};

class Widget final : public ViewportWidget
{
public:
    explicit Widget(Host* host) : ViewportWidget(host) {}
    qreal ratio = 1;
protected:
    int metric(PaintDeviceMetric value) const override
    {
        if (value == PdmDevicePixelRatioScaled) return qRound(ratio * devicePixelRatioFScale());
        if (value == PdmDevicePixelRatio) return qRound(ratio);
        return ViewportWidget::metric(value);
    }
};

inline void Wait(int milliseconds)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < milliseconds)
    {
        QCoreApplication::processEvents();
        QThread::msleep(1);
    }
}

inline void Mouse(Widget& widget, QEvent::Type type, QPointF point)
{
    QMouseEvent event(type, point, widget.mapToGlobal(point),
        type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton,
        type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &event);
}

inline QPointF Project(const Widget& widget, const Host& host, const float point[3])
{
    float screen[2]{};
    Check(GizmoGeometry::ProjectPointToScreen(point, host.camera.viewMatrix, host.camera.projMatrix,
        host.camera.width, host.camera.height, screen), "test point projects");
    return QPointF(screen[0] * widget.width() / host.camera.width,
                   screen[1] * widget.height() / host.camera.height);
}

inline void Run()
{
    QTemporaryDir settings;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
    for (qreal ratio : {1.0, 1.25, 1.5, 2.0})
    {
        Host host;
        Widget widget(&host);
        widget.ratio = ratio;
        widget.resize(801, 601); // fractional DPI must use the rounded framebuffer size
        widget.show();
        Wait(160);
        Check(host.camera.width == qRound(801 * ratio), "attach uses physical width");
        const auto& transform = host.snapshot.objects[0].transform;
        const QPointF center = Project(widget, host, transform.position);
        host.commands.clear();
        Mouse(widget, QEvent::MouseButtonPress, center);
        Check(host.Last(EditorCommand::BeginGizmoDrag), "visible center starts drag at every DPI");
        Mouse(widget, QEvent::MouseMove, center + QPointF(40, -20));
        const auto* move = host.Last(EditorCommand::UpdateObjectTransform);
        Check(move && std::abs(move->transform.position[0] - (1 + 80.f / 801 / 1.3f * 10)) < 0.001f &&
            std::abs(move->transform.position[1] - (0.5f + 40.f / 601 / 1.73f * 10)) < 0.001f,
            "center translation follows logical mouse delta at every DPI");
        Mouse(widget, QEvent::MouseButtonRelease, center);
        Check(host.Last(EditorCommand::EndGizmoDrag), "drag ends");

        host.commands.clear();
        Mouse(widget, QEvent::MouseButtonPress, center);
        QKeyEvent axis(QEvent::KeyPress, Qt::Key_X, Qt::NoModifier);
        QApplication::sendEvent(&widget, &axis);
        Mouse(widget, QEvent::MouseMove, center + QPointF(40, 0));
        const auto* constrained = host.Last(EditorCommand::UpdateObjectTransform);
        Check(constrained && std::abs(constrained->transform.position[0] - (1 + 80.f / 801 / 1.3f * 10)) < 0.001f &&
            constrained->transform.position[1] == 0.5f, "X hotkey uses converted drag coordinates");
        Mouse(widget, QEvent::MouseButtonRelease, center);

        const float scale = GizmoGeometry::ComputeGizmoWorldScale(transform.position, host.camera);
        float start[3] = {transform.position[0] + scale * 0.75f, transform.position[1], 0};
        float end[3] = {transform.position[0] + scale * 1.5f, transform.position[1], 0};
        widget.setGizmoMode(EditorGizmoScale);
        host.commands.clear();
        Mouse(widget, QEvent::MouseButtonPress, Project(widget, host, start));
        Check(host.Last(EditorCommand::BeginGizmoDrag), "visible scale axis starts drag");
        Mouse(widget, QEvent::MouseMove, Project(widget, host, end));
        const auto* scaled = host.Last(EditorCommand::UpdateObjectTransform);
        Check(scaled && std::abs(scaled->transform.scale[0] - 2) < 0.002f && scaled->transform.scale[1] == 1,
            "axis scale uses consistent physical coordinates");
        Mouse(widget, QEvent::MouseButtonRelease, center);

        host.commands.clear();
        const qreal pixelToLogical = qreal(widget.width()) / host.camera.width;
        Mouse(widget, QEvent::MouseButtonPress, center + QPointF(6 * pixelToLogical, 0));
        Mouse(widget, QEvent::MouseMove, center + QPointF(12 * pixelToLogical, 0));
        const auto* uniform = host.Last(EditorCommand::UpdateObjectTransform);
        Check(uniform && std::abs(uniform->transform.scale[0] - 2) < 0.002f &&
            std::abs(uniform->transform.scale[1] - 2) < 0.002f, "center scale uses physical distances");
        Mouse(widget, QEvent::MouseButtonRelease, center);

        widget.setGizmoMode(EditorGizmoRotate);
        start[0] = transform.position[0] + scale * 0.70710678f;
        start[1] = transform.position[1] + scale * 0.70710678f;
        end[0] = transform.position[0]; end[1] = transform.position[1] + scale;
        host.commands.clear();
        Mouse(widget, QEvent::MouseButtonPress, Project(widget, host, start));
        Check(host.Last(EditorCommand::BeginGizmoDrag), "visible rotation ring starts drag");
        Mouse(widget, QEvent::MouseMove, Project(widget, host, end));
        const auto* rotated = host.Last(EditorCommand::UpdateObjectTransform);
        Check(rotated && std::abs(rotated->transform.rotation[2] - 0.38268343f) < 0.002f,
            "ring drag rotates 45 degrees at every DPI");
        Mouse(widget, QEvent::MouseButtonRelease, center);

        host.snapshot.selected = 0;
        host.snapshot.objects[0].type = "Light.Point";
        host.commands.clear();
        Mouse(widget, QEvent::MouseButtonPress, center);
        Check(host.Last(EditorCommand::SelectObject) && host.Last(EditorCommand::SelectObject)->objectId == 1,
            "visible light billboard receives click at every DPI");
        Mouse(widget, QEvent::MouseButtonRelease, center);
        std::printf("Viewport input DPI %.2f: PASS\n", ratio);
    }

    Host host;
    Widget widget(&host);
    widget.resize(800, 600);
    widget.show();
    Wait(160);
    for (qreal ratio : {2.0, 1.25, 1.5, 1.0})
    {
        Mouse(widget, QEvent::MouseButtonPress, Project(widget, host, host.snapshot.objects[0].transform.position));
        host.commands.clear();
        widget.ratio = ratio;
        QEvent duringDrag(QEvent::DevicePixelRatioChange);
        QApplication::sendEvent(&widget, &duringDrag);
        Check(host.Last(EditorCommand::EndGizmoDrag), "DPI transition finishes the active drag");
        Mouse(widget, QEvent::MouseMove, QPointF(450, 300));
        Check(!host.Last(EditorCommand::UpdateObjectTransform), "DPI transition cannot jump the dragged object");
        widget.clearFocus();
        host.commands.clear();
        host.applyResize = false;
        QEvent dpi(QEvent::DevicePixelRatioChange);
        QApplication::sendEvent(&widget, &dpi);
        Check(widget.IsResizing(), "DPI event schedules resize without logical resize or focus");
        Mouse(widget, QEvent::MouseButtonPress, QPointF(400, 300));
        Check(!host.Last(EditorCommand::BeginGizmoDrag) && !host.Last(EditorCommand::PickObject),
            "pending resize suppresses picking");
        Wait(180);
        const auto* resize = host.Last(EditorCommand::ViewportResize);
        Check(resize && resize->w == qRound(800 * ratio) && resize->h == qRound(600 * ratio),
            "DPI-only transition sends new physical size");
        const uint32_t width = resize->w, height = resize->h;
        host.commands.clear();
        Mouse(widget, QEvent::MouseButtonPress, Project(widget, host, host.snapshot.objects[0].transform.position));
        Check(!host.Last(EditorCommand::BeginGizmoDrag) && !host.Last(EditorCommand::PickObject),
            "stale camera is rejected after resize command");
        host.camera.width = width; host.camera.height = height;
        Mouse(widget, QEvent::MouseButtonPress, Project(widget, host, host.snapshot.objects[0].transform.position));
        Check(host.Last(EditorCommand::BeginGizmoDrag), "picking resumes once camera matches new size");
        Mouse(widget, QEvent::MouseButtonRelease, QPointF(400, 300));
        std::printf("Viewport DPI transition %.2f: PASS\n", ratio);
    }
    host.applyResize = true;
    host.commands.clear();
    widget.clearFocus();
    widget.ratio = 2;
    Wait(320); // fallback if a platform does not deliver the DPI event to the child
    Check(host.Last(EditorCommand::ViewportResize) && host.camera.width == 1600,
        "physical-size polling resizes an unfocused viewport without a DPI event");
    host.commands.clear();
    widget.resize(900, 500);
    Wait(180);
    Check(host.Last(EditorCommand::ViewportResize) && host.camera.width == 1800 && host.camera.height == 1000,
        "ordinary resize still updates physical dimensions");
    std::printf("Viewport resize fallback and logical resize: PASS\n");
}
} // namespace ViewportDpiTests
