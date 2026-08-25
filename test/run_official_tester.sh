#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT_DIR"

if [ "$(uname -m)" != "x86_64" ]; then
	echo "The supplied tester binaries require an x86_64/amd64 Linux environment." >&2
	exit 1
fi

if [ ! -x ./tester ] || [ ! -x ./cgi_tester ]; then
	echo "tester and cgi_tester must have executable permissions." >&2
	exit 1
fi

FIXTURE_DIR="$ROOT_DIR/test/official-fixture"
CONFIG_TEMPLATE="$ROOT_DIR/test/official-test.conf.in"
TEMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/webserv-official-tester.XXXXXX")
CONFIG_FILE="$TEMP_DIR/official-test.conf"
SERVER_LOG="$TEMP_DIR/server.log"
UPLOAD_DIR="$TEMP_DIR/uploads"
server_pid=""

cleanup() {
	if [ -n "$server_pid" ]; then
		kill "$server_pid" 2>/dev/null || true
		wait "$server_pid" 2>/dev/null || true
	fi
	if [ -L "$FIXTURE_DIR/directory" ]; then
		rm -f "$FIXTURE_DIR/directory"
	fi
	rm -rf "$TEMP_DIR"
}
trap cleanup EXIT INT TERM

if [ -e "$FIXTURE_DIR/directory" ] || [ -L "$FIXTURE_DIR/directory" ]; then
	echo "$FIXTURE_DIR/directory already exists; remove it before running the tester." >&2
	exit 1
fi

mkdir -p "$UPLOAD_DIR"
ln -s YoupiBanane "$FIXTURE_DIR/directory"
sed \
	-e "s|__FIXTURE__|$FIXTURE_DIR|g" \
	-e "s|__CGI__|$ROOT_DIR/cgi_tester|g" \
	-e "s|__UPLOAD__|$UPLOAD_DIR|g" \
	"$CONFIG_TEMPLATE" >"$CONFIG_FILE"

make re

./webserv "$CONFIG_FILE" >"$SERVER_LOG" 2>&1 &
server_pid=$!
sleep 1

set +e
printf '\n\n\n\n' | ./tester http://127.0.0.1:8080
tester_status=$?
set -e

if [ "$tester_status" -ne 0 ]; then
	echo "webserv log:" >&2
	sed -n '1,160p' "$SERVER_LOG" >&2
fi

exit "$tester_status"
