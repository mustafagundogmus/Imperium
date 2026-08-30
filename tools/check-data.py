#!/usr/bin/env python3
# check-data.py — reads resources/data/ the way the app reads it, and fails the build when
# the two disagree.
#
# Everything under resources/data/ is compiled into the executable and taken apart by hand
# at startup: Catalog::load(), ActionCatalog's constructor and Locale::load() each walk the
# JSON field by field, and every one of them is written to keep going rather than to
# complain. That is the right behaviour at run time — a user with a bad build should still
# get a window — and it is exactly why nothing here is ever caught by the compiler, by the
# link, or by anything on screen.
#
# Two examples, because they are the reason this file exists:
#
#   * A duplicate tweak id is not an error to Catalog::load(). The index is built last
#     (catalog.cpp:459) and the later row simply wins, so two switches share one state:
#     flipping either moves the other's control and writes only one of the two registry
#     sets. Nothing logs it.
#   * An option whose `data` array is one value short is not an error either.
#     catalog.cpp:386 resizes it to the number of registry entries, the missing position
#     becomes an empty QString, and Registry::write turns an empty QString into a DWORD 0
#     (registry.cpp:229-232) on a real key on somebody's machine. That check is the most
#     important one in this file — and it has to cover the switches too, not only the
#     rows that list their positions. A switch's two positions are the `off` and `on`
#     already written on each registry entry (catalog.cpp:433-439); they reach
#     Registry::write through exactly the same call, so `"on": ""` on a DWORD is the same
#     silent zero arrived at from the other direction. Switches are 344 of the 391 rows.
#
# So this runs on every push, on ubuntu-latest, in about a second: no compiler, no Qt, no
# third-party package, standard library only. Where the schema lives in the C++ rather than
# in the JSON — the language list, the accepted hive spellings, the risky-service table —
# it is read out of src/ rather than copied here, for the same reason services.cpp derives
# `svc.risk.<Key>` from the table above it: a rule copied into a second place is a rule
# that drifts.
#
# ERROR fails the run. WARN and INFO never do. The orphan report in particular is as often
# a list of keys reached by runtime concatenation as it is a list of keys nobody uses, and
# a check that talks somebody into deleting one of those is worse than no check at all.

import argparse
import collections
import json
import re
import sys
from pathlib import Path

# --------------------------------------------------------------------------------------
# The parts of the schema that live in the C++.

# Locale::tr and Locale::content are the only two doors into the translation table
# (i18n.cpp:107 and i18n.cpp:128), and the key is the first thing either of them is handed.
# key_expressions() reads the first argument of each call and keys_in() decides which of
# its literals are keys — see both for why that is not a single regex.
#
# SearchField::setPlaceholderKey is the third name here because it is one of those doors
# with a delay on it: it stores a key and hands it to Locale::tr on every language change
# (searchfield.cpp:92, :82), so the literal at the call site is every bit as much a key as
# the ones inside a tr(). The God Mode page's "godmode.search.placeholder" arrived through
# it, and until this pattern was listed here it was a user-visible string that no check in
# this file could see.
TR_CALL = re.compile(r"(?:Locale::(?:tr|content)|setPlaceholderKey)\(")
QSTRING_LITERAL = re.compile(r'QStringLiteral\("([^"]*)"\)')

# The seven literals that are not keys. Each is concatenated with something the table cannot
# see — a catalogue id, a section heading, an option label — so the literal itself will
# never resolve, and a check that did not know them would have failed on the day it was
# written:
#
#   "tweak."          catalog.cpp:78, :87   + <tweak id> + ".name" / ".desc"
#   "opt."            catalog.cpp:42        + the option's own Turkish label
#   "section."        catalog.cpp:94,
#                     action.cpp:32         + the section's own Turkish heading
#   "category."       mainwindow.cpp:322    + the category id
#   "action."         action.cpp:15, :20,
#                     :27                   + <action id> + ".name" / ".desc" / ".note"
#   "godmode."        godmodepage.cpp       + the settings link's id
#   "godmode.group."  godmodepage.cpp       + the link group's id
#
# The keys those calls actually build are checked instead, from the data files, further
# down. Keep this set closed: an eighth prefix appearing here means a new family of keys
# that nothing is checking.
RUNTIME_PREFIXES = frozenset(["tweak.", "opt.", "section.", "category.", "action.",
                              "godmode.", "godmode.group."])

# Registry::write's ladder of type names (registry.cpp:228-270). SZ is not in the ladder —
# it is the fall-through, which is the whole reason unknown type strings have to be caught
# here: a typo does not fail a write, it silently changes the value's kind to REG_SZ, and
# Windows reading a REG_SZ where it wanted a REG_DWORD ignores the tweak entirely.
REGISTRY_TYPES = frozenset(["DWORD", "QWORD", "SZ", "EXPAND_SZ", "MULTI_SZ", "BINARY"])

# Registry::write never sees these two: the engine takes them as instructions rather than
# as data (tweakengine.cpp:311-317) and calls removeKey() or remove() instead, so neither
# is malformed even where the type says DWORD. See registry.h:21-28.
#
# Folded rather than compared literally because isDelete() and isDeleteKey()
# (tweakengine.cpp:30-38) compare Qt::CaseInsensitive: "delete" on a DWORD entry is a
# deletion the app performs, not the silent zero everything else in check 2 is hunting
# for, and flagging it would be this file failing a build over a row that works.
SENTINELS = frozenset(["delete", "delete_key"])

# What QString::toUInt() and toULongLong() will actually parse. Decimal digits and nothing
# else: "0x1" and "-1" both come back as 0 without complaint, which is the failure this
# check is looking for.
DECIMAL = re.compile(r"\A[0-9]+\Z")

# The token an action's script prints as its last line for actionpage.cpp:57-61 to turn
# into a sentence. Everything after a second `|` is an argument to .arg(), not part of the
# key.
RESULT_TOKEN = re.compile(r"ARB\|([A-Za-z0-9_]+)")

# The two id prefixes isSynthesised() (catalog.cpp:69) reserves for rows this app builds
# from the machine rather than from catalog.json. A catalogued id must never borrow one.
SYNTHESISED_PREFIXES = ("svc-", "startup-")


def read_json(path, report):
    """Loads one data file, or records why the run cannot continue."""
    try:
        loaded = json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError:
        report.fatal("%s is missing; resources.qrc compiles it into the binary" % path.name)
        return None
    except UnicodeDecodeError as err:
        # QJsonDocument::fromJson takes UTF-8 and nothing else. A file saved as UTF-16 or
        # as a code page parses as garbage there rather than as JSON.
        report.fatal("%s is not UTF-8: %s at byte %d" % (path.name, err.reason, err.start))
        return None
    except OSError as err:
        report.fatal("%s cannot be read: %s" % (path.name, err.strerror or err))
        return None
    except json.JSONDecodeError as err:
        # Qt's own parser refuses the file the same way and the app comes up empty, so this
        # is the one failure that is visible at run time. It is still cheaper to find here.
        report.fatal("%s is not valid JSON: %s (line %d, column %d)"
                     % (path.name, err.msg, err.lineno, err.colno))
        return None

    # All three files are read through QJsonDocument::object(), which hands back an *empty*
    # object for a document whose top level is an array or a bare value. Nothing throws and
    # nothing logs; the app simply comes up with no tweaks, no actions or no translations
    # at all. Stopping here rather than letting the checks below walk a list is also what
    # keeps this script reporting instead of showing somebody a Python traceback.
    if not isinstance(loaded, dict):
        report.fatal("%s has a %s at its top level, not an object; QJsonDocument::object() "
                     "hands back an empty object for that and the app comes up empty"
                     % (path.name, type(loaded).__name__))
        return None
    return loaded


