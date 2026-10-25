#!/usr/bin/env bash
# build-consumer.sh -- prove that a plugin can consume SpatialCore the way OpenSpatialDelay does.
#
# Creates a throwaway git repository, copies the harness template into it, adds SpatialCore as a REAL
# git submodule from GitHub checked out at exactly <commit>, builds the plugin and the console check in
# Release, and runs the sound check. SpatialCore is never read from the local source folder (D-01):
# the commit must already be pushed.
#
# Usage:
#   build-consumer.sh <40-hex-commit> [-- extra cmake configure args]
#
# Environment:
#   SPATIALCORE_REMOTE_URL  remote to submodule (default https://github.com/AndrewRahman/SpatialCore.git;
#                           must start with https://github.com/)
#   HARNESS_WORK_DIR        working directory (default: a new temporary directory)
#   HARNESS_JOBS            parallel build jobs (default 4)
#   HARNESS_OSD_LIKE=1      OpenSpatialDelay-like configuration (consumer fetches libmysofa v1.3.2,
#                           -ffast-math on the consumer's own targets, CMake policy minimum 3.5)
#
# Exit codes: 2 bad input, 3 the commit could not be fetched or checked out, 4 compiler warnings in
# the harness or in SpatialCore's own sources and public headers, otherwise the sound check's status.

set -euo pipefail

usage()
{
    echo "usage: $(basename "$0") <40-hex-commit> [-- extra cmake configure args]" >&2
    echo "  SPATIALCORE_REMOTE_URL (https://github.com/ only), HARNESS_WORK_DIR, HARNESS_JOBS, HARNESS_OSD_LIKE=1" >&2
}

if [ "$#" -lt 1 ]; then
    usage
    exit 2
fi

SHA="$1"
shift

EXTRA_ARGS=()
if [ "$#" -gt 0 ]; then
    if [ "$1" != "--" ]; then
        echo "error: unexpected argument '$1' (extra cmake arguments go after --)" >&2
        usage
        exit 2
    fi
    shift
    EXTRA_ARGS=("$@")
fi

if ! printf '%s' "$SHA" | grep -Eq '^[0-9a-f]{40}$'; then
    echo "error: '$SHA' is not a 40-character lowercase hex commit id" >&2
    usage
    exit 2
fi

URL="${SPATIALCORE_REMOTE_URL:-https://github.com/AndrewRahman/SpatialCore.git}"
case "$URL" in
    https://github.com/*) ;;
    *)
        echo "error: remote '$URL' is not an https://github.com/ URL" >&2
        usage
        exit 2
        ;;
esac

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEMPLATE_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

if [ -n "${HARNESS_WORK_DIR:-}" ]; then
    WORK="$HARNESS_WORK_DIR"
    mkdir -p "$WORK"
else
    WORK="$(mktemp -d)"
fi
WORK="$(cd "$WORK" && pwd)"
CONSUMER="$WORK/consumer"

echo "==> consumer repository: $CONSUMER"
echo "==> SpatialCore: $URL at $SHA"

rm -rf "$CONSUMER"
mkdir -p "$CONSUMER"

# Copy the template: everything except scripts/.
for entry in "$TEMPLATE_DIR"/*; do
    [ "$(basename "$entry")" = "scripts" ] && continue
    cp -R "$entry" "$CONSUMER/"
done

cd "$CONSUMER"
git init -q
git lfs install --local >/dev/null

# The submodule step is what is being proved: a real submodule of the GitHub remote. LFS smudging is
# skipped for the clone and the checkout; the HRTF data is pulled once, below, from a seeded store.
export GIT_LFS_SKIP_SMUDGE=1
git submodule add --depth 1 "$URL" SpatialCore >/dev/null

if ! git -C SpatialCore fetch --depth 1 origin "$SHA"; then
    echo "error: commit $SHA could not be fetched from $URL (push it first)" >&2
    exit 3
fi
git -C SpatialCore checkout --detach -q FETCH_HEAD

ACTUAL="$(git -C SpatialCore rev-parse HEAD)"
if [ "$ACTUAL" != "$SHA" ]; then
    echo "error: the submodule is at $ACTUAL, not the requested $SHA" >&2
    exit 3
fi
unset GIT_LFS_SKIP_SMUDGE

git add -A
git -c user.name=harness -c user.email=harness@localhost commit -qm consumer
echo "==> git submodule status"
git submodule status

# Seed the submodule's LFS store from this checkout's, so `git lfs pull` downloads only what is
# missing (the HRTF files total about 60 MB and GitHub meters LFS bandwidth).
REPO_ROOT="$(git -C "$SCRIPT_DIR" rev-parse --show-toplevel 2>/dev/null || true)"
if [ -n "$REPO_ROOT" ]; then
    LOCAL_LFS="$(git -C "$REPO_ROOT" rev-parse --path-format=absolute --git-common-dir)/lfs/objects"
    if [ -d "$LOCAL_LFS" ]; then
        SUB_GIT="$(git -C SpatialCore rev-parse --path-format=absolute --git-dir)"
        mkdir -p "$SUB_GIT/lfs"
        cp -R "$LOCAL_LFS" "$SUB_GIT/lfs/objects"
    fi
fi
git -C SpatialCore lfs install --local >/dev/null
git -C SpatialCore lfs pull

CONFIGURE_ARGS=(-S . -B build -DCMAKE_BUILD_TYPE=Release)
if [ "${HARNESS_OSD_LIKE:-0}" = "1" ]; then
    CONFIGURE_ARGS+=(-DHARNESS_OSD_LIKE=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5)
fi
echo "==> cmake ${CONFIGURE_ARGS[*]} ${EXTRA_ARGS[*]-}"
cmake "${CONFIGURE_ARGS[@]}" ${EXTRA_ARGS[@]+"${EXTRA_ARGS[@]}"}

echo "==> build (Release)"
cmake --build build --config Release -j "${HARNESS_JOBS:-4}" 2>&1 | tee build.log

# Warning gate: zero warnings located in the harness's own sources or in SpatialCore's sources and
# public headers. Warnings inside JUCE, libmysofa and other fetched dependencies are only counted.
OWN_WARNINGS="$(grep -E '/consumer/(src|SpatialCore/include|SpatialCore/src)/.*warning:' build.log || true)"
TOTAL_WARNINGS="$(grep -c 'warning:' build.log || true)"
if [ -n "$OWN_WARNINGS" ]; then
    echo "error: compiler warnings in the harness or in SpatialCore:" >&2
    printf '%s\n' "$OWN_WARNINGS" | sort -u >&2
    exit 4
fi
echo "==> warnings: 0 in the harness and in SpatialCore; ${TOTAL_WARNINGS} line(s) mentioning 'warning:' in dependencies"

CHECK="build/HarnessCheck_artefacts/Release/HarnessCheck"
echo "==> sound check: $CHECK"
set +e
"$CHECK"
STATUS=$?
set -e
exit "$STATUS"
