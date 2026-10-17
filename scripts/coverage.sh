#!/usr/bin/env bash
# Source-based coverage for SpatialCore (TEST-01, Phase 6).
#
#   scripts/coverage.sh [build-dir] [-- extra cmake configure arguments]
#
# Builds the instrumented Debug tree (default build-cov/, git-ignored), runs SpatialCoreTests and
# SpatialCoreUITests directly (one profile each; not through ctest, which starts one process per
# test case), merges and exports the profiles with llvm-profdata and llvm-cov, writes an HTML report
# for each executable, and then runs scripts/coverage-report.py. The exit status is the report
# script's: 0 ok, 1 below the target in scripts/coverage-targets.json, 3 a library source missing
# from a report.
#
# Used locally and by the CI `coverage` job. Reports stay inside GitHub in CI (job summary plus a
# downloadable artifact); nothing is sent to an outside service (D-15).
#
# Environment: COVERAGE_JOBS (build parallelism, default the CPU count), LLVM_PROFDATA and LLVM_COV
# (tool overrides; otherwise `xcrun --find` on macOS, else llvm-profdata / llvm-cov on PATH).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
cd "$ROOT"

BUILD_DIR="build-cov"
if [ "$#" -gt 0 ] && [ "$1" != "--" ]; then
    BUILD_DIR="$1"
    shift
fi
if [ "$#" -gt 0 ] && [ "$1" = "--" ]; then
    shift
fi
EXTRA_CMAKE_ARGS=("$@")

