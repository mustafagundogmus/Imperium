#!/usr/bin/env python3
# admx-scan.py — reads Windows' own policy definitions and says which of them the catalogue
# does not have yet.
#
# Every Group Policy setting Windows ships is described twice under
# C:\Windows\PolicyDefinitions: the .admx carries the registry key, the value name, the
# data each position writes and the builds it applies to; the .adml beside it, one per
# installed language, carries the setting's name and Microsoft's own explanation of it.
# That is exactly the shape of a catalogue row, which makes the folder the one source a
# tweak can be checked against rather than copied from — CONTRIBUTING says as much.
#
# This reads all of it, drops what catalog.json already writes, and leaves one JSON file
# per .admx in --out, each holding the policies that are still candidates: key, value,
# on/off data (or the listed positions), the supportedOn text, and the name and explanation
# in every language found. --known takes extra JSON files listing registry values other
# tweakers write (the shape tools/merge.py in the 0.14.0 research used), so a candidate
# can also say whether the popular tools already have it.
#
# Read-only, standard library only, and slow enough to notice (three thousand policies,
# two languages): a maintainer runs it when the catalogue is due a pass, not CI.

import argparse
import glob
import json
import os
import re
import sys
import xml.etree.ElementTree as ET

# Tags are matched by local name only. The namespace ought to be
# http://schemas.microsoft.com/GroupPolicy/2006/07/PolicyDefinitions on every file, and on
# WindowsDefender.adml it is ".../Policysecurity intelligence" instead — a rename of
# "Definitions" to "security intelligence" that ran over the namespace URI too. Matching
# the URI would make Defender's hundred and fifty settings nameless.
NS = ""


def tag(name):
    return name


def strip_namespaces(root):
    for el in root.iter():
        if isinstance(el.tag, str) and "}" in el.tag:
            el.tag = el.tag.split("}", 1)[1]
    return root


def read_xml(path):
    """ElementTree refuses a str that still carries an encoding declaration, and a few
    of Microsoft's files declare `encoding="unicode"`, which Python does not know. Decode
    by the BOM, strip the declaration, parse the text."""
    raw = open(path, "rb").read()
    if raw[:2] in (b"\xff\xfe", b"\xfe\xff"):
        text = raw.decode("utf-16")
    else:
        text = raw.decode("utf-8-sig", errors="replace")
    text = re.sub(r"^\s*<\?xml[^>]*\?>", "", text)
    try:
        return strip_namespaces(ET.fromstring(text))
    except ET.ParseError as err:
        print("skip %s: %s" % (os.path.basename(path), err), file=sys.stderr)
        return None


def strings_of(adml_root):
    """id -> text from the string table, and refId -> label from the presentation table."""
    strings = {}
    labels = {}
    if adml_root is None:
        return strings, labels
    for s in adml_root.iter(tag("string")):
        strings[s.get("id")] = (s.text or "").strip()
    for pres in adml_root.iter(tag("presentation")):
        for child in pres:
            ref = child.get("refId")
            if not ref:
                continue
            # A dropdownList or decimalTextBox carries its label as text; a checkBox too.
            text = "".join(child.itertext()).strip()
            if text:
                labels[(pres.get("id"), ref)] = text
    return strings, labels


def resolve(ref, strings):
    """$(string.X) -> X's text; anything else verbatim."""
    if not ref:
        return ""
    m = re.match(r"\$\(string\.(.+)\)", ref)
    return strings.get(m.group(1), ref) if m else ref


def value_of(node):
    """The data a <value> holds, as the catalogue would write it."""
    if node is None:
        return None
    for child in node:
        if child.tag == tag("decimal"):
            return child.get("value")
        if child.tag == tag("string"):
            return child.text or ""
        if child.tag == tag("delete"):
            return "DELETE"
    return None


