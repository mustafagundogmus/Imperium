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

// The order here is the order the theme switch shows them in, and the numeric values are
// never persisted (a name is — see schemeToString), so this can be reordered freely.
//
// The four added last are all light ones. Of the eight before them only two — Light and
// Sepia — were, so a light-theme user had a cool white and a warm cream and nothing else,
// against six darks. They are appended rather than filed next to Light so that no card a
// user has already learnt the position of moves under them. Grouping the six darks and the
// six lights into the two rows the switch now draws would read better; that is a reorder
// of this line and of Order[] in themeswitch.cpp, which has to keep matching it.
enum class Appearance { Dark, Light, Midnight, Sepia, Ocean, Forest, Dusk, Rose,
                        Mist, Contrast, Meadow, Lilac };

/// How many of them there are, for the places that want to say the number rather than
/// handle each one — the Hakkında page's Görünüm card, today. Counted here rather than
/// derived, because a C++ enum carries no count and giving this one a trailing sentinel
/// would make every -Wswitch-checked switch over it non-exhaustive, which is exactly the
/// check that makes adding a scheme safe. The assertion is `<` rather than `==` on purpose:
/// it stays quiet through a reorder (the enum's numeric values are never persisted, so
/// reordering is allowed) and fires on a ninth scheme wherever it is inserted.
inline constexpr int AppearanceCount = 12;
static_assert(int(Appearance::Lilac) < AppearanceCount,
              "a scheme was added to Appearance without updating AppearanceCount");

/// Which family a scheme belongs to, for the handful of decisions that turn on "is the
/// ground light or dark" rather than on the exact palette — the accent wash's alpha, the
/// darkened accent ink for readable text, the border glow. Midnight is a darker Dark;
/// Sepia is a warmer Light; Mist is a dimmed one, Contrast a maximal one, and Meadow and
/// Lilac are tinted ones.
///
/// Written as a switch rather than a chain of ==, so that -Wswitch reports every scheme
/// added after this line. A scheme missing from here does not fail to compile and does not
/// look wrong at a glance: it draws its own palette correctly and gets the dark family's
/// 13% accent wash and its undarkened accent ink, which on a light ground is the pale
/// amber the accentInk() comment was written about. That is a bug you have to notice by
/// eye, which is exactly the kind this file is meant not to have.
inline bool isLightFamily(Appearance a)
{
    switch (a) {
    case Appearance::Light:
    case Appearance::Sepia:
    case Appearance::Mist:
    case Appearance::Contrast:
    case Appearance::Meadow:
    case Appearance::Lilac:
        return true;
    case Appearance::Dark:
    case Appearance::Midnight:
    case Appearance::Ocean:
    case Appearance::Forest:
    case Appearance::Dusk:
    case Appearance::Rose:
        break;
    }
    return false;
}

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

// ------------------------------------------------------------------ shell ---
//
// Which chrome the window wears. Classic is the sidebar-and-status-bar shell every page
// was designed in; Fluent is the Windows 11 layout from design_handoff_fluent_ui — an
// icon rail, a category pane, a content column with its own header and an apply bar. The
// pages are the same widgets under both; only what surrounds them changes, which is why
// this is a Theme setting rather than a second application.
//
// Fluent brings its own two palettes and its own accent, so while it is in force the
// twelve schemes collapse to their family — a dark pick is Fluent dark, a light pick is
// Fluent light — and palette(), accent(), accentSoft() and accentInk() answer with the
// handoff's tokens. The user's own scheme and accent are kept and come back the moment
// the shell does.
enum class Shell { Classic, Fluent };

Shell shell();
void setShell(Shell s, Persist persist = Persist::Yes);
inline bool fluent() { return shell() == Shell::Fluent; }

QString shellToString(Shell s);
Shell shellFromString(const QString &name);

/// The card's corner radius under the shell in force: the classic shell is square by
/// request, the Fluent one is the handoff's 8px.
int windowRadius();