def read_text(path, report, why):
    """One source file as text, or a fatal naming it. Nothing here parses non-UTF-8 C++."""
    try:
        return path.read_text(encoding="utf-8")
    except FileNotFoundError:
        report.fatal("%s is missing; %s is read out of it" % (path.name, why))
    except UnicodeDecodeError as err:
        report.fatal("%s is not UTF-8: %s at byte %d" % (path.name, err.reason, err.start))
    except OSError as err:
        report.fatal("%s cannot be read: %s" % (path.name, err.strerror or err))
    return None


def read_source_table(path, opening, report, what, closer="};"):
    """The text between `opening` and the first `closer` after it.

    Crude, and deliberately so — a Python script has no business parsing C++, and the three
    regions this reads are each a handful of literals with no nesting. `closer` exists
    because two of them are initialisers, closed by `};`, and the third is a function body,
    closed by a brace in column one: taking the `};` for that one would run past the end of
    the function and pick up whatever the next initialiser in the file happened to contain.

    Returning None rather than an empty set on a miss is the point. A table that has been
    renamed or reshaped has to stop the run loudly, because a check that silently examines
    nothing is indistinguishable from a check that passed.
    """
    text = read_text(path, report, what)
    if text is None:
        return None
    head = text.find(opening)
    if head < 0:
        report.fatal("%s no longer contains `%s`, so %s cannot be read; teach "
                     "tools/check-data.py the new shape" % (path.name, opening, what))
        return None
    tail = text.find(closer, head + len(opening))
    if tail < 0:
        report.fatal("`%s` in %s is not closed by `%s`"
                     % (opening, path.name, closer.strip()))
        return None
    return text[head + len(opening):tail]


def read_sources(repo, report):
    """Every translation unit and header under src/, in a stable order, read once.

    Checks 8 and 9 both walk all of src/, and reading a hundred-odd files twice to answer
    two questions is the sort of thing that turns a one-second check into one nobody runs.

    An empty list is a fatal for the same reason read_source_table() refuses to return an
    empty set: `--repo` pointed somewhere with no src/ would otherwise report "0 literals,
    PASS" and look exactly like a clean run.
    """
    files = sorted(p for p in repo.glob("src/**/*") if p.suffix in (".cpp", ".h"))
    if not files:
        report.fatal("no .cpp or .h files under %s/src; checks 8 and 9 would examine "
                     "nothing and report a pass" % repo.as_posix())
        return []
    out = []
    for path in files:
        text = read_text(path, report, "the list of keys src/ asks for")
        if text is None:
            return []
        out.append((path.relative_to(repo).as_posix(), text))
    return out


# --------------------------------------------------------------------------------------
# Reporting.
#
# A CI log is read by somebody who wants to know which check failed and what the first
# offender was, and a validator that prints one line per missing translation key prints
# fifteen thousand of them. Everything below groups, counts, and shows the first few.

MAX_SHOWN = 20


class Check:
    def __init__(self, number, title):
        self.number = number
        self.title = title
        self.notes = []      # the counts, printed whether or not anything failed
        self.entries = []    # (level, kind, file, where, message)

    def note(self, text):
        self.notes.append(text)

    def error(self, kind, data_file, where, message):
        self.entries.append(("ERROR", kind, data_file, where, message))

    def warn(self, kind, data_file, where, message):
        self.entries.append(("WARN", kind, data_file, where, message))

    def info(self, kind, data_file, where, message):
        self.entries.append(("INFO", kind, data_file, where, message))

    @property
    def verdict(self):
        for level in ("ERROR", "WARN", "INFO"):
            if any(e[0] == level for e in self.entries):
                return {"ERROR": "FAIL", "WARN": "WARN", "INFO": "INFO"}[level]
        return "PASS"


class Report:
    def __init__(self):
        self.checks = []
        self.fatal_message = None

    def check(self, number, title):
        c = Check(number, title)
        self.checks.append(c)
        return c

    def fatal(self, message):
        """A file the rest of the run cannot proceed without."""
        if self.fatal_message is None:
            self.fatal_message = message

    def emit(self):
        if self.fatal_message:
            print("FATAL  %s" % self.fatal_message)
            return 1

        failed = False
        for c in self.checks:
            print("%2d  %-4s  %s" % (c.number, c.verdict, c.title))
            for note in c.notes:
                print("          %s" % note)

            # Capped per kind rather than per check: check 4 alone reports on categories,
            # sections, tweaks and registry entries, and twenty of one kind must not hide
            # the single instance of another.
            by_kind = collections.OrderedDict()
            for entry in c.entries:
                by_kind.setdefault(entry[1], []).append(entry)
            for kind, entries in by_kind.items():
                for level, _kind, data_file, where, message in entries[:MAX_SHOWN]:
                    print("          %-5s %s  %s: %s" % (level, data_file, where, message))
                if len(entries) > MAX_SHOWN:
                    # Named in parentheses rather than run into the sentence: the kinds are
                    # a mix of singular and plural nouns and "and 36 more orphan" is the
                    # sort of thing that makes a reader distrust the rest of the line.
                    print("          ... and %d more (%s)"
                          % (len(entries) - MAX_SHOWN, kind))
            if c.verdict == "FAIL":
                failed = True
            print("")

        errors = sum(1 for c in self.checks for e in c.entries if e[0] == "ERROR")
        if failed:
            checks = sum(1 for c in self.checks if c.verdict == "FAIL")
            print("FAILED — %d error%s across %d check%s"
                  % (errors, "" if errors == 1 else "s",
                     checks, "" if checks == 1 else "s"))
            return 1
        print("OK — %d checks, no errors" % len(self.checks))
        return 0


# --------------------------------------------------------------------------------------
# Walking the catalogue the way Catalog::load() does.
#
# Every accessor below tolerates the wrong JSON type instead of raising, because a
# validator that ends in a Python traceback tells the person who broke catalog.json
# nothing about what they broke. Qt is just as tolerant and just as quiet: toArray() on a
# string gives an empty array and toObject() on a number gives an empty object, so a
# malformed node is a row that silently disappears from the app. Check 4 is where those
# are reported; the rest simply skip what it will name.

def objects(value):
    """The object elements of a JSON array. Anything else in it is skipped.

    `for (const QJsonValue &v : array) { const QJsonObject o = v.toObject(); … }` is what
    Catalog::load() does with every one of these arrays, and toObject() on a string or a
    number is an empty object rather than an error.
    """
    if not isinstance(value, list):
        return []
    return [v for v in value if isinstance(v, dict)]


def numbered_reg(tweak):
    """(position in the file, entry) for every registry entry that is an object.

    The position is the one in catalog.json rather than the one in the vector Catalog::load()
    ends up with, because it is the number somebody has to count to in an editor.
    """
    raw = tweak.get("reg")
    if not isinstance(raw, list):
        return []
    return [(i, r) for i, r in enumerate(raw) if isinstance(r, dict)]


def effective_reg(tweak):
    """The registry entries that survive catalog.cpp:362, in order.

    An entry with an empty hive or path is dropped there, and everything downstream — the
    option arrays, the range generator, the synthesised switch — is sized against what is
    left. Comparing an option's data against the raw count would therefore agree with a
    file that is already broken.
    """
    return [r for r in objects(tweak.get("reg"))
            if isinstance(r.get("hive"), str) and r["hive"]
            and isinstance(r.get("path"), str) and r["path"]]


