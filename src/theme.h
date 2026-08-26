// theme.h — design tokens from design_handoff_windows_tweaker/README.md
//
// Every value here is a literal from the handoff. Nothing is invented, nothing is
// rounded: sizes that are fractional in CSS stay fractional here (11.5px, 10.5px …)
// and are converted to Qt point sizes against the screen's logical DPI so that the
// resulting pixel size is exact.

#pragma once

#include <QColor>
#include <QFont>
#include <QObject>
#include <QString>
#include <QVector>

namespace Theme {

// ---------------------------------------------------------------- colours ---
//
// The handoff specifies one dark palette. A light one was added later, so the tokens
// are looked up through Theme::palette() instead of being compile-time constants —
// the names and their roles are unchanged, they just answer to the current appearance.

enum class Appearance { Dark, Light };

struct Palette
{
    // surfaces
    QColor window;
    QColor surface;         ///< search field
    QColor surfaceHover;    ///< category / row hover
    QColor surfaceActive;   ///< titlebar hover, active segment
    QColor tile;            ///< overview stat tile

    // borders
    QColor borderWindow;
    QColor borderControl;
    QColor divider;
    QColor dividerSoft;     ///< overview info rows
    QColor tileBorder;

    // toggle
    QColor toggleOff;
    QColor toggleOffBorder;
    QColor knobOff;
    QColor knobOn;

    // text
    QColor textPrimary;
    QColor textSecondary;   ///< category label
    QColor textStatus;      ///< "N değişiklik bekliyor"
    QColor textDesc;        ///< tweak description
    QColor textMuted;       ///< inactive segment, ghost button
    QColor textDim;         ///< uppercase section labels
    QColor textFaint;       ///< mono meta
    QColor textFainter;     ///< "4 öğe"
    QColor textMono;        ///< overview values
    QColor placeholder;
    QColor iconStroke;      ///< window control glyphs

