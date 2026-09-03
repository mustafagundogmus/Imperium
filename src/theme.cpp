#include "theme.h"
#include "i18n.h"

#include <QFontDatabase>
#include <QGuiApplication>
#include <QScreen>
#include <QSettings>

#include <cmath>

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

/// One entry per selectable interface face: the four weight slots the styles ask for,
/// as resource paths. A family that has no such weight repeats the nearest one it does
/// have, which is why Oxygen lists its bold twice.
struct FaceFiles
{
    const char *id;
    const char *family;    ///< fallback family name if the resource ever fails to load
    const char *display;   ///< what the settings row calls it
    const char *regular;
    const char *text;      // 450
    const char *medium;    // 500
    const char *semiBold;  // 600
};

const FaceFiles Faces[] = {
    {"plex", "IBM Plex Sans", "IBM Plex",
     ":/fonts/IBMPlexSans-Regular.ttf", ":/fonts/IBMPlexSans-Text.ttf",
     ":/fonts/IBMPlexSans-Medium.ttf",  ":/fonts/IBMPlexSans-SemiBold.ttf"},
    {"monda", "Monda", "Monda",
     ":/fonts/Monda-Regular.ttf", ":/fonts/Monda-Regular.ttf",
     ":/fonts/Monda-Medium.ttf",  ":/fonts/Monda-SemiBold.ttf"},
    {"opensans", "Open Sans", "Open Sans",
     ":/fonts/OpenSans-Regular.ttf", ":/fonts/OpenSans-Regular.ttf",
     ":/fonts/OpenSans-Medium.ttf",  ":/fonts/OpenSans-SemiBold.ttf"},
    {"oxygen", "Oxygen", "Oxygen",
     ":/fonts/Oxygen-Regular.ttf", ":/fonts/Oxygen-Regular.ttf",
     ":/fonts/Oxygen-Regular.ttf", ":/fonts/Oxygen-Bold.ttf"},
    {"redhat", "Red Hat Text", "Red Hat",
     ":/fonts/RedHatText-Regular.ttf", ":/fonts/RedHatText-Regular.ttf",
     ":/fonts/RedHatText-Medium.ttf",  ":/fonts/RedHatText-SemiBold.ttf"},
    {"saira", "Saira", "Saira",
     ":/fonts/Saira-Regular.ttf", ":/fonts/Saira-Regular.ttf",
     ":/fonts/Saira-Medium.ttf",  ":/fonts/Saira-SemiBold.ttf"},
};

const FaceFiles *faceFor(const QString &id)
{
    for (const FaceFiles &f : Faces)
        if (id == QLatin1String(f.id))
            return &f;
    return &Faces[0];
}

QString g_typeface = QStringLiteral("plex");

/// A single multiplier applied to every text size in the app, so the whole interface can
/// be made larger or smaller without touching the individual style definitions. 1.0 is
/// the design's own sizes.
qreal g_fontScale = 1.0;

/// Bumped whenever the styles have to be rebuilt; the Font:: accessors compare against
/// it and recompute on the next call rather than being invalidated one by one.
int g_fontGeneration = 0;

int fontGeneration()
{
    return g_fontGeneration;
}

QString loadFace(const QString &path, const QString &fallback)
{
    // Memoised by path, because QFontDatabase::addApplicationFont() does not deduplicate:
    // it appends a fresh entry and hands the platform database the same bytes again every
    // time it is called. The comment this replaced claimed the opposite, so every typeface
    // pick registered four more copies of four font files, and the two mono faces were
    // re-registered on top of that.
    static QHash<QString, QString> resolved;
    const auto cached = resolved.constFind(path);
    if (cached != resolved.cend())
        return *cached;

    const int id = QFontDatabase::addApplicationFont(path);
    if (id < 0)
        return fallback;   // not cached: a later call may pass a different fallback
    const QStringList families = QFontDatabase::applicationFontFamilies(id);
    if (families.isEmpty())
        return fallback;

    resolved.insert(path, families.first());
    return families.first();
}

QColor g_accent{0xD2, 0xA7, 0x5A};
bool g_compact = false;
Appearance g_appearance = Appearance::Dark;
Shell g_shell = Shell::Classic;

/// \a over composited onto \a under, source-over at 8 bits — what the raster engine does
/// with a translucent fill. The Fluent tokens are translucent and the Palette struct is
/// read as opaque colours (stylesheets take .name(), the ink solver measures contrast), so
/// the palette below carries each token already laid on the ground it is designed for.
QColor flatten(const QColor &over, const QColor &under)
{
    const int a = over.alpha();
    const auto mix = [a](int o, int u) { return (o * a + u * (255 - a) + 127) / 255; };
    return {mix(over.red(), under.red()), mix(over.green(), under.green()),
            mix(over.blue(), under.blue())};
}

/// A straight blend, \a t of the way from \a a to \a b.
QColor mixColors(const QColor &a, const QColor &b, qreal t)
{
    t = qBound(0.0, t, 1.0);
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t,
                            a.greenF() + (b.greenF() - a.greenF()) * t,
                            a.blueF() + (b.blueF() - a.blueF()) * t);
}

/// The two families a QFontDatabase lookup can be memoised against. Segoe UI Variable is a
/// Windows 11 face; on a Windows 10 machine the fallback is the classic Segoe UI, and on a
/// machine with neither (a test box, Wine) the family names below resolve to whatever Qt
/// substitutes, which is still a readable sans.
QString firstInstalled(const QStringList &candidates)
{
    for (const QString &family : candidates)
        if (QFontDatabase::hasFamily(family))
            return family;
    return candidates.last();
}

const QString &fluentSans()
{
    static const QString family = firstInstalled({QStringLiteral("Segoe UI Variable Text"),
                                                  QStringLiteral("Segoe UI Variable"),
                                                  QStringLiteral("Segoe UI")});
    return family;
}

const QString &fluentMono()
{
    static const QString family = firstInstalled({QStringLiteral("Cascadia Mono"),
                                                  QStringLiteral("Consolas")});
    return family;
}

/// The handoff's palette, lifted.
///
/// The handoff's own values ran the text ramp down to #45454E on a #121214 window — 1.97:1,
/// on a 9.5px label. Measured against WCAG, five of its eleven text tokens missed the 4.5:1
/// that small text needs, and the two faintest missed 3:1 as well:
///
///     textDesc  4.21   textMuted 4.41   textDim 3.71   placeholder 2.75
///     textFaint 2.54   textFainter 1.97
///
/// So the ramp was rebuilt against those thresholds instead of against the eyedropper. The
/// surfaces come up a step at the same time — the complaint was that the whole thing read
/// too dark, not only that the text was too dim — and since lifting the background eats
/// into contrast, the text had to move further than the surfaces did. Nothing about the
/// structure changed: same neutral, faintly blue-leaning greys, same ordering from primary
/// down to the faintest label, same 1px-hairline language for the borders.
///
/// Every token clears 4.5:1 against the window, and — since a token is read against
/// whatever it is painted on, not against the window — against every other surface it
/// lands on as well. The annotations below are the window figure, which is the loosest
/// of the two for a dark palette; the binding one is named beside each token.
///
/// The sentence that stood here said the faintest, the "4 öğe" count, sat at 4.52:1 and
/// left it at that. 4.52 is its ratio against the window and nothing else: the count is
/// also drawn on the overview tile and on the live chart, both #1B1B20, where the same
/// grey came to 4.34 and missed. It is #83838F now — 4.77 on the window, 4.58 on the
/// tile — which is the lift the four light schemes below had already been given for this
/// exact reason.
const Palette &darkPalette()
{
    static const Palette p{
        /* window        */ {0x17, 0x17, 0x1B},
        /* surface       */ {0x1E, 0x1E, 0x23},
        /* surfaceHover  */ {0x23, 0x23, 0x29},
        /* surfaceActive */ {0x29, 0x29, 0x2F},
        /* tile          */ {0x1B, 0x1B, 0x20},

        /* borderWindow  */ {0x31, 0x31, 0x3A},
        /* borderControl */ {0x33, 0x33, 0x3B},
        /* divider       */ {0x29, 0x29, 0x2F},
        /* dividerSoft   */ {0x22, 0x22, 0x27},
        /* tileBorder    */ {0x2B, 0x2B, 0x32},

        /* toggleOff       */ {0x2E, 0x2E, 0x36},
        /* toggleOffBorder */ {0x41, 0x41, 0x49},
        /* knobOff         */ {0xA2, 0xA2, 0xAC},
        /* knobOn          */ {0x14, 0x14, 0x14},

        //                                          window   on its binding surface
        /* textPrimary   */ {0xF0, 0xF0, 0xF3},   // 15.72   12.71  surfaceActive
        /* textSecondary */ {0xC0, 0xC0, 0xCA},   //  9.90    8.66  surfaceHover
        /* textStatus    */ {0xB4, 0xB4, 0xBE},   //  8.69    8.69  window
        /* textDesc      */ {0xA2, 0xA2, 0xAD},   //  7.07    6.18  surfaceHover
        /* textMuted     */ {0xA5, 0xA5, 0xB0},   //  7.33    7.33  window
        /* textDim       */ {0x94, 0x94, 0xA0},   //  5.96    5.72  tile
        /* textFaint     */ {0x8E, 0x8E, 0x9A},   //  5.52    4.83  surfaceHover
        /* textFainter   */ {0x83, 0x83, 0x8F},   //  4.77    4.58  tile
        /* textMono      */ {0xDC, 0xDC, 0xE4},   // 13.11   11.46  surfaceHover
        /* placeholder   */ {0x8C, 0x8C, 0x97},   //  5.37    4.99  surface
        /* iconStroke    */ {0xA8, 0xA8, 0xB2},   //  7.58    6.13  surfaceActive

        /* onAccent      */ {0x14, 0x14, 0x14},
        /* scrollThumb   */ {0x35, 0x35, 0x3C},
    };
    return p;
}

