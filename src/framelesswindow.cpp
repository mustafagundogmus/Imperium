#include "framelesswindow.h"
#include "theme.h"

#include <QGuiApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QWindow>

namespace {

constexpr int GrabInside = 6;    // resize band inside the card edge
constexpr int GrabOutside = 4;   // …and just outside it, over the shadow

/// Three box-blur passes approximate a Gaussian closely enough for a drop shadow and
/// cost a fraction of a real convolution.
void boxBlurAlpha(QImage &image, int radius)
{
    if (radius < 1)
        return;

    const int w = image.width();
    const int h = image.height();
    const int diameter = radius * 2 + 1;

    QVector<int> line(qMax(w, h));

    for (int pass = 0; pass < 3; ++pass) {
        // horizontal
        for (int y = 0; y < h; ++y) {
            auto *row = reinterpret_cast<QRgb *>(image.scanLine(y));
            for (int x = 0; x < w; ++x)
                line[x] = qAlpha(row[x]);

            int sum = 0;
            for (int x = -radius; x <= radius; ++x)
                sum += line[qBound(0, x, w - 1)];

            for (int x = 0; x < w; ++x) {
                row[x] = qRgba(0, 0, 0, sum / diameter);
                sum += line[qBound(0, x + radius + 1, w - 1)];
                sum -= line[qBound(0, x - radius, w - 1)];
            }
        }

        // vertical
        for (int x = 0; x < w; ++x) {
            for (int y = 0; y < h; ++y)
                line[y] = qAlpha(reinterpret_cast<QRgb *>(image.scanLine(y))[x]);

            int sum = 0;
            for (int y = -radius; y <= radius; ++y)
                sum += line[qBound(0, y, h - 1)];

            for (int y = 0; y < h; ++y) {
                reinterpret_cast<QRgb *>(image.scanLine(y))[x] = qRgba(0, 0, 0, sum / diameter);
                sum += line[qBound(0, y + radius + 1, h - 1)];
                sum -= line[qBound(0, y - radius, h - 1)];
            }
        }
    }
}

} // namespace

FramelessWindow::FramelessWindow(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlag(Qt::FramelessWindowHint, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setMouseTracking(true);

    m_card = new QWidget(this);
    m_card->setMouseTracking(true);
    m_card->setAttribute(Qt::WA_TranslucentBackground, true);
}

QMargins FramelessWindow::shadowMargins() const
{
    using namespace Theme::Metric;
    if (!m_shadowEnabled || isMaximized() || isFullScreen())
        return {};
    return {ShadowMarginX, ShadowMarginTop, ShadowMarginX, ShadowMarginBottom};
}

QRect FramelessWindow::cardRect() const
{
    return rect().marginsRemoved(shadowMargins());
}

void FramelessWindow::setCardMinimumSize(const QSize &size)
{
    m_cardMinimum = size;

    // Give up the shadow rather than the design's minimum size on a short desktop.
    if (const QScreen *screen = QGuiApplication::primaryScreen()) {
        const QSize available = screen->availableGeometry().size();
        const QMargins full{Theme::Metric::ShadowMarginX, Theme::Metric::ShadowMarginTop,
                            Theme::Metric::ShadowMarginX, Theme::Metric::ShadowMarginBottom};
        const QSize withShadow(size.width() + full.left() + full.right(),
                               size.height() + full.top() + full.bottom());
        m_shadowEnabled = withShadow.width() <= available.width()
                          && withShadow.height() <= available.height();
    }

    const QMargins m = shadowMargins();
    setMinimumSize(size.width() + m.left() + m.right(),
                   size.height() + m.top() + m.bottom());
}

void FramelessWindow::toggleMaximize()
{
    if (isMaximized())
        showNormal();
    else
        showMaximized();
}

void FramelessWindow::changeEvent(QEvent *e)
{
    QWidget::changeEvent(e);
    if (e->type() != QEvent::WindowStateChange)
        return;

    const QMargins m = shadowMargins();
    setMinimumSize(m_cardMinimum.width() + m.left() + m.right(),
                   m_cardMinimum.height() + m.top() + m.bottom());
    m_shadow = QPixmap();
    m_card->setGeometry(cardRect());
    update();
    Q_EMIT maximizedChanged(isMaximized());
}

void FramelessWindow::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    m_card->setGeometry(cardRect());
    m_shadow = QPixmap();
}

