#!/usr/bin/env python3
"""D-15 check: ADMOSCReceiver.h must raise no -Wunused-parameter warning.

The header is included by every consumer translation unit, so an unused named
parameter in it warns in each of them. This script recompiles one translation unit
that includes the header (tests/OSC/ADMOSCReceiverTests.cpp) with
-Wunused-parameter and counts the warnings whose location is the header.

It reuses the exact command CMake recorded in build/compile_commands.json, minus
the output and dependency-file options, so it writes nothing into the build tree.

Usage:   python3 tests/tools/check_unused_params.py [path/to/compile_commands.json]
Exit 0:  the compile succeeded and the count is 0
Exit 1:  the compile failed, or the count is above 0
Exit 2:  the compile database or the entry is missing
Python 3, standard library only.
"""

import json
import os
import shlex
import subprocess
import sys

HEADER = "ADMOSCReceiver.h"
SOURCE_SUFFIX = os.path.join("tests", "OSC", "ADMOSCReceiverTests.cpp")

# Options that name an output file; each takes one argument except -MD/-MMD.
OPTIONS_WITH_ARGUMENT = {"-o", "-MF", "-MT", "-MQ"}
OPTIONS_WITHOUT_ARGUMENT = {"-MD", "-MMD"}


def main() -> int:
    repo_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    database = sys.argv[1] if len(sys.argv) > 1 else os.path.join(repo_root, "build", "compile_commands.json")

    hint = "re-run: cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
    if not os.path.isfile(database):
        print(f"compile database not found: {database} ({hint})", file=sys.stderr)
        return 2

    with open(database, "r", encoding="utf-8") as handle:
        entries = json.load(handle)

    entry = next((e for e in entries if e.get("file", "").endswith(SOURCE_SUFFIX)), None)
    if entry is None:
        print(f"no entry for {SOURCE_SUFFIX} in {database} ({hint})", file=sys.stderr)
        return 2

    if "arguments" in entry:
        argv = list(entry["arguments"])
    else:
        argv = shlex.split(entry["command"])

    cleaned = []
    skip_next = False
    for arg in argv:
        if skip_next:
            skip_next = False
            continue
        if arg in OPTIONS_WITH_ARGUMENT:
            skip_next = True
            continue
        if arg in OPTIONS_WITHOUT_ARGUMENT:
            continue
        cleaned.append(arg)

    cleaned += ["-Wunused-parameter", "-fsyntax-only"]

    result = subprocess.run(cleaned, cwd=entry["directory"], capture_output=True, text=True)

    count = sum(
        1
        for line in result.stderr.splitlines()
        if HEADER in line and "unused parameter" in line
    )

    print(f"{HEADER} unused-parameter warnings: {count}")

    if result.returncode != 0:
        print("the compile itself failed:", file=sys.stderr)
        print(result.stderr[-2000:], file=sys.stderr)
        return 1

    return 0 if count == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
