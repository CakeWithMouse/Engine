#include "ViewportWidget.h"
#include "QtHost.h"
#include "CameraKeys.h"
#include "EditorDialogs.h"
#include "EditorLocale.h"

#include <type_traits>
#include <QPainter>
#include <QPainterPath>
#include <QShowEvent>
#include <QHideEvent>
#include <QResizeEvent>
#include <cmath>
#include <memory>
#include <algorithm>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QTimer>
#include <QElapsedTimer>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QCursor>
#include <QApplication>
#include <QToolButton>
#include <QComboBox>
#include <QButtonGroup>
#include <QMenu>
#include <QAction>
#include <QSignalBlocker>
#include <QLocale>
#include <QRadialGradient>
#include <QDropEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QMimeData>
#include <QUrl>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include "MainWindow.h"
#include <Engine/InputDevice/Keys.h>

namespace {
uint32_t GameKey(QKeyEvent* key)
{
    // Native virtual keys for arbitrary bindings; physical WASD stays layout independent.
    int logical=CameraNavigationKey(key->nativeScanCode(),key->key());
    if (logical=='W'||logical=='A'||logical=='S'||logical=='D'||logical=='Q'||logical=='E')return logical;
    if (key->nativeVirtualKey() && key->nativeVirtualKey()<256) return key->nativeVirtualKey();
    if (logical>=0x20&&logical<=0x7e)return logical;
    switch(logical){case Qt::Key_Control:return 17;case Qt::Key_Shift:return 16;case Qt::Key_Alt:return 18;
    case Qt::Key_Escape:return 27;case Qt::Key_Tab:return 9;case Qt::Key_Return:return 13;
    case Qt::Key_Left:return 37;case Qt::Key_Up:return 38;case Qt::Key_Right:return 39;case Qt::Key_Down:return 40;
    default:return logical>=Qt::Key_F1&&logical<=Qt::Key_F24?112+logical-Qt::Key_F1:0;}
}
}

namespace
{
    QString GizmoModeName(EditorGizmoMode mode)
    {
        switch (mode)
        {
        case EditorGizmoTranslate:
            return Tr("Перемещение", "Move");
        case EditorGizmoRotate:
            return Tr("Вращение", "Rotate");
        case EditorGizmoScale:
            return Tr("Масштаб", "Scale");
        default:
            return Tr("Неизвестно", "Unknown");
        }
    }

    void NormalizeQuat(double q[4])
    {
        const double len = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);

        if (len < 1e-6)
        {
            q[0] = 0.0;
            q[1] = 0.0;
            q[2] = 0.0;
            q[3] = 1.0;
            return;
        }

        const double inv = 1.0 / len;
        q[0] *= inv;
        q[1] *= inv;
        q[2] *= inv;
        q[3] *= inv;
    }

    void MulQuat(const double a[4], const double b[4], double out[4])
    {
        const double x =
            a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1];

        const double y =
            a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0];

        const double z =
            a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3];

        const double w =
            a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2];

        out[0] = x;
        out[1] = y;
        out[2] = z;
        out[3] = w;
    }

    void AxisAngleQuat(double x, double y, double z, double angle, double out[4])
    {
        const double len = std::sqrt(x * x + y * y + z * z);

        if (len < 1e-6)
        {
            out[0] = 0.0;
            out[1] = 0.0;
            out[2] = 0.0;
            out[3] = 1.0;
            return;
        }

        const double inv = 1.0 / len;
        const double half = angle * 0.5;
        const double s = std::sin(half);

        out[0] = x * inv * s;
        out[1] = y * inv * s;
        out[2] = z * inv * s;
        out[3] = std::cos(half);
    }

    QIcon CreateMoveGizmoIcon(const QColor& color)
    {
        QPixmap pix(16, 16);
        pix.fill(Qt::transparent);
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(color, 1.5, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(8, 2, 8, 14);
        p.drawLine(2, 8, 14, 8);
        p.setBrush(color);
        p.setPen(Qt::NoPen);
        QPointF topArrow[] = {{8.0, 1.0}, {6.0, 4.0}, {10.0, 4.0}};
        p.drawPolygon(topArrow, 3);
        QPointF bottomArrow[] = {{8.0, 15.0}, {6.0, 12.0}, {10.0, 12.0}};
        p.drawPolygon(bottomArrow, 3);
        QPointF leftArrow[] = {{1.0, 8.0}, {4.0, 6.0}, {4.0, 10.0}};
        p.drawPolygon(leftArrow, 3);
        QPointF rightArrow[] = {{15.0, 8.0}, {12.0, 6.0}, {12.0, 10.0}};
        p.drawPolygon(rightArrow, 3);
        return QIcon(pix);
    }

    QIcon CreateRotateGizmoIcon(const QColor& color)
    {
        QPixmap pix(16, 16);
        pix.fill(Qt::transparent);
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(color, 1.5, Qt::SolidLine, Qt::RoundCap));
        p.setBrush(Qt::NoBrush);
        p.drawArc(QRectF(2.5, 2.5, 11.0, 11.0), 30 * 16, 270 * 16);
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        QPointF head[] = {{13.5, 7.0}, {10.5, 4.5}, {14.0, 3.5}};
        p.drawPolygon(head, 3);
        return QIcon(pix);
    }

    QIcon CreateScaleGizmoIcon(const QColor& color)
    {
        QPixmap pix(16, 16);
        pix.fill(Qt::transparent);
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(color, 1.5, Qt::SolidLine, Qt::RoundCap));
        p.setBrush(Qt::NoBrush);
        p.drawRect(QRectF(2.5, 7.5, 6.0, 6.0));
        p.drawLine(QPointF(8.5, 7.5), QPointF(13.5, 2.5));
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawRect(QRectF(11.5, 1.5, 3.0, 3.0));
        return QIcon(pix);
    }

    QIcon CreateMagnetIcon(const QColor& color)
    {
        QPixmap pix(16, 16);
        pix.fill(Qt::transparent);
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(color, 2.0, Qt::SolidLine, Qt::FlatCap));
        p.setBrush(Qt::NoBrush);
        QPainterPath path;
        path.moveTo(4, 4);
        path.lineTo(4, 9);
        path.arcTo(QRectF(4, 5, 8, 8), 180, 180);
        path.lineTo(12, 4);
        p.drawPath(path);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0xEF, 0x44, 0x44));
        p.drawRect(QRectF(3, 2, 2.5, 3));
        p.setBrush(QColor(0x3B, 0x82, 0xF6));
        p.drawRect(QRectF(10.5, 2, 2.5, 3));
        return QIcon(pix);
    }

    QIcon CreateSnapGroundIcon(const QColor& color)
    {
        QPixmap pix(16, 16);
        pix.fill(Qt::transparent);
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(QColor(0x22, 0xC5, 0x5E), 1.8, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(2, 13, 14, 13);
        p.setPen(QPen(color, 1.5, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(8, 2, 8, 10);
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        QPointF arrow[] = {{8.0, 11.5}, {5.0, 7.5}, {11.0, 7.5}};
        p.drawPolygon(arrow, 3);
        return QIcon(pix);
    }

    QIcon CreateWireframeIcon(const QColor& color)
    {
        QPixmap pix(16, 16);
        pix.fill(Qt::transparent);
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(color, 1.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        QPointF topFace[] = {{8.0, 2.5}, {13.0, 5.5}, {8.0, 8.5}, {3.0, 5.5}};
        p.drawPolygon(topFace, 4);
        p.drawLine(QPointF(3.0, 5.5), QPointF(3.0, 11.0));
        p.drawLine(QPointF(13.0, 5.5), QPointF(13.0, 11.0));
        p.drawLine(QPointF(8.0, 8.5), QPointF(8.0, 14.0));
        p.drawLine(QPointF(3.0, 11.0), QPointF(8.0, 14.0));
        p.drawLine(QPointF(13.0, 11.0), QPointF(8.0, 14.0));
        return QIcon(pix);
    }

    QIcon CreateSolidIcon(const QColor& color)
    {
        QPixmap pix(16, 16);
        pix.fill(Qt::transparent);
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(color.darker(140), 1.0));
        p.setBrush(color);
        p.drawEllipse(QRectF(2.5, 2.5, 11.0, 11.0));
        return QIcon(pix);
    }

    QIcon CreateMaterialPreviewIcon(const QColor& color)
    {
        QPixmap pix(16, 16);
        pix.fill(Qt::transparent);
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(color, 1.0));
        p.setBrush(color);
        p.drawPie(QRectF(2.5, 2.5, 11.0, 11.0), 90 * 16, 180 * 16);
        p.setBrush(QColor(0xE2, 0xE8, 0xF0));
        p.drawPie(QRectF(2.5, 2.5, 11.0, 11.0), 270 * 16, 180 * 16);
        return QIcon(pix);
    }

    QIcon CreateRenderedIcon(const QColor& color)
    {
        QPixmap pix(16, 16);
        pix.fill(Qt::transparent);
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        QRadialGradient grad(6.0, 6.0, 8.0);
        grad.setColorAt(0.0, Qt::white);
        grad.setColorAt(0.3, color);
        grad.setColorAt(1.0, color.darker(200));
        p.setPen(QPen(color.darker(150), 1.0));
        p.setBrush(grad);
        p.drawEllipse(QRectF(2.5, 2.5, 11.0, 11.0));
        return QIcon(pix);
    }
} // namespace

class ViewportHeaderBar : public QFrame
    {
    public:
        explicit ViewportHeaderBar(QWidget* parent = nullptr) : QFrame(parent)
        {
            setAttribute(Qt::WA_StyledBackground, true);
            setAttribute(Qt::WA_NativeWindow, true);
        }
    protected:
        void mousePressEvent(QMouseEvent* e) override { e->accept(); }
        void mouseReleaseEvent(QMouseEvent* e) override { e->accept(); }
        void mouseMoveEvent(QMouseEvent* e) override { e->accept(); }
    };

class ViewportStatsOverlay : public QFrame
{
public:
    explicit ViewportStatsOverlay(QWidget* parent = nullptr) : QFrame(parent)
    {
        setAttribute(Qt::WA_StyledBackground, true);
        setAttribute(Qt::WA_NativeWindow, true);
        setAttribute(Qt::WA_TransparentForMouseEvents, true);

        setStyleSheet(QStringLiteral(
            "ViewportStatsOverlay {"
            "  background-color: rgba(18, 20, 24, 0.88);"
            "  border: 1px solid rgba(255, 255, 255, 0.10);"
            "  border-radius: 6px;"
            "}"
            "QLabel {"
            "  color: #94A3B8;"
            "  font-family: 'Consolas', 'Segoe UI', monospace;"
            "  font-size: 11px;"
            "  background: transparent;"
            "  border: none;"
            "  padding: 0px;"
            "}"
        ));

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(10, 8, 10, 8);
        layout->setSpacing(3);

        lblFps_ = new QLabel(this);
        lblObjects_ = new QLabel(this);
        lblTris_ = new QLabel(this);
        lblSelected_ = new QLabel(this);
        lblModes_ = new QLabel(this);

        for (auto* lbl : {lblFps_, lblObjects_, lblTris_, lblSelected_, lblModes_})
        {
            lbl->setAttribute(Qt::WA_TransparentForMouseEvents, true);
            lbl->setTextFormat(Qt::RichText);
            layout->addWidget(lbl);
        }
    }

    void UpdateStats(float fps, float frameTimeMs,
                     int totalObjects, int lightCount, int meshCount, uint64_t estTris,
                     const QString& selectedName, const QString& selectedCoords,
                     const QString& shadingStr, const QString& snapStr)
    {
        lblFps_->setText(QStringLiteral(
            "<span style='color:#38BDF8; font-weight:bold;'>FPS:</span> <span style='color:#22C55E; font-weight:bold;'>%1</span> "
            "<span style='color:#64748B;'>(%2 ms)</span>")
            .arg(fps, 0, 'f', 1)
            .arg(frameTimeMs, 0, 'f', 1));

        lblObjects_->setText(QStringLiteral(
            "<span style='color:#38BDF8; font-weight:bold;'>%1:</span> <span style='color:#F1F5F9; font-weight:bold;'>%2</span> | "
            "<span style='color:#F59E0B;'>%3:</span> <b>%4</b> | "
            "<span style='color:#A855F7;'>%5:</span> <b>%6</b>")
            .arg(Tr("Объекты", "Objects")).arg(totalObjects)
            .arg(Tr("Источники", "Lights")).arg(lightCount)
            .arg(Tr("Меши", "Meshes")).arg(meshCount));

        lblTris_->setText(QStringLiteral(
            "<span style='color:#38BDF8; font-weight:bold;'>%1:</span> <span style='color:#F1F5F9; font-weight:bold;'>%2</span>")
            .arg(Tr("Треугольники (est.)", "Tris (est.)"))
            .arg(QLocale().toString(static_cast<qlonglong>(estTris))));

        if (!selectedCoords.isEmpty())
        {
            lblSelected_->setText(QStringLiteral(
                "<span style='color:#38BDF8; font-weight:bold;'>%1:</span> "
                "<span style='color:#38BDF8;'><b>%2</b></span> <span style='color:#CBD5E1;'>(%3)</span>")
                .arg(Tr("Выделено", "Selected"))
                .arg(selectedName)
                .arg(selectedCoords));
        }
        else
        {
            lblSelected_->setText(QStringLiteral(
                "<span style='color:#38BDF8; font-weight:bold;'>%1:</span> <span style='color:#64748B;'>%2</span>")
                .arg(Tr("Выделено", "Selected"))
                .arg(selectedName));
        }

        lblModes_->setText(QStringLiteral(
            "<span style='color:#38BDF8; font-weight:bold;'>%1:</span> <span style='color:#F1F5F9;'>%2</span> | "
            "<span style='color:#38BDF8; font-weight:bold;'>%3:</span> <span style='color:#F1F5F9;'>%4</span>")
            .arg(Tr("Шейдинг", "Shading")).arg(shadingStr)
            .arg(Tr("Привязка", "Snap")).arg(snapStr));

        adjustSize();
    }

protected:
    void mousePressEvent(QMouseEvent* e) override { e->ignore(); }
    void mouseReleaseEvent(QMouseEvent* e) override { e->ignore(); }
    void mouseMoveEvent(QMouseEvent* e) override { e->ignore(); }

private:
    QLabel* lblFps_ = nullptr;
    QLabel* lblObjects_ = nullptr;
    QLabel* lblTris_ = nullptr;
    QLabel* lblSelected_ = nullptr;
    QLabel* lblModes_ = nullptr;
};

class AdjustLastOperationPanel : public QFrame
{
public:
    explicit AdjustLastOperationPanel(EditorQtHost* host, QWidget* parent = nullptr)
        : QFrame(parent), host_(host)
    {
        setAttribute(Qt::WA_StyledBackground, true);
        setStyleSheet(QStringLiteral(
            "AdjustLastOperationPanel {"
            "  background-color: rgba(28, 30, 36, 235);"
            "  border: 1px solid rgba(80, 85, 95, 200);"
            "  border-radius: 6px;"
            "}"
            "QLabel {"
            "  color: #bbb;"
            "  font-size: 11px;"
            "}"
            "QLineEdit, QDoubleSpinBox {"
            "  background: #202228;"
            "  color: #eee;"
            "  border: 1px solid #444;"
            "  border-radius: 3px;"
            "  font-size: 11px;"
            "  padding: 1px 3px;"
            "}"
            "QPushButton#CloseBtn {"
            "  background: transparent;"
            "  color: #888;"
            "  border: none;"
            "  font-weight: bold;"
            "}"
            "QPushButton#CloseBtn:hover {"
            "  color: #fff;"
            "}"
        ));

        auto* mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(8, 6, 8, 8);
        mainLayout->setSpacing(4);

        auto* headerLayout = new QHBoxLayout;
        auto* titleLabel = new QLabel(Tr("Параметры операции", "Adjust Last Operation"), this);
        titleLabel->setStyleSheet(QStringLiteral("font-weight: bold; color: #4fa3ff;"));
        auto* closeBtn = new QPushButton(QStringLiteral("✕"), this);
        closeBtn->setObjectName(QStringLiteral("CloseBtn"));
        closeBtn->setFixedSize(16, 16);
        connect(closeBtn, &QPushButton::clicked, this, &QWidget::hide);
        headerLayout->addWidget(titleLabel);
        headerLayout->addStretch();
        headerLayout->addWidget(closeBtn);
        mainLayout->addLayout(headerLayout);

        auto* nameLayout = new QHBoxLayout;
        nameLayout->addWidget(new QLabel(Tr("Имя", "Name") + QStringLiteral(":"), this));
        nameEdit_ = new QLineEdit(this);
        nameLayout->addWidget(nameEdit_, 1);
        mainLayout->addLayout(nameLayout);

        connect(nameEdit_, &QLineEdit::editingFinished, this, [this] {
            if (updating_ || !objectId_ || !host_) return;
            EditorCommand c;
            c.type = EditorCommand::RenameObject;
            c.objectId = objectId_;
            c.name = nameEdit_->text().trimmed();
            if (!c.name.isEmpty()) host_->PostCommand(c);
        });

        auto* posLayout = new QHBoxLayout;
        posLayout->addWidget(new QLabel(Tr("Позиция", "Pos") + QStringLiteral(":"), this));
        posX_ = new QDoubleSpinBox(this);
        posY_ = new QDoubleSpinBox(this);
        posZ_ = new QDoubleSpinBox(this);
        for (auto* s : {posX_, posY_, posZ_}) {
            s->setRange(-10000.0, 10000.0);
            s->setDecimals(2);
            s->setSingleStep(0.5);
            posLayout->addWidget(s);
            connect(s, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double) {
                if (updating_ || !objectId_ || !host_) return;
                transform_.position[0] = static_cast<float>(posX_->value());
                transform_.position[1] = static_cast<float>(posY_->value());
                transform_.position[2] = static_cast<float>(posZ_->value());
                EditorCommand c;
                c.type = EditorCommand::UpdateObjectTransform;
                c.objectId = objectId_;
                c.transform = transform_;
                c.path = QStringLiteral("adjust");
                host_->PostCommand(c);
            });
        }
        mainLayout->addLayout(posLayout);

        auto* scaleLayout = new QHBoxLayout;
        scaleLayout->addWidget(new QLabel(Tr("Масштаб", "Scale") + QStringLiteral(":"), this));
        scaleX_ = new QDoubleSpinBox(this);
        scaleY_ = new QDoubleSpinBox(this);
        scaleZ_ = new QDoubleSpinBox(this);
        for (auto* s : {scaleX_, scaleY_, scaleZ_}) {
            s->setRange(0.001, 1000.0);
            s->setDecimals(2);
            s->setSingleStep(0.2);
            scaleLayout->addWidget(s);
            connect(s, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double) {
                if (updating_ || !objectId_ || !host_) return;
                transform_.scale[0] = static_cast<float>(scaleX_->value());
                transform_.scale[1] = static_cast<float>(scaleY_->value());
                transform_.scale[2] = static_cast<float>(scaleZ_->value());
                EditorCommand c;
                c.type = EditorCommand::UpdateObjectTransform;
                c.objectId = objectId_;
                c.transform = transform_;
                c.path = QStringLiteral("adjust");
                host_->PostCommand(c);
            });
        }
        mainLayout->addLayout(scaleLayout);

        setMinimumWidth(230);
        hide();
    }

    void ShowForObject(uint64_t id, const QString& name, const EngineSceneTransform& transform)
    {
        objectId_ = id;
        transform_ = transform;
        updating_ = true;
        nameEdit_->setText(name);
        posX_->setValue(transform.position[0]);
        posY_->setValue(transform.position[1]);
        posZ_->setValue(transform.position[2]);
        scaleX_->setValue(transform.scale[0]);
        scaleY_->setValue(transform.scale[1]);
        scaleZ_->setValue(transform.scale[2]);
        updating_ = false;

        adjustSize();
        if (parentWidget()) {
            move(12, parentWidget()->height() - height() - 12);
        }
        show();
        raise();
    }

    void SetObjectId(uint64_t id) { objectId_ = id; }