def option_data(option):
    """One option's data as the list Catalog::load() builds from it.

    catalog.cpp:378-385 accepts a bare string as well as an array — it is how a tweak that
    owns a single key is written — so a string counts as one value, not as its length.

    A value of the wrong JSON type becomes an empty string rather than disappearing,
    because that is what QJsonValue::toString() does with a number or a bool: `"data": 0`
    is not the number zero to the parser, it is no data at all.
    """
    data = option.get("data")
    if isinstance(data, list):
        return [d if isinstance(d, str) else "" for d in data]
    if isinstance(data, str):
        return [data]
    if data is None:
        return []
    return [""]


def tweak_shape(tweak):
    """Which of catalog.cpp:369, :391 and :432 this tweak takes — the three are exclusive.

    Written once and shared by checks 2, 4 and 5 because the boundaries are not where they
    read: `options` is the choice branch only at *two or more* entries, so a one-entry
    `options` next to no `range` is a switch as far as the parser is concerned, and a
    check that tested `if tweak.get("options")` would quietly excuse it from every rule a
    switch has to keep.
    """
    options = tweak.get("options")
    if isinstance(options, list) and len(options) >= 2:
        return "choice"
    if "range" in tweak:
        return "range"
    return "switch"


def tweak_label(tweak):
    """How a row is named in a report: its own id, or a stand-in where it has none."""
    tweak_id = tweak.get("id")
    return tweak_id if isinstance(tweak_id, str) and tweak_id else "<no id>"


def walk(catalog):
    """Yields (category, section, tweak) for every catalogued tweak that is an object."""
    for category in objects(catalog.get("categories")):
        for section in objects(category.get("sections")):
            for tweak in objects(section.get("tweaks")):
                yield category, section, tweak


# --------------------------------------------------------------------------------------
# The checks.

def check_ids(report, catalog):
    c = report.check(1, "catalog.json — tweak ids are unique and are not synthesised")
    # An id of the wrong JSON type is "" to QJsonValue::toString(), which is the same row
    # check 4 reports as having no id; here it only has to not be counted as one.
    ids = [t["id"] for _cat, _sec, t in walk(catalog) if isinstance(t.get("id"), str)]
    counts = collections.Counter(ids)
    for tweak_id, n in sorted(counts.items()):
        if not tweak_id:
            continue
        if n > 1:
            # Catalog::load() indexes into m_byId last (catalog.cpp:459), so the last row
            # with this id wins and the earlier one becomes unreachable through
            # Catalog::tweak(). Both rows are still drawn, and both draw from the same
            # state: flipping one moves the other's control and applies only one of the two
            # registry sets. The startup ids were given their hive and approval key for
            # precisely this reason (catalog.cpp:264-268).
            c.error("duplicate ids", "catalog.json", tweak_id,
                    "appears %d times; the last one wins the id index and the rows share "
                    "one switch" % n)
    for tweak_id in sorted(set(ids)):
        if tweak_id.startswith(SYNTHESISED_PREFIXES):
            # isSynthesised() (catalog.cpp:69) sends anything with these prefixes past the
            # translation table entirely, on the grounds that its text is Windows' own. A
            # catalogued row that borrowed one would show its Turkish name in all ten
            # languages, which is the mirror of the "boot-" bug the comment above that
            # function records.
            c.error("synthesised prefix", "catalog.json", tweak_id,
                    "starts with a prefix isSynthesised() reserves, so this row would skip "
                    "its own tweak.<id>.name lookup")
    # Printed whether or not anything failed, like every other check's count: the run that
    # hides its numbers the moment something goes wrong is the run you cannot compare
    # against the last one.
    c.note("%d tweak ids, %d distinct" % (len(ids), len(counts)))


def writes_a_number(kind):
    """True where Registry::write parses the data instead of copying it."""
    return kind in ("DWORD", "QWORD")


def check_number(c, kind, value, tweak_id, where):
    """One value that will reach Registry::write against a DWORD or QWORD entry.

    QString::toUInt() (registry.cpp:230) does not report a parse failure; it returns 0. An
    empty string, a hex spelling and a negative number all write zero to the key — the
    same fault a short data array causes, arrived at by hand.
    """
    if value.lower() in SENTINELS or DECIMAL.match(value):
        return
    c.error("unparseable number", "catalog.json", "%s %s" % (tweak_id, where),
            "%s wants a decimal number, data is %r; toUInt() would write 0" % (kind, value))


def check_option_data(report, catalog):
    c = report.check(2, "catalog.json — option data lines up with the registry entries")
    positions = 0
    switch_positions = 0
    for _cat, _sec, tweak in walk(catalog):
        tweak_id = tweak_label(tweak)
        regs = effective_reg(tweak)
        shape = tweak_shape(tweak)

        if shape == "switch":
            # A switch lists no positions: catalog.cpp:433-439 builds its two out of each
            # entry's own `off` and `on`, which then go to Registry::write through the
            # same call an option's data does. So the DWORD rule applies to them exactly
            # as it does above — and to 344 of the 391 rows rather than to 23.
            #
            # An absent `off` or `on` is left to check 4: it is the same fault, and one
            # missing field reported twice in two different voices is how a CI log stops
            # being read.
            surviving = {id(r) for r in regs}
            for r_index, reg in numbered_reg(tweak):
                # An entry catalog.cpp:362 drops never reaches Registry::write at all, so
                # its data is not what is wrong with it — check 4 reports the empty hive
                # or path that dropped it.
                if id(reg) not in surviving:
                    continue
                kind = str(reg.get("type", "")).strip().upper()
                if not writes_a_number(kind):
                    continue
                for field in ("off", "on"):
                    if not isinstance(reg.get(field), str):
                        continue
                    switch_positions += 1
                    check_number(c, kind, reg[field], tweak_id,
                                 "reg %d `%s`" % (r_index, field))
            continue

        if shape != "choice":
            continue
        options = tweak["options"]
        for index, option in enumerate(options):
            positions += 1
            if not isinstance(option, dict):
                # toObject() gives an empty object, whose data is one empty string padded
                # out to the registry entries. Check 4 names it; nothing more to say here.
                continue
            data = option_data(option)
            if len(data) != len(regs):
                # This is the one. catalog.cpp:386 calls resize() on the vector, which pads
                # a short array with default-constructed QStrings and truncates a long one.
                # A padded position is an empty string that reaches Registry::write against
                # a real hive, path and value — and for the DWORD that most of this
                # catalogue is, an empty string is the number 0. Nothing warns, nothing
                # logs, and the row reads as applied.
                c.error("length mismatch", "catalog.json",
                        "%s option %d" % (tweak_id, index),
                        "data has %d value%s, reg has %d; catalog.cpp:386 pads the "
                        "difference and writes it" % (len(data), "" if len(data) == 1 else "s",
                                                      len(regs)))
            for i, value in enumerate(data[:len(regs)]):
                kind = str(regs[i].get("type", "")).strip().upper()
                if writes_a_number(kind):
                    check_number(c, kind, value, tweak_id,
                                 "option %d value %d" % (index, i))
    c.note("%d listed choice positions checked against their reg arrays, and %d switch "
           "position%s against the DWORD rule"
           % (positions, switch_positions, "" if switch_positions == 1 else "s"))


def read_hives(repo, report):
    """The hive spellings hiveFromString() accepts, read out of its own body.

    Both the short forms and the HKEY_ ones, because the catalogue may be written either
    way and registry.cpp:75-84 takes both. Read rather than listed for the same reason the
    language table is: this is a closed set the C++ owns.
    """
    body = read_source_table(repo / "src/registry.cpp",
                             "Hive hiveFromString(const QString &name)", report,
                             "the accepted hive spellings", closer="\n}")
    if body is None:
        return frozenset()
    hives = frozenset(re.findall(r'QLatin1String\("([A-Z_]+)"\)', body))
    if not hives:
        report.fatal("hiveFromString() in src/registry.cpp names no hives the validator "
                     "could find; teach tools/check-data.py the new shape")
    return hives