void FramelessWindow::rebuildShadow()
{
    using namespace Theme;

    const QRect card = cardRect();
    if (card.isEmpty() || shadowMargins().isNull()) {
        m_shadow = QPixmap();
        return;
    }

    // Rendered at 1× and scaled up on draw: a shadow has no detail to lose, and this
    // keeps the blur cheap on high-dpi screens.
    QImage image(size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    {
        QPainter p(&image);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, Metric::ShadowAlpha));
        p.drawRoundedRect(card.translated(0, Metric::ShadowOffsetY),
                          Metric::WindowRadius, Metric::WindowRadius);
    }
    boxBlurAlpha(image, Metric::ShadowBlur / 2);

    m_shadow = QPixmap::fromImage(image);
}

void FramelessWindow::paintEvent(QPaintEvent *)
{
    using namespace Theme;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    if (!shadowMargins().isNull()) {
        if (m_shadow.isNull())
            rebuildShadow();
        if (!m_shadow.isNull())
            p.drawPixmap(0, 0, m_shadow);
    }

    const QRectF card = cardRect();
    const qreal radius = isMaximized() ? 0.0 : Metric::WindowRadius;

    p.setPen(QPen(Color::BorderWindow(), 1.0));
    p.setBrush(Color::Window());
    p.drawRoundedRect(card.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
}

Qt::Edges FramelessWindow::edgeAt(const QPoint &pos) const
{
    if (isMaximized() || isFullScreen())
        return {};

    const QRect card = cardRect();
    const QRect outer = card.adjusted(-GrabOutside, -GrabOutside, GrabOutside, GrabOutside);
    if (!outer.contains(pos))
        return {};

    Qt::Edges edges;
    if (pos.x() <= card.left() + GrabInside)   edges |= Qt::LeftEdge;
    if (pos.x() >= card.right() - GrabInside)  edges |= Qt::RightEdge;
    if (pos.y() <= card.top() + GrabInside)    edges |= Qt::TopEdge;
    if (pos.y() >= card.bottom() - GrabInside) edges |= Qt::BottomEdge;
    return edges;
}

void FramelessWindow::mouseMoveEvent(QMouseEvent *e)
{
    const Qt::Edges edges = edgeAt(e->pos());

    Qt::CursorShape shape = Qt::ArrowCursor;
    if ((edges & Qt::LeftEdge && edges & Qt::TopEdge) || (edges & Qt::RightEdge && edges & Qt::BottomEdge))
        shape = Qt::SizeFDiagCursor;
    else if ((edges & Qt::RightEdge && edges & Qt::TopEdge) || (edges & Qt::LeftEdge && edges & Qt::BottomEdge))
        shape = Qt::SizeBDiagCursor;
    else if (edges & (Qt::LeftEdge | Qt::RightEdge))
        shape = Qt::SizeHorCursor;
    else if (edges & (Qt::TopEdge | Qt::BottomEdge))
        shape = Qt::SizeVerCursor;

    if (cursor().shape() != shape)
        setCursor(shape);

    QWidget::mouseMoveEvent(e);
}

void FramelessWindow::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        if (const Qt::Edges edges = edgeAt(e->pos())) {
            if (QWindow *handle = windowHandle()) {
                handle->startSystemResize(edges);
                e->accept();
                return;
            }
        }
    }
    QWidget::mousePressEvent(e);
}

bool FramelessWindow::event(QEvent *e)
{
    // Clicks that land on the transparent shadow margin must not reach the app.
    if (e->type() == QEvent::MouseButtonPress || e->type() == QEvent::MouseButtonDblClick) {
        auto *me = static_cast<QMouseEvent *>(e);
        if (!cardRect().contains(me->pos()) && !edgeAt(me->pos())) {
            e->ignore();
            return true;
        }
    }
    return QWidget::event(e);
}

QPixmap FramelessWindow::grabCard()
{
    // Grab through the window, not the card: the card is translucent and the rounded
    // background plus the 1px border are painted by this widget.
    return grab(cardRect());
}