private:
    EditorQtHost* host_ = nullptr;
    uint64_t objectId_ = 0;
    bool updating_ = false;
    EngineSceneTransform transform_{};

    QLineEdit* nameEdit_ = nullptr;
    QDoubleSpinBox* posX_ = nullptr;
    QDoubleSpinBox* posY_ = nullptr;
    QDoubleSpinBox* posZ_ = nullptr;
    QDoubleSpinBox* scaleX_ = nullptr;
    QDoubleSpinBox* scaleY_ = nullptr;
    QDoubleSpinBox* scaleZ_ = nullptr;
};

class PieMenuWidget : public QWidget
{
public:
    struct PieItem {
        QString label;
        int actionId;
    };

    explicit PieMenuWidget(std::function<void(int)> onSelect, QWidget* parent = nullptr)
        : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint),
          onSelect_(std::move(onSelect))
    {
        setAttribute(Qt::WA_TranslucentBackground);
        setMouseTracking(true);
        setFixedSize(260, 260);

        items_ = {
            {Tr("Перемещение (1)", "Move (1)"), 2},
            {Tr("Вращение (2)", "Rotate (2)"), 3},
            {Tr("Масштаб (3)", "Scale (3)"), 4},
            {Tr("Фокус (F)", "Focus (F)"), 5},
            {Tr("Удалить (Del)", "Delete (Del)"), 6},
            {Tr("Дубликат", "Duplicate"), 7},
            {Tr("+ Куб", "+ Add Cube"), 0},
            {Tr("+ Свет", "+ Add Light"), 1}
        };
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        const QPointF center(width() / 2.0, height() / 2.0);
        const double outerRadius = 120.0;
        const double innerRadius = 38.0;

        p.setPen(QPen(QColor(70, 75, 90, 220), 1.5));
        p.setBrush(QColor(22, 24, 30, 235));
        p.drawEllipse(center, outerRadius, outerRadius);

        for (int i = 0; i < 8; ++i)
        {
            const double midAngleDeg = i * 45.0;
            const double startAngleDeg = midAngleDeg - 22.5;

            if (hoveredSector_ == i)
            {
                QPainterPath path;
                path.moveTo(center);
                path.arcTo(QRectF(center.x() - outerRadius, center.y() - outerRadius, outerRadius * 2, outerRadius * 2), -startAngleDeg, -45.0);
                path.closeSubpath();

                p.setPen(Qt::NoPen);
                p.setBrush(QColor(40, 110, 220, 170));
                p.drawPath(path);
            }

            const double rad = startAngleDeg * 3.1415926535 / 180.0;
            p.setPen(QPen(QColor(60, 65, 80, 150), 1.0));
            p.drawLine(center + QPointF(innerRadius * std::cos(rad), innerRadius * std::sin(rad)),
                       center + QPointF(outerRadius * std::cos(rad), outerRadius * std::sin(rad)));
        }

        p.setPen(QPen(QColor(80, 85, 105, 200), 1.5));
        p.setBrush(QColor(16, 18, 22, 250));
        p.drawEllipse(center, innerRadius, innerRadius);

        p.setPen(QColor(160, 165, 180));
        QFont hubFont = p.font();
        hubFont.setPointSize(8);
        hubFont.setBold(true);
        p.setFont(hubFont);
        p.drawText(QRectF(center.x() - innerRadius, center.y() - innerRadius, innerRadius * 2, innerRadius * 2),
                   Qt::AlignCenter, QStringLiteral("PIE"));

        QFont labelFont = p.font();
        labelFont.setPointSize(8);
        labelFont.setBold(false);
        p.setFont(labelFont);

        const double textRadius = 78.0;
        for (int i = 0; i < 8; ++i)
        {
            const double rad = (i * 45.0) * 3.1415926535 / 180.0;
            const QPointF textPos = center + QPointF(textRadius * std::cos(rad), textRadius * std::sin(rad));
            const QRectF textRect(textPos.x() - 45, textPos.y() - 14, 90, 28);

            p.setPen(hoveredSector_ == i ? QColor(255, 255, 255) : QColor(200, 205, 215));
            p.drawText(textRect, Qt::AlignCenter, items_[i].label);
        }
    }

    void mouseMoveEvent(QMouseEvent* e) override
    {
        const QPointF center(width() / 2.0, height() / 2.0);
        const QPointF diff = e->position() - center;
        const double dist = std::hypot(diff.x(), diff.y());

        int prevHover = hoveredSector_;
        if (dist > 30.0 && dist <= 125.0)
        {
            double angle = std::atan2(diff.y(), diff.x()) * 180.0 / 3.1415926535;
            if (angle < 0.0) angle += 360.0;
            hoveredSector_ = static_cast<int>(std::floor((angle + 22.5) / 45.0)) % 8;
        }
        else
        {
            hoveredSector_ = -1;
        }

        if (hoveredSector_ != prevHover)
            update();
    }

    void mouseReleaseEvent(QMouseEvent* e) override
    {
        if (e->button() == Qt::LeftButton || e->button() == Qt::RightButton)
        {
            if (hoveredSector_ >= 0 && hoveredSector_ < 8 && onSelect_)
            {
                const int actId = items_[hoveredSector_].actionId;
                close();
                onSelect_(actId);
                return;
            }
            close();
        }
    }

    void keyPressEvent(QKeyEvent* e) override
    {
        if (e->key() == Qt::Key_Escape)
        {
            close();
        }
    }