def check_hives_and_types(report, catalog, hives):
    c = report.check(3, "catalog.json — hive and type strings are ones the app knows")
    entries = 0
    for _cat, _sec, tweak in walk(catalog):
        tweak_id = tweak_label(tweak)
        for i, reg in numbered_reg(tweak):
            entries += 1
            hive = str(reg.get("hive", "")).strip().upper()
            if hive and hive not in hives:
                # hiveFromString returns Hive::Invalid, nativeHive() hands back nullptr and
                # the write fails with err.badHive — every single time the row is touched,
                # for every user, with a message that says nothing about which entry is
                # wrong (registry.cpp:213-217).
                c.error("unknown hive", "catalog.json", "%s reg %d" % (tweak_id, i),
                        "hive %r is not one of %s" % (reg.get("hive"),
                                                      ", ".join(sorted(hives))))
            kind = str(reg.get("type", "")).strip().upper()
            if kind and kind not in REGISTRY_TYPES:
                c.error("unknown type", "catalog.json", "%s reg %d" % (tweak_id, i),
                        "type %r is not one of %s; Registry::write falls through to REG_SZ"
                        % (reg.get("type"), ", ".join(sorted(REGISTRY_TYPES))))
    c.note("%d registry entries, %d hive spellings accepted by hiveFromString()"
           % (entries, len(hives)))


def require_text(c, kind, where, node, field, consequence=""):
    """One field Catalog::load() reads with toString(), which is "" for anything else.

    A number, a bool or an array where a string was meant is not a parse error to Qt and
    not a visible one either: the field simply comes back empty, and the row draws a blank
    where its name should be. Reported with what is actually there, because "no `name`" on
    a row whose name is the number 0 sends the reader looking for a field that is present.
    """
    value = node.get(field)
    if isinstance(value, str) and value:
        return True
    if value is None:
        c.error(kind, "catalog.json", where, "no `%s`%s" % (field, consequence))
    elif not isinstance(value, str):
        c.error(kind, "catalog.json", where,
                "`%s` is %r; QJsonValue::toString() reads that as empty%s"
                % (field, value, consequence))
    else:
        c.error(kind, "catalog.json", where, "`%s` is empty%s" % (field, consequence))
    return False


def check_required_fields(report, catalog):
    c = report.check(4, "catalog.json — every category, section, tweak and registry entry "
                        "carries what the parser reads")

    categories = catalog.get("categories")
    if not isinstance(categories, list) or not categories:
        c.error("catalogue", "catalog.json", "categories",
                "no categories array; Catalog::load() would build an empty catalogue")
        return

    counts = collections.Counter()
    for index, category in enumerate(categories):
        counts["category"] += 1
        if not isinstance(category, dict):
            c.error("category", "catalog.json", "category %d" % index,
                    "is %r, not an object; toObject() makes it an empty category with no "
                    "id, which no sidebar row can reach" % (category,))
            continue
        where = category.get("id") if isinstance(category.get("id"), str) else ""
        where = where or "category %d" % index
        for field in ("id", "name", "icon"):
            # The id is what mutableCategory() and Catalog::category() match on and what
            # "category." is completed with; the icon is the 12x12 SVG path the sidebar
            # draws. A category with no id cannot be reached at all.
            require_text(c, "category", where, category, field)

        # An empty sections array is not a fault: `ov` is the overview page and `svc` is
        # filled in by appendServices() at load time.
        sections = category.get("sections")
        for s_index, section in enumerate(sections if isinstance(sections, list) else []):
            counts["section"] += 1
            s_where = "%s / section %d" % (where, s_index)
            if not isinstance(section, dict):
                c.error("section", "catalog.json", s_where,
                        "is %r, not an object; it becomes an untitled section with no "
                        "tweaks" % (section,))
                continue
            # Section::displayTitle() looks up "section." + title (catalog.cpp:94), so an
            # empty title is both a blank heading and a lookup of the bare prefix.
            require_text(c, "section", s_where, section, "title")
            tweaks = section.get("tweaks")
            if not tweaks:
                c.error("section", "catalog.json", s_where, "no `tweaks`")
            elif not isinstance(tweaks, list):
                c.error("section", "catalog.json", s_where,
                        "`tweaks` is %r, not an array; toArray() empties it and the section "
                        "draws a heading with nothing under it" % (tweaks,))

            for t_index, tweak in enumerate(tweaks if isinstance(tweaks, list) else []):
                counts["tweak"] += 1
                t_where = "%s / tweak %d" % (s_where, t_index)
                if not isinstance(tweak, dict):
                    c.error("tweak", "catalog.json", t_where,
                            "is %r, not an object; the row is drawn with no name, no "
                            "description and nothing to write" % (tweak,))
                    continue
                if isinstance(tweak.get("id"), str) and tweak["id"]:
                    t_where = tweak["id"]
                for field in ("id", "name", "desc"):
                    require_text(c, "tweak", t_where, tweak, field)
                if not tweak.get("reg"):
                    c.error("tweak", "catalog.json", t_where,
                            "no `reg`; the row would draw a switch that writes nothing")
                elif not isinstance(tweak.get("reg"), list):
                    c.error("tweak", "catalog.json", t_where,
                            "`reg` is %r, not an array; toArray() empties it and the row "
                            "writes nothing" % (tweak["reg"],))

                shape = tweak_shape(tweak)
                options = tweak.get("options")
                if isinstance(options, list) and len(options) == 1:
                    # catalog.cpp:369 takes the options branch only at two or more. A single
                    # listed position is silently ignored and the tweak becomes a switch
                    # over the reg entries' own on/off — which is not what the author of a
                    # one-option list meant, and nothing says so.
                    c.error("tweak", "catalog.json", t_where,
                            "`options` has one entry; catalog.cpp:369 ignores it and falls "
                            "back to a switch")
                elif options is not None and not isinstance(options, list):
                    c.error("tweak", "catalog.json", t_where,
                            "`options` is %r, not an array; toArray() empties it and the "
                            "row falls back to a switch" % (options,))
                if shape == "choice":
                    for o_index, option in enumerate(options):
                        o_where = "%s option %d" % (t_where, o_index)
                        if not isinstance(option, dict):
                            c.error("option", "catalog.json", o_where,
                                    "is %r, not an object; the position draws empty and "
                                    "writes an empty string to every key" % (option,))
                            continue
                        require_text(c, "option", o_where, option, "label",
                                     "; the segmented control would draw an empty position")
                    default = tweak.get("default", 0)
                    # isinstance(True, int) is true in Python and a bool is not a number to
                    # QJsonValue::toInt() either — it returns the fallback, so `"default":
                    # true` is position 0 rather than position 1.
                    if isinstance(default, bool) or not isinstance(default, int) \
                            or not 0 <= default < len(options):
                        # qBound (catalog.cpp:389) clamps rather than complains, so an
                        # out-of-range default quietly becomes position 0 — which the rest
                        # of the app reads as "what Windows ships".
                        c.error("tweak", "catalog.json", t_where,
                                "`default` is %r, outside 0..%d; qBound clamps it silently"
                                % (default, len(options) - 1))
                    if "range" in tweak:
                        c.error("tweak", "catalog.json", t_where,
                                "carries both `options` and `range`; catalog.cpp:369 takes "
                                "the options branch and the range is never read")

                raw_reg = tweak.get("reg") if isinstance(tweak.get("reg"), list) else []
                for r_index, reg in enumerate(raw_reg):
                    r_where = "%s reg %d" % (t_where, r_index)
                    if not isinstance(reg, dict):
                        c.error("reg", "catalog.json", r_where,
                                "is %r, not an object; catalog.cpp:362 drops it for having "
                                "no hive and every option's data shifts one place left"
                                % (reg,))
                        continue
                    # `value` is deliberately not required: an empty value name is the key's
                    # own default value, which targetSummary() spells out (catalog.cpp:22).
                    for field in ("hive", "path"):
                        require_text(c, "reg", r_where, reg, field,
                                     "; catalog.cpp:362 drops the entry and every option's "
                                     "data shifts one place left")
                    # `type` does not drop the entry — nothing checks it until the write,
                    # where an empty string falls out of Registry::write's ladder into the
                    # REG_SZ branch (registry.cpp:260) and the value is created with the
                    # wrong kind on a real machine.
                    require_text(c, "reg", r_where, reg, "type",
                                 "; Registry::write falls through to REG_SZ")
                    if shape == "switch":
                        # A switch has no listed positions: its two are assembled from each
                        # entry's own `off` and `on` (catalog.cpp:433-439). Missing keys are
                        # a fault; an empty *string* is not — writing one as SZ is how the
                        # classic context menu is switched on (registry.cpp:261-266).
                        for field in ("on", "off"):
                            if field not in reg:
                                c.error("reg", "catalog.json", r_where,
                                        "a switch, but the entry has no `%s`" % field)
                            elif not isinstance(reg[field], str):
                                c.error("reg", "catalog.json", r_where,
                                        "`%s` is %r; toString() reads that as empty, and an "
                                        "empty DWORD is the number 0" % (field, reg[field]))

    c.note("%d categories, %d sections, %d tweaks"
           % (counts["category"], counts["section"], counts["tweak"]))


