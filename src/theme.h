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
    QColor page;            ///< behind the window
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
    QColor closeHover;
    QColor linkHover;
    QColor scrollThumb;
};

const Palette &palette();

Appearance appearance();
void setAppearance(Appearance a);

namespace Color {

inline const QColor &Page()            { return palette().page; }
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
inline const QColor &CloseHover()      { return palette().closeHover; }
inline const QColor &LinkHover()       { return palette().linkHover; }
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
inline constexpr qreal ToggleRadius = 7.5; // spec says 8, clamped to half of 15

inline constexpr int ToggleWidth  = 26;
inline constexpr int ToggleHeight = 15;
inline constexpr int KnobDiameter = 9;
inline constexpr int KnobInset    = 2;     // from the padding box
inline constexpr int KnobTravel   = 13;    // left: 2px → 13px

inline constexpr int WindowButtonWidth = 40;

inline constexpr int SectionGap = 16;      // gap between tweak sections
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

/// Loads the embedded IBM Plex faces. Must be called once after QApplication.
void initFonts();

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
const QFont &upperLabel();         ///< "GERİ YÜKLEME NOKTASI"  sans 10 / .08em
const QFont &restoreValue();       ///< "Bugün, 14:32"          sans 11.5
const QFont &link();               ///< "Yeni oluştur"          sans 10.5
const QFont &pageTitle();          ///< "Gizlilik"              sans 15 / 600 / -.01em
const QFont &pageSub();            ///< "31 tweak · 12 etkin ·" sans 11
const QFont &segment();            ///< Tümü / Etkin / Değişen  sans 11
const QFont &sectionTitle();       ///< "TELEMETRİ"             sans 10 / 500 / .09em
const QFont &sectionCount();       ///< "4 öğe"                 mono 9.5
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
};

/// Accent presets offered by the design (`accent` prop of the mockup).
QList<QColor> accentPresets();
QString accentName(const QColor &c);   ///< Turkish label for a preset, or its hex

QColor accent();
QColor accentSoft();          ///< accent behind a selected row (13% dark / 18% light)
void   setAccent(const QColor &c);

/// The accent as *text*. On the light palette the raw amber is too pale to read, so
/// this is a darkened variant; on the dark palette it is the accent itself.
QColor accentInk();

Notifier *notifier();

// ----------------------------------------------------------------- layout ---
bool compact();               ///< `compact` prop: tweak row padding 4px vs 7px
void setCompact(bool on);

} // namespace Theme