private:
    std::function<void(int)> onSelect_;
    std::vector<PieItem> items_;
    int hoveredSector_ = -1;
};

ViewportWidget::ViewportWidget(EditorQtHost* host, QWidget* parent)
    : QWidget(parent), host_(host)
{
    controlScheme_ = EditorSettings::GetControlScheme();
    controlSchemeSubId_ = EditorSettings::SubscribeControlSchemeChanged([this](ControlScheme scheme) {
        controlScheme_ = scheme;
        updateGizmoToolTip();
    });

    if (host_->HasViewport())
    {
        setAttribute(Qt::WA_NativeWindow, true);
        // Qt не закрашивает и не буферизует: сюда Present'ит движок
        setAttribute(Qt::WA_PaintOnScreen, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
        setAttribute(Qt::WA_OpaquePaintEvent, true);
        setAutoFillBackground(false);
    }
    setFocusPolicy(Qt::StrongFocus);
    setAcceptDrops(true);
    updateGizmoToolTip();

    adjustPanel_ = new AdjustLastOperationPanel(host_, this);
    BuildHeaderBar();

    statsOverlay_ = new ViewportStatsOverlay(this);
    statsOverlay_->move(10, 36);
    statsOverlay_->setVisible(actOverlayStats_ && actOverlayStats_->isChecked());

    langSubId_ = EditorLocale::Instance().SubscribeLanguageChanged([this] {
        RetranslateUi();
    });
    RetranslateUi();

    auto* timer = new QTimer(this);
    auto elapsed = std::make_shared<QElapsedTimer>();
    elapsed->start();

    connect(timer, &QTimer::timeout, this, [this, elapsed]
    {
        const float dt = std::min(0.05f, static_cast<float>(elapsed->restart()) / 1000.0f);
        const float instantFps = (dt > 1e-5f) ? (1.0f / dt) : 60.0f;
        const float instantMs = dt * 1000.0f;
        fpsSmooth_ = fpsSmooth_ * 0.92f + instantFps * 0.08f;
        frameTimeSmooth_ = frameTimeSmooth_ * 0.92f + instantMs * 0.08f;

        if (statsOverlay_ && statsOverlay_->isVisible())
        {
            if (++statsUpdateCounter_ >= 6)
            {
                statsUpdateCounter_ = 0;
                UpdateStatsOverlay();
            }
        }

        const int state=host_->PlayState();
        if(inputState_!=state){inputState_=state;keys_.clear();looking_=rmbPressed_=false;EndObjectDrag();}

        if (!attached_ || !hasFocus() || !isActiveWindow()) return;
        if (state != EditorQtHost::Edit && host_->HasScene()) return;

        const float speed = dt * (keys_.contains(Qt::Key_Shift) ? 4.0f : 1.0f);

        // Если есть выделенный объект, управляем им, иначе камерой
        if (host_->HasSelection())
        {
            float right = static_cast<float>(keys_.contains(Qt::Key_Right) - keys_.contains(Qt::Key_Left)) * speed;
            float up = static_cast<float>(keys_.contains(Qt::Key_PageUp) - keys_.contains(Qt::Key_PageDown)) * speed;
            float forward = static_cast<float>(keys_.contains(Qt::Key_Up) - keys_.contains(Qt::Key_Down)) * speed;

            if (right != 0.0f || up != 0.0f || forward != 0.0f)
            {
                host_->MoveSelectedObject(right, up, forward);
            }
        }
        {
            EditorCommand c;
            c.type = EditorCommand::ViewportCamera;
            c.right = static_cast<float>(keys_.contains(Qt::Key_D) - keys_.contains(Qt::Key_A)) * speed;
            c.up = static_cast<float>(keys_.contains(Qt::Key_E) - keys_.contains(Qt::Key_Q)) * speed;
            c.forward = static_cast<float>(keys_.contains(Qt::Key_W) - keys_.contains(Qt::Key_S)) * speed;
            if (c.right || c.up || c.forward)
            {
                host_->PostCommand(c);
                const float cy = std::cos(camYaw_);
                const float sy = std::sin(camYaw_);
                const float cp = std::cos(camPitch_);
                const float sp = std::sin(camPitch_);
                camPos_[0] += (sy * c.right + cy * cp * c.forward);
                camPos_[1] += (c.up + sp * c.forward);
                camPos_[2] += (-cy * c.right + sy * cp * c.forward);
            }
        }
    });
    timer->start(16);
    setMinimumSize(320, 200);
}

ViewportWidget::~ViewportWidget()
{
    if (langSubId_ != 0)
    {
        EditorLocale::Instance().UnsubscribeLanguageChanged(langSubId_);
        langSubId_ = 0;
    }
    if (controlSchemeSubId_ != 0)
    {
        EditorSettings::UnsubscribeControlSchemeChanged(controlSchemeSubId_);
        controlSchemeSubId_ = 0;
    }
}

QPaintEngine* ViewportWidget::paintEngine() const
{
    // nullptr только когда контент нативный; в режиме заглушки нужен обычный paintEngine
    return host_->HasViewport() ? nullptr : QWidget::paintEngine();
}

void ViewportWidget::showEvent(QShowEvent* e)
{
    QWidget::showEvent(e);
    if (host_->HasViewport() && !attached_)
        SendAttach();
}

void ViewportWidget::hideEvent(QHideEvent* e)
{
    if (attached_)
    {
        EditorCommand c;
        c.type = EditorCommand::ViewportDetach;
        host_->PostCommand(c);
        attached_ = false;
    }
    QWidget::hideEvent(e);
}

void ViewportWidget::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    if (attached_)
        SendResize();
    if (viewportHeaderBar_)
    {
        viewportHeaderBar_->setGeometry(0, 0, width(), 28);
        viewportHeaderBar_->raise();
    }
    if (statsOverlay_)
    {
        statsOverlay_->move(10, 36);
        statsOverlay_->raise();
    }
    if (adjustPanel_ && adjustPanel_->isVisible())
    {
        adjustPanel_->move(12, height() - adjustPanel_->height() - 12);
        adjustPanel_->raise();
    }
}

void ViewportWidget::paintEvent(QPaintEvent*)
{
    if (host_->HasViewport())
        return; // нативный контент, не трогаем
    QPainter p(this);
    p.fillRect(rect(), QColor(30, 30, 30));
    p.setPen(QColor(200, 200, 200));
    p.drawText(rect(), Qt::AlignCenter,
               QStringLiteral("No scene preview is available for this project."));
}

void ViewportWidget::SendAttach()
{
    EditorCommand c;
    c.type = EditorCommand::ViewportAttach;
    c.hwnd = (void*)winId(); // создаёт нативное окно виджета
    c.w = PhysW();
    c.h = PhysH();
    host_->PostCommand(c);
    attached_ = true;
}

void ViewportWidget::SendResize()
{
    EditorCommand c;
    c.type = EditorCommand::ViewportResize;
    c.w = PhysW();
    c.h = PhysH();
    host_->PostCommand(c);
}

uint32_t ViewportWidget::PhysW() const { return static_cast<uint32_t>(std::lround(width() * devicePixelRatioF())); }
uint32_t ViewportWidget::PhysH() const { return static_cast<uint32_t>(std::lround(height() * devicePixelRatioF())); }