/// Not in the handoff. The lightness relationships of the dark palette are mirrored
/// rather than inverted: the same neutral, slightly blue-leaning greys, and the same
/// ordering from primary text down to the faintest label.
///
/// The mirroring used to be literal, and that was the bug. Dark's ramp was reflected as
/// *lightness*, not as contrast, so the faint end of it came out pale rather than dim:
/// measured against this palette's own #FBFBFC window, textDim was 4.28, textFaint and
/// placeholder 2.94, and the "4 öğe" count 2.25 — the token the dark palette holds above
/// 4.5. Seven of the eleven missed the floor on the ground they are actually drawn on.
/// The light family only looked like it had a wider tonal ramp than the dark one; it
/// bought the spread by failing the bar.
///
/// The text now carries the ratios rather than the lightnesses. Each token keeps this
/// palette's own hue — R=G with blue eleven levels up, the curve the surviving tokens
/// already described — and only its lightness moved, down. The surfaces are untouched:
/// somebody chose this cool near-white and it is still a cool near-white.
///
/// Nothing above textStatus needed the floor; textSecondary and textStatus moved anyway,
/// by about a level of the ramp each, because the tokens under them had to come up so far
/// that leaving them put textStatus and textDesc three 8-bit levels apart. A step you
/// cannot see is not a step.
///
/// Two annotations per token: against the window, and against the darkest surface that
/// token is painted on — which for a light palette is the binding one, and is not the
/// window for eight of the eleven. iconStroke is the window-control glyph and shares
/// textDesc's literal, so it drags textDesc up to what surfaceActive needs; textFaint and
/// placeholder share a literal too, as they always have here.
const Palette &lightPalette()
{
    static const Palette p{
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

        //                                          window   on its binding surface
        /* textPrimary   */ {0x18, 0x18, 0x1D},   // 17.10   14.22  surfaceActive
        /* textSecondary */ {0x40, 0x40, 0x4B},   //  9.89    8.84  surfaceHover
        /* textStatus    */ {0x48, 0x48, 0x53},   //  8.72    8.72  window
        /* textDesc      */ {0x53, 0x53, 0x5E},   //  7.34    6.56  surfaceHover
        /* textMuted     */ {0x56, 0x56, 0x61},   //  7.00    7.00  window
        /* textDim       */ {0x5F, 0x5F, 0x6A},   //  6.10    5.89  tile
        /* textFaint     */ {0x66, 0x66, 0x71},   //  5.48    4.90  surfaceHover
        /* textFainter   */ {0x6F, 0x6F, 0x7A},   //  4.80    4.64  tile
        /* textMono      */ {0x34, 0x34, 0x3E},   // 11.90   10.63  surfaceHover
        /* placeholder   */ {0x66, 0x66, 0x71},   //  5.48    5.12  surface
        /* iconStroke    */ {0x53, 0x53, 0x5E},   //  7.34    6.10  surfaceActive

        /* onAccent      */ {0x14, 0x14, 0x14},
        /* scrollThumb   */ {0xC6, 0xC6, 0xCF},
    };
    return p;
}

/// Darker than Dark, and a touch cooler: a near-black ground for an OLED panel or anyone
/// who finds the default too grey. The lightness ordering is the same as Dark's — the same
/// blue-leaning neutral, primary text down to the faintest label — just dropped onto a
/// deeper floor, with the text lifted to hold its contrast against it. The faintest
/// content token, "4 öğe", clears 4.5:1 on the #0B0B0E window like the dark palette's does.
const Palette &midnightPalette()
{
    static const Palette p{
        /* window        */ {0x0B, 0x0B, 0x0E},
        /* surface       */ {0x12, 0x12, 0x16},
        /* surfaceHover  */ {0x17, 0x17, 0x1C},
        /* surfaceActive */ {0x1D, 0x1D, 0x23},
        /* tile          */ {0x0F, 0x0F, 0x13},

        /* borderWindow  */ {0x2A, 0x2A, 0x33},
        /* borderControl */ {0x2E, 0x2E, 0x37},
        /* divider       */ {0x20, 0x20, 0x28},
        /* dividerSoft   */ {0x18, 0x18, 0x1F},
        /* tileBorder    */ {0x24, 0x24, 0x2C},

        /* toggleOff       */ {0x24, 0x24, 0x30},
        /* toggleOffBorder */ {0x3A, 0x3A, 0x44},
        /* knobOff         */ {0xA6, 0xA6, 0xB2},
        /* knobOn          */ {0x0B, 0x0B, 0x0B},

                                                  // window   on its binding surface
        /* textPrimary   */ {0xF4, 0xF4, 0xF8},   // 17.92   15.29  surfaceActive
        /* textSecondary */ {0xC8, 0xC8, 0xD2},   // 11.84   10.76  surfaceHover
        /* textStatus    */ {0xBC, 0xBC, 0xC6},   // 10.43   10.43  window
        /* textDesc      */ {0xAB, 0xAB, 0xB6},   //  8.64    7.85  surfaceHover
        /* textMuted     */ {0xAE, 0xAE, 0xB9},   //  8.94    8.94  window
        /* textDim       */ {0x9C, 0x9C, 0xA8},   //  7.24    7.04  tile
        /* textFaint     */ {0x94, 0x94, 0xA0},   //  6.55    5.96  surfaceHover
        /* textFainter   */ {0x84, 0x84, 0x8E},   //  5.31    5.17  tile
        /* textMono      */ {0xE2, 0xE2, 0xEA},   // 15.26   13.86  surfaceHover
        /* placeholder   */ {0x93, 0x93, 0x9E},   //  6.47    6.15  surface
        /* iconStroke    */ {0xB0, 0xB0, 0xBB},   //  9.15    7.80  surfaceActive

        /* onAccent      */ {0x0B, 0x0B, 0x0B},
        /* scrollThumb   */ {0x30, 0x30, 0x3A},
    };
    return p;
}

/// A warm light theme — cream paper rather than the cool blue-white of Light — with warm
/// brown-grey text. Mirrors Light's relationships hue-shifted to the warm end: higher red,
/// then green, least blue, so nothing turns muddy. Text is a warm near-black brown on the
/// paper, which is very high contrast; the ramp down to the faintest label keeps the same
/// ordering Light uses.
///
/// It inherited Light's mistake with it. Mirroring lightness rather than contrast put six
/// of the eleven tokens under 4.5:1 on the surface they are drawn on — against the #F7F1E6
/// window, textDesc was 4.66, textMuted 4.59, textDim 3.87, textFaint and placeholder 2.80,
/// the "4 öğe" count 2.30 — and the cream ground is dimmer than Light's near-white, so
/// every one of them measured worse again on the card and on a hovered row.
///
/// Only the text moved, and only downward in lightness. Every replacement sits on Sepia's
/// own warmth curve, which its surviving tokens describe: green about 7% below red and
/// blue about 22% below at the dark end, opening to 7% and 25% by the middle of the ramp.
/// So #766B57 became #564D3C and not a grey — the paper, the warmth and the order of
/// emphasis are the ones that shipped, at ratios that are now what the comments say.
///
/// The upper half moved too, for the same reason Light's did: with textDesc at 7.41 the
/// old textStatus at 6.43 would have sat below the token it outranks. textPrimary is
/// where it was.
///
/// Two annotations per token: against the window, and against the darkest surface it is
/// painted on. iconStroke shares textDesc's literal and binds on surfaceActive, the
/// window-button hover, which is the darkest ground any text colour here touches;
/// placeholder shares textFaint's.
const Palette &sepiaPalette()
{
    static const Palette p{
        /* window        */ {0xF7, 0xF1, 0xE6},
        /* surface       */ {0xEF, 0xE8, 0xD9},
        /* surfaceHover  */ {0xEA, 0xE2, 0xD1},
        /* surfaceActive */ {0xE1, 0xD8, 0xC4},
        /* tile          */ {0xF3, 0xEC, 0xDE},

        /* borderWindow  */ {0xD3, 0xC9, 0xB4},
        /* borderControl */ {0xDA, 0xD0, 0xBC},
        /* divider       */ {0xE4, 0xDB, 0xC9},
        /* dividerSoft   */ {0xED, 0xE5, 0xD6},
        /* tileBorder    */ {0xE1, 0xD8, 0xC6},

        /* toggleOff       */ {0xDD, 0xD3, 0xBF},
        /* toggleOffBorder */ {0xC6, 0xBB, 0xA3},
        /* knobOff         */ {0xFF, 0xFB, 0xF3},
        /* knobOn          */ {0x2A, 0x24, 0x1A},

        //                                          window   on its binding surface
        /* textPrimary   */ {0x2E, 0x27, 0x1B},   // 13.13   10.42  surfaceActive
        /* textSecondary */ {0x43, 0x3B, 0x2B},   //  9.84    8.58  surfaceHover
        /* textStatus    */ {0x4C, 0x44, 0x33},   //  8.56    8.56  window
        /* textDesc      */ {0x56, 0x4D, 0x3C},   //  7.41    6.46  surfaceHover
        /* textMuted     */ {0x59, 0x50, 0x3F},   //  7.06    7.06  window
        /* textDim       */ {0x63, 0x5A, 0x48},   //  6.05    5.79  tile
        /* textFaint     */ {0x6B, 0x61, 0x4E},   //  5.42    4.73  surfaceHover
        /* textFainter   */ {0x73, 0x68, 0x55},   //  4.86    4.65  tile
        /* textMono      */ {0x39, 0x32, 0x24},   // 11.28    9.84  surfaceHover
        /* placeholder   */ {0x6B, 0x61, 0x4E},   //  5.42    5.00  surface
        /* iconStroke    */ {0x56, 0x4D, 0x3C},   //  7.41    5.88  surfaceActive

        /* onAccent      */ {0x2A, 0x24, 0x1A},
        /* scrollThumb   */ {0xCF, 0xC4, 0xAC},
    };
    return p;
}

} // namespace

