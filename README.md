<div align="center">

# Arbitrium

**Your Windows. Your rules.**

Windows ships with opinions about your privacy, your bandwidth, and what belongs on your Start menu.
Arbitrium is where you overrule them — 391 of them, one switch at a time.

[![Windows 10 | 11](https://img.shields.io/badge/Windows-10%20%7C%2011-0078D4?style=for-the-badge&logo=windows11&logoColor=white)](#)
[![Portable](https://img.shields.io/badge/Install-Not%20Required-2ea44f?style=for-the-badge)](#)
[![Languages](https://img.shields.io/badge/Languages-10-8957e5?style=for-the-badge)](#every-word-in-ten-languages)
[![License](https://img.shields.io/badge/License-Open%20Source-d2a75a?style=for-the-badge)](#)

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

### ⚙️ 391 tweaks that actually apply here

Not a generic list scraped from a 2016 forum post. Arbitrium checks each tweak against **your**
Windows build. A setting Microsoft retired three versions ago isn't quietly presented as a working
switch — it's shown greyed out, with the reason. Honesty over a longer feature list.

| | |
|---|---|
| **System** · 63 | Core behaviour, lock screen, sign-in, shell policy |
| **File Explorer** · 57 | Extensions, panes, Quick access, launch target |
| **Privacy** · 41 | Telemetry, diagnostics, advertising ID, activity history |
| **Visual Effects** · 35 | Animations, transparency, shadows, thumbnails |
| **Memory & CPU** · 32 | Paging, caching, prefetch, scheduling |
| **Windows Update** · 29 | Deferral, drivers, delivery optimisation, restart behaviour |
| **Network & Internet** · 27 | DNS, IPv6, Teredo, discovery, metered connections |
| **Security hardening** · 23 | SMB, LSA, PowerShell logging, legacy protocols |
| **Cleanup** · 22 | Storage Sense, temp, component store, history |
| **Startup** · 20 | Everything that runs when you sign in |
| **Context menu** · 20 | Take Ownership, Services, cascading power plans, and more |
| **Advanced** · 13 | The ones with sharp edges |
| **Power management** · 9 | Sleep, hibernation, USB selective suspend |

### 🔧 Every Windows service, in one place

All **329** services on your machine, each as a four-position control — *Automatic*, *Delayed*,
*Manual*, *Disabled*. Services Windows genuinely cannot boot without are **locked**, with the reason
stated. You can be reckless somewhere else.

### 📦 Debloat that reads your actual machine

The Apps page doesn't guess from a hardcoded list of app names. It asks Windows which packages were
**preinstalled with your image**, then reads each one's real icon out of its own package folder.
What you see is what you actually have — real names, real logos, real version numbers.

Removal takes the app off **every account** on the machine and deprovisions it, so it doesn't come
back the next time someone new signs in. Shared runtimes and anything Windows itself marks
protected are shown with a padlock instead of a button — visible, but not a foot-gun.

### ⚡ 18 one-shot actions

Remove Edge, OneDrive, or Widgets. Empty temp folders and the component store. Rebuild the icon
cache. Switch DNS to Cloudflare, Google, or Quad9 in one click. Add the hidden **Ultimate
Performance** power plan. Each one tells you up front whether it can be undone — and shows you the
script.

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

### 🛡️ A safety net, on by default

Create a System Restore point straight from the app before you start. Because "reversible" is a
promise worth backing twice.

---

## Every word, in ten languages

Not just the buttons — **everything**. All 391 tweak names and descriptions, every service state,
every action, every label on the dashboard, every line in the log.

🇹🇷 Türkçe · 🇬🇧 English · 🇩🇪 Deutsch · 🇫🇷 Français · 🇪🇸 Español · 🇮🇹 Italiano · 🇵🇹 Português · 🇵🇱 Polski · 🇷🇺 Русский · 🇸🇦 العربية

Switch language from Settings and the entire interface changes instantly. No restart. Not even a flicker.

---

## Looks like it belongs on your desktop

Because a tool you'll actually open should be worth looking at.

- **Dark and light**, both designed rather than inverted
- **Accent colours** you can pick, applied across the whole interface
- **Six typefaces** to choose from
- **Four text sizes**, because not everyone runs a 4K panel at 100%
- A **frameless window** with smooth scrolling and animation everywhere it earns its place

---

## Getting started

1. Download **`Arbitrium-vX.Y.Z-win64.exe`** from the [latest release](https://github.com/shadesofdeath/Arbitrium/releases/latest).
   That single file *is* the application — Qt is linked into it, so there is nothing to unpack
   and nothing to put beside it. (The `.zip` next to it holds the same executable and this
   README, for anyone whose browser dislikes a bare `.exe`.)
2. Run it. **No installation, no setup wizard, no bundled toolbar.** One executable.
3. On first launch, pick your language and your look.
4. Flip what you want. Press **Apply** when you mean it.

> Arbitrium needs administrator rights — it edits system-level settings, and pretending otherwise
> would be dishonest. It asks once, at launch.

---

## Fair warning

This tool changes real Windows settings, and some of them matter. It gives you a log, per-change
revert, restore points, and a confirmation on everything destructive — but it also assumes you're
an adult who read the description.

Start with a restore point. Change things in small batches. The Log page is right there if
something isn't what you expected.

---

<div align="center">

### Built by [ShadesOfDeath](https://github.com/shadesofdeath)

If Arbitrium saved you an afternoon of registry spelunking, you can say thanks:

[![Buy Me a Coffee](https://img.shields.io/badge/Buy%20Me%20a%20Coffee-berkayay-FFDD00?style=for-the-badge&logo=buymeacoffee&logoColor=black)](https://buymeacoffee.com/berkayay)

**[Report a bug](https://github.com/shadesofdeath/Arbitrium/issues)** · **[Star the repo](https://github.com/shadesofdeath/Arbitrium)**

</div>
