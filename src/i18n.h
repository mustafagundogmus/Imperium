// i18n.h — interface language.
//
// Named i18n.h rather than locale.h on purpose: MinGW's C runtime ships its own
// <locale.h> (setlocale, LC_NUMERIC, …), and this project's own src/ directory sits on
// the compiler's include path. A same-named header there does not just shadow that file
// for us — an angle-bracket `#include <locale.h>` reached transitively from inside Qt or
// libstdc++, from any translation unit, can resolve to this one instead and corrupt
// completely unrelated code with errors nowhere near here. Cost one clash to learn.
//
// A small, data-driven translation layer next to Theme's font/accent machinery, not Qt's
// own .ts/.qm pipeline: this app's strings are QStringLiteral calls scattered through
// hand-written C++, not generated from .ui files, and re-running lupdate/lrelease for
// every edit would be one more build step to keep in sync by hand. A flat JSON table —
// one resource file, one key per string, ten language columns — is auditable without
// tooling and costs nothing at runtime beyond a hash lookup.
//
// Coverage is close to total, and this paragraph used to say the opposite. The interface
// chrome (navigation, settings, dialogs, the first-run setup) has always been translated
// into all ten languages; 0.9.8 brought the catalogue with it — every tweak name and
// description, every section heading, every option label, every one-shot action — and
// 0.9.9 finished the job by moving the last hard-coded sentences (the service risk notes,
// two SysInfo values, the ownership dialog's title) into the table as well.
//
// Two things stay in the system's language on purpose, because they are not this app's
// words to translate: the names and descriptions Windows gives its own services and
// startup entries, which are read from the machine at run time, and the numeric labels of
// a range tweak, which are digits and a unit. Settings' language row says exactly this.

#pragma once

#include <QObject>
#include <QString>
#include <QVector>

namespace Locale {

struct Language
{
    QString id;           ///< "tr", "en", … — stored in QSettings
    QString nativeName;   ///< shown in its own language, e.g. "Deutsch" not "German"
    bool rtl = false;
};

/// Every language the build carries, native (Turkish) first since it is the app's own
/// language and always complete by construction.
const QVector<Language> &languages();

/// Loads the translation table from resources and restores the persisted choice. Call
/// once at startup, same shape as Theme::initFonts().
void init();

QString language();                 ///< id of the language in use
void setLanguage(const QString &id); ///< persists, retranslates every open page, no restart

bool isRtl();                       ///< true for the current language's own natural direction

/// Looks up \a key in the current language. Falls back to Turkish, then to the key itself
/// (visibly wrong beats silently missing — a stray "settings.title" in the UI is a bug
/// report waiting to happen, an empty label is not).
QString tr(const QString &key);

/// Same lookup, but for content that already has a perfectly good Turkish string of its
/// own — catalogue tweak names, section titles, action descriptions. Those live in
/// catalog.json / actions.json, so a missing key must fall back to that text rather than
/// to the key: half-translated content stays readable instead of showing "tweak.priv-07".
/// This is what lets the catalogue be translated a category at a time.
QString content(const QString &key, const QString &sourceText);

/// First launch on this machine: no setup has completed yet, so main() shows the wizard
/// instead of going straight to MainWindow.
bool isFirstRunPending();
void markSetupComplete();

class Notifier : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
Q_SIGNALS:
    void languageChanged();
};

Notifier *notifier();

} // namespace Locale