static Palette fluentPalette(Appearance a);

const Palette &palette()
{
    // Under the Fluent shell the palette is the shell's tokens flattened onto its mica —
    // the handoff's for Dark and Light, the scheme's own for the other ten — rebuilt
    // whenever the scheme or the accent it is derived from changes.
    if (g_shell == Shell::Fluent) {
        static Palette cached;
        static Appearance cachedFor = Appearance::Dark;
        static QColor cachedAccent;
        static bool valid = false;
        if (!valid || cachedFor != g_appearance || cachedAccent != g_accent) {
            cached = fluentPalette(g_appearance);
            cachedFor = g_appearance;
            cachedAccent = g_accent;
            valid = true;
        }
        return cached;
    }
    return palette(g_appearance);
}

// Four tinted variants of the dark palette, each generated by rotating the neutral ramp
// to a single hue and preserving every token's lightness. The tint is gentle on purpose:
// a ground you notice, not one that shouts, in keeping with the rest. Ocean leans blue,
// Forest green, Dusk violet, Rose a warm rosé.
//
// Preserving lightness does not preserve the ratio, which is what the sentence here used
// to claim. Rotating a neutral to a hue moves the text and the ground by different
// amounts — the coefficients on R, G and B are not equal — so each of the four came out
// with its own profile, close to the dark palette's but not it. Three of the four are
// above the dark palette on the faintest token and one, Dusk, was below: measured on the
// overview tile the counts read Ocean 4.52, Forest 4.69, Dusk 4.32, Rose 4.39. Dusk and
// Rose are lifted below, in place, and named where they sit. Ocean's 4.52 clears with
// almost nothing to spare and is left alone rather than repainted for tidiness.
// ocean
const Palette &oceanPalette()
{
    static const Palette p{
        /* window          */ {0x16, 0x19, 0x1C},
        /* surface         */ {0x1D, 0x21, 0x24},
        /* surfaceHover    */ {0x20, 0x27, 0x2C},
        /* surfaceActive   */ {0x25, 0x2D, 0x33},
        /* tile            */ {0x1B, 0x1E, 0x20},
        /* borderWindow    */ {0x2C, 0x37, 0x3F},
        /* borderControl   */ {0x2E, 0x39, 0x40},
        /* divider         */ {0x25, 0x2D, 0x33},
        /* dividerSoft     */ {0x1E, 0x26, 0x2B},
        /* tileBorder      */ {0x27, 0x30, 0x36},
        /* toggleOff       */ {0x2A, 0x33, 0x3A},
        /* toggleOffBorder */ {0x39, 0x47, 0x51},
        /* knobOff         */ {0xA1, 0xA8, 0xAD},
        /* knobOn          */ {0x12, 0x14, 0x16},
                                                    // window   on its binding surface
        /* textPrimary     */ {0xF1, 0xF2, 0xF2},   // 15.73   12.47  surfaceActive
        /* textSecondary   */ {0xC1, 0xC6, 0xC9},   // 10.24    8.78  surfaceHover
        /* textStatus      */ {0xB4, 0xBA, 0xBE},   //  9.00    9.00  window
        /* textDesc        */ {0xA2, 0xA8, 0xAD},   //  7.35    6.30  surfaceHover
        /* textMuted       */ {0xA5, 0xAB, 0xB0},   //  7.61    7.61  window
        /* textDim         */ {0x93, 0x9B, 0xA1},   //  6.26    5.94  tile
        /* textFaint       */ {0x8D, 0x95, 0x9B},   //  5.80    4.97  surfaceHover
        /* textFainter     */ {0x7D, 0x86, 0x8D},   //  4.76    4.52  tile
        /* textMono        */ {0xDE, 0xE0, 0xE2},   // 13.33   11.43  surfaceHover
        /* placeholder     */ {0x8A, 0x93, 0x99},   //  5.64    5.18  surface
        /* iconStroke      */ {0xA8, 0xAE, 0xB2},   //  7.87    6.23  surfaceActive
        /* onAccent        */ {0x12, 0x14, 0x16},
        /* scrollThumb     */ {0x2F, 0x3A, 0x42},
    };
    return p;
}

// forest
const Palette &forestPalette()
{
    static const Palette p{
        /* window          */ {0x17, 0x1B, 0x18},
        /* surface         */ {0x1E, 0x23, 0x20},
        /* surfaceHover    */ {0x20, 0x2C, 0x24},
        /* surfaceActive   */ {0x25, 0x33, 0x2A},
        /* tile            */ {0x1B, 0x20, 0x1D},
        /* borderWindow    */ {0x2D, 0x3E, 0x33},
        /* borderControl   */ {0x2F, 0x3F, 0x34},
        /* divider         */ {0x25, 0x33, 0x2A},
        /* dividerSoft     */ {0x1F, 0x2A, 0x23},
        /* tileBorder      */ {0x28, 0x35, 0x2C},
        /* toggleOff       */ {0x2B, 0x39, 0x30},
        /* toggleOffBorder */ {0x3B, 0x4F, 0x42},
        /* knobOff         */ {0xA2, 0xAC, 0xA5},
        /* knobOn          */ {0x12, 0x16, 0x13},
                                                    // window   on its binding surface
        /* textPrimary     */ {0xF1, 0xF2, 0xF1},   // 15.51   11.80  surfaceActive
        /* textSecondary   */ {0xC2, 0xC8, 0xC4},   // 10.24    8.54  surfaceHover
        /* textStatus      */ {0xB5, 0xBD, 0xB8},   //  9.06    9.06  window
        /* textDesc        */ {0xA3, 0xAC, 0xA6},   //  7.47    6.23  surfaceHover
        /* textMuted       */ {0xA6, 0xAF, 0xA9},   //  7.73    7.73  window
        /* textDim         */ {0x94, 0xA0, 0x98},   //  6.42    6.09  tile
        /* textFaint       */ {0x8E, 0x9A, 0x92},   //  5.96    4.97  surfaceHover
        /* textFainter     */ {0x7E, 0x8C, 0x83},   //  4.95    4.69  tile
        /* textMono        */ {0xDE, 0xE2, 0xDF},   // 13.30   11.09  surfaceHover
        /* placeholder     */ {0x8B, 0x98, 0x8F},   //  5.79    5.31  surface
        /* iconStroke      */ {0xA8, 0xB2, 0xAB},   //  7.98    6.07  surfaceActive
        /* onAccent        */ {0x12, 0x16, 0x13},
        /* scrollThumb     */ {0x30, 0x41, 0x36},
    };
    return p;
}

// dusk
const Palette &duskPalette()
{
    static const Palette p{
        /* window          */ {0x19, 0x17, 0x1B},
        /* surface         */ {0x21, 0x1D, 0x24},
        /* surfaceHover    */ {0x27, 0x20, 0x2C},
        /* surfaceActive   */ {0x2D, 0x25, 0x33},
        /* tile            */ {0x1E, 0x1B, 0x20},
        /* borderWindow    */ {0x37, 0x2D, 0x3E},
        /* borderControl   */ {0x38, 0x2E, 0x40},
        /* divider         */ {0x2D, 0x25, 0x33},
        /* dividerSoft     */ {0x25, 0x1F, 0x2A},
        /* tileBorder      */ {0x30, 0x27, 0x36},
        /* toggleOff       */ {0x33, 0x2A, 0x3A},
        /* toggleOffBorder */ {0x47, 0x3A, 0x50},
        /* knobOff         */ {0xA8, 0xA1, 0xAD},
        /* knobOn          */ {0x14, 0x12, 0x16},
                                                    // window   on its binding surface
        /* textPrimary     */ {0xF2, 0xF1, 0xF2},   // 15.80   13.09  surfaceActive
        /* textSecondary   */ {0xC6, 0xC1, 0xC9},   // 10.06    8.93  surfaceHover
        /* textStatus      */ {0xBA, 0xB4, 0xBE},   //  8.78    8.78  window
        /* textDesc        */ {0xA8, 0xA2, 0xAD},   //  7.15    6.35  surfaceHover
        /* textMuted       */ {0xAB, 0xA5, 0xB0},   //  7.41    7.41  window
        /* textDim         */ {0x9B, 0x93, 0xA1},   //  6.00    5.74  tile
        /* textFaint       */ {0x95, 0x8D, 0x9B},   //  5.56    4.94  surfaceHover
        // Was #867D8D: 4.52 on the window, but 4.32 on the tile the count is actually
        // drawn on — the same miss the dark palette had, and the same lift.
        /* textFainter     */ {0x8B, 0x82, 0x92},   //  4.84    4.63  tile
        /* textMono        */ {0xE0, 0xDE, 0xE2},   // 13.32   11.82  surfaceHover
        /* placeholder     */ {0x93, 0x8A, 0x99},   //  5.37    5.01  surface
        /* iconStroke      */ {0xAE, 0xA8, 0xB2},   //  7.67    6.35  surfaceActive
        /* onAccent        */ {0x14, 0x12, 0x16},
        /* scrollThumb     */ {0x3A, 0x2F, 0x42},
    };
    return p;
}

