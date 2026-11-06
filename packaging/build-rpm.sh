#!/bin/bash
# Build the RPMs from the current git tree (committed state plus any
# uncommitted changes to tracked files; new untracked files are not
# included unless 'git add'ed first). Thin wrapper over "make rpm",
# which follows the dist/rpm layout of the Tek4010 and vt05 projects:
#
#   make rpm          tarball in ./dist, RPMs in ./dist-rpm
#   make rpmlint      build, then rpmlint with packaging/rpmlint.toml
#
# Usage: packaging/build-rpm.sh [outdir]    default outdir: ./dist-rpm
set -e
cd "$(git rev-parse --show-toplevel)"
if [ $# -gt 0 ]; then
    out=$(realpath -m "$1")
    exec make rpm RPM_DIR="$out" DIST_DIR="$out/dist"
fi
exec make rpm