    // accents / states
    QColor onAccent;
    QColor scrollThumb;
};

const Palette &palette();

/// A specific palette regardless of what is in use — the theme switch paints a preview
/// of both at once, so it needs to ask for the one it is not currently wearing.
const Palette &palette(Appearance a);

Appearance appearance();
/// Whether a setter writes its new value to QSettings as well as applying it.
///
/// The command-line switches are one-shot overrides for a single run — `--theme light`
/// to take a screenshot, `--accent` to see a colour, `--compact` to check a layout — and
/// they went through the very setters the settings page uses, so trying one rewrote the
/// look the user had saved and it stayed rewritten. Everything else a setter does still
/// happens either way: the styles rebuild, the notifier fires, the window repaints.
enum class Persist { Yes, No };

void setAppearance(Appearance a, Persist persist = Persist::Yes);

namespace Color {

inline const QColor &Window()          { return palette().window; }
inline const QColor &Surface()         { return palette().surface; }
inline const QColor &SurfaceHover()    { return palette().surfaceHover; }
inline const QColor &SurfaceActive()   { return palette().surfaceActive; }
inline const QColor &Tile()            { return palette().tile; }

inline const QColor &BorderWindow()    { return palette().borderWindow; }
inline const QColor &BorderControl()   { return palette().borderControl; }
inline const QColor &Divider()         { return palette().divider; }
inline const QColor &DividerSoft()     { return palette().dividerSoft; }
inline const QColor &TileBorder()      { return palette().tileBorder; }

inline const QColor &ToggleOff()       { return palette().toggleOff; }
inline const QColor &ToggleOffBorder() { return palette().toggleOffBorder; }
inline const QColor &KnobOff()         { return palette().knobOff; }
inline const QColor &KnobOn()          { return palette().knobOn; }

inline const QColor &TextPrimary()     { return palette().textPrimary; }
inline const QColor &TextSecondary()   { return palette().textSecondary; }
inline const QColor &TextStatus()      { return palette().textStatus; }
inline const QColor &TextDesc()        { return palette().textDesc; }
inline const QColor &TextMuted()       { return palette().textMuted; }
inline const QColor &TextDim()         { return palette().textDim; }
inline const QColor &TextFaint()       { return palette().textFaint; }
inline const QColor &TextFainter()     { return palette().textFainter; }
inline const QColor &TextMono()        { return palette().textMono; }
inline const QColor &Placeholder()     { return palette().placeholder; }
inline const QColor &IconStroke()      { return palette().iconStroke; }

inline const QColor &OnAccent()        { return palette().onAccent; }
inline const QColor &ScrollThumb()     { return palette().scrollThumb; }

} // namespace Color

// ---------------------------------------------------------------- metrics ---
namespace Metric {

// The handoff specifies 1060×820; the shell was widened and shortened on request so the
// two-column Genel Bakış grid and the live chart breathe without scrolling.
inline constexpr int WindowWidth   = 1240;
inline constexpr int WindowHeight  = 760;
// The handoff rounds the window to 9px; square corners were requested instead.
inline constexpr int WindowRadius  = 0;

inline constexpr int TitleBarHeight = 36;
inline constexpr int StatusBarHeight = 36;
inline constexpr int SidebarWidth   = 212;

inline constexpr int SearchHeight   = 27;
inline constexpr int CategoryHeight = 28;
inline constexpr int ControlRadius  = 5;   // controls, rows, buttons
inline constexpr int BadgeRadius    = 3;   // ⌃K badge

// The handoff's switch is 26×15 with a 9px knob. The redesigned one is a shade larger
// so the wipe fill and the squash have room to read; everything else in the row grid is
// derived from these, so the layout follows automatically.
inline constexpr int ToggleWidth  = 30;    // the capsule itself
inline constexpr int ToggleHeight = 16;

inline constexpr int WindowButtonWidth = 40;

inline constexpr int SectionGap = 16;      // gap between tweak sections

// The margin every stacked page keeps around its content. Declared here because all seven
// of them want the same four numbers, and each used to carry its own copy — seven places
// to edit, and seven chances for one of them to be missed.
inline constexpr int PagePadLeft   = 18;
inline constexpr int PagePadTop    = 2;
inline constexpr int PagePadRight  = 12;
inline constexpr int PagePadBottom = 16;
inline constexpr int ScrollBarWidth = 8;

// Shadow: CSS `0 24px 80px rgba(0,0,0,.55)`.
// A literal 80px blur would need ~80px of transparent margin on every side; with the
// window already 820px tall that overflows a 960pt-high desktop. The margins below keep
// the same shape and falloff at a size that still fits, and FramelessWindow drops them
// entirely when the screen is too small (or the window is maximised).
inline constexpr int ShadowBlur    = 24;   // CSS blur radius ≈ 2σ
inline constexpr int ShadowOffsetY = 12;
inline constexpr int ShadowMarginX = 28;
inline constexpr int ShadowMarginTop = 14;
inline constexpr int ShadowMarginBottom = 38;
inline constexpr int ShadowAlpha = 140;   // rgba(0,0,0,.55)

} // namespace Metric

// ------------------------------------------------------------------ fonts ---
enum class Family { Sans, Mono };

namespace Weight {
inline constexpr int Regular  = 400;
inline constexpr int Text     = 450;   // IBM Plex "Text" optical weight
inline constexpr int Medium   = 500;
inline constexpr int SemiBold = 600;
}

/// Loads the embedded faces and restores the saved appearance. Once, after QApplication.
void initFonts();

// ---------------------------------------------------------------- typeface ---
//
// The interface face is swappable. Everything the app draws goes through the styles in
// Font::, so pointing those at another family is enough — the mono face does not change
// with it, because the values it carries (versions, registry paths, byte counts) are
// column-aligned technical text and belong in a monospace whatever the rest is set in.

struct Typeface
{
    QString id;      ///< stored in QSettings
    QString name;    ///< shown in the settings row
    QString family;  ///< resolved family name, empty until the face is loaded
};

/// Every face the build carries, the default first.
const QVector<Typeface> &typefaces();

/// Registers a face with the font database if it is not there yet, and returns its
/// resolved family name. Cheap to call repeatedly.
QString loadTypeface(const QString &id);

QString typeface();                    ///< id of the face in use
void setTypeface(const QString &id, Persist persist = Persist::Yes);   ///< rebuilds every style

/// A multiplier over every text size in the app, 1.0 being the design's own sizes.
/// Clamped to [0.85, 1.6]; persisted; rebuilds every style like a face swap does.
qreal fontScale();
void  setFontScale(qreal scale, Persist persist = Persist::Yes);

/// The interface-scale steps the settings page offers, small to large. Kept next to the
/// setter so the row and the clamp above cannot drift apart.
struct FontScaleStep { const char *label; qreal value; };
inline const FontScaleStep FontScaleSteps[] = {
    {"Küçük", 0.9}, {"Normal", 1.0}, {"Büyük", 1.15}, {"Çok büyük", 1.3},
};

/// A font whose *pixel* size is exactly \a px, even when \a px is fractional.
/// \a letterSpacingEm mirrors the CSS `letter-spacing: <n>em` values.
QFont font(Family family, qreal px, int weight = Weight::Regular, qreal letterSpacingEm = 0.0);

inline QFont sans(qreal px, int weight = Weight::Regular, qreal ls = 0.0)
{ return font(Family::Sans, px, weight, ls); }

inline QFont mono(qreal px, int weight = Weight::Regular, qreal ls = 0.0)
{ return font(Family::Mono, px, weight, ls); }

/// Logical DPI used for the px↔pt round-trip above.
qreal logicalDpi();

/// The exact pixel size of \a f, including the fractional part.
qreal pixelSize(const QFont &f);

/// Every distinct text style in the design, named after where it appears.
/// Keeping them here means no widget carries a loose "11.5" in its paint code.
namespace Font {

const QFont &appName();            ///< titlebar "tweaker"      mono 11.5 / 500 / .02em
const QFont &monoMeta();           ///< version, system info, status left   mono 10
const QFont &searchText();         ///< "Tweak ara…"            sans 11.5
const QFont &kbd();                ///< "⌃K" badge              mono 9
const QFont &categoryName();       ///< sidebar row             sans 12.5 / 450
const QFont &categoryNameSelected();///< sidebar row, selected  sans 12.5 / 500
const QFont &categoryCount();      ///< sidebar count           mono 10
const QFont &link();               ///< "Yeni oluştur"          sans 10.5
const QFont &pageTitle();          ///< "Gizlilik"              sans 15 / 600 / -.01em
const QFont &pageSub();            ///< "31 tweak · 12 etkin ·" sans 11
const QFont &segment();            ///< Tümü / Etkin / Değişen  sans 11
const QFont &sectionTitle();       ///< "TELEMETRİ"             sans 10 / 500 / .09em
const QFont &sectionCount();       ///< "4 öğe"                 mono 9.5
const QFont &blockTitle();         ///< "Sistem" (genel bakış)  sans 12.5 / 600 / -.01em
const QFont &tweakName();          ///< sans 12.5 / 450
const QFont &tweakDesc();          ///< sans 10.5
const QFont &tileLabel();          ///< sans 10 / .08em
const QFont &tileValue();          ///< mono 15
const QFont &tileSub();            ///< sans 10
const QFont &infoLabel();          ///< sans 11
const QFont &infoValueMono();      ///< mono 10.5
const QFont &infoValueText();      ///< sans 11
const QFont &statusPending();      ///< "3 değişiklik bekliyor" sans 11
const QFont &buttonGhost();        ///< "Geri al"               sans 11
const QFont &buttonAccent();       ///< "Uygula (3)"            sans 11 / 500

} // namespace Font

// ----------------------------------------------------------------- accent ---
/// Broadcasts token changes so live widgets can repaint without a restart.
class Notifier : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
Q_SIGNALS:
    void accentChanged();
    void compactChanged();
    void appearanceChanged();

    /// Every metric in the app is derived from the fonts, so a listener has to relayout
    /// as well as repaint.
    void typefaceChanged();
};

/// Accent presets offered by the design (`accent` prop of the mockup).
QList<QColor> accentPresets();

QColor accent();
QColor accentSoft();          ///< accent behind a selected row (13% dark / 19% light)
/// The same wash for an appearance that is not the one in force — which is what the theme
/// preview cards need, since each of them draws the palette it is offering rather than the
/// one currently on screen. They used to re-derive it with their own alpha values, drifted
/// from these, so the preview did not match the sidebar it was previewing.
QColor accentSoft(Appearance a);
void   setAccent(const QColor &c, Persist persist = Persist::Yes);

/// The accent as *text*. On the light palette the raw amber is too pale to read, so
/// this is a darkened variant; on the dark palette it is the accent itself.
QColor accentInk();

Notifier *notifier();

// ----------------------------------------------------------------- layout ---
bool compact();               ///< `compact` prop: tweak row padding 4px vs 7px
void setCompact(bool on, Persist persist = Persist::Yes);

} // namespace Theme
