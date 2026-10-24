#!/usr/bin/env python3
"""Per-area coverage report for SpatialCore (TEST-01, Phase 6).

Reads two `llvm-cov export -summary-only` JSON files (the engine one from
SpatialCoreTests, the UI one from SpatialCoreUITests), keeps SpatialCore's own
sources, and writes a markdown table and a JSON file. Standard library only,
Python 3.9 and later. Run it as `python3 -I scripts/coverage-report.py ...`.

    coverage-report.py --core core.json --ui ui.json --root <repo> \
        --markdown summary.md --json summary.json [--targets file.json]
    coverage-report.py --self-test

Exit status: 0 ok, 1 below a target, 2 bad usage or unreadable input,
3 incomplete (a library source is missing from a report).

Percentages are 100 * covered / total, never rounded for a comparison. The
figure shown is truncated (not rounded) to two decimals, so it is never above
the real value; a result exactly at a target passes and one 0.01 point below
fails (the comparison is exact, in rational arithmetic).
"""

import argparse
import decimal
import fractions
import json
import os
import sys
import tempfile

ENGINE_AREAS = ["Algorithms", "Binaural", "Core", "Engine", "IO", "OSC", "Trajectory"]
BINAURAL_DEDICATED = [
    "src/Binaural/PartitionedConvolver.cpp",
    "src/Binaural/BinauralRenderer.cpp",
]
NO_TARGET_LINE = "No coverage target set yet (OQ-4); this run reports only."

EXIT_OK = 0
EXIT_BELOW_TARGET = 1
EXIT_USAGE = 2
EXIT_INCOMPLETE = 3


# ---------------------------------------------------------------- numbers

def percent(covered, count):
    """Unrounded percentage as a float; 0.0 when there is nothing to cover."""
    if count == 0:
        return 0.0
    return 100.0 * covered / count


def trunc2(value):
    """Truncate (never round up) a float percentage to two decimals, as text."""
    d = decimal.Decimal(repr(float(value)))
    return str(d.quantize(decimal.Decimal("0.01"), rounding=decimal.ROUND_DOWN))


