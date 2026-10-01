#!/usr/bin/env bash
# Merges upstream/master into origin/master without ever conflicting on the
# version number, and pushes the result as a single merge commit.
#
# Our package.json / package-lock.json version always differs from upstream's,
# so a plain merge would conflict whenever upstream bumps its version. Instead:
#   1. on a detached HEAD at origin/master, set our version to upstream's
#      (throwaway commit, never pushed) so both sides agree on that line
#   2. merge upstream/master with --no-commit (a real conflict aborts + fails)
#   3. set the version to the next release version
#   4. write a merge commit whose parents are origin/master and upstream/master
#      (git commit-tree), so the throwaway commit never reaches history
#
# Needs remotes "origin" and "upstream" and a git user.name/user.email.
# Writes "pushed" and "version" to $GITHUB_OUTPUT when it is set.
# Set DRY_RUN=1 to skip the push (used for local testing).
set -euo pipefail

output() {
  echo "$1=$2"
  if [ -n "${GITHUB_OUTPUT:-}" ]; then echo "$1=$2" >> "$GITHUB_OUTPUT"; fi
}

# set_version <x.y.z>: update package.json and package-lock.json (if present)
set_version() {
  NEW_VERSION="$1" node -e '
    const fs = require("fs");
    const v = process.env.NEW_VERSION;
    for (const f of ["package.json", "package-lock.json"]) {
      if (!fs.existsSync(f)) continue;
      const j = JSON.parse(fs.readFileSync(f, "utf8"));
      if (j.version !== undefined) j.version = v;
      if (j.packages && j.packages[""] && j.packages[""].version !== undefined) {
        j.packages[""].version = v;
      }
      fs.writeFileSync(f, JSON.stringify(j, null, 2) + "\n");
    }'
}

# align_with_upstream: copy each file's version fields from upstream/master, so
# that the version lines are identical on both sides of the merge (even if
# upstream's package.json and package-lock.json versions disagree)
align_with_upstream() {
  node -e '
    const fs = require("fs");
    const { execSync } = require("child_process");
    for (const f of ["package.json", "package-lock.json"]) {
      let up;
      try { up = JSON.parse(execSync("git show upstream/master:" + f, {stdio: ["ignore", "pipe", "ignore"]})); }
      catch (e) { continue; }
      if (!fs.existsSync(f)) continue;
      const j = JSON.parse(fs.readFileSync(f, "utf8"));
      if (up.version !== undefined) j.version = up.version;
      if (up.packages && up.packages[""] && j.packages && j.packages[""]) {
        j.packages[""].version = up.packages[""].version;
      }
      fs.writeFileSync(f, JSON.stringify(j, null, 2) + "\n");
    }'
}

# version_of <rev>: version field of package.json at that revision
version_of() {
  git show "$1:package.json" | node -e 'console.log(JSON.parse(require("fs").readFileSync(0, "utf8")).version)'
}

git fetch origin master
git fetch upstream master

new=$(git rev-list --count origin/master..upstream/master)
echo "Upstream commits to merge: $new"
if [ "$new" -eq 0 ]; then
  output pushed false
  exit 0
fi

our_ver=$(version_of origin/master)
up_ver=$(version_of upstream/master)
echo "Our version: $our_ver, upstream version: $up_ver"

git checkout --detach origin/master
align_with_upstream
if ! git diff --quiet; then
  git commit -q -a -m "throwaway: align version with upstream"
fi

if ! git merge --no-commit --no-ff upstream/master; then
  git merge --abort || true
  echo "::error::Merging upstream conflicts. Locally run: git fetch https://github.com/freakified/TimeStylePebble.git master && git merge FETCH_HEAD, resolve, and push."
  exit 1
fi

# next release version: the higher of the two versions, with the minor bumped
# (the Pebble tooling only keeps major.minor, see the README note / workflow docs)
highest=$(printf '%s\n%s\n' "$our_ver" "$up_ver" | sort -V | tail -n 1)
IFS=. read -r major minor _ <<< "$highest"
next="$major.$((minor + 1)).0"
echo "Next version: $next"

set_version "$next"
git add package.json package-lock.json
tree=$(git write-tree)
sha=$(git commit-tree "$tree" \
  -p origin/master -p upstream/master \
  -m "Merge upstream TimeStyle (v$up_ver)")
echo "Merge commit: $sha"

if [ "${DRY_RUN:-}" = "1" ]; then
  echo "DRY_RUN: not pushing"
  git update-ref refs/heads/dry-run-result "$sha"
else
  git push origin "$sha:refs/heads/master"
fi

output pushed true
output version "$next"
