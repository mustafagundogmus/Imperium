<#
.SYNOPSIS
    Captures the README's screenshots from a built Arbitrium, one PNG per page.

.DESCRIPTION
    The application already knows how to photograph itself: --screenshot <path> writes a PNG
    of the window and exits, and --theme, --typeface, --category and --search decide what is
    in it. This script is only the list of shots worth keeping, so that refreshing them for a
    release is one command rather than a careful afternoon of window arranging.

    Two things it deliberately does not do.

    It does not run in CI. Arbitrium is manifested requireAdministrator and the Overview page
    reads the live machine, so a screenshot taken on a runner would advertise the facts of an
    unactivated Azure VM. These are taken on a real machine, by the person whose machine it
    is, and committed.

    It does not photograph the Overview page unless asked. That page is the one that shows
    activation state, the BIOS and SMBIOS strings, disk serials, BitLocker status and the
    machine name — a good screenshot of it is a bad thing to publish. -IncludeOverview exists
    because the choice should be available, not because it is a good idea; look at the PNG
    before committing it.

.PARAMETER Exe
    The built Arbitrium.exe. Defaults to build\Arbitrium.exe beside the repository.

.PARAMETER OutDir
    Where the PNGs go. Defaults to docs\images, which is where README.md would reference them.

.PARAMETER IncludeOverview
    Also photograph the dashboard. Read the note above first.

.EXAMPLE
    # From an ELEVATED PowerShell, so the eight launches ask for consent once rather than
    # eight times — the executable requests administrator rights whoever starts it.
    .\tools\screenshots.ps1 -Exe .\build\Arbitrium.exe
#>

[CmdletBinding()]
param(
    [string] $Exe = (Join-Path $PSScriptRoot '..\build\Arbitrium.exe'),
    [string] $OutDir = (Join-Path $PSScriptRoot '..\docs\images'),
    [switch] $IncludeOverview
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path $Exe)) {
    Write-Error "No executable at $Exe. Build one, or pass -Exe."
    exit 1
}
$Exe = (Resolve-Path $Exe).Path

# One consent prompt instead of one per shot. The executable asks for administrator rights
# however it is started, so an unelevated shell means answering UAC for every line of the
# table below.
$elevated = ([Security.Principal.WindowsPrincipal] `
    [Security.Principal.WindowsIdentity]::GetCurrent()
).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $elevated) {
    Write-Warning 'Not elevated. Arbitrium will ask for consent once per screenshot.'
    Write-Warning 'Run this from an elevated PowerShell to be asked once.'
}

if (-not (Test-Path $OutDir)) { New-Item -ItemType Directory -Path $OutDir -Force | Out-Null }
$OutDir = (Resolve-Path $OutDir).Path

# The page ids are the ones Sidebar::isPinnedPage and the catalogue accept; --category takes
# either. Delay is per shot because the pages that go and ask the machine something need
# longer than the ones that only read the catalogue.
$shots = @(
    @{ Name = 'privacy';   Category = 'priv';       Theme = 'dark';  Delay = 1200 }
    @{ Name = 'appearance';Category = 'vis';        Theme = 'light'; Delay = 1200 }
    @{ Name = 'explorer';  Category = 'exp';        Theme = 'ocean'; Delay = 1200 }
    @{ Name = 'debloat';   Category = 'debloat';    Theme = 'dark';  Delay = 4000 }
    @{ Name = 'actions';   Category = 'actions';    Theme = 'dark';  Delay = 1200 }
    @{ Name = 'log';       Category = 'journal';    Theme = 'dark';  Delay = 1500 }
    @{ Name = 'trustedinstaller'; Category = 'tilauncher'; Theme = 'midnight'; Delay = 1500 }
    @{ Name = 'settings';  Category = 'settings';   Theme = 'sepia'; Delay = 1500 }
)

if ($IncludeOverview) {
    Write-Warning 'Including the dashboard. It shows activation, BIOS/SMBIOS, disk serials'
    Write-Warning 'and the machine name. Look at the PNG before you commit it.'
    # Longest delay of the set: the heavy reads arrive in three stages behind the page, and
    # the live chart needs a few seconds of samples before it is worth photographing.
    $shots += @{ Name = 'overview'; Category = 'ov'; Theme = 'dark'; Delay = 8000 }
}

$written = 0
foreach ($shot in $shots) {
    $path = Join-Path $OutDir "$($shot.Name).png"
    Write-Host ("{0,-18} {1,-11} {2,5} ms" -f $shot.Name, $shot.Theme, $shot.Delay)

    # Not $args, which is one of PowerShell's automatic variables: shadowing
    # it inside a script is a good way to confuse the next reader.
    $argList = @(
        '--category', $shot.Category
        '--theme', $shot.Theme
        '--screenshot', $path
        '--screenshot-delay', $shot.Delay
    )
    # -Wait, because the next launch must not race this one for the window; the app writes
    # the PNG and quits by itself once the delay is up.
    Start-Process -FilePath $Exe -ArgumentList $argList -Wait

    if (Test-Path $path) {
        $kb = [math]::Round((Get-Item $path).Length / 1KB)
        Write-Host ("  -> {0} ({1} KB)" -f $path, $kb) -ForegroundColor Green
        $written++
    } else {
        Write-Warning "  -> nothing written for $($shot.Name)"
    }
}

Write-Host ""
Write-Host "$written of $($shots.Count) written to $OutDir"
if ($written -lt $shots.Count) { exit 1 }