bool ViewportWidget::event(QEvent* e)
{
    if (e->type() == QEvent::DragEnter)
    {
        auto* de = static_cast<QDragEnterEvent*>(e);
        if (de->mimeData()->hasUrls() || de->mimeData()->hasText())
        {
            de->acceptProposedAction();
            return true;
        }
    }
    if (e->type() == QEvent::DragMove)
    {
        auto* dm = static_cast<QDragMoveEvent*>(e);
        dm->acceptProposedAction();
        return true;
    }
    if (e->type() == QEvent::Drop)
    {
        auto* drop = static_cast<QDropEvent*>(e);
        HandleViewportDrop(drop);
        drop->acceptProposedAction();
        return true;
    }

    if(e->type()==QEvent::FocusIn || (e->type()==QEvent::WindowActivate && hasFocus())){
        EditorCommand c;c.type=EditorCommand::InputFocus;c.w=1;host_->PostCommand(c);
    }
    if (e->type() == QEvent::FocusOut || e->type() == QEvent::WindowDeactivate || e->type() == QEvent::Hide)
    {
        EditorCommand c;c.type=EditorCommand::InputFocus;c.w=0;host_->PostCommand(c);
        looking_ = false;
        rmbPressed_ = false;
        rmbMoved_ = false;
        mmbPressed_ = false;

        EndObjectDrag();

        keys_.clear();
    }

    // Qt owns editor/UI shortcuts and text widgets. Only this focused widget feeds the game.
    if(attached_ && host_->HasScene() && host_->PlayState()!=EditorQtHost::Edit)
    {
        const bool isFocused = hasFocus() || (window() && window()->focusWidget() == this);
        if((e->type()==QEvent::KeyPress||e->type()==QEvent::KeyRelease)&&isFocused){
            auto* key=static_cast<QKeyEvent*>(e);if(key->isAutoRepeat())return true;
            const int nav=CameraNavigationKey(key->nativeScanCode(),key->key());
            if((nav==Qt::Key_Space||nav==Qt::Key_P)&&key->modifiers()==Qt::NoModifier){
                if(e->type()==QEvent::KeyPress){EditorCommand c;c.type=nav==Qt::Key_Space?EditorCommand::TogglePlay:EditorCommand::TogglePause;host_->PostCommand(c);}return true;
            }
            EditorCommand f;f.type=EditorCommand::InputFocus;f.w=1;host_->PostCommand(f);
            EditorCommand c;c.type=EditorCommand::InputKey;c.w=GameKey(key);c.h=e->type()==QEvent::KeyPress;host_->PostCommand(c);return true;
        }
        if(e->type()==QEvent::MouseButtonPress||e->type()==QEvent::MouseButtonRelease){
            auto* mouse=static_cast<QMouseEvent*>(e);if(e->type()==QEvent::MouseButtonPress){activateWindow();setFocus(Qt::MouseFocusReason);lastMouse_=mouse->position();}
            EditorCommand c;c.type=EditorCommand::InputKey;c.w=static_cast<uint32_t>(mouse->button()==Qt::RightButton?Keys::RightButton:mouse->button()==Qt::MiddleButton?Keys::MiddleButton:Keys::LeftButton);
            c.h=e->type()==QEvent::MouseButtonPress;host_->PostCommand(c);return true;
        }
        if(e->type()==QEvent::MouseMove&&hasFocus()){
            auto* mouse=static_cast<QMouseEvent*>(e);auto delta=mouse->position()-lastMouse_;lastMouse_=mouse->position();
            EditorCommand c;c.type=EditorCommand::InputPointer;c.yaw=static_cast<float>(delta.x());c.pitch=static_cast<float>(delta.y());host_->PostCommand(c);return true;
        }
        if(e->type()==QEvent::Wheel&&hasFocus()){
            EditorCommand c;c.type=EditorCommand::InputPointer;c.zoom=static_cast<float>(static_cast<QWheelEvent*>(e)->angleDelta().y())/120.f;host_->PostCommand(c);return true;
        }
        if(e->type()==QEvent::ContextMenu)return true;
    }

    if (qEnvironmentVariableIsSet("ENGINE_VIEWPORT_INPUT_TRACE") && e->type() == QEvent::KeyPress)
        host_->Log(qPrintable(
            QString("InputKey key=%1 focus=%2").arg(static_cast<QKeyEvent*>(e)->key()).arg(hasFocus())));

    // Глушим стандартное контекстное меню, чтобы показать своё.
    if (e->type() == QEvent::ContextMenu)
    {
        e->accept();
        return true;
    }

    if (e->type() == QEvent::MouseButtonPress)
    {
        auto* mouse = static_cast<QMouseEvent*>(e);

        activateWindow();
        setFocus(Qt::MouseFocusReason);

        if (qEnvironmentVariableIsSet("ENGINE_VIEWPORT_INPUT_TRACE"))
            host_->Log(qPrintable(
                QString("InputFocus focus=%1 active=%2 policy=%3").arg(hasFocus()).arg(isActiveWindow()).arg(focusPolicy
                    ())));

        if (mouse->button() == Qt::LeftButton && attached_ && host_->HasScene())
        {
            lmbPressed_ = true;
            lmbMoved_ = false;
            draggingSelectedObject_ = false;

            lmbPressPos_ = mouse->position().toPoint();
            lastDragPos_ = mouse->position();

            EditorCommand c;
            c.type = EditorCommand::PickObject;
            c.right = static_cast<float>(2.0 * mouse->position().x() / width() - 1.0);
            c.up = static_cast<float>(1.0 - 2.0 * mouse->position().y() / height());
            host_->PostCommand(c);

            return true;
        }
        if (mouse->button() == Qt::RightButton)
        {
            rmbPressed_ = true;
            rmbMoved_ = false;
            rmbPressPos_ = mouse->position().toPoint();
            lastMouse_ = mouse->position();
            return true;
        }
        if (mouse->button() == Qt::MiddleButton)
        {
            mmbPressed_ = true;
            mmbPressPos_ = mouse->position().toPoint();
            lastMouse_ = mouse->position();
            return true;
        }
    }

    if (e->type() == QEvent::MouseMove)
    {
        auto* mouse = static_cast<QMouseEvent*>(e);

        if (mmbPressed_)
        {
            const auto delta = mouse->position() - lastMouse_;
            lastMouse_ = mouse->position();
            EditorCommand c;
            c.type = EditorCommand::ViewportCamera;
            if (mouse->modifiers() & Qt::ShiftModifier)
            {
                c.right = static_cast<float>(-delta.x()) * 0.01f;
                c.up = static_cast<float>(delta.y()) * 0.01f;
            }
            else
            {
                c.yaw = static_cast<float>(delta.x()) * 0.005f;
                c.pitch = static_cast<float>(delta.y()) * 0.005f;
            }
            host_->PostCommand(c);
            return true;
        }

        if (rmbPressed_ && !rmbMoved_)
        {
            const QPoint delta = mouse->position().toPoint() - rmbPressPos_;

            if (delta.manhattanLength() >= 4)
            {
                rmbMoved_ = true;
                looking_ = attached_;
                lastMouse_ = mouse->position();
            }
        }

        if (looking_)
        {
            const auto delta = mouse->position() - lastMouse_;
            lastMouse_ = mouse->position();

            EditorCommand c;
            c.type = EditorCommand::ViewportCamera;
            c.yaw = static_cast<float>(delta.x()) * 0.005f;
            c.pitch = static_cast<float>(delta.y()) * 0.005f;

            camYaw_ -= c.yaw;
            camPitch_ -= c.pitch;
            camPitch_ = std::max(-1.5f, std::min(1.5f, camPitch_));

            host_->PostCommand(c);
            return true;
        }

        if (lmbPressed_ && !draggingSelectedObject_)
        {
            const QPoint delta = mouse->position().toPoint() - lmbPressPos_;

            if (!lmbMoved_ && delta.manhattanLength() > 6)
                lmbMoved_ = true;

            if (lmbMoved_ && host_->HasSelection())
            {
                if (BeginObjectDrag(mouse->position()))
                    return true;
            }
        }

        if (draggingSelectedObject_)
        {
            const bool isCtrlPressed = (mouse->modifiers() & Qt::ControlModifier) != 0;
            const bool isSnapActive = snapEnabled_ || isCtrlPressed;
            const QPointF total = mouse->position() - dragStartPos_;

            if (gizmoMode_ == EditorGizmoTranslate)
            {
                float dx = 0.0f;
                float dy = 0.0f;
                float dz = 0.0f;

                if (activeAxis_ == AxisLock::X)
                {
                    dx = static_cast<float>(total.x()) * 0.02f;
                }
                else if (activeAxis_ == AxisLock::Y)
                {
                    dy = static_cast<float>(-total.y()) * 0.02f;
                }
                else if (activeAxis_ == AxisLock::Z)
                {
                    dz = static_cast<float>(-total.y()) * 0.02f;
                }
                else
                {
                    if (mouse->modifiers() & Qt::ShiftModifier)
                    {
                        dz = static_cast<float>(-total.y()) * 0.02f;
                    }
                    else
                    {
                        dx = static_cast<float>(total.x()) * 0.02f;
                        dy = static_cast<float>(-total.y()) * 0.02f;
                    }
                }

                float newX = dragStartTransform_.position[0] + dx;
                float newY = dragStartTransform_.position[1] + dy;
                float newZ = dragStartTransform_.position[2] + dz;

                if (isSnapActive && moveSnapStep_ > 1e-4)
                {
                    if (activeAxis_ == AxisLock::X || activeAxis_ == AxisLock::All)
                        newX = static_cast<float>(std::round(newX / moveSnapStep_) * moveSnapStep_);
                    if (activeAxis_ == AxisLock::Y || activeAxis_ == AxisLock::All)
                        newY = static_cast<float>(std::round(newY / moveSnapStep_) * moveSnapStep_);
                    if (activeAxis_ == AxisLock::Z || activeAxis_ == AxisLock::All)
                        newZ = static_cast<float>(std::round(newZ / moveSnapStep_) * moveSnapStep_);
                }

                EngineSceneTransform t = dragStartTransform_;
                t.position[0] = newX;
                t.position[1] = newY;
                t.position[2] = newZ;
                PostSelectedTransform(t);
                lastDragPos_ = mouse->position();
            }
            else
            {
                ApplyRotateScaleGizmo(mouse->position(), isSnapActive);
                lastDragPos_ = mouse->position();
            }

            return true;
        }
    }

    if (e->type() == QEvent::MouseButtonRelease)
    {
        auto* mouse = static_cast<QMouseEvent*>(e);

        if (mouse->button() == Qt::LeftButton && (lmbPressed_ || draggingSelectedObject_))
        {
            EndObjectDrag();
            return true;
        }

        if (mouse->button() == Qt::MiddleButton)
        {
            mmbPressed_ = false;
            return true;
        }

        if (mouse->button() == Qt::RightButton)
        {
            looking_ = false;

            // Если не было движения мышью — показываем меню.
            if (rmbPressed_ && !rmbMoved_)
            {
                lastSpawnPos_ = ComputeSpawnPosition(mouse->position().toPoint());
                emit contextMenuRequested(mapToGlobal(mouse->position().toPoint()));
            }

            rmbPressed_ = false;
            rmbMoved_ = false;
            keys_.remove(Qt::Key_W);
            keys_.remove(Qt::Key_A);
            keys_.remove(Qt::Key_S);
            keys_.remove(Qt::Key_D);
            keys_.remove(Qt::Key_Q);
            keys_.remove(Qt::Key_E);

            return true;
        }
    }

    if (!attached_)
        return QWidget::event(e);

    if (e->type() == QEvent::Wheel && hasFocus())
    {
        EditorCommand c;
        c.type = EditorCommand::ViewportCamera;
        c.zoom = static_cast<float>(static_cast<QWheelEvent*>(e)->angleDelta().y()) / 120.0f;
        host_->PostCommand(c);
        return true;
    }

    if ((e->type() == QEvent::KeyPress || e->type() == QEvent::KeyRelease) && hasFocus())
    {
        auto* key = static_cast<QKeyEvent*>(e);
        if ((key->modifiers() & Qt::ControlModifier) && e->type()==QEvent::KeyPress) return QWidget::event(e);

        if (!key->isAutoRepeat())
        {
            if (e->type() == QEvent::KeyPress && key->key() == Qt::Key_Escape) {
                ClearSelection();
                return true;
            }
            if (e->type() == QEvent::KeyPress && key->key() == Qt::Key_End && host_->HasScene()) {
                SnapToGround();
                return true;
            }
            // Обработка Tab для циклического выбора объектов
            if (e->type() == QEvent::KeyPress && key->key() == Qt::Key_Tab)
            {
                host_->FetchSceneAndSelectNext();
                return true;
            }

            if (e->type() == QEvent::KeyPress && key->key() == Qt::Key_F && host_->HasScene())
            {
                EditorCommand c;
                c.type = EditorCommand::FocusObject;
                host_->PostCommand(c);
                return true;
            }

            if (e->type() == QEvent::KeyPress)
            {
                const int navigationKey = CameraNavigationKey(key->nativeScanCode(), key->key());

                // Shift+A: Quick Add Object menu under cursor
                if ((key->modifiers() & Qt::ShiftModifier) && (navigationKey == 'A' || key->key() == Qt::Key_A))
                {
                    lastSpawnPos_ = ComputeSpawnPosition(mapFromGlobal(QCursor::pos()));
                    emit addMenuRequested(QCursor::pos());
                    return true;
                }

                // Pie Menu on ~ / Russian Ё (scan code 0x29)
                if (navigationKey == '~' || key->key() == Qt::Key_AsciiTilde || key->key() == Qt::Key_QuoteLeft || key->nativeScanCode() == 0x29)
                {
                    lastSpawnPos_ = ComputeSpawnPosition(mapFromGlobal(QCursor::pos()));
                    auto* pie = new PieMenuWidget([this](int actionId) {
                        switch (actionId)
                        {
                        case 0:
                            emit addPrimitiveRequested(QStringLiteral("Primitive.Cube"), QStringLiteral("Cube"), lastSpawnPos_);
                            break;
                        case 1:
                            emit addPrimitiveRequested(QStringLiteral("Light.Point"), QStringLiteral("Point Light"), lastSpawnPos_);
                            break;
                        case 2:
                            setGizmoMode(EditorGizmoTranslate);
                            break;
                        case 3:
                            setGizmoMode(EditorGizmoRotate);
                            break;
                        case 4:
                            setGizmoMode(EditorGizmoScale);
                            break;
                        case 5:
                            {
                                EditorCommand c; c.type = EditorCommand::FocusObject; host_->PostCommand(c);
                            }
                            break;
                        case 6:
                            {
                                EditorCommand c; c.type = EditorCommand::RemoveObject; host_->PostCommand(c);
                            }
                            break;
                        case 7:
                            {
                                EditorCommand c; c.type = EditorCommand::DuplicateObject; host_->PostCommand(c);
                            }
                            break;
                        }
                    }, this);
                    const QPoint globalPos = QCursor::pos();
                    pie->move(globalPos.x() - pie->width() / 2, globalPos.y() - pie->height() / 2);
                    pie->show();
                    return true;
                }

                switch (key->key())
                {
                case Qt::Key_1:
                    setGizmoMode(EditorGizmoTranslate);
                    return true;

                case Qt::Key_2:
                    setGizmoMode(EditorGizmoRotate);
                    return true;

                case Qt::Key_3:
                    setGizmoMode(EditorGizmoScale);
                    return true;

                default:
                    break;
                }

                if (!rmbPressed_)
                {
                    const int navKey = CameraNavigationKey(key->nativeScanCode(), key->key());
                    if (controlScheme_ == ControlScheme::Blender)
                    {
                        if (navKey == 'G' || key->key() == Qt::Key_G) {
                            setGizmoMode(EditorGizmoTranslate);
                            return true;
                        }
                        if (navKey == 'R' || key->key() == Qt::Key_R) {
                            setGizmoMode(EditorGizmoRotate);
                            return true;
                        }
                        if (navKey == 'S' || key->key() == Qt::Key_S) {
                            setGizmoMode(EditorGizmoScale);
                            return true;
                        }
                    }
                    else
                    {
                        if (navKey == 'W' || key->key() == Qt::Key_W) {
                            setGizmoMode(EditorGizmoTranslate);
                            return true;
                        }
                        if (navKey == 'E' || key->key() == Qt::Key_E) {
                            setGizmoMode(EditorGizmoRotate);
                            return true;
                        }
                        if (navKey == 'R' || key->key() == Qt::Key_R) {
                            setGizmoMode(EditorGizmoScale);
                            return true;
                        }
                        if (navKey == 'Q' || key->key() == Qt::Key_Q) {
                            setGizmoMode(EditorGizmoTranslate);
                            return true;
                        }
                    }

                    if (navKey == 'X' || key->key() == Qt::Key_X) {
                        SetAxisLock(activeAxis_ == AxisLock::X ? AxisLock::All : AxisLock::X);
                        return true;
                    }
                    if (navKey == 'Y' || key->key() == Qt::Key_Y) {
                        SetAxisLock(activeAxis_ == AxisLock::Y ? AxisLock::All : AxisLock::Y);
                        return true;
                    }
                    if (navKey == 'Z' || key->key() == Qt::Key_Z) {
                        SetAxisLock(activeAxis_ == AxisLock::Z ? AxisLock::All : AxisLock::Z);
                        return true;
                    }
                }
            }

            const int navigationKey = CameraNavigationKey(key->nativeScanCode(), key->key());

            if ((navigationKey == Qt::Key_Space || navigationKey == Qt::Key_P) && isActiveWindow())
            {
                if (e->type() == QEvent::KeyPress)
                {
                    EditorCommand c;
                    c.type = navigationKey == Qt::Key_Space ? EditorCommand::TogglePlay : EditorCommand::TogglePause;
                    host_->PostCommand(c);
                }

                return true;
            }

            if (e->type() == QEvent::KeyPress)
                keys_.insert(navigationKey);
            else
                keys_.remove(navigationKey);
        }

        return true;
    }

    return QWidget::event(e);
}

