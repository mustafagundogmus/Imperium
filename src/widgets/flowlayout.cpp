#include "flowlayout.h"

#include <QWidget>

FlowLayout::FlowLayout(QWidget *parent, int hGap, int vGap)
    : QLayout(parent)
    , m_hGap(hGap)
    , m_vGap(vGap)
{
    setContentsMargins(0, 0, 0, 0);
}

FlowLayout::~FlowLayout()
{
    while (QLayoutItem *item = takeAt(0))
        delete item;
}

void FlowLayout::addItem(QLayoutItem *item)
{
    m_items.append(item);
}

int FlowLayout::count() const
{
    return int(m_items.size());
}

QLayoutItem *FlowLayout::itemAt(int index) const
{
    return index >= 0 && index < m_items.size() ? m_items.at(index) : nullptr;
}

QLayoutItem *FlowLayout::takeAt(int index)
{
    if (index >= 0 && index < m_items.size())
        return m_items.takeAt(index);
    return nullptr;
}

Qt::Orientations FlowLayout::expandingDirections() const
{
    return {};
}

bool FlowLayout::hasHeightForWidth() const
{
    return true;
}

int FlowLayout::heightForWidth(int width) const
{
    return doLayout(QRect(0, 0, width, 0), true);
}

QSize FlowLayout::minimumSize() const
{
    // Wide enough for the widest tile: a line can always hold one.
    QSize size;
    for (const QLayoutItem *item : m_items)
        size = size.expandedTo(item->minimumSize());
    const QMargins m = contentsMargins();
    return size + QSize(m.left() + m.right(), m.top() + m.bottom());
}

QSize FlowLayout::sizeHint() const
{
    return minimumSize();
}

void FlowLayout::setGeometry(const QRect &rect)
{
    QLayout::setGeometry(rect);
    doLayout(rect, false);
}

int FlowLayout::doLayout(const QRect &rect, bool dryRun) const
{
    const QMargins m = contentsMargins();
    const QRect inner = rect.adjusted(m.left(), m.top(), -m.right(), -m.bottom());
    int x = inner.x();
    int y = inner.y();
    int lineHeight = 0;

    for (QLayoutItem *item : m_items) {
        // A hidden tile — one the filter took out — takes no room and breaks no line.
        if (item->widget() && item->widget()->isHidden())
            continue;
        const QSize hint = item->sizeHint();
        int nextX = x + hint.width() + m_hGap;
        if (nextX - m_hGap > inner.right() + 1 && lineHeight > 0) {
            x = inner.x();
            y += lineHeight + m_vGap;
            nextX = x + hint.width() + m_hGap;
            lineHeight = 0;
        }
        if (!dryRun)
            item->setGeometry(QRect(QPoint(x, y), hint));
        x = nextX;
        lineHeight = qMax(lineHeight, hint.height());
    }
    return y + lineHeight - rect.y() + m.bottom();
}
