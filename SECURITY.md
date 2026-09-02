# Security policy

Arbitrium runs as administrator, writes to `HKLM`, starts programs under the TrustedInstaller
account and replaces its own executable when it updates. That is four reasons to take a
report seriously, and to make reporting easy.

## Reporting a vulnerability

**Please do not open a public issue for a security problem.** Use GitHub's private
vulnerability reporting for this repository:

**https://github.com/shadesofdeath/Arbitrium/security/advisories/new**

It reaches the maintainer only, it lets us talk in private until a fix is out, and it
gives you credit in the advisory when it is published (or anonymity, if you prefer).

Include what you can of:

- the Arbitrium version (title bar, or `--version`) and the Windows build;
- what an attacker needs — a file next to the executable, a poisoned `PATH`, a crafted
  preset, a crafted release response, local standard-user access;
- steps that reproduce it, and what it achieves.

You will get an acknowledgement within a few days and a fix or a reasoned answer within
thirty. Fixes ship as a new release; the advisory is published with it.

## What counts

Anything that lets a less-privileged party act through Arbitrium's elevated token, or
through the TrustedInstaller token it can obtain. In particular:

- a program, DLL or script that Arbitrium runs or loads being resolvable from somewhere an
  attacker controls (the working directory, `PATH`, a user-writable folder);
- a preset, `.reg` import, catalogue file or update response that makes the application
  write something the user did not choose;
- the self-updater accepting a binary it should not — a redirect off GitHub, a digest it
  did not verify, a race on the file swap;
- the take-ownership shell verbs or the TrustedInstaller launcher being usable by a
  standard user without the consent prompt.

Out of scope: tweaks that do what their description says but that you consider unwise
(open an ordinary issue — the description may need a stronger warning), and SmartScreen's
*unknown publisher* dialog, which is explained in the README.

## Supported versions

Only the [latest release](https://github.com/shadesofdeath/Arbitrium/releases/latest)
receives fixes. The application checks for a newer release on launch (at most once a day,
and only if that setting is on) and can replace itself; there is no reason to stay on an
older one.

## Verifying a release

The executable is not Authenticode-signed — the README explains why — so the two checks
that exist are the ones to make:

```powershell
# The download is whole: compare against the .sha256 published beside it.
Get-FileHash Arbitrium-vX.Y.Z-win64.exe -Algorithm SHA256

# The bytes came from this repository's release workflow, at that commit.
gh attestation verify Arbitrium-vX.Y.Z-win64.exe --repo shadesofdeath/Arbitrium
```

The attestation is signed by GitHub at publish time and binds the file's digest to the
repository, the commit and the workflow run that built it. A file that fails it is not one
this project produced.