void ViewportWidget::setGizmoMode(EditorGizmoMode mode)
{
    if (gizmoMode_ == mode)
        return;

    if (draggingSelectedObject_)
        return;

    gizmoMode_ = mode;

    host_->SetGizmoMode(mode);

    if (btnTranslate_ && btnRotate_ && btnScale_)
    {
        QSignalBlocker b1(btnTranslate_);
        QSignalBlocker b2(btnRotate_);
        QSignalBlocker b3(btnScale_);
        btnTranslate_->setChecked(mode == EditorGizmoTranslate);
        btnRotate_->setChecked(mode == EditorGizmoRotate);
        btnScale_->setChecked(mode == EditorGizmoScale);
    }

    updateGizmoToolTip();
    emit gizmoModeChanged(mode);
}

void ViewportWidget::SetShadingMode(ViewportShadingMode mode)
{
    shadingMode_ = mode;
    if (btnShadingWireframe_ && btnShadingSolid_ && btnShadingUnlit_ && btnShadingRendered_)
    {
        QSignalBlocker b1(btnShadingWireframe_);
        QSignalBlocker b2(btnShadingSolid_);
        QSignalBlocker b3(btnShadingUnlit_);
        QSignalBlocker b4(btnShadingRendered_);
        btnShadingWireframe_->setChecked(mode == ViewportShadingMode::Wireframe);
        btnShadingSolid_->setChecked(mode == ViewportShadingMode::Solid);
        btnShadingUnlit_->setChecked(mode == ViewportShadingMode::Unlit);
        btnShadingRendered_->setChecked(mode == ViewportShadingMode::Rendered);
    }
    emit shadingModeChanged(mode);
}

void ViewportWidget::SnapToGround()
{
    uint64_t id = 0;
    EngineSceneTransform transform{};
    if (SelectedTransform(id, transform))
    {
        transform.position[1] = static_cast<float>(std::abs(transform.scale[1]) * 0.5f);
        EditorCommand c;
        c.type = EditorCommand::UpdateObjectTransform;
        c.objectId = id;
        c.transform = transform;
        c.path = QStringLiteral("snap_ground");
        host_->PostCommand(c);
    }
}

void ViewportWidget::updateGizmoToolTip()
{
    const QString modeName = GizmoModeName(gizmoMode_);

    if (controlScheme_ == ControlScheme::Blender)
    {
        setToolTip(
            QStringLiteral("Gizmo: %1 | Keys: G Move, R Rotate, S Scale, 1/2/3\n"
                "End: Snap to Ground | Space: Play/Stop | P: Pause | MMB: Orbit | Shift+MMB: Pan | RMB: Look (WASD) | Tab: select | Arrows/PgUp/PgDn: move")
            .arg(modeName));
    }
    else
    {
        setToolTip(
            QStringLiteral("Gizmo: %1 | Keys: W Move, E Rotate, R Scale, 1/2/3\n"
                "End: Snap to Ground | Space: Play/Stop | P: Pause | RMB: Look (WASD) | MMB/Shift+MMB: Pan | Tab: select | Arrows/PgUp/PgDn: move")
            .arg(modeName));
    }
}