def pct_text(covered, count):
    """Truncated two-decimal text computed from integers (no float error)."""
    if count == 0:
        return "0.00"
    hundredths = (covered * 10000) // count
    return "%d.%02d" % (hundredths // 100, hundredths % 100)


def meets_target(covered, count, target):
    """Exact comparison: covered/count*100 >= target, with no rounding."""
    if count == 0:
        return False
    actual = fractions.Fraction(covered * 100, count)
    return actual >= fractions.Fraction(repr(float(target))).limit_denominator(10 ** 9)


# ---------------------------------------------------------------- reading

def relative_name(filename, roots):
    for root in roots:
        prefix = root.rstrip("/") + "/"
        if filename.startswith(prefix):
            return filename[len(prefix):]
    return filename


def read_files(path, roots):
    """Return {relative filename: (lines covered, lines, branches covered, branches)}."""
    with open(path, "r") as handle:
        data = json.load(handle)
    out = {}
    for entry in data["data"][0]["files"]:
        rel = relative_name(entry["filename"], roots)
        lines = entry["summary"]["lines"]
        branches = entry["summary"]["branches"]
        out[rel] = (lines["covered"], lines["count"], branches["covered"], branches["count"])
    return out


def own_sources(root):
    """The repository's own .cpp files under src/, relative to root, split engine / UI."""
    engine, ui = [], []
    src = os.path.join(root, "src")
    for dirpath, _dirs, names in os.walk(src):
        for name in names:
            if not name.endswith(".cpp"):
                continue
            rel = os.path.relpath(os.path.join(dirpath, name), root).replace(os.sep, "/")
            if rel.startswith("src/UI/"):
                ui.append(rel)
            else:
                engine.append(rel)
    return sorted(engine), sorted(ui)


# ---------------------------------------------------------------- aggregation

def area_of(rel, areas):
    """Area name for src/<Area>/... or include/SpatialCore/<Area>/..., else None."""
    parts = rel.split("/")
    if len(parts) >= 3 and parts[0] == "src" and parts[1] in areas:
        return parts[1]
    if len(parts) >= 4 and parts[0] == "include" and parts[1] == "SpatialCore" and parts[2] in areas:
        return parts[2]
    return None


def add(total, row):
    return [total[0] + row[0], total[1] + row[1], total[2] + row[2], total[3] + row[3]]


def aggregate_engine(files):
    areas = {name: [0, 0, 0, 0] for name in ENGINE_AREAS}
    other = [0, 0, 0, 0]
    kept = {}
    for rel, row in files.items():
        if rel.startswith("src/UI/") or rel.startswith("include/SpatialCore/UI/"):
            continue
        area = area_of(rel, ENGINE_AREAS)
        if area is not None:
            areas[area] = add(areas[area], row)
            kept[rel] = row
        elif rel.startswith("src/") or rel.startswith("include/SpatialCore/"):
            # Umbrella headers and anything else of ours: counted, never dropped.
            other = add(other, row)
            kept[rel] = row
    total = [0, 0, 0, 0]
    for row in areas.values():
        total = add(total, row)
    total = add(total, other)
    return areas, other, total, kept


def aggregate_ui(files):
    total = [0, 0, 0, 0]
    kept = {}
    for rel, row in files.items():
        if rel.startswith("src/UI/") or rel.startswith("include/SpatialCore/UI/"):
            total = add(total, row)
            kept[rel] = row
    return total, kept


def missing_sources(wanted, files):
    return [rel for rel in wanted if rel not in files]


def build_report(core_files, ui_files, engine_sources, ui_sources):
    """Pure function: numbers in, report dict out. Raises Incomplete when a source is missing."""
    missing_core = missing_sources(engine_sources, core_files)
    missing_ui = missing_sources(ui_sources, ui_files)
    if missing_core or missing_ui:
        raise Incomplete(missing_core, missing_ui, len(engine_sources), len(ui_sources))
    areas, other, engine_total, engine_kept = aggregate_engine(core_files)
    ui_total, ui_kept = aggregate_ui(ui_files)
    return {
        "areas": areas,
        "other": other,
        "engine_total": engine_total,
        "engine_files": engine_kept,
        "ui_total": ui_total,
        "ui_files": ui_kept,
    }


class Incomplete(Exception):
    def __init__(self, missing_core, missing_ui, n_core, n_ui):
        Exception.__init__(self)
        self.missing_core = missing_core
        self.missing_ui = missing_ui
        self.n_core = n_core
        self.n_ui = n_ui

    def message(self):
        lines = []
        if self.missing_core:
            lines.append("coverage incomplete: %d of %d engine sources measured; missing: %s" % (
                self.n_core - len(self.missing_core), self.n_core, ", ".join(self.missing_core)))
        if self.missing_ui:
            lines.append("coverage incomplete: %d of %d UI sources measured; missing: %s" % (
                self.n_ui - len(self.missing_ui), self.n_ui, ", ".join(self.missing_ui)))
        return "\n".join(lines)


# ---------------------------------------------------------------- output

def row_cells(label, row):
    return "| %s | %d | %d | %s | %s |" % (
        label, row[0], row[1], pct_text(row[0], row[1]), pct_text(row[2], row[3]))


def weakest(engine_files, count=5):
    scored = []
    for rel, row in engine_files.items():
        if rel.startswith("src/") and row[1] > 0:
            scored.append((percent(row[0], row[1]), rel, row))
    scored.sort()
    return scored[:count]


def read_targets(path):
    if not path or not os.path.isfile(path):
        return None
    with open(path, "r") as handle:
        data = json.load(handle)
    return data if isinstance(data, dict) else None


def number_or_none(value):
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        return None
    return value


def evaluate_targets(report, targets):
    """Return (list of text lines, failed?). No targets: the report-only line."""
    if targets is None:
        return [NO_TARGET_LINE], False
    lines, failed = [], False
    for key, label, total in (
        ("engine_line_percent", "Engine", report["engine_total"]),
        ("ui_line_percent", "UI", report["ui_total"]),
    ):
        target = number_or_none(targets.get(key))
        if target is None:
            continue
        if meets_target(total[0], total[1], target):
            lines.append("Target: %s%% - PASS (%s, now %s%%)" % (target, label, pct_text(total[0], total[1])))
        else:
            failed = True
            lines.append("BELOW TARGET (%s): target %s%%, now %s%% (%d of %d lines)" % (
                label, target, pct_text(total[0], total[1]), total[0], total[1]))
    if not lines:
        lines.append("A targets file is present but sets no target; this run reports only.")
    return lines, failed


def render_markdown(report, target_lines):
    et, ut = report["engine_total"], report["ui_total"]
    out = []
    out.append("Engine: %s%% of the lines of SpatialCore's sound code ran at least once in the "
               "automated tests (%d of %d lines)." % (pct_text(et[0], et[1]), et[0], et[1]))
    out.append("")
    out.append("## Engine")
    out.append("")
    out.append("| Area | Lines covered | Lines | Line % | Branch % |")
    out.append("|---|---|---|---|---|")
    for name in ENGINE_AREAS:
        out.append(row_cells(name, report["areas"][name]))
    if report["other"][1] > 0:
        out.append(row_cells("Other (umbrella headers)", report["other"]))
    out.append(row_cells("**Engine total**", et))
    out.append("")
    out.append("Each area counts its `src/<Area>` sources and its `include/SpatialCore/<Area>` headers.")
    out.append("")
    out.append("## UI")
    out.append("")
    out.append("Measured on its own, from `SpatialCoreUITests` alone; it cannot move the engine figure.")
    out.append("")
    out.append("| Part | Lines covered | Lines | Line % | Branch % |")
    out.append("|---|---|---|---|---|")
    out.append(row_cells("**UI total**", ut))
    out.append("")
    out.append("## Weakest files")
    out.append("")
    out.append("The five engine sources with the lowest line coverage.")
    out.append("")
    out.append("| File | Lines covered | Lines | Line % |")
    out.append("|---|---|---|---|")
    for _pct, rel, row in weakest(report["engine_files"]):
        out.append("| %s | %d | %d | %s |" % (rel, row[0], row[1], pct_text(row[0], row[1])))
    out.append("")
    out.append("## Target")
    out.append("")
    out.extend(target_lines)
    out.append("")
    return "\n".join(out)


def render_json(report):
    def block(row):
        return {
            "lines_covered": row[0], "lines": row[1], "line_percent": percent(row[0], row[1]),
            "branches_covered": row[2], "branches": row[3], "branch_percent": percent(row[2], row[3]),
        }
    dedicated = {}
    for rel in BINAURAL_DEDICATED:
        row = report["engine_files"].get(rel)
        if row is not None:
            dedicated[rel] = block(row)
    return {
        "engine": {
            "total": block(report["engine_total"]),
            "areas": {name: block(report["areas"][name]) for name in ENGINE_AREAS},
            "other": block(report["other"]),
            "files": {rel: block(row) for rel, row in sorted(report["engine_files"].items())},
        },
        "ui": {
            "total": block(report["ui_total"]),
            "files": {rel: block(row) for rel, row in sorted(report["ui_files"].items())},
        },
        "dedicated_test_targets": dedicated,
    }


# ---------------------------------------------------------------- self-test

def _fake_row(covered, count):
    return (covered, count, 0, 0)


def self_test():
    failures = []

    def check(label, ok):
        print("%s: %s" % ("PASS" if ok else "FAIL", label))
        if not ok:
            failures.append(label)

    engine_src = ["src/Core/A.cpp"]
    ui_src = ["src/UI/B.cpp"]
    ui_files = {"src/UI/B.cpp": _fake_row(1, 10)}

    def report_for(covered, count):
        core = {"src/Core/A.cpp": _fake_row(covered, count)}
        return build_report(core, ui_files, engine_src, ui_src)

    at = report_for(9300, 10000)
    lines, failed = evaluate_targets(at, {"engine_line_percent": 93})
    check("9300 of 10000 against a target of 93 passes (equal)", not failed and "PASS" in lines[0])

    below = report_for(9299, 10000)
    lines, failed = evaluate_targets(below, {"engine_line_percent": 93})
    check("9299 of 10000 against a target of 93 fails", failed and "BELOW TARGET" in lines[0])

    check("92.996 displays as 92.99 (float truncation)", trunc2(92.996) == "92.99")
    check("92.996 displays as 92.99 (integer truncation)", pct_text(92996, 100000) == "92.99")
    check("93.9999 is not shown as 94.00", pct_text(939999, 1000000) == "93.99")
    check("a value of exactly 29 is not shown as 28.99", pct_text(29, 100) == "29.00")

    try:
        build_report({"src/Core/A.cpp": _fake_row(5, 10)}, ui_files,
                     engine_src + ["src/Core/Missing.cpp"], ui_src)
        check("a report missing a source raises Incomplete", False)
    except Incomplete as problem:
        check("a report missing a source raises Incomplete", "src/Core/Missing.cpp" in problem.message())

    try:
        build_report({"src/Core/A.cpp": _fake_row(5, 10)}, {}, engine_src, ui_src)
        check("a UI report missing a source raises Incomplete", False)
    except Incomplete as problem:
        check("a UI report missing a source raises Incomplete", "src/UI/B.cpp" in problem.message())

    # End to end through the files and exit status, including exit 3 for the incomplete case.
    with tempfile.TemporaryDirectory() as tmp:
        root = os.path.join(tmp, "repo")
        os.makedirs(os.path.join(root, "src", "Core"))
        os.makedirs(os.path.join(root, "src", "UI"))
        for rel in ("src/Core/A.cpp", "src/Core/B.cpp", "src/UI/U.cpp"):
            with open(os.path.join(root, rel), "w") as handle:
                handle.write("// stub\n")

        def export(path, entries):
            files = []
            for rel, covered, count in entries:
                files.append({"filename": os.path.join(root, rel), "summary": {
                    "lines": {"count": count, "covered": covered},
                    "branches": {"count": 0, "covered": 0}}})
            with open(path, "w") as handle:
                json.dump({"data": [{"files": files}]}, handle)

        core_ok = os.path.join(tmp, "core.json")
        core_short = os.path.join(tmp, "core-short.json")
        ui_ok = os.path.join(tmp, "ui.json")
        export(core_ok, [("src/Core/A.cpp", 5, 10), ("src/Core/B.cpp", 5, 10)])
        export(core_short, [("src/Core/A.cpp", 5, 10)])
        export(ui_ok, [("src/UI/U.cpp", 1, 10)])
        md, js = os.path.join(tmp, "o.md"), os.path.join(tmp, "o.json")
        base = ["--quiet", "--ui", ui_ok, "--root", root, "--markdown", md, "--json", js]
        check("end to end: a complete report exits 0",
              main(["--core", core_ok] + base) == EXIT_OK and os.path.getsize(md) > 0)
        check("end to end: a report missing a source exits 3",
              main(["--core", core_short] + base) == EXIT_INCOMPLETE)
        targets = os.path.join(tmp, "targets.json")
        with open(targets, "w") as handle:
            json.dump({"_decision": "note", "engine_line_percent": 50}, handle)
        check("end to end: exactly at the target exits 0",
              main(["--core", core_ok, "--targets", targets] + base) == EXIT_OK)
        with open(targets, "w") as handle:
            json.dump({"engine_line_percent": 50.01}, handle)
        check("end to end: just below the target exits 1",
              main(["--core", core_ok, "--targets", targets] + base) == EXIT_BELOW_TARGET)

    if failures:
        print("self-test FAILED: %d case(s)" % len(failures))
        return 1
    print("self-test passed")
    return 0


# ---------------------------------------------------------------- main

def main(argv):
    parser = argparse.ArgumentParser(description="SpatialCore per-area coverage report")
    parser.add_argument("--core")
    parser.add_argument("--ui")
    parser.add_argument("--root")
    parser.add_argument("--markdown")
    parser.add_argument("--json")
    parser.add_argument("--targets")
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--quiet", action="store_true", help="do not print the markdown")
    args = parser.parse_args(argv)

    if args.self_test:
        return self_test()
    for name in ("core", "ui", "root", "markdown", "json"):
        if not getattr(args, name):
            print("coverage-report: --%s is required" % name, file=sys.stderr)
            return EXIT_USAGE

    roots = [args.root, os.path.realpath(args.root)]
    try:
        core_files = read_files(args.core, roots)
        ui_files = read_files(args.ui, roots)
        targets = read_targets(args.targets)
    except (OSError, ValueError, KeyError, IndexError) as problem:
        print("coverage-report: cannot read input: %s" % problem, file=sys.stderr)
        return EXIT_USAGE

    engine_sources, ui_sources = own_sources(os.path.realpath(args.root))
    try:
        report = build_report(core_files, ui_files, engine_sources, ui_sources)
    except Incomplete as problem:
        print(problem.message(), file=sys.stderr)
        return EXIT_INCOMPLETE

    target_lines, failed = evaluate_targets(report, targets)
    with open(args.markdown, "w") as handle:
        handle.write(render_markdown(report, target_lines))
    with open(args.json, "w") as handle:
        json.dump(render_json(report), handle, indent=2, sort_keys=True)
        handle.write("\n")
    if not args.quiet:
        with open(args.markdown, "r") as handle:
            print(handle.read())
    return EXIT_BELOW_TARGET if failed else EXIT_OK


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