// rose
const Palette &rosePalette()
{
    static const Palette p{
        /* window          */ {0x1B, 0x17, 0x18},
        /* surface         */ {0x23, 0x1E, 0x1F},
        /* surfaceHover    */ {0x2C, 0x20, 0x23},
        /* surfaceActive   */ {0x33, 0x25, 0x28},
        /* tile            */ {0x20, 0x1B, 0x1C},
        /* borderWindow    */ {0x3E, 0x2D, 0x31},
        /* borderControl   */ {0x3F, 0x2F, 0x33},
        /* divider         */ {0x33, 0x25, 0x28},
        /* dividerSoft     */ {0x2A, 0x1F, 0x22},
        /* tileBorder      */ {0x35, 0x28, 0x2B},
        /* toggleOff       */ {0x39, 0x2B, 0x2E},
        /* toggleOffBorder */ {0x4F, 0x3B, 0x3F},
        /* knobOff         */ {0xAC, 0xA2, 0xA4},
        /* knobOn          */ {0x16, 0x12, 0x13},
                                                    // window   on its binding surface
        /* textPrimary     */ {0xF2, 0xF1, 0xF1},   // 15.75   12.97  surfaceActive
        /* textSecondary   */ {0xC8, 0xC2, 0xC3},   // 10.12    8.94  surfaceHover
        /* textStatus      */ {0xBD, 0xB5, 0xB7},   //  8.84    8.84  window
        /* textDesc        */ {0xAC, 0xA3, 0xA5},   //  7.22    6.38  surfaceHover
        /* textMuted       */ {0xAF, 0xA6, 0xA8},   //  7.48    7.48  window
        /* textDim         */ {0xA0, 0x94, 0x97},   //  6.07    5.81  tile
        /* textFaint       */ {0x9A, 0x8E, 0x91},   //  5.63    4.97  surfaceHover
        // Was #8C7E81: 4.58 on the window, 4.39 on the tile.
        /* textFainter     */ {0x8F, 0x81, 0x84},   //  4.77    4.57  tile
        /* textMono        */ {0xE2, 0xDE, 0xDF},   // 13.32   11.77  surfaceHover
        /* placeholder     */ {0x98, 0x8B, 0x8E},   //  5.43    5.03  surface
        /* iconStroke      */ {0xB2, 0xA8, 0xAB},   //  7.68    6.32  surfaceActive
        /* onAccent        */ {0x16, 0x12, 0x13},
        /* scrollThumb     */ {0x41, 0x30, 0x34},
    };
    return p;
}

