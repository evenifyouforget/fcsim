#!/usr/bin/env bash
# build_subdirs.sh — build each feature branch listed in deploy.json and
# assemble the outputs into html/<subdir>/.
#
# Usage:
#   scripts/build_subdirs.sh [HTML_OUT_DIR]
#
# HTML_OUT_DIR defaults to ./html.  Run from the repo root after the main
# build has already populated HTML_OUT_DIR with the base site.
#
# Venv cache:
#   On GitHub Actions set VENV_CACHE_DIR to $GITHUB_WORKSPACE/.venv-cache so
#   caches persist across steps.  Locally it defaults to
#   ~/.cache/fcsim-venv-cache, giving you warm reinstalls across runs.

set -euo pipefail

HTML_OUT="${1:-html}"
DEPLOY_JSON="deploy.json"
VENV_CACHE_DIR="${VENV_CACHE_DIR:-$HOME/.cache/fcsim-venv-cache}"

if [ ! -f "$DEPLOY_JSON" ]; then
  echo "build_subdirs.sh: $DEPLOY_JSON not found; nothing to do"
  exit 0
fi

# Parse deploy.json: emit "dir=branch" lines, one per entry.
read_deploy_json() {
  python3 -c "
import json, sys
data = json.load(open('$DEPLOY_JSON'))
for k, v in data.items():
    print(f'{k}={v}')
"
}

while IFS="=" read -r dir branch; do
  if [ "$branch" = "null" ]; then
    echo "=== $dir: frozen artifact (null branch), skipping build ==="
    continue
  fi

  echo "=== Building branch '$branch' -> $HTML_OUT/$dir/ ==="

  # Ensure the branch exists as a local ref so git-clone --local can find it.
  # In CI only the checked-out branch is local; feature branches must be fetched.
  # +refs/heads/...:refs/heads/... force-updates the local ref if it already
  # exists, which is safe here because we never check these branches out in the
  # parent repo (we're on v6-deploy/main).
  git fetch origin "+refs/heads/${branch}:refs/heads/${branch}"

  WORK_DIR="$(mktemp -d)"
  # --local avoids copying object files; --no-hardlinks ensures the clone is
  # fully independent so scons doesn't clobber the parent repo's build dir.
  git clone --local --no-hardlinks --branch "$branch" . "$WORK_DIR"

  (
    cd "$WORK_DIR"

    REQ_HASH=$(cat requirements*.txt install_requirements.sh 2>/dev/null \
               | sha256sum | cut -c1-16 || echo "nohash")
    CACHE_KEY="venv-${branch//\//-}-${REQ_HASH}"
    CACHE_DIR="$VENV_CACHE_DIR/$CACHE_KEY"

    if [ -d "$CACHE_DIR" ]; then
      echo "  Restoring venv from cache: $CACHE_KEY"
      cp -a "$CACHE_DIR" .venv
    fi

    bash install_requirements.sh
    export PATH="$PWD/.venv/bin:$PATH"

    mkdir -p "$VENV_CACHE_DIR"
    # Replace stale cache entry atomically via a temp dir + mv.
    TMP_CACHE="$(mktemp -d -p "$VENV_CACHE_DIR")"
    cp -a .venv "$TMP_CACHE/venv"
    rm -rf "$CACHE_DIR"
    mv "$TMP_CACHE/venv" "$CACHE_DIR"
    rmdir "$TMP_CACHE"

    scons || true
  ) || true

  mkdir -p "$HTML_OUT/$dir"
  if [ -d "$WORK_DIR/html" ]; then
    cp -r "$WORK_DIR/html/." "$HTML_OUT/$dir/"
    echo "  Copied html/ -> $HTML_OUT/$dir/"
  else
    echo "  WARNING: $WORK_DIR/html missing for branch $branch; subdir will be empty"
  fi

  rm -rf "$WORK_DIR"
done < <(read_deploy_json)
