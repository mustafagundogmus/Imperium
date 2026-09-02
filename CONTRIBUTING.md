# Contributing to Arbitrium

Thanks for wanting to help. This page is the short version of how the project works, so a
first contribution lands the first time rather than after three rounds of review. Most of
the longer reasoning lives in comments at the top of the source files they belong to — the
codebase is written to be read, and reading the file you are about to change is the best
preparation there is.

## The three kinds of contribution

**A wrong or missing tweak.** The most valuable kind, and the one that needs no compiler.
Every tweak is a row in [`resources/data/catalog.json`](resources/data/catalog.json); its
text in ten languages is in [`resources/data/i18n.json`](resources/data/i18n.json). See
[Adding or fixing a tweak](#adding-or-fixing-a-tweak) below.

**A bug in the application.** Open an issue first if it is not obvious, so the fix can be
discussed before it is written; the bug report template asks for the things that make a
report reproducible (the build number from the title bar, the page, the tweak id).

**A translation.** All ten languages ship complete and the checker enforces that, so a new
string means ten strings. If you can only write one or two of them, say so in the pull
request and write the rest in English as a placeholder — a wrong-language string that is
clearly marked is better than a missing key, which the checker rejects.

## Building

Qt 6 (Widgets, Svg, Network), CMake 3.16 or newer, and a C++17 compiler. The shipped
executable is a MinGW build against a static Qt; a shared Qt from the Qt installer is
what you want for development, and CMake runs `windeployqt` after every link so the
executable starts from Explorer.

```powershell
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/mingw_64
cmake --build build --parallel
```

The tree builds with **zero warnings** under `-Wall -Wextra -Wshadow`, and a pull request
should keep it that way. `-Werror` is deliberately not on — see the comment in
`CMakeLists.txt` — so it is on you to read the build output.

The application is manifested `requireAdministrator` and asks for consent when it starts.
`build\Arbitrium.exe --self-test <path>` exercises the registry round-trip, the journal's
undo, presets, the `.reg` exporter and the action engine against scratch keys under
`HKCU\Software\Arbitrium\SelfTest`, and writes what it found to `<path>`. Run it before you
open a pull request that touches any of those.

## The data files

The catalogue, the actions and the translations are read at runtime out of the compiled
resource, so a malformed one is not a build error — it is a page that comes up empty. The
checker that stands in for the compiler is:

```powershell
python tools/check-data.py
```

Standard library only, twelve checks, and CI runs it on every commit. Run it before every
commit that touches `resources/data/`.

## Adding or fixing a tweak

A row looks like this:

```json
{
  "id": "priv-app-device-inventory",
  "name": "Uygulama kullanım verisi toplama",
  "desc": "Windows'un hangi uygulamaların ne kadar kullanıldığını kaydetmesini durdurur.",
  "reg": [
    {"hive": "HKLM", "path": "SOFTWARE\\Policies\\Microsoft\\Windows\\AppCompat",
     "value": "DisableInventory", "type": "DWORD", "on": "1", "off": "DELETE"}
  ]
}
```

The rules that reviews actually check:

- **The row does what its name says, for the value it writes.** Check the value against
  Microsoft's documentation or the `.admx` it comes from, not against another tweaker's
  list — several rows that came from lists were found writing values Windows never reads.
  Say in the pull request where the value is documented.
- **`off` is what Windows ships, not the opposite of `on`.** For a policy value that does
  not exist on a stock machine, `off` is `"DELETE"` (not configured), never an explicit
  `0` or `1` that would grey out the Settings UI. For a value that does exist by default,
  `off` is that default. The app restores the machine's own previous value from the journal
  when it can; `off` is what a preset applied to a fresh machine writes.
- **Types are Windows' types.** `MouseSpeed`, `InitialKeyboardIndicators` and the like are
  `REG_SZ` even though they hold numbers. DWORD data is decimal in the catalogue; hex is not
  parsed.
- **One owner per registry value.** Two rows writing the same value fight each other; the
  checker does not catch this yet, so search the file for the value name first. Services
  are synthesised from the machine — do not add a row that writes `Services\<name>\Start`.
- **Positions, not switches, where Windows has positions.** A setting with three states is
  an `options` row; a numeric one is a `range`. Read how `ch-launchto` and
  `cln-do-cache-age` are written.
- **Gate it to the builds it works on.** `minBuild` / `maxBuild` take Windows build numbers
  (22000 = 11 21H2, 22621 = 22H2, 22631 = 23H2, 26100 = 24H2). A row that is silently
  ignored on some build is a row that lies there.
- **Say the cost in `desc`.** If disabling it stops something — updates, a driver's
  telemetry that a control panel depends on, Alt+Tab speed — the description says so
  before the benefit, in one or two sentences.
- **Every row has its ten translations**: `tweak.<id>.name` and `tweak.<id>.desc` in
  `i18n.json`, and `opt.<label>` for every option label that carries a word. The checker
  fails without them.

For a script rather than a registry value — something that runs once and cannot be read
back — the place is [`resources/data/actions.json`](resources/data/actions.json). Actions
show their whole script before they run and report through an `ARB|…` token the page turns
into words; the engine prepends `$ErrorActionPreference = 'Stop'` so a failing cmdlet
fails the action.

## Code

- Read the header comment of the file you are changing. It says why the file is shaped the
  way it is, and a change that contradicts it is usually a change that reopens a bug the
  comment describes.
- **Never launch a program by bare name.** The application runs elevated, and a bare
  `powershell.exe` is resolved through a `PATH` the process inherited from whoever started
  it. `src/winpaths.h` has the absolute paths; use them.
- **Never read a registry key through `QSettings` to learn whether it exists.** `QSettings`
  creates what it cannot open. `Registry::openKey` guards against this, `Registry::keyExists`
  is the direct question.
- **Nothing on the GUI thread walks a filesystem or waits on the network.** The cleaner
  and the updater show how it is done.
- **Every string a user sees goes through `Locale::tr`**, and every key exists in all ten
  languages. `Locale::content` is for catalogue text that has a Turkish source string to
  fall back on.
- Comments explain *why*, in full sentences, and are kept when the code they explain
  changes. A comment that names a bug the code prevents is the most useful kind.

## Pull requests

- One change per pull request, described in terms of what the user sees or what the
  machine does differently.
- A `CHANGELOG.md` entry under the unreleased section (Turkish; the maintainer will write
  the English release note from it — or include both).
- Say which Windows build you tested on. The title bar shows it.
- The checklist in the pull request template is the review's checklist; going through it
  yourself first is the fastest route to a merge.

## Reporting a security problem

Not through an issue. See [SECURITY.md](SECURITY.md).