namespace {

// Four light schemes, added because only two of the eight before them were light: a light
// user had Light's cool white and Sepia's warm cream and nothing else, against six darks.
// Each of the four is a different job rather than another shade of the same pale grey —
// see the comment on each — and none of them is warm paper, because Sepia already is.
//
// They are built on darkPalette()'s own measured ratio profile rather than on an
// eyedropper. Every text token was solved for the contrast ratio the dark palette's
// matching token has against its window — 15.72 for textPrimary, 9.90 for textSecondary,
// 7.07 for textDesc and so on down the ramp — and then checked against the darkest surface
// that token is actually painted on, which is not always the window:
//
//     textPrimary   surfaceActive, the active segment's own label
//     iconStroke    surfaceActive, a hovered window-control glyph — the darkest ground
//                   any colour here is asked to sit on, and the one that sets textDesc,
//                   because iconStroke and textDesc share a literal
//     textSecondary, textDesc, textFaint, textMono   surfaceHover: a hovered sidebar or
//                   debloat row, and a pressed ghost button, which is where textMono goes
//     placeholder   surface, the search field
//     textDim, textFainter                 tile, the overview cards and the chart
//     textStatus, textMuted                the window, and nowhere darker — textMuted is
//                   the inactive segment and the sort glyph, and neither fills behind
//                   itself; the ghost button that also wears it is unfilled until pressed,
//                   and by then it is hovered and wearing textMono instead
//
// The one deliberate change to the profile is textFainter, lifted from dark's then-4.52
// to 4.75, because on a light palette that token also lands on the overview tile and 4.52
// against the window would come to 4.34 there. That reasoning applied to the dark palette
// too and has since been applied to it: it is at 4.77 now, and the four tinted darks were
// measured on the tile at the same time.
//
// Watch what the floor does to a light ramp. A ratio of 4.5:1 against a near-white ground
// is reached at about 45% grey, so everything from textDesc down is packed into a narrow
// band and the ramp cannot have the spread a dark one does. Light and Sepia looked as
// though they did have that spread, and only because they did not clear the floor —
// Light's "4 öğe" count measured 2.25 against its own window. Both were rebuilt the way
// these four were built; the spread is gone from them too, which is the correct way round.
//
// Each token below carries two figures: its ratio against that palette's own window, and
// its ratio on the binding surface from the table above, which is the one that has to
// clear the floor. They used to carry only the first, and a reader had to take the
// aggregate claim on trust — which was worth withdrawing, because three rows of that
// table were wrong. The four schemes still clear on the corrected grounds; the second
// figure is there so the next reader does not have to trust that either.

/// A light theme that is not a lamp. The ground is a cool grey-blue carrying 9.2% less
/// relative luminance than Light's near-white (0.877 against 0.965), for someone who works
/// in a bright room all day and finds a #FBFBFC window glaring but does not want to go dark
/// for it. It is dimmed, not washed out: every token carries the ratio the dark palette's
/// counterpart carries, so the light coming off the screen drops and the legibility does
/// not. The dimming is paid for at the other end of the ramp — holding 15.72 on a #EEF1F5
/// ground puts textPrimary at #16171A where Light's sits at #18181D — which is the same
/// arithmetic in reverse as the trap this file fell into with the dark palette: moving the
/// ground moves the text with it, and the text always has to travel further.
const Palette &mistPalette()
{
    static const Palette p{
        /* window        */ {0xEE, 0xF1, 0xF5},
        /* surface       */ {0xE4, 0xE8, 0xEE},
        /* surfaceHover  */ {0xDD, 0xE2, 0xEA},
        /* surfaceActive */ {0xD2, 0xD8, 0xE3},
        /* tile          */ {0xEA, 0xED, 0xF2},

        /* borderWindow  */ {0xB6, 0xBD, 0xCA},
        /* borderControl */ {0xC0, 0xC7, 0xD3},
        /* divider       */ {0xD3, 0xDA, 0xE3},
        /* dividerSoft   */ {0xE0, 0xE5, 0xEC},
        /* tileBorder    */ {0xCF, 0xD6, 0xE1},

        /* toggleOff       */ {0xCD, 0xD4, 0xDE},
        /* toggleOffBorder */ {0xB1, 0xB8, 0xC6},
        /* knobOff         */ {0xFF, 0xFF, 0xFF},
        /* knobOn          */ {0x14, 0x16, 0x1A},

        //                                          window   on its binding surface
        /* textPrimary   */ {0x16, 0x17, 0x1A},   // 15.82   12.52  surfaceActive
        /* textSecondary */ {0x38, 0x3A, 0x41},   // 10.02    8.73  surfaceHover
        /* textStatus    */ {0x40, 0x43, 0x4A},   //  8.74    8.74  window
        /* textDesc      */ {0x4B, 0x4E, 0x57},   //  7.34    6.39  surfaceHover
        /* textMuted     */ {0x4D, 0x51, 0x5A},   //  7.02    7.02  window
        /* textDim       */ {0x56, 0x5A, 0x64},   //  6.09    5.88  tile
        /* textFaint     */ {0x5C, 0x60, 0x6B},   //  5.55    4.83  surfaceHover
        /* textFainter   */ {0x65, 0x6A, 0x76},   //  4.78    4.62  tile
        /* textMono      */ {0x25, 0x27, 0x2B},   // 13.20   11.50  surfaceHover
        /* placeholder   */ {0x5D, 0x61, 0x6C},   //  5.46    5.03  surface
        /* iconStroke    */ {0x4B, 0x4E, 0x57},   //  7.34    5.81  surfaceActive
        /* onAccent      */ {0x14, 0x16, 0x1A},
        /* scrollThumb   */ {0xB8, 0xC0, 0xCC},
    };
    return p;
}

/// The one scheme that trades the design's soft register for legibility, on purpose. A
/// pure white ground, near-black text, and borders at #9898A3 instead of the hairline
/// hints the rest of the app uses — for low vision, for a laptop screen in direct
/// sunlight, and for a projector, which eats the mid greys every other palette lives in.
///
/// Its bar is WCAG AAA, 7:1, not the 4.5:1 the others hold, and it clears it on every
/// ground each token touches: the faintest, the "4 öğe" count, is 7.77 against the window
/// and 7.52 against the overview tile it also lands on. Nothing here is decorative enough
/// to be let off — that is the whole offer.
///
/// That includes the one colour not written down here. The accent ink is solved at runtime
/// against whatever accent the user picked, and it was solved to a hardcoded 4.5 for every
/// scheme, so this one shipped a 4.55 selected-category label under a 7:1 promise. It is
/// held to the same 7:1 now; see textFloor() beside accentInk().
const Palette &contrastPalette()
{
    static const Palette p{
        /* window        */ {0xFF, 0xFF, 0xFF},
        /* surface       */ {0xF4, 0xF4, 0xF7},
        /* surfaceHover  */ {0xEC, 0xEC, 0xF1},
        /* surfaceActive */ {0xDF, 0xDF, 0xE6},
        /* tile          */ {0xFB, 0xFB, 0xFD},

        /* borderWindow  */ {0x98, 0x98, 0xA3},
        /* borderControl */ {0xA3, 0xA3, 0xAE},
        /* divider       */ {0xC3, 0xC3, 0xCD},
        /* dividerSoft   */ {0xD8, 0xD8, 0xE0},
        /* tileBorder    */ {0xB3, 0xB3, 0xBF},

        /* toggleOff       */ {0xCC, 0xCC, 0xD6},
        /* toggleOffBorder */ {0x88, 0x88, 0x94},
        /* knobOff         */ {0xFF, 0xFF, 0xFF},
        /* knobOn          */ {0x00, 0x00, 0x00},

        //                                          window   on its binding surface
        /* textPrimary   */ {0x08, 0x08, 0x09},   // 20.02   15.10  surfaceActive
        /* textSecondary */ {0x30, 0x30, 0x33},   // 13.16   11.17  surfaceHover
        /* textStatus    */ {0x38, 0x38, 0x3C},   // 11.67   11.67  window
        /* textDesc      */ {0x43, 0x43, 0x47},   //  9.85    8.36  surfaceHover
        /* textMuted     */ {0x45, 0x45, 0x49},   //  9.54    9.54  window
        /* textDim       */ {0x4A, 0x4A, 0x4F},   //  8.81    8.52  tile
        /* textFaint     */ {0x4D, 0x4D, 0x52},   //  8.40    7.14  surfaceHover
        /* textFainter   */ {0x52, 0x52, 0x57},   //  7.77    7.52  tile
        /* textMono      */ {0x1E, 0x1E, 0x20},   // 16.64   14.13  surfaceHover
        /* placeholder   */ {0x50, 0x50, 0x55},   //  8.02    7.30  surface
        /* iconStroke    */ {0x43, 0x43, 0x47},   //  9.85    7.43  surfaceActive
        /* onAccent      */ {0x00, 0x00, 0x00},
        /* scrollThumb   */ {0x99, 0x99, 0xA5},
    };
    return p;
}

/// The light family's answer to Forest, and the reason the tinted four are not all dark.
/// Green is the hue that stays calm at high lightness — blue goes clinical and red goes
/// pink — so this is the one that gives a light theme an identity instead of another grey.
/// The tint runs through the text as well as the surfaces, green highest and red and blue
/// held back equally, so the ramp reads as one material rather than grey type on a green
/// card. Warm paper is Sepia's and is deliberately not attempted here.
const Palette &meadowPalette()
{
    static const Palette p{
        /* window        */ {0xF2, 0xF7, 0xF1},
        /* surface       */ {0xE6, 0xEE, 0xE4},
        /* surfaceHover  */ {0xE0, 0xE9, 0xDE},
        /* surfaceActive */ {0xD4, 0xDF, 0xD2},
        /* tile          */ {0xED, 0xF3, 0xEC},

        /* borderWindow  */ {0xBD, 0xC9, 0xBA},
        /* borderControl */ {0xC6, 0xD1, 0xC3},
        /* divider       */ {0xD7, 0xE0, 0xD5},
        /* dividerSoft   */ {0xE3, 0xEA, 0xE1},
        /* tileBorder    */ {0xD2, 0xDC, 0xD0},

        /* toggleOff       */ {0xD3, 0xDD, 0xD1},
        /* toggleOffBorder */ {0xB7, 0xC3, 0xB4},
        /* knobOff         */ {0xFF, 0xFF, 0xFF},
        /* knobOn          */ {0x16, 0x1B, 0x15},

        //                                          window   on its binding surface
        /* textPrimary   */ {0x19, 0x1D, 0x19},   // 15.72   12.42  surfaceActive
        /* textSecondary */ {0x37, 0x40, 0x36},   //  9.93    8.66  surfaceHover
        /* textStatus    */ {0x3E, 0x48, 0x3D},   //  8.80    8.80  window
        /* textDesc      */ {0x48, 0x54, 0x47},   //  7.34    6.41  surfaceHover
        /* textMuted     */ {0x4B, 0x57, 0x4A},   //  7.01    7.01  window
        /* textDim       */ {0x53, 0x61, 0x52},   //  6.05    5.82  tile
        /* textFaint     */ {0x59, 0x67, 0x58},   //  5.52    4.81  surfaceHover
        /* textFainter   */ {0x61, 0x71, 0x60},   //  4.79    4.61  tile
        /* textMono      */ {0x26, 0x2C, 0x25},   // 13.17   11.49  surfaceHover
        /* placeholder   */ {0x59, 0x68, 0x58},   //  5.46    5.00  surface
        /* iconStroke    */ {0x48, 0x54, 0x47},   //  7.34    5.80  surfaceActive
        /* onAccent      */ {0x16, 0x1B, 0x15},
        /* scrollThumb   */ {0xC0, 0xCB, 0xBD},
    };
    return p;
}

/// The light family's answer to Dusk, and the other half of the reason Meadow exists: cool
/// without being blue. A violet cast takes the yellow edge off a white panel, which is what
/// somebody who dislikes a cold blue-white screen and also dislikes Sepia's yellow is
/// reaching for. Its ramp is the palest of the four at the faint end — textFainter is
/// #73697A against a #F6F4FC window — because violet carries the least luminance per byte
/// of the three tints, so the same 4.78 ratio lands on a lighter-looking colour.
const Palette &lilacPalette()
{
    static const Palette p{
        /* window        */ {0xF6, 0xF4, 0xFC},
        /* surface       */ {0xEC, 0xE8, 0xF5},
        /* surfaceHover  */ {0xE5, 0xE1, 0xF1},
        /* surfaceActive */ {0xD9, 0xD4, 0xE9},
        /* tile          */ {0xF2, 0xEF, 0xF9},

        /* borderWindow  */ {0xC2, 0xBB, 0xD4},
        /* borderControl */ {0xCB, 0xC4, 0xDB},
        /* divider       */ {0xDB, 0xD6, 0xE9},
        /* dividerSoft   */ {0xE7, 0xE3, 0xF2},
        /* tileBorder    */ {0xD6, 0xD0, 0xE5},

        /* toggleOff       */ {0xD7, 0xD2, 0xE6},
        /* toggleOffBorder */ {0xBB, 0xB4, 0xCE},
        /* knobOff         */ {0xFF, 0xFF, 0xFF},
        /* knobOn          */ {0x17, 0x14, 0x1D},

        //                                          window   on its binding surface
        /* textPrimary   */ {0x1C, 0x1A, 0x1E},   // 15.84   11.96  surfaceActive
        /* textSecondary */ {0x41, 0x3B, 0x45},   //  9.95    8.46  surfaceHover
        /* textStatus    */ {0x49, 0x43, 0x4E},   //  8.77    8.77  window
        /* textDesc      */ {0x56, 0x4E, 0x5B},   //  7.31    6.21  surfaceHover
        /* textMuted     */ {0x57, 0x50, 0x5D},   //  7.11    7.11  window
        /* textDim       */ {0x63, 0x5A, 0x69},   //  6.03    5.78  tile
        /* textFaint     */ {0x68, 0x5F, 0x6F},   //  5.58    4.74  surfaceHover
        /* textFainter   */ {0x73, 0x69, 0x7A},   //  4.78    4.59  tile
        /* textMono      */ {0x2C, 0x28, 0x2F},   // 13.27   11.28  surfaceHover
        /* placeholder   */ {0x6A, 0x61, 0x71},   //  5.41    4.89  surface
        /* iconStroke    */ {0x56, 0x4E, 0x5B},   //  7.31    5.52  surfaceActive
        /* onAccent      */ {0x17, 0x14, 0x1D},
        /* scrollThumb   */ {0xC4, 0xBD, 0xD7},
    };
    return p;
}

} // namespace

