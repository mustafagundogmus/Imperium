#include "theme.h"

#include <QFontDatabase>
#include <QGuiApplication>
#include <QScreen>
#include <QSettings>

namespace Theme {
namespace {

// Resolved family name per face. IBM Plex ships "Text" as a real 450 optical weight,
// and depending on the platform font database Qt exposes it either as a style of
// "IBM Plex Sans" or as its own family "IBM Plex Sans Text" — so we record whichever
// family name each file actually registered under instead of guessing.
struct FaceTable
{
    QString sans[4];   // Regular, Text, Medium, SemiBold
    QString mono[2];   // Regular, Medium
    bool loaded = false;
};

FaceTable &faces()
{
    static FaceTable t;
    return t;
}

QString loadFace(const QString &path, const QString &fallback)
{
    const int id = QFontDatabase::addApplicationFont(path);
    if (id < 0)
        return fallback;
    const QStringList families = QFontDatabase::applicationFontFamilies(id);
    return families.isEmpty() ? fallback : families.first();
}

QColor g_accent{0xD2, 0xA7, 0x5A};
bool g_compact = false;
Appearance g_appearance = Appearance::Dark;

/// The handoff's palette, verbatim.
const Palette &darkPalette()
{
    static const Palette p{
        /* page          */ {0x09, 0x09, 0x0B},
        /* window        */ {0x12, 0x12, 0x14},
        /* surface       */ {0x16, 0x16, 0x1A},
        /* surfaceHover  */ {0x18, 0x18, 0x1D},
        /* surfaceActive */ {0x1C, 0x1C, 0x21},
        /* tile          */ {0x14, 0x14, 0x17},

        /* borderWindow  */ {0x26, 0x26, 0x2B},
        /* borderControl */ {0x26, 0x26, 0x2C},
        /* divider       */ {0x1D, 0x1D, 0x22},
        /* dividerSoft   */ {0x17, 0x17, 0x1B},
        /* tileBorder    */ {0x1F, 0x1F, 0x24},

        /* toggleOff       */ {0x23, 0x23, 0x29},
        /* toggleOffBorder */ {0x33, 0x33, 0x3A},
        /* knobOff         */ {0x8A, 0x8A, 0x93},
        /* knobOn          */ {0x14, 0x14, 0x14},

        /* textPrimary   */ {0xE8, 0xE8, 0xEA},
        /* textSecondary */ {0xA3, 0xA3, 0xAC},
        /* textStatus    */ {0x9A, 0x9A, 0xA3},
        /* textDesc      */ {0x77, 0x77, 0x7F},
        /* textMuted     */ {0x7A, 0x7A, 0x84},
        /* textDim       */ {0x6E, 0x6E, 0x78},
        /* textFaint     */ {0x55, 0x55, 0x5E},
        /* textFainter   */ {0x45, 0x45, 0x4E},
        /* textMono      */ {0xC6, 0xC6, 0xCE},
        /* placeholder   */ {0x5A, 0x5A, 0x64},
        /* iconStroke    */ {0x8A, 0x8A, 0x93},

        /* onAccent      */ {0x14, 0x14, 0x14},
        /* closeHover    */ {0x3A, 0x1D, 0x1F},
        /* linkHover     */ {0xE4, 0xC2, 0x88},
        /* scrollThumb   */ {0x23, 0x23, 0x27},
    };
    return p;
}

/// Not in the handoff. The lightness relationships of the dark palette are mirrored
/// rather than inverted: the same neutral, slightly blue-leaning greys, and the same
/// ordering from primary text down to the faintest label.
const Palette &lightPalette()
{
    static const Palette p{
        /* page          */ {0xE9, 0xE9, 0xED},
        /* window        */ {0xFB, 0xFB, 0xFC},
        /* surface       */ {0xF3, 0xF3, 0xF6},
        /* surfaceHover  */ {0xEE, 0xEE, 0xF2},
        /* surfaceActive */ {0xE6, 0xE6, 0xEB},
        /* tile          */ {0xF7, 0xF7, 0xF9},

        /* borderWindow  */ {0xCF, 0xCF, 0xD6},
        /* borderControl */ {0xD7, 0xD7, 0xDD},
        /* divider       */ {0xE4, 0xE4, 0xE9},
        /* dividerSoft   */ {0xED, 0xED, 0xF1},
        /* tileBorder    */ {0xE2, 0xE2, 0xE8},

        /* toggleOff       */ {0xDB, 0xDB, 0xE2},
        /* toggleOffBorder */ {0xC3, 0xC3, 0xCC},
        /* knobOff         */ {0xFF, 0xFF, 0xFF},
        /* knobOn          */ {0x14, 0x14, 0x14},

        /* textPrimary   */ {0x18, 0x18, 0x1D},
        /* textSecondary */ {0x45, 0x45, 0x50},
        /* textStatus    */ {0x50, 0x50, 0x5B},
        /* textDesc      */ {0x6B, 0x6B, 0x76},
        /* textMuted     */ {0x6E, 0x6E, 0x79},
        /* textDim       */ {0x77, 0x77, 0x82},
        /* textFaint     */ {0x93, 0x93, 0x9D},
        /* textFainter   */ {0xA9, 0xA9, 0xB2},
        /* textMono      */ {0x34, 0x34, 0x3E},
        /* placeholder   */ {0x93, 0x93, 0x9D},
        /* iconStroke    */ {0x6B, 0x6B, 0x76},

        /* onAccent      */ {0x14, 0x14, 0x14},
        /* closeHover    */ {0xF2, 0xD6, 0xD8},
        /* linkHover     */ {0x8A, 0x63, 0x1E},
        /* scrollThumb   */ {0xC6, 0xC6, 0xCF},
    };
    return p;
}

} // namespace

const Palette &palette()
{
    return g_appearance == Appearance::Light ? lightPalette() : darkPalette();
}

Appearance appearance()
{
    return g_appearance;
}

void setAppearance(Appearance a)
{
    if (a == g_appearance)
        return;
    g_appearance = a;
    QSettings().setValue(QStringLiteral("appearance/theme"),
                         a == Appearance::Light ? QStringLiteral("light") : QStringLiteral("dark"));
    Q_EMIT notifier()->appearanceChanged();
}

/// Logical DPI of the primary screen. Qt derives a font's pixel size from
/// `pointSize * dpi / 72`, so dividing by the very same DPI round-trips exactly.
qreal logicalDpi()
{
    static qreal dpi = []() -> qreal {
        if (const QScreen *s = QGuiApplication::primaryScreen()) {
            const qreal d = s->logicalDotsPerInch();
            if (d > 1.0)
                return d;
        }
        return 96.0;
    }();
    return dpi;
}

qreal pixelSize(const QFont &f)
{
    if (f.pixelSize() > 0)
        return f.pixelSize();
    return f.pointSizeF() * logicalDpi() / 72.0;
}

void initFonts()
{
    FaceTable &t = faces();
    if (t.loaded)
        return;
    t.loaded = true;

    t.sans[0] = loadFace(QStringLiteral(":/fonts/IBMPlexSans-Regular.ttf"),  QStringLiteral("IBM Plex Sans"));
    t.sans[1] = loadFace(QStringLiteral(":/fonts/IBMPlexSans-Text.ttf"),     t.sans[0]);
    t.sans[2] = loadFace(QStringLiteral(":/fonts/IBMPlexSans-Medium.ttf"),   t.sans[0]);
    t.sans[3] = loadFace(QStringLiteral(":/fonts/IBMPlexSans-SemiBold.ttf"), t.sans[0]);
    t.mono[0] = loadFace(QStringLiteral(":/fonts/IBMPlexMono-Regular.ttf"),  QStringLiteral("IBM Plex Mono"));
    t.mono[1] = loadFace(QStringLiteral(":/fonts/IBMPlexMono-Medium.ttf"),   t.mono[0]);

    // Restore user overrides for the two props the mockup itself exposes.
    QSettings s;
    const QString accent = s.value(QStringLiteral("appearance/accent")).toString();
    if (QColor(accent).isValid())
        g_accent = QColor(accent);
    g_compact = s.value(QStringLiteral("layout/compact"), false).toBool();
    g_appearance = s.value(QStringLiteral("appearance/theme")).toString() == QLatin1String("light")
                       ? Appearance::Light
                       : Appearance::Dark;
}

QFont font(Family family, qreal px, int weight, qreal letterSpacingEm)
{
    const FaceTable &t = faces();

    QString name;
    if (family == Family::Sans) {
        if (weight >= Weight::SemiBold)   name = t.sans[3];
        else if (weight >= Weight::Medium) name = t.sans[2];
        else if (weight >= Weight::Text)   name = t.sans[1];
        else                               name = t.sans[0];
        if (name.isEmpty())
            name = QStringLiteral("IBM Plex Sans");
    } else {
        name = (weight >= Weight::Medium) ? t.mono[1] : t.mono[0];
        if (name.isEmpty())
            name = QStringLiteral("IBM Plex Mono");
    }

    QFont f(name);
    f.setWeight(static_cast<QFont::Weight>(weight));
    f.setPointSizeF(px * 72.0 / logicalDpi());
    f.setStyleStrategy(QFont::PreferAntialias);
    if (!qFuzzyIsNull(letterSpacingEm))
        f.setLetterSpacing(QFont::AbsoluteSpacing, px * letterSpacingEm);
    return f;
}

namespace Font {

// Each style is built once, on first use, after initFonts() has registered the faces.
#define TWEAKER_FONT(NAME, EXPR)                 \
    const QFont &NAME()                          \
    {                                            \
        static const QFont f = (EXPR);           \
        return f;                                \
    }

TWEAKER_FONT(appName,             mono(11.5, Weight::Medium, 0.02))
TWEAKER_FONT(monoMeta,            mono(10))
TWEAKER_FONT(searchText,          sans(11.5))
TWEAKER_FONT(kbd,                 mono(9))
TWEAKER_FONT(categoryName,        sans(12.5, Weight::Text))
TWEAKER_FONT(categoryNameSelected,sans(12.5, Weight::Medium))
TWEAKER_FONT(categoryCount,       mono(10))
TWEAKER_FONT(upperLabel,          sans(10, Weight::Regular, 0.08))
TWEAKER_FONT(restoreValue,        sans(11.5))
TWEAKER_FONT(link,                sans(10.5))
TWEAKER_FONT(pageTitle,           sans(15, Weight::SemiBold, -0.01))
TWEAKER_FONT(pageSub,             sans(11))
TWEAKER_FONT(segment,             sans(11))
TWEAKER_FONT(sectionTitle,        sans(10, Weight::Medium, 0.09))
TWEAKER_FONT(sectionCount,        mono(9.5))
TWEAKER_FONT(tweakName,           sans(12.5, Weight::Text))
TWEAKER_FONT(tweakDesc,           sans(10.5))
TWEAKER_FONT(tileLabel,           sans(10, Weight::Regular, 0.08))
TWEAKER_FONT(tileValue,           mono(15))
TWEAKER_FONT(tileSub,             sans(10))
TWEAKER_FONT(infoLabel,           sans(11))
TWEAKER_FONT(infoValueMono,       mono(10.5))
TWEAKER_FONT(infoValueText,       sans(11))
TWEAKER_FONT(statusPending,       sans(11))
TWEAKER_FONT(buttonGhost,         sans(11))
TWEAKER_FONT(buttonAccent,        sans(11, Weight::Medium))

#undef TWEAKER_FONT

} // namespace Font

QList<QColor> accentPresets()
{
    return {QColor(0xD2, 0xA7, 0x5A),   // amber (default)
            QColor(0x7F, 0xB8, 0xA4),   // sage
            QColor(0x8E, 0x9B, 0xD8),   // periwinkle
            QColor(0xC4, 0xC4, 0xCC)};  // neutral
}

QColor accent()
{
    return g_accent;
}

QColor accentSoft()
{
    QColor c = g_accent;
    // The mockup's `accent + "21"` → 13%. On white that wash all but disappears, so the
    // light palette leans on it a little harder to keep the selected row readable.
    c.setAlpha(g_appearance == Appearance::Light ? 0x30 : 0x21);
    return c;
}

QColor accentInk()
{
    if (g_appearance != Appearance::Light)
        return g_accent;

    // Amber at full strength is ~5:1 against #121214 but under 2:1 against white.
    // Darkening the value while keeping hue and saturation preserves the identity.
    QColor c = g_accent.toHsv();
    return QColor::fromHsv(c.hue(), qMin(255, int(c.saturation() * 1.15)),
                           int(c.value() * 0.62));
}

QString accentName(const QColor &c)
{
    static const QHash<QString, QString> names{
        {QStringLiteral("#d2a75a"), QStringLiteral("Amber")},
        {QStringLiteral("#7fb8a4"), QStringLiteral("Adaçayı")},
        {QStringLiteral("#8e9bd8"), QStringLiteral("Lavanta")},
        {QStringLiteral("#c4c4cc"), QStringLiteral("Nötr")},
    };
    return names.value(c.name(QColor::HexRgb).toLower(), c.name(QColor::HexRgb).toUpper());
}

void setAccent(const QColor &c)
{
    if (!c.isValid() || c == g_accent)
        return;
    g_accent = c;
    QSettings().setValue(QStringLiteral("appearance/accent"), c.name(QColor::HexRgb));
    Q_EMIT notifier()->accentChanged();
}

Notifier *notifier()
{
    static Notifier n;
    return &n;
}

bool compact()
{
    return g_compact;
}

void setCompact(bool on)
{
    if (on == g_compact)
        return;
    g_compact = on;
    QSettings().setValue(QStringLiteral("layout/compact"), on);
    Q_EMIT notifier()->compactChanged();
}

} // namespace Theme
