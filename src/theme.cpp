#include "theme.h"
#include "i18n.h"

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
/// Every token that carries content now clears 4.5:1; the faintest decorative one, the
/// "4 öğe" count, sits at 4.52:1.
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

        /* textPrimary   */ {0xF0, 0xF0, 0xF3},   // 15.72:1
        /* textSecondary */ {0xC0, 0xC0, 0xCA},   //  9.90:1
        /* textStatus    */ {0xB4, 0xB4, 0xBE},   //  8.69:1
        /* textDesc      */ {0xA2, 0xA2, 0xAD},   //  7.07:1
        /* textMuted     */ {0xA5, 0xA5, 0xB0},   //  7.33:1
        /* textDim       */ {0x94, 0x94, 0xA0},   //  5.96:1
        /* textFaint     */ {0x8E, 0x8E, 0x9A},   //  5.52:1
        /* textFainter   */ {0x7F, 0x7F, 0x8B},   //  4.52:1
        /* textMono      */ {0xDC, 0xDC, 0xE4},   // 13.11:1
        /* placeholder   */ {0x8C, 0x8C, 0x97},   //  5.37:1
        /* iconStroke    */ {0xA8, 0xA8, 0xB2},   //  7.58:1

        /* onAccent      */ {0x14, 0x14, 0x14},
        /* scrollThumb   */ {0x35, 0x35, 0x3C},
    };
    return p;
}

/// Not in the handoff. The lightness relationships of the dark palette are mirrored
/// rather than inverted: the same neutral, slightly blue-leaning greys, and the same
/// ordering from primary text down to the faintest label.
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

        /* textPrimary   */ {0xF4, 0xF4, 0xF8},
        /* textSecondary */ {0xC8, 0xC8, 0xD2},
        /* textStatus    */ {0xBC, 0xBC, 0xC6},
        /* textDesc      */ {0xAB, 0xAB, 0xB6},
        /* textMuted     */ {0xAE, 0xAE, 0xB9},
        /* textDim       */ {0x9C, 0x9C, 0xA8},
        /* textFaint     */ {0x94, 0x94, 0xA0},
        /* textFainter   */ {0x84, 0x84, 0x8E},
        /* textMono      */ {0xE2, 0xE2, 0xEA},
        /* placeholder   */ {0x93, 0x93, 0x9E},
        /* iconStroke    */ {0xB0, 0xB0, 0xBB},

        /* onAccent      */ {0x0B, 0x0B, 0x0B},
        /* scrollThumb   */ {0x30, 0x30, 0x3A},
    };
    return p;
}

/// A warm light theme — cream paper rather than the cool blue-white of Light — with warm
/// brown-grey text. Mirrors Light's lightness relationships hue-shifted to the warm end:
/// higher red, then green, least blue, so nothing turns muddy. Text is a warm near-black
/// brown on the paper, which is very high contrast; the ramp down to the faintest label
/// keeps the same ordering Light uses.
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

        /* textPrimary   */ {0x2E, 0x27, 0x1B},
        /* textSecondary */ {0x55, 0x4C, 0x3B},
        /* textStatus    */ {0x5F, 0x56, 0x45},
        /* textDesc      */ {0x76, 0x6B, 0x57},
        /* textMuted     */ {0x77, 0x6C, 0x58},
        /* textDim       */ {0x83, 0x78, 0x63},
        /* textFaint     */ {0x9C, 0x90, 0x78},
        /* textFainter   */ {0xAC, 0xA0, 0x86},
        /* textMono      */ {0x45, 0x3D, 0x2D},
        /* placeholder   */ {0x9C, 0x90, 0x78},
        /* iconStroke    */ {0x76, 0x6B, 0x57},

        /* onAccent      */ {0x2A, 0x24, 0x1A},
        /* scrollThumb   */ {0xCF, 0xC4, 0xAC},
    };
    return p;
}

} // namespace

const Palette &palette()
{
    return palette(g_appearance);
}

