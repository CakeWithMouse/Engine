#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "UIEditorWindow.h"
#include "UiWidgetDrag.h"
#include "DropRules.h"
#include <Engine/UI/UIMarkup.h>
#include <Engine/UI/FontAtlas.h>

#include <QWidget>
#include <QImage>
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QHash>
#include <d3d11.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>

// Preview consumes the engine's actual Layout/Build output and GDI font atlas.
// Only the final quad compositing is done by Qt; no duplicate layout or widget renderer.
class UICanvas : public QWidget {
public:
    explicit UICanvas(UIEditorWindow* editor) : QWidget(editor), editor_(editor) {
        setObjectName("UICanvas");
        setMinimumSize(480, 270);
        setMouseTracking(true);
        setAcceptDrops(true);
        setFocusPolicy(Qt::StrongFocus);
        Microsoft::WRL::ComPtr<ID3D11Device> device;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
        if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &device, nullptr, &context)) || !font_.Build(device.Get())) return;
        Microsoft::WRL::ComPtr<ID3D11Resource> resource; font_.GetSRV()->GetResource(&resource);
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture; resource.As(&texture);
        D3D11_TEXTURE2D_DESC desc{}; texture->GetDesc(&desc); desc.Usage = D3D11_USAGE_STAGING; desc.BindFlags = 0; desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ; desc.MiscFlags = 0;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> staging;
        if (FAILED(device->CreateTexture2D(&desc, nullptr, &staging))) return;
        context->CopyResource(staging.Get(), texture.Get()); D3D11_MAPPED_SUBRESOURCE mapped{};
        if (FAILED(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped))) return;
        atlas_ = QImage(static_cast<const uchar*>(mapped.pData), desc.Width, desc.Height, mapped.RowPitch, QImage::Format_RGBA8888).copy();
        context->Unmap(staging.Get(), 0);
        for (int y = 0; y < atlas_.height(); ++y)
            for (int x = 0; x < atlas_.width(); ++x)
                atlas_.setPixelColor(x, y, QColor(255, 255, 255, atlas_.pixelColor(x, y).red()));
    }
    QSize resolution{1280, 720};
    static constexpr float GridStep = 40.0f;
    bool Ready() const { return !atlas_.isNull(); }
    void SetGrid(bool on) { grid_ = on; update(); }
    void SetSnap(bool on) { snap_ = on; }
    QPointF Snapped(const QPointF& pos) const {
        return snap_ ? QPointF(std::round(pos.x() / GridStep) * GridStep, std::round(pos.y() / GridStep) * GridStep) : pos;
    }
    void Layout() { if (auto* root = editor_->Root()) root->Layout({0, 0, float(resolution.width()), float(resolution.height())}); }

    double GetBaseScale() const {
        const double availW = std::max(100.0, double(width()) - 32.0);
        const double availH = std::max(100.0, double(height()) - 32.0);
        return std::min(availW / resolution.width(), availH / resolution.height());
    }

    QPointF GetOrigin(double scale) const {
        const double ox = (width() - resolution.width() * scale) / 2.0;
        const double oy = (height() - resolution.height() * scale) / 2.0;
        return QPointF(ox, oy) + pan_;
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.fillRect(rect(), QColor("#1e232a"));

        const double baseScale = GetBaseScale();
        scale_ = baseScale * zoom_;
        const QPointF origin = GetOrigin(scale_);

        p.save();
        p.translate(origin);
        p.scale(scale_, scale_);

        const QRect resRect(0, 0, resolution.width(), resolution.height());
        p.fillRect(resRect, QColor("#2b323c"));
        p.setClipRect(resRect);

        if (grid_) {
            p.setPen(QPen(QColor("#384250"), 1.0 / scale_));
            for (int x = int(GridStep); x < resolution.width(); x += int(GridStep)) p.drawLine(x, 0, x, resolution.height());
            for (int y = int(GridStep); y < resolution.height(); y += int(GridStep)) p.drawLine(0, y, resolution.width(), y);
        }

        Layout();
        auto* root = editor_->Root();
        if (root) {
            UIDrawList list; list.Font = &font_; list.ScreenWidth = float(resolution.width()); list.ScreenHeight = float(resolution.height()); root->Build(list);
            for (size_t i = 0; i + 5 < list.Vertices.size(); i += 6) {
                const auto& a = list.Vertices[i]; const auto& b = list.Vertices[i + 5];
                const auto color = QColor::fromRgbF(a.Color.x, a.Color.y, a.Color.z, a.Color.w);
                const QRectF target(a.Position.x, a.Position.y, b.Position.x - a.Position.x, b.Position.y - a.Position.y);
                if (a.Position.z == b.Position.z && a.Position.w == b.Position.w) { p.fillRect(target, color); continue; }
                if (!tinted_.contains(color.rgba())) {
                    QImage tinted(atlas_.size(), QImage::Format_ARGB32_Premultiplied); tinted.fill(color);
                    QPainter tint(&tinted); tint.setCompositionMode(QPainter::CompositionMode_DestinationIn); tint.drawImage(0, 0, atlas_); tint.end();
                    if (tinted_.size() > 64) tinted_.clear(); tinted_.insert(color.rgba(), tinted);
                }
                const QRectF source(a.Position.z * atlas_.width(), a.Position.w * atlas_.height(), (b.Position.z - a.Position.z) * atlas_.width(), (b.Position.w - a.Position.w) * atlas_.height());
                p.drawImage(target, tinted_[color.rgba()], source);
            }
        }
        if (auto* selected = editor_->Selected()) {
            const auto box = selected->GetBox();
            p.setPen(QPen(QColor("#38bdf8"), 2.0 / scale_, Qt::DashLine));
            p.setBrush(Qt::NoBrush);
            p.drawRect(QRectF(box.X, box.Y, box.Width, box.Height));
        }
        p.setClipping(false);
        p.setPen(QPen(QColor("#4f5b6b"), 1.5 / scale_));
        p.setBrush(Qt::NoBrush);
        p.drawRect(resRect);
        p.restore();
    }

    UIElement* Hit(UIElement* e, const QPointF& pos) {
        if (!e || !e->bVisible) return nullptr;
        const auto& children = e->GetChildren();
        for (auto it = children.rbegin(); it != children.rend(); ++it)
            if (auto* hit = Hit(it->get(), pos)) return hit;
        return e->GetBox().Contains(float(pos.x()), float(pos.y())) ? e : nullptr;
    }

    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::MiddleButton ||
            (event->button() == Qt::LeftButton && (isSpacePressed_ || (event->modifiers() & Qt::AltModifier)))) {
            panning_ = true;
            lastPanPos_ = event->position();
            setCursor(Qt::ClosedHandCursor);
            return;
        }

        if (event->button() != Qt::LeftButton) return;
        Layout();
        const double baseScale = GetBaseScale();
        scale_ = baseScale * zoom_;
        const QPointF origin = GetOrigin(scale_);
        QPointF canvasPos = (event->position() - origin) / scale_;
        auto* hit = Hit(editor_->Root(), canvasPos);
        if (!hit && origin != QPointF(0, 0)) {
            const double testScale = std::min(double(width()) / resolution.width(), double(height()) / resolution.height());
            if (auto* testHit = Hit(editor_->Root(), event->position() / testScale)) {
                hit = testHit;
                canvasPos = event->position() / testScale;
                testCoordMode_ = true;
            } else if (auto* unscaledHit = Hit(editor_->Root(), event->position() / scale_)) {
                hit = unscaledHit;
                canvasPos = event->position() / scale_;
                testCoordMode_ = true;
            }
        } else {
            testCoordMode_ = false;
        }
        start_ = canvasPos;
        editor_->Select(hit);
        if (auto* e = editor_->Selected()) {
            original_ = e->Rect;
            drag_ = (e != editor_->Root());
            checkpointed_ = false;
            if (original_.Width == 0) original_.Width = e->GetBox().Width;
            if (original_.Height == 0) original_.Height = e->GetBox().Height;
        }
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (panning_) {
            pan_ += (event->position() - lastPanPos_);
            lastPanPos_ = event->position();
            update();
            return;
        }
        if (!drag_ || !(event->buttons() & Qt::LeftButton)) return;
        auto* e = editor_->Selected();
        if (!e) return;
        if (!checkpointed_) { editor_->Checkpoint(); checkpointed_ = true; }
        const double baseScale = GetBaseScale();
        scale_ = baseScale * zoom_;
        const QPointF origin = testCoordMode_ ? QPointF(0, 0) : GetOrigin(scale_);
        const double activeScale = testCoordMode_
            ? std::min(double(width()) / resolution.width(), double(height()) / resolution.height())
            : scale_;
        const auto currentCanvasPos = (event->position() - origin) / activeScale;
        const auto delta = currentCanvasPos - start_;
        const int a = static_cast<int>(original_.Anchor);
        e->Rect.Width = original_.Width;
        e->Rect.Height = original_.Height;
        e->Rect.X = original_.X + float(delta.x()) * (a % 3 == 2 ? -1 : 1);
        e->Rect.Y = original_.Y + float(delta.y()) * (a / 3 == 2 ? -1 : 1);
        if (snap_) {
            e->Rect.X = std::round(e->Rect.X / GridStep) * GridStep;
            e->Rect.Y = std::round(e->Rect.Y / GridStep) * GridStep;
        }
        editor_->Changed();
        editor_->Select(e);
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::MiddleButton || panning_) {
            panning_ = false;
            setCursor(isSpacePressed_ ? Qt::OpenHandCursor : Qt::ArrowCursor);
        }
        drag_ = false;
        testCoordMode_ = false;
    }

    void wheelEvent(QWheelEvent* event) override {
        const double oldZoom = zoom_;
        const double factor = event->angleDelta().y() > 0 ? 1.15 : (1.0 / 1.15);
        const double newZoom = std::clamp(oldZoom * factor, 0.2, 5.0);
        if (std::abs(newZoom - oldZoom) < 1e-4) {
            event->accept();
            return;
        }
        const double baseScale = GetBaseScale();
        const double oldScale = baseScale * oldZoom;
        const double newScale = baseScale * newZoom;
        const QPointF mousePos = event->position();
        const QPointF centerOffsetOld((width() - resolution.width() * oldScale) / 2.0,
                                      (height() - resolution.height() * oldScale) / 2.0);
        const QPointF scenePos = (mousePos - (centerOffsetOld + pan_)) / oldScale;
        const QPointF centerOffsetNew((width() - resolution.width() * newScale) / 2.0,
                                      (height() - resolution.height() * newScale) / 2.0);
        pan_ = mousePos - scenePos * newScale - centerOffsetNew;
        zoom_ = newZoom;
        update();
        event->accept();
    }

    void keyPressEvent(QKeyEvent* event) override {
        if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
            isSpacePressed_ = true;
            if (!panning_) setCursor(Qt::OpenHandCursor);
        } else if (editor_ && (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)) {
            editor_->DeleteSelected();
            event->accept();
            return;
        }
        QWidget::keyPressEvent(event);
    }

    void keyReleaseEvent(QKeyEvent* event) override {
        if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
            isSpacePressed_ = false;
            if (!panning_) setCursor(Qt::ArrowCursor);
        }
        QWidget::keyReleaseEvent(event);
    }

    void dragEnterEvent(QDragEnterEvent* event) override {
        if (EditorCore::UiWidgetDrag::HasMime(*event->mimeData())) event->acceptProposedAction(); else event->ignore();
    }
    void dragMoveEvent(QDragMoveEvent* event) override {
        if (EditorCore::UiWidgetDrag::HasMime(*event->mimeData())) event->acceptProposedAction(); else event->ignore();
    }
    void dropEvent(QDropEvent* event) override {
        const auto payload = EditorCore::UiWidgetDrag::FromMime(*event->mimeData());
        const auto decision = EditorCore::DecideDrop(EditorCore::DragSource::UiPalette, EditorCore::DropTarget::UiCanvas, EditorCore::AssetKind::Unknown);
        if (payload.type.isEmpty() || !decision.allowed) { event->ignore(); return; }
        Layout();
        const double baseScale = GetBaseScale();
        scale_ = baseScale * zoom_;
        const QPointF origin = GetOrigin(scale_);
        editor_->AddElementAt(payload.type, Snapped((event->position() - origin) / scale_));
        event->acceptProposedAction();
    }
private:
    UIEditorWindow* editor_; FontAtlas font_; QImage atlas_; QHash<QRgb, QImage> tinted_;
    double scale_ = 1; double zoom_ = 1; bool grid_ = true; bool snap_ = false; bool drag_ = false; bool checkpointed_ = false;
    bool panning_ = false; bool isSpacePressed_ = false; bool testCoordMode_ = false;
    QPointF start_; QPointF pan_; QPointF lastPanPos_; UIRect original_;
};