def check_ranges(report, catalog):
    c = report.check(5, "catalog.json — range defaults land on the grid the slider generates")
    ranges = 0
    for _cat, _sec, tweak in walk(catalog):
        if tweak_shape(tweak) != "range":
            continue
        ranges += 1
        tweak_id = tweak_label(tweak)
        spec = tweak["range"]
        if not isinstance(spec, dict):
            c.error("range", "catalog.json", tweak_id,
                    "`range` is %r, not an object; toObject() empties it and the slider is "
                    "generated as the single position 0" % (spec,))
            continue

        def whole(field, fallback):
            """The int catalog.cpp reads out of `field`, or None if it cannot.

            Absent is not wrong: catalog.cpp:398 defaults `step` to 1 and :400 defaults the
            range default to `min`, so a range written without either is a range this app
            reads exactly as its author meant. Only a field that is *there* and is not a
            whole number is a fault — QJsonValue::toInt() hands back the fallback for a
            string or a bool without saying so, which is how a `"step": "5"` becomes 1 and
            a twelve-stop slider becomes a sixty-stop one.
            """
            if field not in spec:
                return fallback
            value = spec[field]
            if isinstance(value, bool) or not isinstance(value, int):
                c.error("range", "catalog.json", tweak_id,
                        "`%s` is %r, not a whole number; toInt() falls back to %r and says "
                        "nothing" % (field, value, fallback))
                return None
            return value

        low = whole("min", 0)
        high = whole("max", 0)
        step = whole("step", 1)
        # min is the C++ fallback for the default, so it has to be known before default is.
        default = whole("default", low) if low is not None else None
        if low is None or high is None or step is None or default is None:
            continue
        if step < 1:
            # qMax(1, …) at catalog.cpp:398 replaces it, so the slider silently gets a
            # stop for every single value between min and max instead of the grid asked for.
            c.error("range", "catalog.json", tweak_id,
                    "`step` is %d; catalog.cpp:398 replaces anything below 1 with 1 and "
                    "generates a stop per value" % step)
            step = 1
        if high < low:
            # The generator loop is `for (v = from; v <= to_; v += step)`, so max below min
            # produces no positions at all and the tweak draws a slider with nothing on it.
            c.error("range", "catalog.json", tweak_id,
                    "max %d is below min %d; catalog.cpp:404 generates no positions"
                    % (high, low))
            continue
        if not low <= default <= high:
            c.error("range", "catalog.json", tweak_id,
                    "default %d is outside %d..%d" % (default, low, high))
            continue
        if (default - low) % step != 0:
            # catalog.cpp:424-431 snaps it and prints a qWarning, which reaches nobody: the
            # released build is a windowed application with no console attached. The
            # snapped position is then what the whole app treats as "what Windows ships",
            # so a range whose default missed the grid quietly reports the wrong baseline.
            c.error("range", "catalog.json", tweak_id,
                    "default %d is not on the %d/%d grid; catalog.cpp:424 snaps it and the "
                    "warning goes to a console that does not exist" % (default, low, step))
    c.note("%d range tweaks" % ranges)


def check_i18n_coverage(report, i18n, languages):
    c = report.check(6, "i18n.json — every key is present, and filled in, for every language")
    missing = collections.Counter()
    empty = collections.Counter()
    malformed = []
    for key in sorted(i18n):
        entry = i18n[key]
        if not isinstance(entry, dict):
            malformed.append(key)
            continue
        for lang in languages:
            if lang not in entry:
                missing[lang] += 1
            elif not isinstance(entry[lang], str) or not entry[lang].strip():
                # Locale::tr falls back to Turkish for an empty string (i18n.cpp:116) and
                # Locale::content falls back to the catalogue's own text, so an empty value
                # is not a crash — it is a row that stays Turkish in nine languages and
                # never gets reported, because it looks exactly like a translation nobody
                # has written yet.
                empty[lang] += 1

    for key in malformed[:MAX_SHOWN]:
        c.error("malformed entry", "i18n.json", key,
                "is not an object of language -> text")
    if len(malformed) > MAX_SHOWN:
        c.error("malformed entry", "i18n.json", "(%d more)" % (len(malformed) - MAX_SHOWN),
                "same")

    # Counted per language rather than per key on purpose: one missing column is 1575
    # lines, and the number is the only part anybody reads.
    for lang in languages:
        if missing[lang] or empty[lang]:
            c.error("coverage", "i18n.json", lang,
                    "%d key%s missing, %d empty, of %d"
                    % (missing[lang], "" if missing[lang] == 1 else "s",
                       empty[lang], len(i18n)))
    c.note("%d keys x %d languages (%s)"
           % (len(i18n), len(languages), " ".join(languages)))


def collect_risk_keys(repo, report):
    """svc.risk.<Key> for every row of RiskyServices[], exactly as services.cpp:287 spells it.

    Read out of the C++ rather than listed here, because the key is derived from the table
    entry at run time and the comment above that table (services.cpp:100) promises the two
    cannot drift. A copy in this file would be the drift.
    """
    body = read_source_table(repo / "src/services.cpp", "const char *const RiskyServices[] = {",
                             report, "the risky-service list")
    if body is None:
        return []
    names = re.findall(r'"([^"]+)"', body)
    if not names:
        report.fatal("RiskyServices[] in src/services.cpp is empty as this reads it; teach "
                     "tools/check-data.py the new shape")
    return names