// Four tinted variants of the dark palette, each generated by rotating the neutral ramp
// to a single hue and preserving every token's lightness — so the contrast ratios stay
// the dark palette's (the faintest content token clears 4.5:1 in all four). The tint is
// gentle on purpose: a ground you notice, not one that shouts, in keeping with the rest.
// Ocean leans blue, Forest green, Dusk violet, Rose a warm rosé.
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
        /* textPrimary     */ {0xF1, 0xF2, 0xF2},
        /* textSecondary   */ {0xC1, 0xC6, 0xC9},
        /* textStatus      */ {0xB4, 0xBA, 0xBE},
        /* textDesc        */ {0xA2, 0xA8, 0xAD},
        /* textMuted       */ {0xA5, 0xAB, 0xB0},
        /* textDim         */ {0x93, 0x9B, 0xA1},
        /* textFaint       */ {0x8D, 0x95, 0x9B},
        /* textFainter     */ {0x7D, 0x86, 0x8D},
        /* textMono        */ {0xDE, 0xE0, 0xE2},
        /* placeholder     */ {0x8A, 0x93, 0x99},
        /* iconStroke      */ {0xA8, 0xAE, 0xB2},
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
        /* textPrimary     */ {0xF1, 0xF2, 0xF1},
        /* textSecondary   */ {0xC2, 0xC8, 0xC4},
        /* textStatus      */ {0xB5, 0xBD, 0xB8},
        /* textDesc        */ {0xA3, 0xAC, 0xA6},
        /* textMuted       */ {0xA6, 0xAF, 0xA9},
        /* textDim         */ {0x94, 0xA0, 0x98},
        /* textFaint       */ {0x8E, 0x9A, 0x92},
        /* textFainter     */ {0x7E, 0x8C, 0x83},
        /* textMono        */ {0xDE, 0xE2, 0xDF},
        /* placeholder     */ {0x8B, 0x98, 0x8F},
        /* iconStroke      */ {0xA8, 0xB2, 0xAB},
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
        /* textPrimary     */ {0xF2, 0xF1, 0xF2},
        /* textSecondary   */ {0xC6, 0xC1, 0xC9},
        /* textStatus      */ {0xBA, 0xB4, 0xBE},
        /* textDesc        */ {0xA8, 0xA2, 0xAD},
        /* textMuted       */ {0xAB, 0xA5, 0xB0},
        /* textDim         */ {0x9B, 0x93, 0xA1},
        /* textFaint       */ {0x95, 0x8D, 0x9B},
        /* textFainter     */ {0x86, 0x7D, 0x8D},
        /* textMono        */ {0xE0, 0xDE, 0xE2},
        /* placeholder     */ {0x93, 0x8A, 0x99},
        /* iconStroke      */ {0xAE, 0xA8, 0xB2},
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
        /* textPrimary     */ {0xF2, 0xF1, 0xF1},
        /* textSecondary   */ {0xC8, 0xC2, 0xC3},
        /* textStatus      */ {0xBD, 0xB5, 0xB7},
        /* textDesc        */ {0xAC, 0xA3, 0xA5},
        /* textMuted       */ {0xAF, 0xA6, 0xA8},
        /* textDim         */ {0xA0, 0x94, 0x97},
        /* textFaint       */ {0x9A, 0x8E, 0x91},
        /* textFainter     */ {0x8C, 0x7E, 0x81},
        /* textMono        */ {0xE2, 0xDE, 0xDF},
        /* placeholder     */ {0x98, 0x8B, 0x8E},
        /* iconStroke      */ {0xB2, 0xA8, 0xAB},
        /* onAccent        */ {0x16, 0x12, 0x13},
        /* scrollThumb     */ {0x41, 0x30, 0x34},
    };
    return p;
}

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
    return Appearance::Dark;   // the default, and the fallback for anything unrecognised
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
    return g_accent;
}

QColor accentSoft(Appearance a)
{
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

QColor accentInk()
{
    if (!isLightFamily(g_appearance))
        return g_accent;

    // Amber at full strength is ~5:1 against #121214 but under 2:1 against white.
    // Darkening the value while keeping hue and saturation preserves the identity.
    QColor c = g_accent.toHsv();
    return QColor::fromHsv(c.hue(), qMin(255, int(c.saturation() * 1.15)),
                           int(c.value() * 0.62));
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