// The name a scheme is stored under. A name rather than the enum's number so the order
// above can change without silently repointing everyone's saved theme.
QString schemeToString(Appearance a)
{
    switch (a) {
    case Appearance::Light:    return QStringLiteral("light");
    case Appearance::Midnight: return QStringLiteral("midnight");
    case Appearance::Sepia:    return QStringLiteral("sepia");
    case Appearance::Ocean:    return QStringLiteral("ocean");
    case Appearance::Forest:   return QStringLiteral("forest");
    case Appearance::Dusk:     return QStringLiteral("dusk");
    case Appearance::Rose:     return QStringLiteral("rose");
    case Appearance::Mist:     return QStringLiteral("mist");
    case Appearance::Contrast: return QStringLiteral("contrast");
    case Appearance::Meadow:   return QStringLiteral("meadow");
    case Appearance::Lilac:    return QStringLiteral("lilac");
    case Appearance::Dark:     break;
    }
    return QStringLiteral("dark");
}

Appearance schemeFromString(const QString &name)
{
    if (name == QLatin1String("light"))    return Appearance::Light;
    if (name == QLatin1String("midnight")) return Appearance::Midnight;
    if (name == QLatin1String("sepia"))    return Appearance::Sepia;
    if (name == QLatin1String("ocean"))    return Appearance::Ocean;
    if (name == QLatin1String("forest"))   return Appearance::Forest;
    if (name == QLatin1String("dusk"))     return Appearance::Dusk;
    if (name == QLatin1String("rose"))     return Appearance::Rose;
    if (name == QLatin1String("mist"))     return Appearance::Mist;
    if (name == QLatin1String("contrast")) return Appearance::Contrast;
    if (name == QLatin1String("meadow"))   return Appearance::Meadow;
    if (name == QLatin1String("lilac"))    return Appearance::Lilac;
    return Appearance::Dark;   // the default, and the fallback for anything unrecognised
}

QColor solveInk(Appearance a);   // defined with accentInk() below

