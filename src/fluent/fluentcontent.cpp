#include "fluentcontent.h"
#include "applybar.h"
#include "fluentheader.h"
#include "../css.h"
#include "../theme.h"

#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>

namespace {
constexpr qreal Radius = Theme::Fluent::WindowRadius;
constexpr int StackInset = 18;   // 36 − the pages' own PagePadLeft
} // namespace

FluentContent::FluentContent(FluentHeader *header, QWidget *stack, ApplyBar *bar, QWidget *parent)
    : QWidget(parent)
{
    auto *column = new QVBoxLayout(this);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);

    header->setParent(this);
    column->addWidget(header);

    auto *inset = new QHBoxLayout;
    inset->setContentsMargins(StackInset, 0, StackInset, 0);
    inset->setSpacing(0);
    stack->setParent(this);
    inset->addWidget(stack, 1);
    column->addLayout(inset, 1);

    bar->setParent(this);
    column->addWidget(bar);

    connect(Theme::notifier(), &Theme::Notifier::appearanceChanged, this, qOverload<>(&QWidget::update));
}

void FluentContent::paintEvent(QPaintEvent *)
{
    const Theme::Fluent::Tokens &t = Theme::Fluent::tokens();

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF r = rect();
    QPainterPath path;
    path.moveTo(r.left(), r.bottom());
    path.lineTo(r.left(), r.top() + Radius);
    path.arcTo(r.left(), r.top(), 2 * Radius, 2 * Radius, 180, -90);
    path.lineTo(r.right(), r.top());
    path.lineTo(r.right(), r.bottom());
    path.closeSubpath();
    p.fillPath(path, t.surface);

    // The 1px border along the top and the left, following the same corner.
    QPainterPath edge;
    edge.moveTo(r.left() + 0.5, r.bottom());
    edge.lineTo(r.left() + 0.5, r.top() + Radius);
    edge.arcTo(r.left() + 0.5, r.top() + 0.5, 2 * Radius, 2 * Radius, 180, -90);
    edge.lineTo(r.right(), r.top() + 0.5);
    p.setPen(QPen(t.cardBorder, 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawPath(edge);
}
