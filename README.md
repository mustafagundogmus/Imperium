<div align="center">

# Arbitrium

**Your Windows. Your rules.**

Windows ships with opinions about your privacy, your bandwidth, and what belongs on your Start menu.
Arbitrium is where you overrule them — 408 of them, one switch at a time.

[![Windows 10 | 11](https://img.shields.io/badge/Windows-10%20%7C%2011-0078D4?style=for-the-badge&logo=windows11&logoColor=white)](#)
[![Portable](https://img.shields.io/badge/Install-Not%20Required-2ea44f?style=for-the-badge)](#)
[![Languages](https://img.shields.io/badge/Languages-10-8957e5?style=for-the-badge)](#every-word-in-ten-languages)
[![License](https://img.shields.io/badge/License-MIT-d2a75a?style=for-the-badge)](LICENSE)

</div>

---

## Every other tweaker asks you to trust it. This one shows its work.

Most "optimizers" are a wall of checkboxes and a promise. You click *Apply*, something happens
somewhere, and if your machine acts strange next Tuesday — good luck. Arbitrium was built by
someone who got tired of that deal.

**Nothing is written until you say so.** Flip as many switches as you like. Nothing touches your
system until you press *Apply* — and the status bar counts exactly how many changes are waiting
while you decide.

**Every write is recorded — and reversible.** Before a value is changed, Arbitrium records what
was there: the old data, its type, and whether the key existed at all. The **Log** page reads that
history back and lets you revert any single change, individually, even months later — even for a
tweak that has since been rewritten or removed. Not "restore defaults". *Your* value, the one that
was actually on *your* machine.

**No black boxes.** Every one-shot action shows you the exact script it will run, in full, before
it runs. Read it, then decide.

---

## What's inside

### ⚙️ 408 tweaks that actually apply here

Not a generic list scraped from a 2016 forum post. Arbitrium checks each tweak against **your**
Windows build. A setting Microsoft retired three versions ago isn't quietly presented as a working
switch — it's shown greyed out, with the reason. Honesty over a longer feature list.

| | |
|---|---|
| **System** · 69 | Core behaviour, lock screen, sign-in, shell policy |
| **File Explorer** · 60 | Extensions, panes, Quick access, launch target |
| **Privacy** · 46 | Telemetry, diagnostics, advertising ID, activity history |
| **Visual Effects** · 35 | Animations, transparency, shadows, thumbnails |
| **Memory & CPU** · 30 | Paging, caching, prefetch, scheduling |
| **Windows Update** · 30 | Deferral, drivers, delivery optimisation, restart behaviour |
| **Network & Internet** · 28 | DNS, IPv6, Teredo, discovery, metered connections |
| **Security hardening** · 22 | SMB, LSA, PowerShell logging, legacy protocols |
| **Cleanup** · 22 | Storage Sense, temp, component store, history |
| **Startup** · 22 | Everything that runs when you sign in |
| **Context menu** · 20 | Take Ownership, Services, cascading power plans, and more |
| **Advanced** · 16 | The ones with sharp edges |
| **Power management** · 8 | Sleep, hibernation, USB selective suspend |

### 🔧 Every Windows service, in one place

All **329** services on your machine, each as a four-position control — *Automatic*, *Delayed*,
*Manual*, *Disabled*. Services Windows genuinely cannot boot without are **locked**, with the reason
stated. You can be reckless somewhere else.

### ⏱️ Every scheduled task, the same way

The Task Scheduler holds a couple of hundred tasks on an ordinary machine, forty of them
hidden, and that is where the telemetry uploaders, the compatibility appraisers and the
vendor updaters actually live. Arbitrium lists **all of them**, grouped by folder the way
Task Scheduler does, each one a switch. Rows that cost something say so up front — Defender's
scans, the update orchestrator, System Restore, TRIM — and the handful Windows protects with
an ACL that turns even an administrator away are shown **locked** with the reason, rather than
as a switch that fails. Every change is journalled and reverts like any other.

### 📦 Debloat that reads your actual machine

The Apps page doesn't guess from a hardcoded list of app names. It asks Windows which packages were
**preinstalled with your image**, then reads each one's real icon out of its own package folder.
What you see is what you actually have — real names, real logos, real version numbers.

Removal takes the app off **every account** on the machine and deprovisions it, so it doesn't come
back the next time someone new signs in. Shared runtimes and anything Windows itself marks
protected are shown with a padlock instead of a button — visible, but not a foot-gun.

### ⚡ 25 one-shot actions

Remove Edge, OneDrive, or Widgets. Empty temp folders and the component store. Rebuild the icon
cache. Switch DNS to Cloudflare, Google, or Quad9 in one click. Add the hidden **Ultimate
Performance** power plan. Each one tells you up front whether it can be undone — and shows you the
script.

### 🧹 A disk cleaner that measures before it deletes

Sixteen targets in four groups — Windows' temp and update caches, servicing logs, crash
dumps and error reports; this user's temp and Internet cache and the recycle bin; the caches
of six browsers, GPU shader caches, Discord, Teams, Steam, Spotify and the Store; and, kept
apart and unchecked, the ones with a cost: Prefetch, a previous Windows installation, old
restore points and the component store. Each is **measured in the background** first, so the
number on the row and the figure in the sidebar are what a clean would actually free, not a
guess. Files in use are skipped and counted, nothing goes through the Recycle Bin, and the
confirmation lists exactly which targets and how much.

### 🛡️ Launch anything as TrustedInstaller

The account that owns the files and registry keys even an administrator is refused. Point the
**TrustedInstaller** page at a program or a file — or use the one-tap shortcuts for a shell,
PowerShell, the registry editor, or the file manager — and it starts under that account, able to
read and write all of it without changing a single permission. No service to install, no token
juggling; it uses the elevated session the app already runs in. `whoami` in the launched shell
says `nt service\trustedinstaller`, which is the whole point.

### 📊 A dashboard that isn't decoration

Live CPU and memory graphs at one-second resolution, alongside **28 blocks** of the machine facts
you actually go looking for: activation state, Secure Boot, TPM, BIOS and SMBIOS, core isolation,
uptime, pending restarts and *why*, last restore point, every network adapter, every volume — plus
BitLocker and SMART per drive, Windows Update state, scheduled tasks, driver problems, privacy
posture, virtualisation, accounts, sensors and the last bugcheck code.

Nothing here blocks the window. The heavy reads arrive in three stages behind the page, and a
value that genuinely cannot be read says so rather than guessing.

### 💾 Profiles that travel

Save your entire configuration as a portable XML profile and apply it on the next machine — every
tweak, service and startup entry in one file, each remembered as the *position* it sits at rather
than a bare on/off. Or export just your pending changes as a plain **`.reg`** file that Windows
understands without Arbitrium anywhere in sight. Your setup shouldn't be a hostage.

### 🛡️ A safety net you can see the state of

Settings shows the date of your last System Restore point and puts Windows' own System Protection
window one click away. Arbitrium does not create the point for you: creating one changes the
system, and this build writes nothing you did not ask it to. Knowing whether you have a net, and
being one click from making one, is the part software can honestly do for you.

---

## Every word, in ten languages

Not just the buttons — **everything**. All 408 tweak names and descriptions, every service state,
every action, every label on the dashboard, every line in the log.

🇹🇷 Türkçe · 🇬🇧 English · 🇩🇪 Deutsch · 🇫🇷 Français · 🇪🇸 Español · 🇮🇹 Italiano · 🇵🇹 Português · 🇵🇱 Polski · 🇷🇺 Русский · 🇸🇦 العربية

Switch language from Settings and the entire interface changes instantly. No restart. Not even a flicker.

---

## Looks like it belongs on your desktop

Because a tool you'll actually open should be worth looking at.

- **Eight themes** — Dark and Light, a deeper Midnight and a warm Sepia, and four gently tinted darks (Ocean, Forest, Dusk, Rose), each a full palette rather than an inversion
- **Eight accent colours** you can pick, applied across the whole interface
- **Six typefaces** to choose from
- **Four text sizes**, because not everyone runs a 4K panel at 100%
- A **frameless window** with smooth scrolling and animation everywhere it earns its place

---

## Getting started

1. Download **`Arbitrium-vX.Y.Z-win64.exe`** from the [latest release](https://github.com/shadesofdeath/Arbitrium/releases/latest).
   That single file *is* the application — Qt is linked into it, so there is nothing to unpack
   and nothing to put beside it. (The `.zip` next to it holds the same executable, this README,
   the licence and a `licenses/` folder for everything inside the binary that is not this
   project's to relicense — for anyone whose browser dislikes a bare `.exe`.)
2. Run it. **No installation, no setup wizard, no bundled toolbar.** One executable. Windows will
   stop you once on the way; [what it says, and what you can check instead of taking my word for
   it](#windows-will-warn-you-about-this-file), is a section of its own.
3. On first launch, pick your language and your look.
4. Flip what you want. Press **Apply** when you mean it.

> Arbitrium needs administrator rights — it edits system-level settings, and pretending otherwise
> would be dishonest. It asks once, at launch.

---

## Windows will warn you about this file

Between *download it* and *it opens* there are two dialogs the list above doesn't mention.

The first is Microsoft Defender SmartScreen: a dialog headed **Windows protected your PC**, saying
it *prevented an unrecognized app from starting*, with one button — **Don't run**. The way past it
is **More info**, which unfolds the file name, *Publisher: Unknown publisher*, and a **Run anyway**
button. The second is the administrator prompt, which for a file like this one asks whether you
want to allow *this app from an unknown publisher* to make changes to your device.

Both are telling you the truth. Arbitrium's executable carries no Authenticode signature, and that
is a decision rather than an oversight. A certificate that changes those two dialogs starts at a
couple of hundred dollars a year, has to be kept on a hardware token or a cloud HSM since the
CA/Browser Forum began requiring that in June 2023, and — in the kind an individual can actually be
issued — replaces *Unknown publisher* with a legal name, checked against documents a certificate
authority keeps on file. It would not even clear that first dialog on day one: SmartScreen is
reputation-based, and a certificate that has never signed anything has no reputation either.

So instead of asking you to trust a pseudonym, here is what you can check. A release cut since
[the release workflow](.github/workflows/release.yml) gained provenance carries three files rather
than two — the executable, the zip, and `Arbitrium-vX.Y.Z-win64.sha256`.

**The sums file tells you the download is whole.** It holds one line each for the executable and
the zip, taken from the exact bytes that were uploaded.

```powershell
Get-FileHash Arbitrium-vX.Y.Z-win64.exe -Algorithm SHA256
```

Compare that against the matching line in the `.sha256` — PowerShell prints uppercase and the file
is lowercase, and that should be the only difference. What it is worth is narrow enough to say out
loud: it catches a download that arrived truncated or corrupted, and nothing else. Anyone who could
replace the executable on a release could replace the sums file in the same motion.

**The attestation tells you where the binary came from.** At publish time GitHub signs a statement
binding the digest of the executable and of the zip to this repository, the commit they were built
from, and the workflow run that built them. Checking that statement needs no key from anyone — only
the GitHub CLI, signed in:

```powershell
gh attestation verify Arbitrium-vX.Y.Z-win64.exe --repo shadesofdeath/Arbitrium
```

A file that is not byte for byte what that run produced has no attestation for its digest, and the
command fails. One that is prints back the repository and the workflow file that built it, with the
commit itself one `--format json` away — and that repository is the one you are reading, which is a
better reason to press **Run anyway** than a certificate would have given you.

---

## Fair warning

This tool changes real Windows settings, and some of them matter. It gives you a log, per-change
revert, a shortcut to Windows' System Protection, and a confirmation on everything destructive —
but it also assumes you're an adult who read the description.

Start with a restore point. Change things in small batches. The Log page is right there if
something isn't what you expected.

---

## License

Arbitrium is released under the **[MIT License](LICENSE)** — read it, fork it, ship it, build
something else out of it. Keep the copyright notice with it and the rest is yours.

Three pieces inside it carry licences of their own, and they stay with the binary:

- **Qt 6.11** — statically linked, used under the [LGPL v3](https://www.qt.io/licensing). The
  release workflow, [`.github/workflows/release.yml`](.github/workflows/release.yml), builds that
  Qt from the upstream tarballs and records every configure flag it was built with — which is what
  you need to reproduce this binary, or to relink it against a Qt of your own. The LGPL-3.0 text
  sits in [`resources/licenses/`](resources/licenses), beside the GPL-3.0 whose terms its own
  opening paragraph incorporates.
- **Six typefaces** — IBM Plex, Monda, Open Sans, Oxygen, Red Hat Text and Saira, every one of
  them compiled into the executable through [`resources.qrc`](resources/resources.qrc) and every
  one under the SIL Open Font License 1.1, with a [`LICENSE-*.txt`](resources/fonts) of its own.
- **lucide** — the glyphs that title the Genel Bakış cards, under the
  [ISC License](https://github.com/lucide-icons/lucide/blob/main/LICENSE). They are compiled into
  [`src/icons.cpp`](src/icons.cpp) as path data rather than shipped as files, and the text in
  [`resources/licenses/`](resources/licenses) is the upstream licence in full, because a few of
  the glyphs used here are on its Feather-derived list and carry that project's MIT notice in the
  same file.

The release zip carries all nine of those files in a `licenses/` folder, so a downloaded copy is
complete without the repository beside it.

---

<div align="center">

### Built by [ShadesOfDeath](https://github.com/shadesofdeath)

If Arbitrium saved you an afternoon of registry spelunking, you can say thanks:

[![Buy Me a Coffee](https://img.shields.io/badge/Buy%20Me%20a%20Coffee-berkayay-FFDD00?style=for-the-badge&logo=buymeacoffee&logoColor=black)](https://buymeacoffee.com/berkayay)

**[Report a bug](https://github.com/shadesofdeath/Arbitrium/issues)** · **[Star the repo](https://github.com/shadesofdeath/Arbitrium)**

</div>