namespace Fluent {

// Every value is a literal from design_handoff_fluent_ui/README.md's "Design Tokens".
const Tokens &tokens(bool light)
{
    static const Tokens dark = {
        QColor(0x10, 0x14, 0x18),                 // desk
        QColor(0x20, 0x20, 0x20),                 // mica
        QColor(0x27, 0x27, 0x27),                 // surface
        QColor(0x2B, 0x2B, 0x2B),                 // card
        QColor(255, 255, 255, 0x14),              // cardBorder .08
        QColor(255, 255, 255, 0x0F),              // divider .06
        QColor(255, 255, 255, 0x1A),              // winBorder .10
        QColor(0xFF, 0xFF, 0xFF),                 // text
        QColor(255, 255, 255, 0xB8),              // textSec .72
        QColor(255, 255, 255, 0x73),              // textMuted .45
        QColor(255, 255, 255, 0x0F),              // subtleHover .06
        QColor(255, 255, 255, 0x08),              // rowHover .03
        QColor(255, 255, 255, 0x14),              // selected .08
        QColor(255, 255, 255, 0x0F),              // controlBg .06
        QColor(255, 255, 255, 0x17),              // controlBorder .09
        QColor(255, 255, 255, 0x1A),              // controlHover .10
        QColor(255, 255, 255, 0x0F),              // iconBg .06
        QColor(255, 255, 255, 0x24),              // track .14
        QColor(0x4C, 0xC2, 0xFF),                 // accent
        QColor(0x4C, 0xC2, 0xFF, 0x26),           // accentSoft .15
        QColor(0x8F, 0xDB, 0xFF),                 // accentText
        QColor(0x00, 0x00, 0x00),                 // onAccent
        QColor(0x6C, 0xCB, 0x5F),                 // ok
        QColor(255, 255, 255, 0xC7),              // knobOff .78
        QColor(255, 255, 255, 0x8C),              // toggleOffBorder .55
        QColor(0xC4, 0x2B, 0x1C),                 // closeHover
    };
    static const Tokens lightTokens = {
        QColor(0xE4, 0xE8, 0xEE),                 // desk
        QColor(0xF3, 0xF3, 0xF3),                 // mica
        QColor(0xF9, 0xF9, 0xF9),                 // surface
        QColor(0xFF, 0xFF, 0xFF),                 // card
        QColor(0, 0, 0, 0x14),                    // cardBorder .08
        QColor(0, 0, 0, 0x0F),                    // divider .06
        QColor(0, 0, 0, 0x1F),                    // winBorder .12
        QColor(0x1B, 0x1B, 0x1B),                 // text
        QColor(0, 0, 0, 0x9E),                    // textSec .62
        QColor(0, 0, 0, 0x6B),                    // textMuted .42
        QColor(0, 0, 0, 0x0D),                    // subtleHover .05
        QColor(0, 0, 0, 0x05),                    // rowHover .02
        QColor(0, 0, 0, 0x0F),                    // selected .06
        QColor(255, 255, 255, 0xB3),              // controlBg .70
        QColor(0, 0, 0, 0x1A),                    // controlBorder .10
        QColor(255, 255, 255, 0xF2),              // controlHover .95
        QColor(0, 0, 0, 0x0D),                    // iconBg .05
        QColor(0, 0, 0, 0x1F),                    // track .12
        QColor(0x00, 0x5F, 0xB8),                 // accent
        QColor(0x00, 0x5F, 0xB8, 0x1F),           // accentSoft .12
        QColor(0x00, 0x5F, 0xB8),                 // accentText
        QColor(0xFF, 0xFF, 0xFF),                 // onAccent
        QColor(0x0F, 0x7B, 0x0F),                 // ok
        QColor(0, 0, 0, 0x99),                    // knobOff .60
        QColor(0, 0, 0, 0x73),                    // toggleOffBorder .45
        QColor(0xC4, 0x2B, 0x1C),                 // closeHover
    };
    return light ? lightTokens : dark;
}

/// Dark and Light are the handoff; the other ten schemes are the handoff's structure on the
/// scheme's own ground. Mica is the scheme's window; surface and card step up from it the
/// way the handoff's do (#202020 → #272727 → #2B2B2B: 3% and 5% towards the text on a dark
/// ground, and towards white on a light one); the accent is the user's, with its ink solved
/// against the scheme's window like the classic shell does, and the text on it chosen by
/// its luminance. Everything translucent — hovers, borders, the muted text — stays the
/// handoff's, because those are neutral washes and read the same on any ground.
static Tokens tintedTokens(Appearance a)
{
    const bool light = isLightFamily(a);
    Tokens t = tokens(light);
    const Palette &scheme = palette(a);
    t.mica = scheme.window;
    if (light) {
        t.surface = mixColors(scheme.window, QColor(Qt::white), 0.55);
        t.card = mixColors(scheme.window, QColor(Qt::white), 0.85);
        t.desk = scheme.window.darker(108);
    } else {
        t.surface = mixColors(scheme.window, scheme.textPrimary, 0.035);
        t.card = mixColors(scheme.window, scheme.textPrimary, 0.055);
        t.desk = scheme.window.darker(130);
    }
    t.accent = g_accent;
    t.accentSoft = g_accent;
    t.accentSoft.setAlpha(light ? 0x1F : 0x26);
    t.accentText = light ? solveInk(a) : g_accent;
    // WCAG's relative luminance, against the 0.179 that separates black-on from white-on.
    const auto channel = [](qreal v) { return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
    const qreal lum = 0.2126 * channel(g_accent.redF()) + 0.7152 * channel(g_accent.greenF())
                      + 0.0722 * channel(g_accent.blueF());
    t.onAccent = lum > 0.179 ? QColor(Qt::black) : QColor(Qt::white);
    return t;
}

const Tokens &tokens(Appearance a)
{
    if (a == Appearance::Dark)
        return tokens(false);
    if (a == Appearance::Light)
        return tokens(true);

    // One derived set per scheme, rebuilt when the accent moves under it. Returned by
    // reference, so the storage has to outlive the call: an array indexed by the scheme.
    static Tokens derived[AppearanceCount];
    static bool valid[AppearanceCount] = {};
    static QColor derivedFor;
    if (derivedFor != g_accent) {
        for (bool &v : valid)
            v = false;
        derivedFor = g_accent;
    }
    const int i = qBound(0, int(a), AppearanceCount - 1);
    if (!valid[i]) {
        derived[i] = tintedTokens(a);
        valid[i] = true;
    }
    return derived[i];
}

const Tokens &tokens()
{
    return tokens(g_appearance);
}

} // namespace Fluent

/// The Fluent tokens laid onto mica, in the Palette's own slots, so that every page that
/// was written against the classic tokens — the overview, the settings, the journal, the
/// cleaner — draws itself in the handoff's colours without knowing the shell changed. The
/// slot names are the classic design's; the comment beside each says which token it takes.
static Palette fluentPalette(Appearance a)
{
    const Fluent::Tokens &t = Fluent::tokens(a);
    const QColor ground = t.mica;
    Palette p;
    p.window = t.mica;
    p.surface = flatten(t.controlBg, ground);          // search field, control fills
    p.surfaceHover = flatten(t.subtleHover, ground);
    p.surfaceActive = flatten(t.selected, ground);
    p.tile = t.card;

    p.borderWindow = flatten(t.winBorder, ground);
    p.borderControl = flatten(t.controlBorder, ground);
    p.divider = flatten(t.divider, ground);
    p.dividerSoft = flatten(t.divider, ground);
    p.tileBorder = flatten(t.cardBorder, ground);

    p.toggleOff = flatten(t.controlBg, ground);
    p.toggleOffBorder = flatten(t.toggleOffBorder, ground);
    p.knobOff = flatten(t.knobOff, ground);
    p.knobOn = t.onAccent;

    p.textPrimary = t.text;
    p.textSecondary = flatten(t.textSec, ground);
    p.textStatus = flatten(t.textSec, ground);
    p.textDesc = flatten(t.textSec, ground);
    p.textMuted = flatten(t.textMuted, ground);
    p.textDim = flatten(t.textMuted, ground);
    p.textFaint = flatten(t.textMuted, ground);
    p.textFainter = flatten(t.textMuted, ground);
    p.textMono = t.text;
    p.placeholder = flatten(t.textMuted, ground);
    p.iconStroke = flatten(t.textSec, ground);

    p.onAccent = t.onAccent;
    p.scrollThumb = flatten(QColor(128, 128, 128, 0x59), ground);   // rgba(128,128,128,.35)
    return p;
}

Shell shell()
{
    return g_shell;
}

QString shellToString(Shell s)
{
    return s == Shell::Fluent ? QStringLiteral("fluent") : QStringLiteral("classic");
}

Shell shellFromString(const QString &name)
{
    return name == QLatin1String("fluent") ? Shell::Fluent : Shell::Classic;
}

int windowRadius()
{
    return g_shell == Shell::Fluent ? Fluent::WindowRadius : Metric::WindowRadius;
}

void setShell(Shell s, Persist persist)
{
    if (persist == Persist::Yes)
        QSettings().setValue(QStringLiteral("appearance/shell"), shellToString(s));
    if (s == g_shell)
        return;
    g_shell = s;

    // Three signals, in this order. The window rebuilds its chrome on the first; every
    // style is stale on the second, because font() answers with a different family under
    // each shell; and the third repaints the tree and rebuilds Qt's own palette.
    ++g_fontGeneration;
    Q_EMIT notifier()->shellChanged();
    Q_EMIT notifier()->typefaceChanged();
    Q_EMIT notifier()->appearanceChanged();
}

const Palette &palette(Appearance a)
{
    switch (a) {
    case Appearance::Light:    return lightPalette();
    case Appearance::Midnight: return midnightPalette();
    case Appearance::Sepia:    return sepiaPalette();
    case Appearance::Ocean:    return oceanPalette();
    case Appearance::Forest:   return forestPalette();
    case Appearance::Dusk:     return duskPalette();
    case Appearance::Rose:     return rosePalette();
    case Appearance::Mist:     return mistPalette();
    case Appearance::Contrast: return contrastPalette();
    case Appearance::Meadow:   return meadowPalette();
    case Appearance::Lilac:    return lilacPalette();
    case Appearance::Dark:     break;
    }
    return darkPalette();
}

Appearance appearance()
{
    return g_appearance;
}

void setAppearance(Appearance a, Persist persist)
{
    // Persisted before the identity guard: a command-line --theme sets the value without
    // saving it, and the guard would then swallow the settings-page pick of that same
    // value — the one call that should write it back — leaving the saved theme stale.
    if (persist == Persist::Yes)
        QSettings().setValue(QStringLiteral("appearance/theme"), schemeToString(a));
    if (a == g_appearance)
        return;
    g_appearance = a;
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

/// Points the four sans slots at \a face, registering its files on first use.
static void applyFace(const FaceFiles &face)
{
    FaceTable &t = faces();
    t.sans[0] = loadFace(QLatin1String(face.regular),  QLatin1String(face.family));
    t.sans[1] = loadFace(QLatin1String(face.text),     t.sans[0]);
    t.sans[2] = loadFace(QLatin1String(face.medium),   t.sans[0]);
    t.sans[3] = loadFace(QLatin1String(face.semiBold), t.sans[0]);
}

void initFonts()
{
    FaceTable &t = faces();
    if (t.loaded)
        return;
    t.loaded = true;

    QSettings s;

    // Restore the user's overrides before the faces are registered: the interface face
    // is one of them, and loading the wrong one first would only be thrown away.
    const QString accent = s.value(QStringLiteral("appearance/accent")).toString();
    if (QColor(accent).isValid())
        g_accent = QColor(accent);
    g_compact = s.value(QStringLiteral("layout/compact"), false).toBool();
    g_fontScale = qBound(0.85, s.value(QStringLiteral("appearance/fontScale"), 1.0).toDouble(), 1.6);
    g_appearance = schemeFromString(s.value(QStringLiteral("appearance/theme")).toString());
    g_shell = shellFromString(s.value(QStringLiteral("appearance/shell")).toString());
    g_typeface = QString::fromLatin1(
        faceFor(s.value(QStringLiteral("appearance/typeface")).toString())->id);

    applyFace(*faceFor(g_typeface));

    // The mono face is fixed: it carries versions, paths and byte counts, which are
    // column-aligned technical text whatever the interface face happens to be.
    t.mono[0] = loadFace(QStringLiteral(":/fonts/IBMPlexMono-Regular.ttf"), QStringLiteral("IBM Plex Mono"));
    t.mono[1] = loadFace(QStringLiteral(":/fonts/IBMPlexMono-Medium.ttf"),  t.mono[0]);
}

const QVector<Typeface> &typefaces()
{
    static QVector<Typeface> list = [] {
        QVector<Typeface> v;
        for (const FaceFiles &f : Faces)
            v.append({QString::fromLatin1(f.id), QString::fromLatin1(f.display), QString()});
        return v;
    }();
    return list;
}

QString loadTypeface(const QString &id)
{
    // No cache of its own: loadFace() memoises by resource path, which is the level that
    // catches applyFace()'s four loads per typeface and the two mono ones as well.
    const FaceFiles *face = faceFor(id);
    return loadFace(QLatin1String(face->regular), QString::fromLatin1(face->family));
}

QString typeface()
{
    return g_typeface;
}

void setTypeface(const QString &id, Persist persist)
{
    const FaceFiles *face = faceFor(id);
    const QString resolved = QString::fromLatin1(face->id);   // resolve before storing
    if (persist == Persist::Yes)
        QSettings().setValue(QStringLiteral("appearance/typeface"), resolved);
    if (resolved == g_typeface)
        return;

    g_typeface = resolved;
    applyFace(*face);
    ++g_fontGeneration;   // every style recomputes on its next call
    Q_EMIT notifier()->typefaceChanged();
}

qreal fontScale()
{
    return g_fontScale;
}

void setFontScale(qreal scale, Persist persist)
{
    scale = qBound(0.85, scale, 1.6);
    if (persist == Persist::Yes)
        QSettings().setValue(QStringLiteral("appearance/fontScale"), scale);
    if (qFuzzyCompare(scale, g_fontScale))
        return;

    g_fontScale = scale;

    // Same consequence as a face swap — every style is rebuilt and every widget has to
    // relayout as well as repaint — so it rides the same signal rather than a new one.
    ++g_fontGeneration;
    Q_EMIT notifier()->typefaceChanged();
}

QFont font(Family family, qreal px, int weight, qreal letterSpacingEm)
{
    const FaceTable &t = faces();

    QString name;
    if (g_shell == Shell::Fluent) {
        // The handoff prototyped in IBM Plex and says so: on Windows the face to ship is
        // Segoe UI Variable, with Cascadia Mono for the technical text. Neither is
        // embedded — both are the system's own — so the interface-face setting is left
        // where it is and comes back with the classic shell.
        name = family == Family::Sans ? fluentSans() : fluentMono();
    } else if (family == Family::Sans) {
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

    // The interface scale multiplies the requested size here, at the one gate every
    // style passes through, so a larger setting enlarges the whole app evenly — letter
    // spacing included, since it is expressed relative to the (now scaled) size.
    px *= g_fontScale;

    QFont f(name);
    f.setWeight(static_cast<QFont::Weight>(weight));
    f.setPointSizeF(px * 72.0 / logicalDpi());
    f.setStyleStrategy(QFont::PreferAntialias);
    if (!qFuzzyIsNull(letterSpacingEm))
        f.setLetterSpacing(QFont::AbsoluteSpacing, px * letterSpacingEm);
    return f;
}

namespace Font {

// Each style is built on first use, after initFonts() has registered the faces, and
// rebuilt whenever the interface face changes under it — hence the generation check
// rather than a plain function-local constant.
#define TWEAKER_FONT(NAME, EXPR)                 \
    const QFont &NAME()                          \
    {                                            \
        static QFont f;                          \
        static int generation = -1;              \
        if (generation != fontGeneration()) {    \
            generation = fontGeneration();       \
            f = (EXPR);                          \
        }                                        \
        return f;                                \
    }

TWEAKER_FONT(appName,             mono(11.5, Weight::Medium, 0.02))
TWEAKER_FONT(monoMeta,            mono(10))
TWEAKER_FONT(searchText,          sans(11.5))
TWEAKER_FONT(kbd,                 mono(9))
TWEAKER_FONT(categoryName,        sans(12.5, Weight::Text))
TWEAKER_FONT(categoryNameSelected,sans(12.5, Weight::Medium))
TWEAKER_FONT(categoryCount,       mono(10))
TWEAKER_FONT(pageTitle,           sans(15, Weight::SemiBold, -0.01))
TWEAKER_FONT(pageSub,             sans(11))
TWEAKER_FONT(segment,             sans(11))
TWEAKER_FONT(sectionTitle,        sans(10, Weight::Medium, 0.09))
TWEAKER_FONT(sectionCount,        mono(9.5))
TWEAKER_FONT(blockTitle,          sans(12.5, Weight::SemiBold, -0.01))
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
    // All desaturated on purpose: the accent is a wash behind a selected row and the text
    // on a pill, never a full field, so a saturated hue would shout. The four originals
    // stay first (and amber first of all — it is the default and what a fresh install
    // shows); the four added keep the same muted register and spread the hue wheel so no
    // two read as the same colour at a glance.
    return {QColor(0xD2, 0xA7, 0x5A),   // amber (default)
            QColor(0x7F, 0xB8, 0xA4),   // sage
            QColor(0x8E, 0x9B, 0xD8),   // periwinkle
            QColor(0xC4, 0xC4, 0xCC),   // neutral
            QColor(0xC9, 0x8A, 0x8A),   // clay rose
            QColor(0x6F, 0xB0, 0xC4),   // muted sky
            QColor(0xA9, 0x8A, 0xD0),   // soft violet
            QColor(0x9F, 0xC4, 0x8A)};  // muted green
}

QColor accent()
{
    if (g_shell == Shell::Fluent)
        return Fluent::tokens().accent;
    return g_accent;
}

QColor accentSoft(Appearance a)
{
    if (g_shell == Shell::Fluent)
        return Fluent::tokens(a).accentSoft;
    QColor c = g_accent;
    // The mockup's `accent + "21"` → 13%. On white that wash all but disappears, so the
    // light palette leans on it a little harder to keep the selected row readable.
    c.setAlpha(isLightFamily(a) ? 0x30 : 0x21);
    return c;
}

QColor accentSoft()
{
    return accentSoft(g_appearance);
}

namespace {

/// WCAG 2.x relative luminance, the sRGB formula. The palettes above were all solved
/// against it offline; accentInk() is the one colour the app cannot solve offline,
/// because the accent is the user's and can be any value they type after --accent.
qreal relativeLuminance(const QColor &c)
{
    const auto channel = [](qreal v) {
        return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * channel(c.redF()) + 0.7152 * channel(c.greenF()) + 0.0722 * channel(c.blueF());
}

qreal contrastRatio(const QColor &a, const QColor &b)
{
    const qreal la = relativeLuminance(a);
    const qreal lb = relativeLuminance(b);
    return (qMax(la, lb) + 0.05) / (qMin(la, lb) + 0.05);
}

/// \a over composited onto \a under the way QPainter's default source-over does it, at
/// 8-bit precision. accentSoft() is a translucent fill, so the pixel a label is actually
/// read against is this, not either colour on its own.
QColor composite(const QColor &over, const QColor &under)
{
    const int a = over.alpha();
    const auto mix = [a](int o, int u) { return (o * a + u * (255 - a) + 127) / 255; };
    return {mix(over.red(), under.red()), mix(over.green(), under.green()),
            mix(over.blue(), under.blue())};
}

/// The contrast floor a scheme's body text is built to. Every palette above is solved to
/// 4.5:1 — WCAG AA for small text — except Contrast, which offers AAA and holds its own
/// text tokens to 7:1.
///
/// accentInk() is the one ink this app cannot solve offline, so it is the one place that
/// has to be told which floor it is working to. It was not: the 4.5 was written into the
/// loop below, and Contrast — the single scheme whose entire offer is that nothing here is
/// decorative enough to be let off — handed its selected category label and its ticked
/// debloat status line an ink between 4.55 and 5.92 on all eight presets, against the 7:1
/// its own comment promises a few hundred lines up. Every other scheme is unaffected.
///
/// A switch rather than `a == Contrast` so that -Wswitch names every scheme added after
/// this line. A second AAA scheme that forgot this would draw its own palette correctly
/// and be wrong only in the one colour that is computed rather than written down, which is
/// the kind of bug this file keeps having to find by measuring.
qreal textFloor(Appearance a)
{
    switch (a) {
    case Appearance::Contrast:
        return 7.0;
    case Appearance::Dark:
    case Appearance::Light:
    case Appearance::Midnight:
    case Appearance::Sepia:
    case Appearance::Ocean:
    case Appearance::Forest:
    case Appearance::Dusk:
    case Appearance::Rose:
    case Appearance::Mist:
    case Appearance::Meadow:
    case Appearance::Lilac:
        break;
    }
    return 4.5;
}

} // namespace

QColor accentInk()
{
    // The handoff names its own ink: #8FDBFF on dark, the accent itself on light, both
    // solved by its author against its own grounds; a tinted scheme's is solved below.
    if (g_shell == Shell::Fluent)
        return Fluent::tokens().accentText;
    if (!isLightFamily(g_appearance))
        return g_accent;
    return solveInk(g_appearance);
}

/// The darkened accent that reads as text on \a a's light ground — the loop accentInk()
/// describes, for any scheme rather than only the one in force, so the Fluent shell can
/// ask for the ink of the scheme it is tinting with.
QColor solveInk(Appearance a)
{

    // The default amber #D2A75A is 8.01:1 against this palette's own #17171B window and
    // 2.23:1 against white: on a dark ground it is already ink, on a light one it cannot
    // be. Darkening the value while keeping hue and saturation preserves the identity.
    // (This sentence read "~5:1 against #121214 but under 2:1 against white" until it was
    // measured. Both halves were wrong, and the second was wrong across the bar it quoted.)
    //
    // How far to darken was a flat ×0.62 until it was measured, and the flat figure does
    // not hold the floor. On the shipped Light palette `neutral` came to 3.86 against the
    // selection wash and `muted green` to 4.20; on the dimmer light schemes amber and sage
    // join them, and 18 of the 48 accent × scheme pairs were under the 4.5:1 the palettes
    // above are built to. A darker flat factor fixes those by dragging down the ones that
    // already passed: ×0.50 turns amber from #82632D into #695024, which is no longer the
    // colour the user picked.
    //
    // So the value is stepped down one 8-bit level at a time and stops at the first one
    // that clears the floor. Four of the eight presets — periwinkle, clay rose, soft
    // violet and muted sky — come out at exactly the ×0.62 they had, on every light
    // scheme; only the ones that were failing move, and only as far as they must.
    //
    // Both grounds are checked because the wash is not always the darker one. accentSoft()
    // is the accent itself at 19% over the window, so for the preset accents — all mid
    // tones — it darkens the ground and binds; a user who types --accent #FFFFF0 gets a
    // wash lighter than the window, and then the bare window binds instead.

    // Recomputed only when the accent or the scheme changes. This runs inside the paint
    // event of every sidebar row, and the loop below can walk a hundred levels.
    static QColor memoAccent;
    static Appearance memoAppearance = Appearance::Dark;
    static QColor memo;
    if (memo.isValid() && memoAccent == g_accent && memoAppearance == a)
        return memo;

    const QColor hsv = g_accent.toHsv();
    const int hue = hsv.hue();
    const int sat = qMin(255, int(hsv.saturation() * 1.15));

    // The wash is written out rather than taken from accentSoft(): under the Fluent shell
    // that function answers from the tinted tokens, which are what this is computing.
    const QColor bare = palette(a).window;
    QColor wash = g_accent;
    wash.setAlpha(isLightFamily(a) ? 0x30 : 0x21);
    const QColor washed = composite(wash, bare);

    // The same floor as this scheme's text tokens — 4.5:1, or Contrast's 7:1 — because
    // this ink is the selected category's own label, a ticked debloat row's status line
    // and the chosen language chip, all of them body-sized text and none of them exempt.
    //
    // The guard is on the value and not on the ratio because black clears the floor
    // against every ground a light scheme has, so the loop always ends. That still holds
    // at 7:1: the wash is the accent at 19% over the window, so on Contrast's white ground
    // it can be no darker than #CFCFCF even if the user types --accent #000000, and black
    // is 13.3:1 against that.
    const qreal minRatio = textFloor(a);
    int value = int(hsv.value() * 0.62);
    while (value > 0) {
        const QColor ink = QColor::fromHsv(hue, sat, value);
        if (qMin(contrastRatio(ink, bare), contrastRatio(ink, washed)) >= minRatio)
            break;
        --value;
    }

    memoAccent = g_accent;
    memoAppearance = a;
    memo = QColor::fromHsv(hue, sat, value);
    return memo;
}

void setAccent(const QColor &c, Persist persist)
{
    if (!c.isValid())
        return;   // a bad --accent value is dropped, never stored
    if (persist == Persist::Yes)
        QSettings().setValue(QStringLiteral("appearance/accent"), c.name(QColor::HexRgb));
    if (c == g_accent)
        return;
    g_accent = c;
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

void setCompact(bool on, Persist persist)
{
    if (persist == Persist::Yes)
        QSettings().setValue(QStringLiteral("layout/compact"), on);
    if (on == g_compact)
        return;
    g_compact = on;
    Q_EMIT notifier()->compactChanged();
}

} // namespace Theme