def collect_sidebar_group_keys(repo, report):
    """The headings the sidebar's Groups[] table hands to Locale::tr.

    Check 8 finds keys by reading the literal inside a tr() call, and these are not written
    that way: they are `const char *` in a table, completed with QString::fromLatin1() at
    the call (sidebar.cpp:183), so the key never stands next to a tr() anywhere in src/ and
    the scanner walks straight past it. Check 9 has always said so out loud, and for years
    that was harmless because the headings in the table were all long since translated.

    It stopped being harmless the day a heading was added. "sidebar.group.tools" arrived
    with the God Mode page and no check in this file could see it, and a heading that is
    not in the table is not an untranslated word — it is the literal "sidebar.group.tools"
    drawn over a group of rows, in all ten languages, until somebody notices.

    Read out of the C++ for the same reason collect_risk_keys() is: a copy here would be
    the drift it is meant to catch.
    """
    body = read_source_table(repo / "src/views/sidebar.cpp", "constexpr GroupDef Groups[] = {",
                             report, "the sidebar's group headings")
    if body is None:
        return []
    # `{ "sidebar.group.tools", {"godmode", …} }` — the heading is the string that has the
    # id array after it, which is what tells it apart from the ids themselves. The Genel
    # Bakış row carries nullptr there and is meant to match nothing: it has no heading.
    keys = re.findall(r'\{\s*"([^"]+)"\s*,\s*\{', body)
    if not keys:
        report.fatal("the Groups[] table in src/views/sidebar.cpp does not have the shape "
                     "tools/check-data.py reads; teach it the new one")
    return keys


def check_service_risk_keys(report, i18n, risky):
    c = report.check(7, "i18n.json — every RiskyServices[] row has its svc.risk key")
    for name in risky:
        key = "svc.risk." + name
        if key not in i18n:
            # services.cpp:287 builds this key from the table entry and hands it to the row
            # as riskNoteKey; LiveDescription::text() puts it through Locale::tr
            # (catalog.cpp:58). A key that is not there comes back as itself, so the service
            # row reads "dikkat: svc.risk.Spooler" — which is what the fallback at
            # i18n.cpp:125 is for, and is still a bug on somebody's screen.
            c.error("missing risk note", "i18n.json", key,
                    "RiskyServices[] names %s but the table has no note for it" % name)
    c.note("%d risky services named in src/services.cpp" % len(risky))


def key_expressions(text):
    """(offset, first argument) for every Locale::tr / Locale::content call in one file.

    The first argument, not the whole call: `Locale::content(key, sourceText)` takes the
    fallback text as its second, and that is a Turkish sentence rather than a key. Read to
    the matching parenthesis rather than by regex because `.arg(...)` follows most of these
    calls and a lazy match would stop inside it.
    """
    for match in TR_CALL.finditer(text):
        i = match.end()          # just past the opening parenthesis
        start = i
        depth = 1
        comma = -1
        n = len(text)
        while i < n:
            ch = text[i]
            if ch in "\"'":
                # Skipped whole: a quoted parenthesis or comma is text, not syntax, and
                # QLatin1Char('(') appears in this codebase.
                i += 1
                while i < n and text[i] != ch:
                    i += 2 if text[i] == "\\" else 1
            elif ch == "(":
                depth += 1
            elif ch == ")":
                depth -= 1
                if depth == 0:
                    break
            elif ch == "," and depth == 1 and comma < 0:
                comma = i
            i += 1
        if i >= n:
            continue             # unbalanced: not something to guess about
        yield match.start(), text[start:comma if comma >= 0 else i]


def keys_in(expression):
    """The keys one first argument can evaluate to.

    A `+` means the literal is a prefix completed with something the table cannot see — a
    catalogue id, a section heading — so only the first one is taken, which is what
    separates a whole key from a prefix. Without a `+` every literal in the expression is a
    key in its own right, which is how the ternaries at settingspage.cpp:105 and
    debloatpage.cpp:273 pick between two of them; matching only the first would leave one
    branch of each unchecked, and the branch nobody exercises is exactly where a typo sits.
    """
    literals = QSTRING_LITERAL.findall(expression)
    if "+" in expression:
        return literals[:1]
    return literals


def collect_source_keys(sources, group_keys=()):
    """Every literal handed straight to Locale::tr or Locale::content, with where it was.

    `group_keys` are the sidebar headings, which reach tr() through a table rather than
    through a call the scanner can read — see collect_sidebar_group_keys(). They are merged
    in here rather than checked separately so that a missing one is reported in the same
    voice, at the same check, as every other key src/ asks for.
    """
    found = collections.OrderedDict()
    for name, text in sources:
        for offset, expression in key_expressions(text):
            line = text.count("\n", 0, offset) + 1
            for key in keys_in(expression):
                found.setdefault(key, []).append("%s:%d" % (name, line))
    for key in group_keys:
        found.setdefault(key, []).append("src/views/sidebar.cpp, the Groups[] table")
    return found


def check_source_keys(report, i18n, source_keys):
    c = report.check(8, "src/ — every literal handed to Locale tr/content resolves")
    checked = 0
    for key in sorted(source_keys):
        if key in RUNTIME_PREFIXES:
            continue
        checked += 1
        if key not in i18n:
            # tr() logs a warning nobody sees and returns the key itself (i18n.cpp:111-112),
            # which is the deliberate choice recorded in i18n.h:57 — a stray "settings.title"
            # on screen is a bug report waiting to happen. This is the check that files it
            # before a user has to.
            c.error("missing key", "i18n.json", key,
                    "asked for at %s; tr() would render the key itself"
                    % source_keys[key][0])
    c.note("%d literals, %d runtime prefixes whitelisted (%s)"
           % (checked, len(RUNTIME_PREFIXES),
              " ".join(sorted(RUNTIME_PREFIXES))))


def collect_data_keys(catalog, actions, links):
    """The keys the data files build at run time, spelled out.

    These are the other half of check 8: the prefixes above are completed with a catalogue
    id, a section heading, an option label or a settings-link id, none of which appear in
    src/ at all.
    """
    def text(node, field):
        value = node.get(field)
        return value if isinstance(value, str) else ""

    keys = set()
    for category in objects(catalog.get("categories")):
        if text(category, "id"):
            keys.add("category." + category["id"])          # mainwindow.cpp:322
        for section in objects(category.get("sections")):
            if text(section, "title"):
                keys.add("section." + section["title"])      # catalog.cpp:94
            for tweak in objects(section.get("tweaks")):
                if text(tweak, "id"):
                    keys.add("tweak." + tweak["id"] + ".name")   # catalog.cpp:78
                    keys.add("tweak." + tweak["id"] + ".desc")   # catalog.cpp:87
                for option in objects(tweak.get("options")):
                    label = text(option, "label")
                    # displayLabel() skips a label that starts with a digit: a range's
                    # positions are numbers with a unit and there is nothing to translate
                    # (catalog.cpp:38-41).
                    if label and not label[0].isdigit():
                        keys.add("opt." + label)                 # catalog.cpp:42
    for section in objects(actions.get("sections")):
        if text(section, "title"):
            keys.add("section." + section["title"])              # action.cpp:32
        for action in objects(section.get("actions")):
            if not text(action, "id"):
                continue
            keys.add("action." + action["id"] + ".name")          # action.cpp:15
            keys.add("action." + action["id"] + ".desc")          # action.cpp:20
            if action.get("note"):
                keys.add("action." + action["id"] + ".note")      # action.cpp:27
    for group in objects(links.get("groups")):
        if text(group, "id"):
            keys.add("godmode.group." + group["id"])              # godmodepage.cpp, build()
        for item in objects(group.get("items")):
            if text(item, "id"):
                keys.add("godmode." + item["id"])                 # godmodepage.cpp, label()
    return keys


