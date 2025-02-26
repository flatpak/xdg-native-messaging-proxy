#!/usr/bin/env bash
#
# - Runs pytest with the required environment to run tests on an x-n-m-p build
# - By default, the tests run on the firstbuild directory that is found inside
#   the source tree
# - The BUILDDIR environment variable can be set to a specific build directory
# - All arguments are passed along to pytest
#
# Examples:
#
#   ./run-test.sh ./test_xnmp.py -k test_cat -s
#

set -euo pipefail

function fail()
{
  sed -n '/^#$/,/^$/p' "${BASH_SOURCE[0]}"
  echo "$1"
  exit 1
}

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

PYTEST=$(command -v "pytest-3" || command -v "pytest") || fail "pytest is missing"

BUILDDIR=${BUILDDIR:-$(find "${SCRIPT_DIR}/.." -maxdepth 2 -name "build.ninja" -printf "%h\n" -quit)}

[ ! -f "${BUILDDIR}/build.ninja" ] && fail "Path '${BUILDDIR}' does not appear to be a build dir"

echo "Running tests on build dir: $(readlink -f "${BUILDDIR}")"
echo ""

export XDG_NATIVE_MESSAGING_PROXY_PATH="$BUILDDIR/src/xdg-native-messaging-proxy"

exec "$PYTEST" "$@"