void ViewportWidget::BuildHeaderBar()
{
    viewportHeaderBar_ = new ViewportHeaderBar(this);
    viewportHeaderBar_->setObjectName(QStringLiteral("ViewportHeaderBar"));
    viewportHeaderBar_->setFixedHeight(28);
    viewportHeaderBar_->setStyleSheet(QStringLiteral(
        "ViewportHeaderBar#ViewportHeaderBar {"
        "  background-color: rgba(24, 26, 31, 0.95);"
        "  border-bottom: 1px solid #2B2E38;"
        "}"
        "QToolButton, QPushButton {"
        "  background: transparent;"
        "  color: #C5CBD3;"
        "  border: 1px solid transparent;"
        "  border-radius: 3px;"
        "  padding: 1px 4px;"
        "  font-size: 11px;"
        "  min-height: 20px;"
        "}"
        "QToolButton:hover, QPushButton:hover {"
        "  background: #252830;"
        "  color: #FFFFFF;"
        "  border: 1px solid #3B404D;"
        "}"
        "QToolButton:checked, QPushButton:checked {"
        "  background: #1E3A5F;"
        "  color: #60A5FA;"
        "  border: 1px solid #3B82F6;"
        "  font-weight: bold;"
        "}"
        "QComboBox {"
        "  background: #1A1C23;"
        "  color: #E2E8F0;"
        "  border: 1px solid #333742;"
        "  border-radius: 3px;"
        "  padding: 1px 3px;"
        "  font-size: 11px;"
        "  height: 20px;"
        "}"
        "QComboBox::drop-down {"
        "  border: none;"
        "  width: 14px;"
        "}"
        "QComboBox QAbstractItemView {"
        "  background: #181A1F;"
        "  color: #E2E8F0;"
        "  selection-background-color: #3B82F6;"
        "  border: 1px solid #333742;"
        "}"
    ));

    auto* layout = new QHBoxLayout(viewportHeaderBar_);
    layout->setContentsMargins(6, 2, 6, 2);
    layout->setSpacing(3);

    auto makeSep = [this, layout]() {
        auto* sep = new QFrame(viewportHeaderBar_);
        sep->setFrameShape(QFrame::VLine);
        sep->setFrameShadow(QFrame::Plain);
        sep->setStyleSheet(QStringLiteral("color: #2E323D; margin: 2px 2px;"));
        layout->addWidget(sep);
        return sep;
    };

    // --- Левая группа: Инструменты Гизмо ---
    gizmoGroup_ = new QButtonGroup(this);
    gizmoGroup_->setExclusive(true);

    const QColor iconColor(0xC5, 0xCB, 0xD3);

    btnTranslate_ = new QToolButton(viewportHeaderBar_);
    btnTranslate_->setIcon(CreateMoveGizmoIcon(iconColor));
    btnTranslate_->setCheckable(true);
    btnTranslate_->setChecked(gizmoMode_ == EditorGizmoTranslate);
    btnTranslate_->setToolTip(Tr("Перемещение (W / G)", "Move (W / G)"));
    btnTranslate_->setFixedSize(24, 22);
    gizmoGroup_->addButton(btnTranslate_, EditorGizmoTranslate);
    layout->addWidget(btnTranslate_);

    btnRotate_ = new QToolButton(viewportHeaderBar_);
    btnRotate_->setIcon(CreateRotateGizmoIcon(iconColor));
    btnRotate_->setCheckable(true);
    btnRotate_->setChecked(gizmoMode_ == EditorGizmoRotate);
    btnRotate_->setToolTip(Tr("Вращение (E / R)", "Rotate (E / R)"));
    btnRotate_->setFixedSize(24, 22);
    gizmoGroup_->addButton(btnRotate_, EditorGizmoRotate);
    layout->addWidget(btnRotate_);

    btnScale_ = new QToolButton(viewportHeaderBar_);
    btnScale_->setIcon(CreateScaleGizmoIcon(iconColor));
    btnScale_->setCheckable(true);
    btnScale_->setChecked(gizmoMode_ == EditorGizmoScale);
    btnScale_->setToolTip(Tr("Масштаб (R / S)", "Scale (R / S)"));
    btnScale_->setFixedSize(24, 22);
    gizmoGroup_->addButton(btnScale_, EditorGizmoScale);
    layout->addWidget(btnScale_);

    connect(gizmoGroup_, &QButtonGroup::idClicked, this, [this](int id) {
        setGizmoMode(static_cast<EditorGizmoMode>(id));
    });

    makeSep();

    axisGroup_ = new QButtonGroup(this);
    axisGroup_->setExclusive(true);

    btnAxisAll_ = new QToolButton(viewportHeaderBar_);
    btnAxisAll_->setText(QStringLiteral("All"));
    btnAxisAll_->setCheckable(true);
    btnAxisAll_->setChecked(true);
    btnAxisAll_->setToolTip(Tr("Все оси (Свободно)", "All Axes (Free)"));
    btnAxisAll_->setStyleSheet(QStringLiteral(
        "QToolButton { background-color: #262930; color: #94A3B8; border: 1px solid #3A3F4B; border-radius: 4px; font-weight: bold; font-size: 10px; min-width: 24px; min-height: 20px; max-height: 20px; padding: 1px 4px; }"
        "QToolButton:hover { background-color: #333842; color: #F1F5F9; }"
        "QToolButton:checked { background-color: #334155; color: #F8FAFC; border: 1px solid #64748B; }"
    ));
    axisGroup_->addButton(btnAxisAll_, static_cast<int>(AxisLock::All));
    layout->addWidget(btnAxisAll_);

    btnAxisX_ = new QToolButton(viewportHeaderBar_);
    btnAxisX_->setText(QStringLiteral("X"));
    btnAxisX_->setCheckable(true);
    btnAxisX_->setToolTip(Tr("Ограничение по оси X (Клавиша X)", "Constrain to X Axis (X key)"));
    btnAxisX_->setStyleSheet(QStringLiteral(
        "QToolButton { background-color: #262930; color: #EF4444; border: 1px solid #3A3F4B; border-radius: 4px; font-weight: bold; font-size: 10px; min-width: 20px; min-height: 20px; max-height: 20px; padding: 1px 4px; }"
        "QToolButton:hover { background-color: #3A2326; }"
        "QToolButton:checked { background-color: #7F1D1D; color: #FCA5A5; border: 1px solid #EF4444; }"
    ));
    axisGroup_->addButton(btnAxisX_, static_cast<int>(AxisLock::X));
    layout->addWidget(btnAxisX_);

    btnAxisY_ = new QToolButton(viewportHeaderBar_);
    btnAxisY_->setText(QStringLiteral("Y"));
    btnAxisY_->setCheckable(true);
    btnAxisY_->setToolTip(Tr("Ограничение по оси Y (Клавиша Y)", "Constrain to Y Axis (Y key)"));
    btnAxisY_->setStyleSheet(QStringLiteral(
        "QToolButton { background-color: #262930; color: #22C55E; border: 1px solid #3A3F4B; border-radius: 4px; font-weight: bold; font-size: 10px; min-width: 20px; min-height: 20px; max-height: 20px; padding: 1px 4px; }"
        "QToolButton:hover { background-color: #1F3626; }"
        "QToolButton:checked { background-color: #14532D; color: #86EFAC; border: 1px solid #22C55E; }"
    ));
    axisGroup_->addButton(btnAxisY_, static_cast<int>(AxisLock::Y));
    layout->addWidget(btnAxisY_);

    btnAxisZ_ = new QToolButton(viewportHeaderBar_);
    btnAxisZ_->setText(QStringLiteral("Z"));
    btnAxisZ_->setCheckable(true);
    btnAxisZ_->setToolTip(Tr("Ограничение по оси Z (Клавиша Z)", "Constrain to Z Axis (Z key)"));
    btnAxisZ_->setStyleSheet(QStringLiteral(
        "QToolButton { background-color: #262930; color: #3B82F6; border: 1px solid #3A3F4B; border-radius: 4px; font-weight: bold; font-size: 10px; min-width: 20px; min-height: 20px; max-height: 20px; padding: 1px 4px; }"
        "QToolButton:hover { background-color: #1E293B; }"
        "QToolButton:checked { background-color: #1E3A8A; color: #93C5FD; border: 1px solid #3B82F6; }"
    ));
    axisGroup_->addButton(btnAxisZ_, static_cast<int>(AxisLock::Z));
    layout->addWidget(btnAxisZ_);

    connect(axisGroup_, &QButtonGroup::idClicked, this, [this](int id) {
        SetAxisLock(static_cast<AxisLock>(id));
    });

    comboOrientation_ = new QComboBox(viewportHeaderBar_);
    comboOrientation_->addItem(Tr("Мир", "World"));
    comboOrientation_->addItem(Tr("Локально", "Local"));
    comboOrientation_->setFixedWidth(72);
    connect(comboOrientation_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int idx) {
        localOrientation_ = (idx == 1);
    });
    layout->addWidget(comboOrientation_);

    makeSep();

    // --- Центральная группа: Привязка (Snapping) ---
    btnSnap_ = new QToolButton(viewportHeaderBar_);
    btnSnap_->setIcon(CreateMagnetIcon(iconColor));
    btnSnap_->setText(Tr("Привязка", "Snap"));
    btnSnap_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    btnSnap_->setCheckable(true);
    btnSnap_->setChecked(snapEnabled_);
    btnSnap_->setToolTip(Tr("Привязка к сетке (Ctrl для временного снапа)", "Snapping (Hold Ctrl for temp snap)"));
    btnSnap_->setFixedHeight(22);
    connect(btnSnap_, &QToolButton::toggled, this, [this](bool checked) {
        snapEnabled_ = checked;
    });
    layout->addWidget(btnSnap_);

    comboMoveSnap_ = new QComboBox(viewportHeaderBar_);
    const std::vector<std::pair<QString, double>> moveSteps = {
        {QStringLiteral("0.1 m"), 0.1},
        {QStringLiteral("0.25 m"), 0.25},
        {QStringLiteral("0.5 m"), 0.5},
        {QStringLiteral("1.0 m"), 1.0},
        {QStringLiteral("2.0 m"), 2.0},
        {QStringLiteral("5.0 m"), 5.0},
        {QStringLiteral("10.0 m"), 10.0}
    };
    for (const auto& [label, val] : moveSteps) {
        comboMoveSnap_->addItem(label, val);
    }
    comboMoveSnap_->setCurrentIndex(3); // 1.0 m
    moveSnapStep_ = 1.0;
    comboMoveSnap_->setFixedWidth(64);
    comboMoveSnap_->setToolTip(Tr("Шаг перемещения (м)", "Move Snap Step (m)"));
    connect(comboMoveSnap_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        moveSnapStep_ = comboMoveSnap_->currentData().toDouble();
    });
    layout->addWidget(comboMoveSnap_);

    comboRotateSnap_ = new QComboBox(viewportHeaderBar_);
    const std::vector<std::pair<QString, double>> rotSteps = {
        {QStringLiteral("5°"), 5.0},
        {QStringLiteral("15°"), 15.0},
        {QStringLiteral("30°"), 30.0},
        {QStringLiteral("45°"), 45.0},
        {QStringLiteral("90°"), 90.0}
    };
    for (const auto& [label, val] : rotSteps) {
        comboRotateSnap_->addItem(label, val);
    }
    comboRotateSnap_->setCurrentIndex(1); // 15°
    rotateSnapStep_ = 15.0;
    comboRotateSnap_->setFixedWidth(52);
    comboRotateSnap_->setToolTip(Tr("Шаг угла вращения", "Rotate Snap Step"));
    connect(comboRotateSnap_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        rotateSnapStep_ = comboRotateSnap_->currentData().toDouble();
    });
    layout->addWidget(comboRotateSnap_);

    comboScaleSnap_ = new QComboBox(viewportHeaderBar_);
    const std::vector<std::pair<QString, double>> scaleSteps = {
        {QStringLiteral("0.1"), 0.1},
        {QStringLiteral("0.25"), 0.25},
        {QStringLiteral("0.5"), 0.5},
        {QStringLiteral("1.0"), 1.0}
    };
    for (const auto& [label, val] : scaleSteps) {
        comboScaleSnap_->addItem(label, val);
    }
    comboScaleSnap_->setCurrentIndex(1); // 0.25
    scaleSnapStep_ = 0.25;
    comboScaleSnap_->setFixedWidth(52);
    comboScaleSnap_->setToolTip(Tr("Шаг масштабирования", "Scale Snap Step"));
    connect(comboScaleSnap_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        scaleSnapStep_ = comboScaleSnap_->currentData().toDouble();
    });
    layout->addWidget(comboScaleSnap_);

    btnSnapGround_ = new QToolButton(viewportHeaderBar_);
    btnSnapGround_->setIcon(CreateSnapGroundIcon(iconColor));
    btnSnapGround_->setToolTip(Tr("Посадить на землю (End)", "Snap to Ground (End)"));
    btnSnapGround_->setFixedSize(24, 22);
    connect(btnSnapGround_, &QToolButton::clicked, this, &ViewportWidget::SnapToGround);
    layout->addWidget(btnSnapGround_);

    layout->addStretch();

    // --- Правая группа: Режимы шейдинга и Оверлеи ---
    shadingGroup_ = new QButtonGroup(this);
    shadingGroup_->setExclusive(true);

    btnShadingWireframe_ = new QToolButton(viewportHeaderBar_);
    btnShadingWireframe_->setIcon(CreateWireframeIcon(iconColor));
    btnShadingWireframe_->setCheckable(true);
    btnShadingWireframe_->setToolTip(Tr("Сетка (Wireframe)", "Wireframe"));
    btnShadingWireframe_->setFixedSize(24, 22);
    shadingGroup_->addButton(btnShadingWireframe_, static_cast<int>(ViewportShadingMode::Wireframe));
    layout->addWidget(btnShadingWireframe_);

    btnShadingSolid_ = new QToolButton(viewportHeaderBar_);
    btnShadingSolid_->setIcon(CreateSolidIcon(iconColor));
    btnShadingSolid_->setCheckable(true);
    btnShadingSolid_->setChecked(true);
    btnShadingSolid_->setToolTip(Tr("Сплошной (Solid)", "Solid"));
    btnShadingSolid_->setFixedSize(24, 22);
    shadingGroup_->addButton(btnShadingSolid_, static_cast<int>(ViewportShadingMode::Solid));
    layout->addWidget(btnShadingSolid_);

    btnShadingUnlit_ = new QToolButton(viewportHeaderBar_);
    btnShadingUnlit_->setIcon(CreateMaterialPreviewIcon(iconColor));
    btnShadingUnlit_->setCheckable(true);
    btnShadingUnlit_->setToolTip(Tr("Без света / Текстуры (Unlit)", "Unlit / Material Preview"));
    btnShadingUnlit_->setFixedSize(24, 22);
    shadingGroup_->addButton(btnShadingUnlit_, static_cast<int>(ViewportShadingMode::Unlit));
    layout->addWidget(btnShadingUnlit_);

    btnShadingRendered_ = new QToolButton(viewportHeaderBar_);
    btnShadingRendered_->setIcon(CreateRenderedIcon(QColor(0x38, 0xBD, 0xF8)));
    btnShadingRendered_->setCheckable(true);
    btnShadingRendered_->setToolTip(Tr("Полный рендер (Rendered)", "Rendered"));
    btnShadingRendered_->setFixedSize(24, 22);
    shadingGroup_->addButton(btnShadingRendered_, static_cast<int>(ViewportShadingMode::Rendered));
    layout->addWidget(btnShadingRendered_);

    connect(shadingGroup_, &QButtonGroup::idClicked, this, [this](int id) {
        SetShadingMode(static_cast<ViewportShadingMode>(id));
    });

    makeSep();

    btnOverlays_ = new QToolButton(viewportHeaderBar_);
    btnOverlays_->setText(Tr("Оверлеи", "Overlays") + QStringLiteral(" ▾"));
    btnOverlays_->setPopupMode(QToolButton::InstantPopup);
    btnOverlays_->setFixedHeight(22);

    menuOverlays_ = new QMenu(btnOverlays_);
    menuOverlays_->setStyleSheet(QStringLiteral(
        "QMenu {"
        "  background: #181A1F;"
        "  color: #E2E8F0;"
        "  border: 1px solid #333742;"
        "  padding: 4px;"
        "}"
        "QMenu::item {"
        "  padding: 4px 20px 4px 24px;"
        "  border-radius: 3px;"
        "}"
        "QMenu::item:selected {"
        "  background: #252830;"
        "  color: #FFFFFF;"
        "}"
    ));

    actOverlayGrid_ = menuOverlays_->addAction(Tr("Показать сетку", "Show Grid"));
    actOverlayGrid_->setCheckable(true);
    actOverlayGrid_->setChecked(true);

    actOverlayBackface_ = menuOverlays_->addAction(Tr("Отсечение задних граней", "Backface Culling"));
    actOverlayBackface_->setCheckable(true);
    actOverlayBackface_->setChecked(true);

    actOverlayShadows_ = menuOverlays_->addAction(Tr("Тени", "Shadows"));
    actOverlayShadows_->setCheckable(true);
    actOverlayShadows_->setChecked(true);

    actOverlayOutline_ = menuOverlays_->addAction(Tr("Контур выделения", "Selection Outline"));
    actOverlayOutline_->setCheckable(true);
    actOverlayOutline_->setChecked(true);

    actOverlayGizmos_ = menuOverlays_->addAction(Tr("Служебные маркеры", "Gizmos & Icons"));
    actOverlayGizmos_->setCheckable(true);
    actOverlayGizmos_->setChecked(true);

    menuOverlays_->addSeparator();

    actOverlayStats_ = menuOverlays_->addAction(Tr("Статистика сцены", "Scene Statistics / Debug Info"));
    actOverlayStats_->setCheckable(true);
    actOverlayStats_->setChecked(true);

    connect(actOverlayStats_, &QAction::toggled, this, [this](bool checked) {
        if (statsOverlay_) {
            statsOverlay_->setVisible(checked);
            if (checked) {
                statsOverlay_->raise();
                UpdateStatsOverlay();
            }
        }
    });

    btnOverlays_->setMenu(menuOverlays_);
    layout->addWidget(btnOverlays_);

    viewportHeaderBar_->setGeometry(0, 0, width(), 28);
    viewportHeaderBar_->show();
    viewportHeaderBar_->raise();
}

