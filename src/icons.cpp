#include "icons.h"

#include <QHash>
#include <QPainter>
#include <QSvgRenderer>

namespace Icons {
namespace {

QHash<QString, QPixmap> &cache()
{
    static QHash<QString, QPixmap> c;
    return c;
}

QPixmap render(const QString &key, const QString &inner, qreal viewBox,
               const QSize &size, qreal dpr)
{
    const QString id = QStringLiteral("%1|%2x%3|%4").arg(key).arg(size.width()).arg(size.height()).arg(dpr);
    const auto it = cache().constFind(id);
    if (it != cache().cend())
        return *it;

    const QString doc = QStringLiteral(
                            "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%1\" height=\"%1\" "
                            "viewBox=\"0 0 %1 %1\">%2</svg>")
                            .arg(viewBox)
                            .arg(inner);

    QSvgRenderer renderer(doc.toUtf8());
    QPixmap pm(QSize(qRound(size.width() * dpr), qRound(size.height() * dpr)));
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    {
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        renderer.render(&p, QRectF(0, 0, size.width(), size.height()));
    }
    cache().insert(id, pm);
    return pm;
}

QString hex(const QColor &c)
{
    return c.name(QColor::HexRgb);
}

constexpr qreal LucideViewBox = 24.0;

/// The stroke, in lucide's own 24 units, that comes out as exactly one logical pixel once
/// the glyph is scaled into a \a size box — because a stroke of w units renders at
/// w * size / 24, so one pixel is 24 / size.
///
/// Derived rather than fixed, which is the same thing category() above gets for free: it
/// renders a 12-unit viewBox into a 12px box, so its literal stroke of 1.0 *is* one pixel.
/// A constant cannot do that here, because these glyphs are sized off the title's line box
/// and that moves with the interface scale — lucide's own stroke of 2 would be 0.92px at
/// the smallest of the four steps and 1.33px at the largest, and anything under a whole
/// pixel is the failure Css::hairline documents: the raster engine blends it across two
/// rows at partial alpha, and the glyph reads visibly lighter than the title beside it.
/// Measured on all six faces, the four steps ask for boxes of 11, 13, 14 and 16px.
constexpr qreal lucideStroke(int size)
{
    return LucideViewBox / (size > 0 ? qreal(size) : LucideViewBox);
}

} // namespace

QPixmap strokePath(const QString &d, qreal viewBox, const QSize &size,
                   const QColor &c, qreal strokeWidth, qreal dpr)
{
    const QString inner = QStringLiteral(
                              "<path d=\"%1\" fill=\"none\" stroke=\"%2\" stroke-width=\"%3\" "
                              "stroke-linecap=\"round\" stroke-linejoin=\"round\"/>")
                              .arg(d, hex(c))
                              .arg(strokeWidth);
    return render(QStringLiteral("p:%1:%2:%3:%4").arg(d, hex(c)).arg(strokeWidth).arg(viewBox),
                  inner, viewBox, size, dpr);
}

QPixmap fragment(const QString &cacheKey, const QString &inner, qreal viewBox,
                 const QSize &size, qreal dpr)
{
    return render(cacheKey, inner, viewBox, size, dpr);
}

QPixmap search(const QColor &c, qreal dpr)
{
    const QString inner = QStringLiteral(
                              "<circle cx=\"5.2\" cy=\"5.2\" r=\"3.4\" fill=\"none\" stroke=\"%1\" stroke-width=\"1.1\"/>"
                              "<path d=\"M8 8l2.6 2.6\" fill=\"none\" stroke=\"%1\" stroke-width=\"1.1\"/>")
                              .arg(hex(c));
    // The mockup renders a 12-unit viewBox into an 11×11 box.
    return fragment(QStringLiteral("search:%1").arg(hex(c)), inner, 12, QSize(11, 11), dpr);
}

QPixmap windowMinimize(const QColor &c, qreal dpr)
{
    const QString inner = QStringLiteral("<path d=\"M1 5.5h8\" stroke=\"%1\" stroke-width=\"1\"/>").arg(hex(c));
    return fragment(QStringLiteral("wmin:%1").arg(hex(c)), inner, 10, QSize(10, 10), dpr);
}

QPixmap windowMaximize(const QColor &c, qreal dpr)
{
    const QString inner = QStringLiteral(
                              "<rect x=\"1.5\" y=\"1.5\" width=\"7\" height=\"7\" fill=\"none\" "
                              "stroke=\"%1\" stroke-width=\"1\"/>")
                              .arg(hex(c));
    return fragment(QStringLiteral("wmax:%1").arg(hex(c)), inner, 10, QSize(10, 10), dpr);
}

QPixmap windowRestore(const QColor &c, qreal dpr)
{
    // Not in the mockup (which only ever shows the restored state) but required once the
    // maximise button actually works. Drawn in the same 10-unit grid and 1px stroke.
    const QString inner = QStringLiteral(
                              "<path d=\"M3 3V1.5h5.5V7H7\" fill=\"none\" stroke=\"%1\" stroke-width=\"1\"/>"
                              "<rect x=\"1.5\" y=\"3\" width=\"5.5\" height=\"5.5\" fill=\"none\" "
                              "stroke=\"%1\" stroke-width=\"1\"/>")
                              .arg(hex(c));
    return fragment(QStringLiteral("wres:%1").arg(hex(c)), inner, 10, QSize(10, 10), dpr);
}

QPixmap windowClose(const QColor &c, qreal dpr)
{
    const QString inner = QStringLiteral("<path d=\"M2 2l6 6M8 2l-6 6\" stroke=\"%1\" stroke-width=\"1\"/>").arg(hex(c));
    return fragment(QStringLiteral("wcls:%1").arg(hex(c)), inner, 10, QSize(10, 10), dpr);
}

QPixmap sort(const QColor &c, qreal dpr)
{
    const QString inner = QStringLiteral(
                              "<path d=\"M2 3.5h9M3.5 6.5h6M5 9.5h3\" fill=\"none\" stroke=\"%1\" "
                              "stroke-width=\"1.1\" stroke-linecap=\"round\"/>")
                              .arg(hex(c));
    return fragment(QStringLiteral("sort:%1").arg(hex(c)), inner, 13, QSize(13, 13), dpr);
}

QPixmap category(const QString &pathData, const QColor &c, qreal dpr)
{
    return strokePath(pathData, 12, QSize(12, 12), c, 1.0, dpr);
}

QPixmap lucide(const Glyph &g, const QColor &c, int size, qreal dpr)
{
    // Iconify hangs the stroke on a wrapping <g> rather than on each element, which is why
    // the shapes below carry no attributes of their own: colour and weight are decided
    // here, once, for the whole set.
    QString inner = QStringLiteral("<g fill=\"none\" stroke=\"%1\" stroke-width=\"%2\" "
                                   "stroke-linecap=\"round\" stroke-linejoin=\"round\">")
                        .arg(hex(c))
                        .arg(lucideStroke(size));
    inner += QString::fromLatin1(g.shapes);
    inner += QStringLiteral("</g>");

    // The name and the colour are all this adds to the key. They are enough: the shapes are
    // a compile-time constant only ever reached through the name, so one name cannot stand
    // for two drawings, and the stroke is a pure function of the size, which render()
    // already puts in the id.
    return fragment(QStringLiteral("lu:%1:%2").arg(QString::fromLatin1(g.name), hex(c)),
                    inner, LucideViewBox, QSize(size, size), dpr);
}

// The glyphs themselves, verbatim from https://api.iconify.design/lucide/<name>.svg with
// the presentation attributes removed (lucide() supplies those) and nothing else touched.
// The trailing comment on each names the Genel Bakış card it titles; overviewpage.cpp is
// where the two are actually paired.
namespace Lucide {

const Glyph AppWindow{   // m_system
    "app-window",
    "<rect width=\"20\" height=\"16\" x=\"2\" y=\"4\" rx=\"2\"/><path d=\"M10 4v4M2 8h20M6 4v4\"/>"};
const Glyph BatteryCharging{   // m_power
    "battery-charging",
    "<path d=\"m11 7l-3 5h4l-3 5m5.856-11H16a2 2 0 0 1 2 2v8a2 2 0 0 1-2 2h-2.935M22 14v-4M5.14 18H4a2 2 0 0 1-2-2V8a2 2 0 0 1 2-2h2.936\"/>"};
const Glyph Box{   // m_virtualisation
    "box",
    "<path d=\"M21 8a2 2 0 0 0-1-1.73l-7-4a2 2 0 0 0-2 0l-7 4A2 2 0 0 0 3 8v8a2 2 0 0 0 1 1.73l7 4a2 2 0 0 0 2 0l7-4A2 2 0 0 0 21 16Z\"/>"
    "<path d=\"m3.3 7l8.7 5l8.7-5M12 22V12\"/>"};
const Glyph CalendarClock{   // m_tasks
    "calendar-clock",
    "<path d=\"M16 14v2.2l1.6 1M16 2v3m5 2.338V5a2 2 0 0 0-2-2H5a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h2.338M3 9h5.859M8 2v3\"/>"
    "<circle cx=\"16\" cy=\"16\" r=\"6\"/>"};
const Glyph CircuitBoard{   // m_hardware
    "circuit-board",
    "<rect width=\"18\" height=\"18\" x=\"3\" y=\"3\" rx=\"2\"/><path d=\"M11 9h4a2 2 0 0 0 2-2V3\"/>"
    "<circle cx=\"9\" cy=\"9\" r=\"2\"/><path d=\"M7 21v-4a2 2 0 0 1 2-2h4\"/>"
    "<circle cx=\"15\" cy=\"15\" r=\"2\"/>"};
const Glyph Clock{   // m_session
    "clock",
    "<circle cx=\"12\" cy=\"12\" r=\"10\"/><path d=\"M12 6v6l4 2\"/>"};
const Glyph Cpu{   // m_processor
    "cpu",
    "<path d=\"M12 20v2m0-20v2m5 16v2m0-20v2M2 12h2m-2 5h2M2 7h2m16 5h2m-2 5h2M20 7h2M7 20v2M7 2v2\"/>"
    "<rect width=\"16\" height=\"16\" x=\"4\" y=\"4\" rx=\"2\"/>"
    "<rect width=\"8\" height=\"8\" x=\"8\" y=\"8\" rx=\"1\"/>"};
const Glyph Download{   // m_update
    "download",
    "<path d=\"M12 15V3m9 12v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4\"/><path d=\"m7 10l5 5l5-5\"/>"};
const Glyph EyeOff{   // m_privacy
    "eye-off",
    "<path d=\"M10.733 5.076a10.744 10.744 0 0 1 11.205 6.575a1 1 0 0 1 0 .696a10.8 10.8 0 0 1-1.444 2.49m-6.41-.679a3 3 0 0 1-4.242-4.242\"/>"
    "<path d=\"M17.479 17.499a10.75 10.75 0 0 1-15.417-5.151a1 1 0 0 1 0-.696a10.75 10.75 0 0 1 4.446-5.143M2 2l20 20\"/>"};
const Glyph Gauge{   // m_performance
    "gauge",
    "<path d=\"m12 14l4-4M3.34 19a10 10 0 1 1 17.32 0\"/>"};
const Glyph Globe{   // m_locale
    "globe",
    "<circle cx=\"12\" cy=\"12\" r=\"10\"/>"
    "<path d=\"M12 2a14.5 14.5 0 0 0 0 20a14.5 14.5 0 0 0 0-20M2 12h20\"/>"};
const Glyph HardDrive{   // m_storage
    "hard-drive",
    "<path d=\"M10 16h.01m-7.798-4.423a2 2 0 0 0-.212.896V18a2 2 0 0 0 2 2h16a2 2 0 0 0 2-2v-5.527a2 2 0 0 0-.212-.896L18.55 5.11A2 2 0 0 0 16.76 4H7.24a2 2 0 0 0-1.79 1.11zm19.734.436H2.054M6 16h.01\"/>"};
const Glyph HeartPulse{   // m_diskHealth
    "heart-pulse",
    "<path d=\"M2 9.5a5.5 5.5 0 0 1 9.591-3.676a.56.56 0 0 0 .818 0A5.49 5.49 0 0 1 22 9.5c0 2.29-1.5 4-3 5.5l-5.492 5.313a2 2 0 0 1-3 .019L5 15c-1.5-1.5-3-3.2-3-5.5\"/>"
    "<path d=\"M3.22 13H9.5l.5-1l2 4.5l2-7l1.5 3.5h5.27\"/>"};
const Glyph Layers{   // m_processes
    "layers",
    "<path d=\"M12.83 2.18a2 2 0 0 0-1.66 0L2.6 6.08a1 1 0 0 0 0 1.83l8.58 3.91a2 2 0 0 0 1.66 0l8.58-3.9a1 1 0 0 0 0-1.83z\"/>"
    "<path d=\"M2 12a1 1 0 0 0 .58.91l8.6 3.91a2 2 0 0 0 1.65 0l8.58-3.9A1 1 0 0 0 22 12\"/>"
    "<path d=\"M2 17a1 1 0 0 0 .58.91l8.6 3.91a2 2 0 0 0 1.65 0l8.58-3.9A1 1 0 0 0 22 17\"/>"};
const Glyph Lock{   // m_encryption
    "lock",
    "<rect width=\"18\" height=\"11\" x=\"3\" y=\"11\" rx=\"2\" ry=\"2\"/>"
    "<path d=\"M7 11V7a5 5 0 0 1 10 0v4\"/>"};
const Glyph MemoryStick{   // m_memory
    "memory-stick",
    "<path d=\"M12 12v-2m0 8v-2m4-4v-2m0 8v-2M2 11h1.5M20 18v-2m.5-5H22M4 18v-2m4-4v-2m0 8v-2\"/>"
    "<rect width=\"20\" height=\"10\" x=\"2\" y=\"6\" rx=\"2\"/>"};
const Glyph Microchip{   // m_firmware
    "microchip",
    "<path d=\"M10 12h4m-4 5h4M10 7h4m4 5h2m-2 6h2M18 6h2M4 12h2m-2 6h2M4 6h2\"/>"
    "<rect width=\"12\" height=\"20\" x=\"6\" y=\"2\" rx=\"2\"/>"};
const Glyph Monitor{   // m_display
    "monitor",
    "<rect width=\"20\" height=\"14\" x=\"2\" y=\"3\" rx=\"2\"/><path d=\"M8 21h8m-4-4v4\"/>"};
const Glyph Network{   // m_network
    "network",
    "<rect width=\"6\" height=\"6\" x=\"16\" y=\"16\" rx=\"1\"/>"
    "<rect width=\"6\" height=\"6\" x=\"2\" y=\"16\" rx=\"1\"/>"
    "<rect width=\"6\" height=\"6\" x=\"9\" y=\"2\" rx=\"1\"/>"
    "<path d=\"M5 16v-3a1 1 0 0 1 1-1h12a1 1 0 0 1 1 1v3m-7-4V8\"/>"};
const Glyph Package{   // m_software
    "package",
    "<path d=\"M11 21.73a2 2 0 0 0 2 0l7-4A2 2 0 0 0 21 16V8a2 2 0 0 0-1-1.73l-7-4a2 2 0 0 0-2 0l-7 4A2 2 0 0 0 3 8v8a2 2 0 0 0 1 1.73zm1 .27V12\"/>"
    "<path d=\"M3.29 7L12 12l8.71-5M7.5 4.27l9 5.15\"/>"};
const Glyph Plug{   // m_drivers
    "plug",
    "<path d=\"M12 22v-5m3-9V2m2 6a1 1 0 0 1 1 1v4a4 4 0 0 1-4 4h-4a4 4 0 0 1-4-4V9a1 1 0 0 1 1-1zM9 8V2\"/>"};
const Glyph ShieldCheck{   // m_security
    "shield-check",
    "<path d=\"M20 13c0 5-3.5 7.5-7.66 8.95a1 1 0 0 1-.67-.01C7.5 20.5 4 18 4 13V6a1 1 0 0 1 1-1c2 0 4.5-1.2 6.24-2.72a1.17 1.17 0 0 1 1.52 0C14.51 3.81 17 5 19 5a1 1 0 0 1 1 1z\"/>"
    "<path d=\"m9 12l2 2l4-4\"/>"};
const Glyph SlidersHorizontal{   // m_catalog
    "sliders-horizontal",
    "<path d=\"M10 5H3m9 14H3M14 3v4m2 10v4m5-9h-9m9 7h-5m5-14h-7m-6 5v4m0-2H3\"/>"};
const Glyph SunMoon{   // AboutPage's Görünüm card — the light/dark pair the themes are
    "sun-moon",
    "<path d=\"M12 2v2m2.837 12.385a6 6 0 1 1-7.223-7.222c.624-.147.97.66.715 1.248a4 4 0 0 0 5.26 5.259c.589-.255 1.396.09 1.248.715M16 12a4 4 0 0 0-4-4m7-3l-1.256 1.256M20 12h2\"/>"};
const Glyph Thermometer{   // m_sensors
    "thermometer",
    "<path d=\"M14 4v10.54a4 4 0 1 1-4 0V4a2 2 0 0 1 4 0\"/>"};
const Glyph TriangleAlert{   // m_integrity
    "triangle-alert",
    "<path d=\"m21.73 18l-8-14a2 2 0 0 0-3.48 0l-8 14A2 2 0 0 0 4 21h16a2 2 0 0 0 1.73-3M12 9v4m0 4h.01\"/>"};
const Glyph User{   // m_user
    "user",
    "<path d=\"M19 21v-2a4 4 0 0 0-4-4H9a4 4 0 0 0-4 4v2\"/><circle cx=\"12\" cy=\"7\" r=\"4\"/>"};
const Glyph Users{   // m_accounts
    "users",
    "<path d=\"M16 21v-2a4 4 0 0 0-4-4H6a4 4 0 0 0-4 4v2M16 3.128a4 4 0 0 1 0 7.744M22 21v-2a4 4 0 0 0-3-3.87\"/>"
    "<circle cx=\"9\" cy=\"7\" r=\"4\"/>"};
const Glyph Wifi{   // m_connection
    "wifi",
    "<path d=\"M12 20h.01M2 8.82a15 15 0 0 1 20 0M5 12.859a10 10 0 0 1 14 0m-10.5 3.57a5 5 0 0 1 7 0\"/>"};

} // namespace Lucide

} // namespace Icons