def main():
    parser = argparse.ArgumentParser(
        description="Lists the Group Policy settings under PolicyDefinitions that "
                    "catalog.json does not write yet, one JSON file per .admx.")
    parser.add_argument("--definitions", default=r"C:\Windows\PolicyDefinitions")
    parser.add_argument("--catalog", default=os.path.join(os.path.dirname(__file__), "..",
                                                          "resources", "data", "catalog.json"))
    parser.add_argument("--out", required=True, help="directory for the per-ADMX JSON files")
    parser.add_argument("--known", nargs="*", default=[],
                        help="JSON files whose entries carry a `reg` list; their values are "
                             "marked known_online")
    parser.add_argument("--languages", nargs="*", default=None,
                        help="ADML folders to read (default: every one present)")
    args = parser.parse_args()

    catalog = json.load(open(args.catalog, encoding="utf-8"))
    have = set()
    for c in catalog["categories"]:
        for s in c["sections"]:
            for t in s["tweaks"]:
                for r in t["reg"]:
                    have.add((r["path"] + "\\" + r["value"]).lower())

    known = set()
    for path in args.known:
        for item in json.load(open(path, encoding="utf-8")):
            for r in item.get("reg", []):
                if r.get("path") and r.get("value"):
                    known.add((r["path"] + "\\" + r["value"]).lower())

    langs = args.languages or sorted(d for d in os.listdir(args.definitions)
                                     if os.path.isdir(os.path.join(args.definitions, d)))
    os.makedirs(args.out, exist_ok=True)

    # First pass: every category and every supportedOn definition, across all files, so
    # a parentCategory ref into windows.admx resolves from any other file.
    categories = {}    # "ns:name" or name -> (displayName ref, parent ref, admx)
    supported = {}     # name -> display ref, admx
    namespaces = {}    # admx -> its target prefix
    files = sorted(glob.glob(os.path.join(args.definitions, "*.admx")))
    roots = {}
    for f in files:
        root = read_xml(f)
        if root is None:
            continue
        roots[f] = root
        pn = root.find(tag("policyNamespaces"))
        target = pn.find(tag("target")) if pn is not None else None
        prefix = target.get("prefix") if target is not None else os.path.basename(f)
        namespaces[f] = prefix
        for cat in root.iter(tag("category")):
            parent = cat.find(tag("parentCategory"))
            categories[prefix + ":" + cat.get("name")] = (cat.get("displayName"),
                                                          parent.get("ref") if parent is not None else None,
                                                          f)
        for d in root.iter(tag("definition")):
            supported[prefix + ":" + d.get("name")] = (d.get("displayName"), f)

    adml = {}   # (admx path, lang) -> (strings, labels)

    def table(f, lang):
        key = (f, lang)
        if key not in adml:
            path = os.path.join(args.definitions, lang,
                                os.path.splitext(os.path.basename(f))[0] + ".adml")
            adml[key] = strings_of(read_xml(path)) if os.path.exists(path) else ({}, {})
        return adml[key]

    def qualify(ref, prefix):
        return ref if ":" in ref else prefix + ":" + ref

    def category_path(ref, prefix, lang):
        parts = []
        seen = set()
        while ref and ref not in seen:
            seen.add(ref)
            entry = categories.get(qualify(ref, prefix))
            if not entry:
                parts.append(ref)
                break
            display, parent, f = entry
            parts.append(resolve(display, table(f, lang)[0]))
            ref = parent
            prefix = namespaces.get(f, prefix)
        return " > ".join(reversed(parts))

    total = 0
    written = 0
    for f in files:
        root = roots.get(f)
        if root is None:
            continue
        prefix = namespaces[f]
        out = []
        for p in root.iter(tag("policy")):
            total += 1
            key = p.get("key", "")
            vname = p.get("valueName")
            entry = {
                "admx": os.path.basename(f),
                "name": p.get("name"),
                "class": p.get("class"),
                "key": key,
                "valueName": vname,
                "on": None, "off": None,
                "extra_on": [], "extra_off": [],
                "elements": [],
                "supportedOn": "",
                "category": {},
                "displayName": {},
                "explain": {},
                "in_catalog": False,
                "known_online": False,
            }
            ev = p.find(tag("enabledValue"))
            dv = p.find(tag("disabledValue"))
            if vname:
                entry["on"] = value_of(ev) if ev is not None else "1"
                entry["off"] = value_of(dv) if dv is not None else "DELETE"
            for list_name, target in (("enabledList", "extra_on"), ("disabledList", "extra_off")):
                lst = p.find(tag(list_name))
                if lst is None:
                    continue
                for item in lst.iter(tag("item")):
                    entry[target].append({"key": item.get("key", key), "valueName": item.get("valueName"),
                                          "value": value_of(item.find(tag("value")))})
            elements = p.find(tag("elements"))
            if elements is not None:
                for e in elements:
                    kind = e.tag
                    el = {"kind": kind, "id": e.get("id"), "key": e.get("key", key),
                          "valueName": e.get("valueName"), "label": {}}
                    if kind == "enum":
                        el["items"] = []
                        for item in e.iter(tag("item")):
                            el["items"].append({"displayName": item.get("displayName"),
                                                "value": value_of(item.find(tag("value")))})
                    if kind == "decimal":
                        el["min"] = e.get("minValue")
                        el["max"] = e.get("maxValue")
                    if kind == "boolean":
                        tv = e.find(tag("trueValue"))
                        fv = e.find(tag("falseValue"))
                        el["true"] = value_of(tv) if tv is not None else "1"
                        el["false"] = value_of(fv) if fv is not None else "0"
                    entry["elements"].append(el)
            so = p.find(tag("supportedOn"))
            if so is not None:
                ref = so.get("ref", "")
                d = supported.get(qualify(ref, prefix))
                entry["supportedOn"] = ref
                if d:
                    # The first language whose .adml carries the definition's text: the
                    # language folders are not all complete.
                    for lang in langs:
                        text = resolve(d[0], table(d[1], lang)[0])
                        if not text.startswith("$("):
                            entry["supportedOn"] = text
                            break

            # Coverage: the policy's own value, or any value an element writes.
            values = []
            if vname:
                values.append((key + "\\" + vname).lower())
            for el in entry["elements"]:
                if el.get("valueName"):
                    values.append((el["key"] + "\\" + el["valueName"]).lower())
            for item in entry["extra_on"]:
                if item.get("valueName"):
                    values.append((item["key"] + "\\" + item["valueName"]).lower())
            entry["in_catalog"] = any(v in have for v in values)
            entry["known_online"] = any(v in known for v in values)

            parent = p.find(tag("parentCategory"))
            pres_id = None
            pres = p.get("presentation")
            if pres:
                m = re.match(r"\$\(presentation\.(.+)\)", pres)
                pres_id = m.group(1) if m else None
            for lang in langs:
                strings, labels = table(f, lang)
                entry["displayName"][lang] = resolve(p.get("displayName"), strings)
                entry["explain"][lang] = resolve(p.get("explainText"), strings)
                if parent is not None:
                    entry["category"][lang] = category_path(parent.get("ref"), prefix, lang)
                for el in entry["elements"]:
                    el["label"][lang] = labels.get((pres_id, el["id"]), "")
                    for item in el.get("items", []):
                        item.setdefault("display", {})[lang] = resolve(item["displayName"], strings)

            if not entry["in_catalog"]:
                out.append(entry)

        if out:
            name = os.path.splitext(os.path.basename(f))[0] + ".json"
            with open(os.path.join(args.out, name), "w", encoding="utf-8") as fh:
                json.dump(out, fh, indent=1, ensure_ascii=False)
            written += len(out)

    print("%d policies in %d files; %d not in the catalogue, written to %s; languages %s"
          % (total, len(files), written, args.out, " ".join(langs)))


if __name__ == "__main__":
    main()