case "$BUILD_DIR" in
    /*) ;;
    *) BUILD_DIR="$ROOT/$BUILD_DIR" ;;
esac
COV_DIR="$BUILD_DIR/coverage"

if [ -n "${COVERAGE_JOBS:-}" ]; then
    JOBS="$COVERAGE_JOBS"
elif command -v sysctl >/dev/null 2>&1 && sysctl -n hw.ncpu >/dev/null 2>&1; then
    JOBS="$(sysctl -n hw.ncpu)"
elif command -v nproc >/dev/null 2>&1; then
    JOBS="$(nproc)"
else
    JOBS=2
fi

find_tool() {
    # find_tool <override value> <tool name>
    local override="$1" name="$2"
    if [ -n "$override" ]; then
        printf '%s' "$override"
    elif command -v xcrun >/dev/null 2>&1 && xcrun --find "$name" >/dev/null 2>&1; then
        xcrun --find "$name"
    elif command -v "$name" >/dev/null 2>&1; then
        command -v "$name"
    else
        echo "coverage.sh: cannot find $name (set LLVM_PROFDATA / LLVM_COV, or install LLVM)" >&2
        exit 2
    fi
}
LLVM_PROFDATA_BIN="$(find_tool "${LLVM_PROFDATA:-}" llvm-profdata)"
LLVM_COV_BIN="$(find_tool "${LLVM_COV:-}" llvm-cov)"
echo "coverage.sh: llvm-profdata=$LLVM_PROFDATA_BIN llvm-cov=$LLVM_COV_BIN jobs=$JOBS build=$BUILD_DIR"

# Only SpatialCore's own sources are measured: JUCE, Catch2, libmysofa, the tests and the generated
# BinaryData are excluded, and nothing else (no library source is ever dropped to raise the number).
IGNORE_REGEX='(_deps|/tests/|/usr/|Applications|Library/Developer|juce_binarydata)'

SECONDS=0
if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
    cmake -S "$ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Debug -DSPATIALCORE_ENABLE_COVERAGE=ON \
        ${EXTRA_CMAKE_ARGS[@]+"${EXTRA_CMAKE_ARGS[@]}"}
fi
cmake --build "$BUILD_DIR" -j "$JOBS"
echo "coverage.sh: build finished after ${SECONDS}s"

rm -rf "$COV_DIR"
mkdir -p "$COV_DIR/prof" "$COV_DIR/html"

CORE_EXE="$BUILD_DIR/tests/SpatialCoreTests"
UI_EXE="$BUILD_DIR/tests/SpatialCoreUITests"
for exe in "$CORE_EXE" "$UI_EXE"; do
    if [ ! -x "$exe" ]; then
        echo "coverage.sh: $exe was not built" >&2
        exit 2
    fi
done

run_tests() {
    # run_tests <label> <executable>: one profile per executable; a failing run fails the script.
    # A Debug run prints libmysofa's parser trace (hundreds of thousands of lines) and expected JUCE
    # assertion lines; through a CI log pipe that output stalls the test process, so it goes to a
    # file and only the verdict (the exit status) and the failure blocks are printed.
    local label="$1" exe="$2"
    local started="$SECONDS" status=0 log="$COV_DIR/$1-tests.log"
    echo "coverage.sh: running $label (output in $log)"
    (cd "$(dirname "$exe")" && LLVM_PROFILE_FILE="$COV_DIR/prof/$label-%p.profraw" "$exe") > "$log" 2>&1 || status=$?
    tail -4 "$log"
    if [ "$status" -ne 0 ]; then
        echo "coverage.sh: $label failed with exit status $status" >&2
        grep -n -B8 -A14 'FAILED:' "$log" | head -300 || true
        tail -40 "$log"
        exit "$status"
    fi
    echo "coverage.sh: $label finished in $((SECONDS - started))s"
}
run_tests core "$CORE_EXE"
run_tests ui "$UI_EXE"

# SpatialCoreUITests links SpatialCoreUI as a static archive, so a UI source with no referenced symbol
# carries no coverage mapping and would silently vanish from the report (RESEARCH Pitfall 4). Each UI
# object file goes to llvm-cov with -object, which brings its mapping back with zero hits and leaves
# the numbers of the files that are linked unchanged (checked for PresetBrowser, SMLLookAndFeel and
# SpatialMapComponent, Plan 06-07). The engine objects are not needed: SpatialCoreTests pulls in every
# engine source, the export is identical with or without them, and passing them only adds noise
# ("functions have mismatched data"). A source that is ever missing makes the report script exit 3.
UI_OBJECTS=()
if [ -d "$BUILD_DIR/CMakeFiles/SpatialCoreUI.dir/src/UI" ]; then
    while IFS= read -r obj; do
        UI_OBJECTS+=(-object "$obj")
    done < <(find "$BUILD_DIR/CMakeFiles/SpatialCoreUI.dir/src/UI" \( -name '*.o' -o -name '*.obj' \) | sort)
fi

for label in core ui; do
    "$LLVM_PROFDATA_BIN" merge -sparse "$COV_DIR/prof/$label"-*.profraw -o "$COV_DIR/$label.profdata"
done

"$LLVM_COV_BIN" export "$CORE_EXE" \
    -instr-profile="$COV_DIR/core.profdata" -summary-only \
    -ignore-filename-regex="$IGNORE_REGEX" > "$COV_DIR/core-summary.json"
"$LLVM_COV_BIN" export "$UI_EXE" ${UI_OBJECTS[@]+"${UI_OBJECTS[@]}"} \
    -instr-profile="$COV_DIR/ui.profdata" -summary-only \
    -ignore-filename-regex="$IGNORE_REGEX" > "$COV_DIR/ui-summary.json"

"$LLVM_COV_BIN" show "$CORE_EXE" \
    -instr-profile="$COV_DIR/core.profdata" -format=html -output-dir="$COV_DIR/html/engine" \
    -ignore-filename-regex="$IGNORE_REGEX" >/dev/null
"$LLVM_COV_BIN" show "$UI_EXE" ${UI_OBJECTS[@]+"${UI_OBJECTS[@]}"} \
    -instr-profile="$COV_DIR/ui.profdata" -format=html -output-dir="$COV_DIR/html/ui" \
    -ignore-filename-regex="$IGNORE_REGEX" >/dev/null

REPORT_ARGS=(--core "$COV_DIR/core-summary.json" --ui "$COV_DIR/ui-summary.json" --root "$ROOT"
    --markdown "$COV_DIR/summary.md" --json "$COV_DIR/summary.json")
if [ -f "$ROOT/scripts/coverage-targets.json" ]; then
    REPORT_ARGS+=(--targets "$ROOT/scripts/coverage-targets.json")
fi

status=0
python3 -I "$ROOT/scripts/coverage-report.py" "${REPORT_ARGS[@]}" || status=$?
echo "coverage.sh: done after ${SECONDS}s, report status $status"
exit "$status"