bool ViewportWidget::SelectedTransform(uint64_t& id, EngineSceneTransform& transform)
{
    const auto view = host_->ReadSnapshot();

    if (!view.selected)
        return false;

    auto found = std::find_if(
        view.objects.begin(),
        view.objects.end(),
        [&](const auto& o)
        {
            return o.id == view.selected;
        });

    if (found == view.objects.end())
        return false;

    id = found->id;
    transform = found->transform;
    return true;
}

bool ViewportWidget::BeginObjectDrag(const QPointF& pos)
{
    if (draggingSelectedObject_)
        return true;

    if (!attached_ || !host_->HasScene())
        return false;

    if (host_->PlayState() != EditorQtHost::Edit)
        return false;

    uint64_t id = 0;
    EngineSceneTransform transform{};

    if (!SelectedTransform(id, transform))
        return false;

    draggingSelectedObject_ = true;
    dragObjectId_ = id;
    dragStartTransform_ = transform;
    dragStartPos_ = pos;
    lastDragPos_ = pos;

    EditorCommand c;
    c.type = EditorCommand::BeginGizmoDrag;
    c.objectId = id;
    host_->PostCommand(c);

    gizmoDragSession_ = true;

    return true;
}

void ViewportWidget::EndObjectDrag()
{
    if (gizmoDragSession_)
    {
        EditorCommand c;
        c.type = EditorCommand::EndGizmoDrag;
        c.objectId = dragObjectId_;
        host_->PostCommand(c);

        gizmoDragSession_ = false;
    }

    draggingSelectedObject_ = false;
    dragObjectId_ = 0;
    lmbPressed_ = false;
    lmbMoved_ = false;
}

void ViewportWidget::PostSelectedTransform(const EngineSceneTransform& transform)
{
    if (!dragObjectId_)
        return;

    EditorCommand c;
    c.type = EditorCommand::UpdateObjectTransform;
    c.objectId = dragObjectId_;
    c.transform = transform;
    c.path = QStringLiteral("gizmo");

    host_->PostCommand(c);
}

void ViewportWidget::ApplyRotateScaleGizmo(const QPointF& pos, bool isSnapActive)
{
    if (!draggingSelectedObject_)
        return;

    if (gizmoMode_ == EditorGizmoTranslate)
        return;

    const QPointF total = pos - dragStartPos_;

    EngineSceneTransform t = dragStartTransform_;

    if (gizmoMode_ == EditorGizmoRotate)
    {
        double yaw = 0.0;
        double pitch = 0.0;
        double roll = 0.0;

        if (activeAxis_ == AxisLock::X)
        {
            pitch = -total.y() * static_cast<double>(rotateSensitivity_);
        }
        else if (activeAxis_ == AxisLock::Y)
        {
            yaw = total.x() * static_cast<double>(rotateSensitivity_);
        }
        else if (activeAxis_ == AxisLock::Z)
        {
            roll = total.x() * static_cast<double>(rotateSensitivity_);
        }
        else
        {
            yaw = total.x() * static_cast<double>(rotateSensitivity_);
            pitch = -total.y() * static_cast<double>(rotateSensitivity_);
        }

        if (isSnapActive && rotateSnapStep_ > 1e-4)
        {
            const double deg2rad = 3.14159265358979323846 / 180.0;
            const double stepRad = rotateSnapStep_ * deg2rad;
            yaw = std::round(yaw / stepRad) * stepRad;
            pitch = std::round(pitch / stepRad) * stepRad;
            roll = std::round(roll / stepRad) * stepRad;
        }

        double startRot[4] = {
            static_cast<double>(dragStartTransform_.rotation[0]),
            static_cast<double>(dragStartTransform_.rotation[1]),
            static_cast<double>(dragStartTransform_.rotation[2]),
            static_cast<double>(dragStartTransform_.rotation[3])
        };

        NormalizeQuat(startRot);

        double qYaw[4];
        double qPitch[4];
        double qRoll[4];
        double qTmp1[4];
        double qTmp2[4];
        double qNew[4];

        AxisAngleQuat(0.0, 1.0, 0.0, yaw, qYaw);
        AxisAngleQuat(1.0, 0.0, 0.0, pitch, qPitch);
        AxisAngleQuat(0.0, 0.0, 1.0, roll, qRoll);

        MulQuat(qYaw, qPitch, qTmp1);
        MulQuat(qTmp1, qRoll, qTmp2);
        MulQuat(qTmp2, startRot, qNew);
        NormalizeQuat(qNew);

        using RotT = std::remove_reference_t<decltype(t.rotation[0])>;

        for (int i = 0; i < 4; ++i)
            t.rotation[i] = static_cast<RotT>(qNew[i]);
    }
    else if (gizmoMode_ == EditorGizmoScale)
    {
        double factor = std::exp((total.x() - total.y()) * static_cast<double>(scaleSensitivity_));
        factor = std::max(0.01, std::min(100.0, factor));

        if (isSnapActive && scaleSnapStep_ > 1e-4)
        {
            factor = std::max(scaleSnapStep_, std::round(factor / scaleSnapStep_) * scaleSnapStep_);
        }

        using ScaleT = std::remove_reference_t<decltype(t.scale[0])>;

        for (int i = 0; i < 3; ++i)
        {
            bool modifyAxis = true;
            if (activeAxis_ == AxisLock::X && i != 0) modifyAxis = false;
            if (activeAxis_ == AxisLock::Y && i != 1) modifyAxis = false;
            if (activeAxis_ == AxisLock::Z && i != 2) modifyAxis = false;

            if (modifyAxis)
            {
                const double base = dragStartTransform_.scale[i];
                double newScale = base * factor;
                if (isSnapActive && scaleSnapStep_ > 1e-4)
                {
                    newScale = std::max(scaleSnapStep_, std::round(newScale / scaleSnapStep_) * scaleSnapStep_);
                }
                t.scale[i] = static_cast<ScaleT>(newScale);
            }
            else
            {
                t.scale[i] = dragStartTransform_.scale[i];
            }
        }
    }

    PostSelectedTransform(t);
}

std::array<double, 3> ViewportWidget::ComputeSpawnPosition(const QPoint& viewportPos) const
{
    if (EditorSettings::GetSpawnLocationMode() == SpawnLocationMode::WorldOrigin)
    {
        return {0.0, 0.0, 0.0};
    }

    const float w = width() > 0 ? static_cast<float>(width()) : 800.0f;
    const float h = height() > 0 ? static_cast<float>(height()) : 600.0f;

    const float ndcX = 2.0f * static_cast<float>(viewportPos.x()) / w - 1.0f;
    const float ndcY = 1.0f - 2.0f * static_cast<float>(viewportPos.y()) / h;

    const float aspect = w / h;
    const float tanHalfFov = 0.577f;

    const float cy = std::cos(camYaw_);
    const float sy = std::sin(camYaw_);
    const float cp = std::cos(camPitch_);
    const float sp = std::sin(camPitch_);

    const float fwdX = cy * cp;
    const float fwdY = sp;
    const float fwdZ = sy * cp;

    const float rightX = sy;
    const float rightY = 0.0f;
    const float rightZ = -cy;

    const float upX = -fwdY * rightZ;
    const float upY = fwdZ * rightX - fwdX * rightZ;
    const float upZ = fwdY * rightX;

    float rx = fwdX + ndcX * aspect * tanHalfFov * rightX + ndcY * tanHalfFov * upX;
    float ry = fwdY + ndcX * aspect * tanHalfFov * rightY + ndcY * tanHalfFov * upY;
    float rz = fwdZ + ndcX * aspect * tanHalfFov * rightZ + ndcY * tanHalfFov * upZ;

    const float rLen = std::sqrt(rx * rx + ry * ry + rz * rz);
    if (rLen > 1e-6f)
    {
        rx /= rLen;
        ry /= rLen;
        rz /= rLen;
    }

    if (ry < -0.01f && camPos_[1] > 0.0f)
    {
        const float t = -camPos_[1] / ry;
        if (t > 0.1f && t < 200.0f)
        {
            return {
                static_cast<double>(camPos_[0] + rx * t),
                0.0,
                static_cast<double>(camPos_[2] + rz * t)
            };
        }
    }

    return {
        static_cast<double>(camPos_[0] + fwdX * 10.0f),
        0.0,
        static_cast<double>(camPos_[2] + fwdZ * 10.0f)
    };
}

void ViewportWidget::ShowAdjustLastOperation(uint64_t id, const QString& name, const EngineSceneTransform& transform)
{
    if (adjustPanel_) {
        adjustPanel_->ShowForObject(id, name, transform);
    }
}

void ViewportWidget::SetAdjustLastOperationObjectId(uint64_t id)
{
    if (adjustPanel_) {
        adjustPanel_->SetObjectId(id);
    }
}