def check_orphans(report, i18n, source_keys, data_keys, result_keys, risk_keys, sources):
    c = report.check(9, "i18n.json — keys nothing appears to reference (informational)")

    referenced = set(source_keys) | set(data_keys) | set(result_keys) | set(risk_keys)
    # A second, deliberately loose sweep: any quoted literal anywhere in src/ that happens
    # to be a key counts as a reference. Several families never reach Locale::tr directly —
    # the sidebar's group headings, the TrustedInstaller launcher's rows and DeepInfo's
    # fields are `const char *` tables handed to tr() through QString::fromLatin1 — and
    # calling those orphans would be wrong in the most expensive direction.
    literal = re.compile(r'"([^"\n]*)"')
    for _name, text in sources:
        for match in literal.finditer(text):
            if match.group(1) in i18n:
                referenced.add(match.group(1))

    orphans = sorted(k for k in i18n if k not in referenced)
    for key in orphans:
        # Never an error. This list has always held keys that are reached by a
        # concatenation this script cannot see, and deleting one of those turns a working
        # sentence into a raw lookup key in ten languages at once. It is here to be read,
        # not to be acted on without checking.
        c.info("orphan", "i18n.json", key, "no reference found in src/ or the data files")
    c.note("%d of %d keys have no reference this script can see"
           % (len(orphans), len(i18n)))


def check_actions(report, actions, i18n):
    c = report.check(10, "actions.json — required fields, translation keys and result tokens")

    sections = actions.get("sections")
    if not isinstance(sections, list) or not sections:
        c.error("catalogue", "actions.json", "sections", "no sections array")
        return

    count = 0
    tokens = 0
    for s_index, section in enumerate(sections):
        s_where = "section %d" % s_index
        if not isinstance(section, dict):
            c.error("section", "actions.json", s_where,
                    "is %r, not an object; it becomes an untitled section with no actions "
                    "and action.cpp:91 drops it" % (section,))
            continue
        if not isinstance(section.get("title"), str) or not section["title"]:
            c.error("section", "actions.json", s_where,
                    "no `title`; ActionSection::displayTitle() looks up section. + title")
        else:
            # displayTitle() is Locale::content (action.cpp:32), so a heading with no key
            # falls back to the Turkish in this file rather than showing itself — which
            # made it the one string on the page that could ship untranslated with nothing
            # on screen looking wrong. The Yeniden başlatma section was added with no key
            # and the only thing that noticed was a person reading the diff; that is not a
            # mechanism. Same reasoning as the action.<id>.name check further down.
            key = "section." + section["title"]
            if key not in i18n:
                c.error("missing key", "i18n.json", key,
                        "actions.json's section \"%s\" is looked up as %s and the table "
                        "has no heading for it" % (section["title"], key))
        raw_actions = section.get("actions")
        for a_index, action in enumerate(raw_actions if isinstance(raw_actions, list) else []):
            count += 1
            a_where = "%s / action %d" % (s_where, a_index)
            if not isinstance(action, dict):
                c.error("action", "actions.json", a_where,
                        "is %r, not an object; action.cpp:87 drops it for having no id"
                        % (action,))
                continue
            action_id = action["id"] if isinstance(action.get("id"), str) else ""
            where = action_id or a_where
            # The constructor drops an action with no id or no run (action.cpp:87), and a
            # section left with no actions is dropped with it (action.cpp:91) — so a typo
            # here does not fail anything, it removes a button from the page.
            if not action_id:
                c.error("action", "actions.json", where,
                        "no `id`; action.cpp:87 drops the action and the button never "
                        "appears")
            raw_run = action.get("run")
            if not script_lines(action):
                c.error("action", "actions.json", where,
                        "no `run`; action.cpp:87 drops the action and the button never "
                        "appears")
            elif not isinstance(raw_run, list):
                c.error("action", "actions.json", where,
                        "`run` is %r, not an array; toArray() empties it and action.cpp:87 "
                        "drops the action" % (raw_run,))
            else:
                for l_index, line in enumerate(raw_run):
                    if not isinstance(line, str):
                        # line.toString() (action.cpp:81) makes it a blank line rather than
                        # dropping it, so the action still has a script, still draws its
                        # button, still reports success, and does nothing at all.
                        c.error("action", "actions.json", where,
                                "`run` line %d is %r; toString() makes it a blank line and "
                                "the step silently does not run" % (l_index, line))
            for field in ("name", "desc"):
                if not isinstance(action.get(field), str) or not action[field]:
                    c.error("action", "actions.json", where, "no `%s`" % field)
            if not isinstance(action.get("reversible", False), bool):
                # toBool() reads anything that is not a bool as false, so the action loses
                # its "this can be undone" line without anything saying so.
                c.error("action", "actions.json", where,
                        "`reversible` is %r, not a boolean" % action.get("reversible"))

            if not action_id:
                continue
            wanted = ["action.%s.name" % action_id, "action.%s.desc" % action_id]
            if action.get("note"):
                wanted.append("action.%s.note" % action_id)
            for key in wanted:
                if key not in i18n:
                    # Locale::content falls back to the Turkish in actions.json rather than
                    # to the key (i18n.cpp:136), so this one is quiet by design and shows up
                    # only as an untranslated row. The whole catalogue is translated today;
                    # this check is what keeps a new action from shipping half-done.
                    c.error("missing key", "i18n.json", key,
                            "actions.json defines %s but the table has no %s"
                            % (action_id, key.rsplit(".", 1)[1]))

            # A script cannot know the interface language, so it prints "ARB|<code>" as its
            # last line and actionpage.cpp:57-61 turns the code into a sentence through
            # Locale::tr. tr() renders a missing key as itself, so a typo in the token puts
            # "actions.result.tmpCleaned" in the status bar as the outcome of a successful
            # run.
            for code in result_codes(action):
                tokens += 1
                key = "actions.result." + code
                if key not in i18n:
                    c.error("missing result key", "i18n.json", key,
                            "%s prints ARB|%s and actionpage.cpp:59 looks this up"
                            % (action_id, code))

    c.note("%d actions in %d sections, %d ARB result tokens"
           % (count, len(sections), tokens))