namespace Fluent {

/// The handoff's design tokens, dark and light. Kept with their alpha: the pane rows, the
/// hover washes and the control fills are translucent in the design and are painted over
/// whatever is beneath them, which is how one token reads right on mica, on surface and on
/// a card. Palette gets opaque composites of the same values for the code that reads a
/// token as a colour to compare against or to name in a stylesheet.
struct Tokens
{
    QColor desk, mica, surface, card, cardBorder, divider, winBorder;
    QColor text, textSec, textMuted;
    QColor subtleHover, rowHover, selected;
    QColor controlBg, controlBorder, controlHover, iconBg, track;
    QColor accent, accentSoft, accentText, onAccent, ok;
    QColor knobOff, toggleOffBorder, closeHover;
};

/// The tokens for the scheme in force. Dark and Light are the handoff's own two sets,
/// exactly; any other scheme keeps the Fluent layout and takes its colours from the
/// scheme — the scheme's window for mica, two lighter steps of it for surface and card,
/// the user's accent — so that Ocean, Forest and Rose mean something under this shell
/// rather than collapsing to the one grey.
const Tokens &tokens();
const Tokens &tokens(Appearance a);
/// The handoff's base set by family, untinted.
const Tokens &tokens(bool light);

// The handoff's fixed measurements. Literal, like Metric:: above: nothing here scales with
// the font, because the design gives every one of them in pixels.
inline constexpr int WindowWidth = 1280;
inline constexpr int WindowHeight = 800;
inline constexpr int MinWidth = 1100;
inline constexpr int MinHeight = 680;
inline constexpr int WindowRadius = 8;
inline constexpr int TitleBarHeight = 48;
inline constexpr int RailWidth = 56;
inline constexpr int PaneWidth = 232;
inline constexpr int ApplyBarHeight = 56;
inline constexpr int WindowButtonWidth = 46;
inline constexpr qreal ContentPadX = 36.0;

} // namespace Fluent

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

/// The two colours a tweak row's risk badge is drawn in: amber for a row that costs a
/// convenience, red for one that is not recommended. Not palette tokens: the badge has
/// to read as a warning on every one of the twelve schemes, and the rose and sepia
/// palettes have no red or amber of their own to borrow. Two fixed pairs instead, one
/// per family, each chosen to clear 4.5:1 against that family's window and hovered-row
/// grounds — the same floor the text tokens are measured against in theme.cpp.
inline QColor Warn()
{
    return isLightFamily(appearance()) ? QColor(0x9A, 0x62, 0x00) : QColor(0xE3, 0xA8, 0x4C);
}
inline QColor Danger()
{
    return isLightFamily(appearance()) ? QColor(0xB8, 0x2E, 0x2E) : QColor(0xE8, 0x6B, 0x6B);
}

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
// The track, not the thumb. SmoothScrollArea insets the handle by a 2px border on each
// side, so the grabbable part is this minus 4: at the 8 this used to be, that came to a
// 4px thumb, which is thinner than the window's own border and needs a deliberate aim.
// 12 doubles it to 8px — the same width as Windows 11's own expanded scrollbar thumb, so
// it meets a hand that has been trained on that one — and 12 is also PagePadRight, which
// means the bar occupies exactly the column every page already reserves as its right
// gutter and content keeps a full 12px of air from it at every interface scale.
inline constexpr int ScrollBarWidth = 12;

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

    /// The window has to be rebuilt around the same pages. Fired before typefaceChanged
    /// and appearanceChanged, which follow it on the same switch — the chrome is replaced
    /// first, then everything measures and repaints against the new tokens.
    void shellChanged();
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

/// The accent as *text*. On a light palette the raw amber is too pale to read, so this is
/// a darkened variant; on a dark palette it is the accent itself.
///
/// How far it darkens is measured rather than fixed — the value comes down one level at a
/// time until the ink clears the scheme's own floor against both grounds it is drawn on,
/// the window and the selection wash. That floor is 4.5:1 for eleven of the twelve and
/// 7:1 for Contrast, which offers AAA and has to be held to it here too. An accent that
/// already cleared its floor at the design's ×0.62 keeps exactly the colour it had; see
/// textFloor() and the reasoning in theme.cpp.
QColor accentInk();

Notifier *notifier();

// ----------------------------------------------------------------- layout ---
bool compact();               ///< `compact` prop: tweak row padding 4px vs 7px
void setCompact(bool on, Persist persist = Persist::Yes);

} // namespace Theme