void ViewportWidget::SetAxisLock(AxisLock axis)
{
    activeAxis_ = axis;

    if (btnAxisAll_ && btnAxisX_ && btnAxisY_ && btnAxisZ_)
    {
        QSignalBlocker b0(btnAxisAll_);
        QSignalBlocker b1(btnAxisX_);
        QSignalBlocker b2(btnAxisY_);
        QSignalBlocker b3(btnAxisZ_);
        btnAxisAll_->setChecked(axis == AxisLock::All);
        btnAxisX_->setChecked(axis == AxisLock::X);
        btnAxisY_->setChecked(axis == AxisLock::Y);
        btnAxisZ_->setChecked(axis == AxisLock::Z);
    }
}

void ViewportWidget::ClearSelection()
{
    EditorCommand c;
    c.type = EditorCommand::SelectObject;
    c.objectId = 0;
    host_->PostCommand(c);
}

void ViewportWidget::RetranslateUi()
{
    updateGizmoToolTip();

    if (btnTranslate_)
        btnTranslate_->setToolTip(Tr("Перемещение (W / G)", "Move (W / G)"));
    if (btnRotate_)
        btnRotate_->setToolTip(Tr("Вращение (E / R)", "Rotate (E / R)"));
    if (btnScale_)
        btnScale_->setToolTip(Tr("Масштаб (R / S)", "Scale (R / S)"));

    if (btnAxisAll_)
        btnAxisAll_->setToolTip(Tr("Все оси (Свободно)", "All Axes (Free)"));
    if (btnAxisX_)
        btnAxisX_->setToolTip(Tr("Ограничение по оси X (Клавиша X)", "Constrain to X Axis (X key)"));
    if (btnAxisY_)
        btnAxisY_->setToolTip(Tr("Ограничение по оси Y (Клавиша Y)", "Constrain to Y Axis (Y key)"));
    if (btnAxisZ_)
        btnAxisZ_->setToolTip(Tr("Ограничение по оси Z (Клавиша Z)", "Constrain to Z Axis (Z key)"));

    if (comboOrientation_)
    {
        const int cur = comboOrientation_->currentIndex();
        comboOrientation_->setItemText(0, Tr("Мир", "World"));
        comboOrientation_->setItemText(1, Tr("Локально", "Local"));
        comboOrientation_->setCurrentIndex(cur);
        comboOrientation_->setToolTip(Tr("Ориентация трансформации", "Transform Orientation"));
    }

    if (btnSnap_)
    {
        btnSnap_->setText(Tr("Привязка", "Snap"));
        btnSnap_->setToolTip(Tr("Привязка к сетке (Ctrl для временного снапа)", "Snapping (Hold Ctrl for temp snap)"));
    }
    if (comboMoveSnap_)
        comboMoveSnap_->setToolTip(Tr("Шаг перемещения (м)", "Move Snap Step (m)"));
    if (comboRotateSnap_)
        comboRotateSnap_->setToolTip(Tr("Шаг вращения", "Rotate Snap Step"));
    if (comboScaleSnap_)
        comboScaleSnap_->setToolTip(Tr("Шаг масштабирования", "Scale Snap Step"));
    if (btnSnapGround_)
        btnSnapGround_->setToolTip(Tr("Посадить на землю (End)", "Snap to Ground (End)"));

    if (btnShadingWireframe_)
        btnShadingWireframe_->setToolTip(Tr("Сетка (Wireframe)", "Wireframe"));
    if (btnShadingSolid_)
        btnShadingSolid_->setToolTip(Tr("Сплошной (Solid)", "Solid"));
    if (btnShadingUnlit_)
        btnShadingUnlit_->setToolTip(Tr("Без света / Текстуры (Unlit)", "Unlit / Material Preview"));
    if (btnShadingRendered_)
        btnShadingRendered_->setToolTip(Tr("Полный рендер (Rendered)", "Rendered"));

    if (btnOverlays_)
    {
        btnOverlays_->setText(Tr("Оверлеи", "Overlays") + QStringLiteral(" ▾"));
        btnOverlays_->setToolTip(Tr("Оверлеи вьюпорта", "Viewport Overlays"));
    }

    if (actOverlayGrid_)
        actOverlayGrid_->setText(Tr("Показать сетку", "Show Grid"));
    if (actOverlayBackface_)
        actOverlayBackface_->setText(Tr("Отсечение задних граней", "Backface Culling"));
    if (actOverlayShadows_)
        actOverlayShadows_->setText(Tr("Тени", "Shadows"));
    if (actOverlayOutline_)
        actOverlayOutline_->setText(Tr("Контур выделения", "Selection Outline"));
    if (actOverlayGizmos_)
        actOverlayGizmos_->setText(Tr("Служебные маркеры", "Gizmos & Icons"));
    if (actOverlayStats_)
        actOverlayStats_->setText(Tr("Статистика сцены", "Scene Statistics / Debug Info"));

    UpdateStatsOverlay();
}

void ViewportWidget::UpdateStatsOverlay()
{
    if (!statsOverlay_ || !statsOverlay_->isVisible())
        return;

    const auto snap = host_->ReadSnapshot();
    const int totalObjects = static_cast<int>(snap.objects.size());
    int lightCount = 0;
    int meshCount = 0;
    uint64_t estTris = 0;

    QString selectedName = Tr("Нет", "None");
    QString selectedCoords;

    for (const auto& obj : snap.objects)
    {
        const bool isLight = obj.hasPointLight ||
            (obj.type.find("Light") != std::string::npos) ||
            (obj.type.find("light") != std::string::npos);

        if (isLight)
            lightCount++;
        else
            meshCount++;

        if (obj.type.find("Cube") != std::string::npos || obj.type.find("Box") != std::string::npos)
            estTris += 12;
        else if (obj.type.find("Plane") != std::string::npos || obj.type.find("Quad") != std::string::npos)
            estTris += 2;
        else if (obj.type.find("Sphere") != std::string::npos)
            estTris += 960;
        else if (obj.type.find("Cylinder") != std::string::npos)
            estTris += 240;
        else if (!isLight)
            estTris += 500;

        if (snap.selected != 0 && obj.id == snap.selected)
        {
            selectedName = QString::fromStdString(obj.name.empty() ? obj.type : obj.name);
            selectedCoords = QStringLiteral("%1, %2, %3")
                .arg(obj.transform.position[0], 0, 'f', 2)
                .arg(obj.transform.position[1], 0, 'f', 2)
                .arg(obj.transform.position[2], 0, 'f', 2);
        }
    }

    QString shadingStr;
    switch (shadingMode_)
    {
    case ViewportShadingMode::Wireframe: shadingStr = Tr("Сетка", "Wireframe"); break;
    case ViewportShadingMode::Solid: shadingStr = Tr("Сплошной", "Solid"); break;
    case ViewportShadingMode::Unlit: shadingStr = Tr("Без света", "Unlit"); break;
    case ViewportShadingMode::Rendered: shadingStr = Tr("Рендер", "Rendered"); break;
    }

    QString snapStr;
    if (snapEnabled_)
    {
        snapStr = QStringLiteral("%1 (%2m, %3°)")
            .arg(Tr("Вкл", "On"))
            .arg(moveSnapStep_, 0, 'f', (moveSnapStep_ < 1.0 ? 2 : 1))
            .arg(rotateSnapStep_, 0, 'f', 0);
    }
    else
    {
        snapStr = Tr("Выкл", "Off");
    }

    statsOverlay_->UpdateStats(fpsSmooth_, frameTimeSmooth_,
                               totalObjects, lightCount, meshCount, estTris,
                               selectedName, selectedCoords,
                               shadingStr, snapStr);
}

void ViewportWidget::HandleViewportDrop(QDropEvent* drop)
{
    if (!drop || !drop->mimeData()) return;

    QStringList paths;
    if (drop->mimeData()->hasUrls()) {
        for (const QUrl& url : drop->mimeData()->urls()) {
            const QString local = url.toLocalFile();
            if (!local.isEmpty()) paths << local;
        }
    } else if (drop->mimeData()->hasText()) {
        const QString text = drop->mimeData()->text().trimmed();
        if (QFile::exists(text)) paths << text;
    }

    if (paths.isEmpty()) return;

    auto* editor = qobject_cast<EditorMainWindow*>(window());
    const QPoint dropPos = drop->position().toPoint();
    const auto spawnPos = ComputeSpawnPosition(dropPos);

    for (const QString& path : paths) {
        QFileInfo fi(path);
        const QString ext = fi.suffix().toLower();
        const bool isMat = (ext == QStringLiteral("mat"));
        const bool isModel = (ext == QStringLiteral("fbx") || ext == QStringLiteral("obj") ||
                              ext == QStringLiteral("blend") || ext == QStringLiteral("gltf") ||
                              ext == QStringLiteral("glb"));
        const bool isTex = (ext == QStringLiteral("png") || ext == QStringLiteral("jpg") ||
                            ext == QStringLiteral("jpeg") || ext == QStringLiteral("bmp") ||
                            ext == QStringLiteral("tga") || ext == QStringLiteral("dds"));
        const bool isScene = (ext == QStringLiteral("json") && path.endsWith(QStringLiteral(".scene.json"), Qt::CaseInsensitive));

        const QString contentRoot = QString::fromUtf8(host_->host().contentRoot ? host_->host().contentRoot : "");
        const bool isInsideProject = !contentRoot.isEmpty() && path.startsWith(contentRoot, Qt::CaseInsensitive);

        if (isMat) {
            // Pick object under dropPos
            EditorCommand pc;
            pc.type = EditorCommand::PickObject;
            pc.right = static_cast<float>(2.0 * dropPos.x() / width() - 1.0);
            pc.up = static_cast<float>(1.0 - 2.0 * dropPos.y() / height());
            host_->PostCommand(pc);

            if (editor) editor->UseAsset(path);
        } else if (isTex) {
            EditorCommand pc;
            pc.type = EditorCommand::PickObject;
            pc.right = static_cast<float>(2.0 * dropPos.x() / width() - 1.0);
            pc.up = static_cast<float>(1.0 - 2.0 * dropPos.y() / height());
            host_->PostCommand(pc);

            if (isInsideProject) {
                if (editor) editor->UseAsset(path);
            } else {
                if (editor) editor->ImportAsset(true, path);
            }
        } else if (isModel) {
            if (isInsideProject) {
                EditorCommand c;
                c.type = EditorCommand::CreateModel;
                c.name = fi.completeBaseName();
                QDir content(contentRoot);
                const QString rel = content.relativeFilePath(path);
                c.path = QStringLiteral("/Game/") + QDir::fromNativeSeparators(rel);
                c.transform.position[0] = static_cast<float>(spawnPos[0]);
                c.transform.position[1] = static_cast<float>(spawnPos[1]);
                c.transform.position[2] = static_cast<float>(spawnPos[2]);
                c.transform.scale[0] = c.transform.scale[1] = c.transform.scale[2] = 1.0f;
                host_->PostCommand(c);
            } else {
                AssetImportDialog dialog(path, contentRoot + QStringLiteral("/Models"), this);
                if (dialog.exec() == QDialog::Accepted) {
                    auto settings = dialog.GetSettings();
                    if (editor) editor->ImportAsset(false, path);
                    if (settings.spawnInScene) {
                        EditorCommand c;
                        c.type = EditorCommand::CreateModel;
                        c.name = fi.completeBaseName();
                        c.path = QStringLiteral("/Game/Models/") + fi.completeBaseName() + QStringLiteral(".glb");
                        c.transform.position[0] = static_cast<float>(spawnPos[0]);
                        c.transform.position[1] = static_cast<float>(spawnPos[1]);
                        c.transform.position[2] = static_cast<float>(spawnPos[2]);
                        c.transform.scale[0] = c.transform.scale[1] = c.transform.scale[2] = static_cast<float>(settings.scaleFactor);
                        host_->PostCommand(c);
                    }
                }
            }
        } else if (isScene) {
            if (editor) editor->OpenScene(path);
        }
    }
}
