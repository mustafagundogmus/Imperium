<!--
Thanks. One change per pull request, described in terms of what the user sees or what the
machine does differently. The checklist below is the review's checklist; going through it
first is the fastest route to a merge. Delete the lines that do not apply.
-->

## What this changes

<!-- What a user sees, or what the machine does, differently. Link the issue if there is one. -->

## Why

<!-- The bug, the wrong value, the missing thing. For a catalogue change: where the correct
     behaviour is documented (Microsoft docs, the .admx, a stock machine you read it on). -->

## Tested on

<!-- Windows edition and build from the title bar, e.g. "Windows 11 Pro · 26100.4351".
     For a tweak: what the registry held before, what it held after Apply, what Revert put back. -->

## Checklist

- [ ] Builds with **zero warnings** under `-Wall -Wextra -Wshadow`
- [ ] `python tools/check-data.py` passes (for anything under `resources/data/`)
- [ ] Every new or changed string exists in **all ten languages** in `i18n.json`
- [ ] Catalogue rows: the value is documented, `off` is what Windows ships (`DELETE` for a policy that is absent by default), the type is Windows' type, no other row writes the same value, `minBuild`/`maxBuild` are set where they matter, and the description states the cost
- [ ] No program is launched by bare name and no registry key is probed through `QSettings` (see CONTRIBUTING.md)
- [ ] Nothing new walks the filesystem or waits on the network on the GUI thread
- [ ] `--self-test` still passes, if the change touches the registry layer, the journal, presets or the action engine
- [ ] `CHANGELOG.md` has an entry under the unreleased section
