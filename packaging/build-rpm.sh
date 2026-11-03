#!/bin/bash
# Build the RPMs from the current git tree (committed state plus any
# uncommitted changes to tracked files; new untracked files are not included).
#   packaging/build-rpm.sh [topdir]     default topdir: ./rpmbuild
set -e
cd "$(git rev-parse --show-toplevel)"
top=$(realpath -m "${1:-rpmbuild}")
tree=$(git stash create || true)
tree=${tree:-HEAD}
rev=$(git rev-parse --short HEAD)
date=$(git show -s --format=%cd --date=format:%Y%m%d HEAD)
mkdir -p "$top"/{SOURCES,BUILD,RPMS,SRPMS}
git archive --prefix=pidp11/ "$tree" | gzip > "$top/SOURCES/pidp11.tar.gz"
rpmbuild -ba --define "_topdir $top" --define "gitrev $rev" --define "gitdate $date" \
    packaging/pidp11.spec