def check_settings_links(report, links, i18n):
    c = report.check(11, "settings-links.json — ids are unique and every one has its "
                         "godmode key")

    # The God Mode page has no Turkish of its own to fall back on. A tweak or an action
    # carries its name in catalog.json / actions.json, so Locale::content can show that when
    # a translation is missing (i18n.cpp:136); a settings link carries only an id, and
    # godmodepage.cpp asks Locale::tr for "godmode.<id>" — which renders a missing key as
    # itself (i18n.cpp:111). So the failure here is not an untranslated row, it is a row
    # labelled "godmode.taskschd" in all ten languages.
    #
    # Presence is all this checks. Whether the ten columns are actually filled in is check
    # 6's business: it measures every key in the table, so a key that exists here and is
    # blank in Polish fails there rather than being reported twice in two voices.

    groups = links.get("groups")
    if not isinstance(groups, list) or not groups:
        c.error("catalogue", "settings-links.json", "groups",
                "no groups array; SettingsLinks::groups() would come back empty and the "
                "page would draw nothing but its search box")
        return

    seen_links = collections.Counter()
    seen_groups = collections.Counter()
    count = 0
    for g_index, group in enumerate(groups):
        g_where = "group %d" % g_index
        if not isinstance(group, dict):
            c.error("group", "settings-links.json", g_where,
                    "is %r, not an object; toObject() gives it no id and settingslinks.cpp "
                    "drops it" % (group,))
            continue
        group_id = group["id"] if isinstance(group.get("id"), str) else ""
        where = group_id or g_where
        if not group_id:
            # settingslinks.cpp drops a group with no id, so this does not fail anything —
            # it removes a whole section from the page.
            c.error("group", "settings-links.json", where,
                    "no `id`; the group is dropped and its links go with it")
        else:
            seen_groups[group_id] += 1
            key = "godmode.group." + group_id
            if key not in i18n:
                c.error("missing key", "i18n.json", key,
                        "settings-links.json defines group %s but the table has no heading "
                        "for it" % group_id)

        items = group.get("items")
        if not isinstance(items, list) or not items:
            c.error("group", "settings-links.json", where,
                    "`items` is %r; a group with nothing under it is dropped and its "
                    "heading never appears" % (items,))
            continue

        for i_index, item in enumerate(items):
            count += 1
            i_where = "%s / item %d" % (where, i_index)
            if not isinstance(item, dict):
                c.error("link", "settings-links.json", i_where,
                        "is %r, not an object; it has neither an id nor a target and is "
                        "dropped" % (item,))
                continue
            link_id = item["id"] if isinstance(item.get("id"), str) else ""
            target = item["target"] if isinstance(item.get("target"), str) else ""
            if not link_id:
                c.error("link", "settings-links.json", i_where,
                        "no `id`; there is no label to look up and the row is dropped")
            if not target:
                c.error("link", "settings-links.json", link_id or i_where,
                        "no `target`; there is nothing to open and the row is dropped")
            # kindOf() reads a colon as a scheme and sends the target to
            # QDesktopServices or to explorer.exe; everything else is a file name, and
            # resolveSystemFile() turns it into an absolute path under System32 before
            # anything is launched. That resolution is the whole reason this page is safe
            # in a process that always runs elevated out of Downloads — the same
            # search-order class this project already fixed for tbs.dll. A target that
            # carries a path of its own quietly gets around it: it still resolves, just to
            # somewhere nobody audited. Bare file names only.
            elif ":" not in target and re.search(r"[\\/]", target):
                c.error("link", "settings-links.json", link_id or i_where,
                        "target \"%s\" is a path rather than a bare file name; "
                        "resolveSystemFile() joins it to System32 and the search-order "
                        "guarantee this page rests on stops meaning anything" % target)
            if not link_id:
                continue

            seen_links[link_id] += 1
            key = "godmode." + link_id
            if key not in i18n:
                c.error("missing key", "i18n.json", key,
                        "settings-links.json defines %s but the table has no label for it; "
                        "Locale::tr would draw the key itself" % link_id)

    for link_id, n in sorted(seen_links.items()):
        if n > 1:
            # Two rows sharing an id share their label, so the page shows the same name
            # twice over two different targets — and there is only one godmode.<id> key to
            # write, which means no wording can tell them apart.
            c.error("duplicate ids", "settings-links.json", link_id,
                    "appears %d times; both rows would draw the same label over different "
                    "targets" % n)
    for group_id, n in sorted(seen_groups.items()):
        if n > 1:
            c.error("duplicate ids", "settings-links.json", group_id,
                    "is the id of %d groups; both would draw the same heading" % n)

    c.note("%d links in %d groups, %d distinct ids"
           % (count, len(groups), len(seen_links)))


def script_lines(action):
    """One action's script as action.cpp:79-81 assembles it.

    `for (const QJsonValue &line : run) action.run << line.toString();` — a line that is
    not a string becomes an empty one rather than being dropped, and a `run` that is not an
    array at all becomes no script, which action.cpp:87 then removes the button for.
    """
    run = action.get("run")
    if not isinstance(run, list):
        return []
    return [line if isinstance(line, str) else "" for line in run]


def result_codes(action):
    """Every ARB|<code> token one action's script prints."""
    return RESULT_TOKEN.findall("\n".join(script_lines(action)))


def collect_result_keys(actions):
    """actions.result.<code> for every token the scripts print. See check 10."""
    keys = set()
    for section in objects(actions.get("sections")):
        for action in objects(section.get("actions")):
            for code in result_codes(action):
                keys.add("actions.result." + code)
    return keys


def read_languages(repo, report):
    """The ten ids from Locale's own table, so an eleventh needs no edit here."""
    body = read_source_table(repo / "src/i18n.cpp", "const QVector<Language> Languages = {",
                             report, "the language list")
    if body is None:
        return []
    rows = re.findall(
        r'\{\s*QStringLiteral\("([A-Za-z-]{2,5})"\)\s*,\s*QStringLiteral\("[^"]*"\)\s*,'
        r'\s*(?:true|false)\s*\}', body)
    if not rows:
        report.fatal("the Languages table in src/i18n.cpp does not have the shape "
                     "tools/check-data.py reads; teach it the new one")
    return rows


def main():
    parser = argparse.ArgumentParser(
        description="Validates resources/data/*.json against the code that reads it.")
    parser.add_argument("--repo", type=Path,
                        default=Path(__file__).resolve().parent.parent,
                        help="repository root (default: the one this script lives in)")
    # Separate from --repo because half the schema is in src/ and half is in the JSON, and
    # the thing anybody actually wants to point somewhere else is the JSON: a candidate
    # catalogue, a file a contributor sent, a copy with a defect deliberately in it to see
    # whether this script still catches it. Without this, checking one edited catalog.json
    # means copying all of src/ next to it so that the language, hive and risky-service
    # tables can still be read.
    parser.add_argument("--data", type=Path, default=None,
                        help="directory holding catalog.json, actions.json, "
                             "settings-links.json and i18n.json "
                             "(default: <repo>/resources/data)")
    args = parser.parse_args()
    repo = args.repo.resolve()
    data = args.data.resolve() if args.data else repo / "resources/data"

    # A section heading, a tweak id or an orphaned key can be Turkish, and one of them being
    # unprintable must not turn a data error into a UnicodeEncodeError on a Windows console
    # or a CI runner with a C locale.
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except (AttributeError, ValueError):
        pass

    print("Arbitrium data check — %s" % data.as_posix())
    if data != repo / "resources/data":
        print("                       schema read from %s/src" % repo.as_posix())
    print("")

    report = Report()
    catalog = read_json(data / "catalog.json", report)
    actions = read_json(data / "actions.json", report)
    links = read_json(data / "settings-links.json", report)
    i18n = read_json(data / "i18n.json", report)
    if report.fatal_message:
        return report.emit()

    # Everything the schema needs out of src/ is read here, before a single check runs. A
    # table that has been renamed is a fault in this script rather than in the data, and it
    # has to stop the run while there is still nothing on screen to mistake for a result.
    sources = read_sources(repo, report)
    languages = read_languages(repo, report)
    hives = read_hives(repo, report)
    risky = collect_risk_keys(repo, report)
    group_keys = collect_sidebar_group_keys(repo, report)
    if report.fatal_message:
        return report.emit()

    check_ids(report, catalog)
    check_option_data(report, catalog)
    check_hives_and_types(report, catalog, hives)
    check_required_fields(report, catalog)
    check_ranges(report, catalog)
    check_i18n_coverage(report, i18n, languages)
    check_service_risk_keys(report, i18n, risky)

    source_keys = collect_source_keys(sources, group_keys)
    data_keys = collect_data_keys(catalog, actions, links)
    result_keys = collect_result_keys(actions)
    risk_keys = ["svc.risk." + name for name in risky]

    check_source_keys(report, i18n, source_keys)
    check_orphans(report, i18n, source_keys, data_keys, result_keys, risk_keys, sources)
    check_actions(report, actions, i18n)
    check_settings_links(report, links, i18n)

    return report.emit()


if __name__ == "__main__":
    sys.exit(main())
