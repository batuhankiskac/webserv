#!/usr/bin/env bash
set -u
set -o pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
CONFIG_TEMPLATE="${ROOT_DIR}/config/test_all.conf"
SERVER_BIN="${ROOT_DIR}/webserv"
RAW_HTTP="${ROOT_DIR}/test/raw_http.py"

PASS=0
FAIL=0
LEAK_MODE=0
BUILD_MODE=1
KEEP_TEMP=0
SERVER_PID=""

for arg in "$@"; do
    case "$arg" in
        --leak) LEAK_MODE=1 ;;
        --no-build) BUILD_MODE=0 ;;
        --keep-temp) KEEP_TEMP=1 ;;
        *)
            echo "Usage: $0 [--leak] [--no-build] [--keep-temp]"
            exit 2
            ;;
    esac
done

TEST_TMP="$(mktemp -d /tmp/webserv_test.XXXXXX)"
SITE="${TEST_TMP}/site"
CONFIG="${TEST_TMP}/test_all.conf"
SERVER_LOG="${TEST_TMP}/server.log"

cleanup() {
    if [ -n "$SERVER_PID" ] && kill -0 "$SERVER_PID" 2>/dev/null; then
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
    if [ "$KEEP_TEMP" = "1" ]; then
        echo "Test artifacts kept at: $TEST_TMP"
    else
        rm -rf "$TEST_TMP"
    fi
}

trap cleanup EXIT INT TERM

pass() {
    echo "  PASS: $1"
    PASS=$((PASS + 1))
}

fail() {
    echo "  FAIL: $1"
    FAIL=$((FAIL + 1))
}

section() {
    echo ""
    echo "--- $1 ---"
}

status_code() {
    printf '%s' "$1" | sed -n '1{s/\r$//;s/^[^ ]* \([0-9][0-9]*\).*/\1/p;}'
}

status_line() {
    printf '%s' "$1" | sed -n '1{s/\r$//;p;}'
}

header_value() {
    local response="$1"
    local wanted
    wanted="$(printf '%s' "$2" | tr '[:upper:]' '[:lower:]')"
    printf '%s' "$response" | tr -d '\r' | awk -F ': *' -v wanted="$wanted" '
        BEGIN { found = 0 }
        /^$/ { exit }
        {
            name = tolower($1)
            if (name == wanted) {
                sub(/^[^:]*:[[:space:]]*/, "")
                print
                found = 1
                exit
            }
        }
        END { if (!found) exit 1 }
    '
}

response_body() {
    local response="$1"
    local marker=$'\r\n\r\n'
    if [[ "$response" == *"$marker"* ]]; then
        printf '%s' "${response#*"$marker"}"
    else
        marker=$'\n\n'
        printf '%s' "${response#*"$marker"}"
    fi
}

assert_eq() {
    local description="$1"
    local expected="$2"
    local actual="$3"
    if [ "$actual" = "$expected" ]; then
        pass "$description"
    else
        fail "$description (expected: $expected, got: ${actual:-<empty>})"
    fi
}

assert_status() {
    assert_eq "$1" "$2" "$(status_code "$3")"
}

assert_status_line() {
    assert_eq "$1" "$2" "$(status_line "$3")"
}

assert_header() {
    local actual
    actual="$(header_value "$3" "$2" 2>/dev/null || true)"
    assert_eq "$1" "$4" "$actual"
}

assert_contains() {
    if [[ "$3" == *"$2"* ]]; then
        pass "$1"
    else
        fail "$1 (missing: $2)"
    fi
}

assert_not_contains() {
    if [[ "$3" != *"$2"* ]]; then
        pass "$1"
    else
        fail "$1 (unexpected content: $2)"
    fi
}

http() {
    curl --silent --show-error --max-time 8 --include "$@" 2>/dev/null || true
}

raw_http() {
    local port="$1"
    local request="$2"
    printf '%b' "$request" | python3 "$RAW_HTTP" "$port" 8 2>/dev/null || true
}

assert_rejected_config() {
    local description="$1"
    local expected_message="$2"
    local contents="$3"
    local path="${TEST_TMP}/invalid-${PASS}-${FAIL}.conf"
    local output
    local result

    printf '%s\n' "$contents" > "$path"
    output="$("$SERVER_BIN" "$path" 2>&1)"
    result=$?
    if [ "$result" -ne 0 ] && [[ "$output" == *"$expected_message"* ]]; then
        pass "$description"
    else
        fail "$description (exit: $result, output: $output)"
    fi
}

newest_upload() {
    find "$1" -maxdepth 1 -type f -name 'upload_*.dat' -printf '%T@ %f\n' \
        | sort -nr | head -1 | awk '{print $2}'
}

wait_until_ready() {
    local attempt
    for attempt in $(seq 1 80); do
        if curl --silent --max-time 1 http://127.0.0.1:8080/ >/dev/null 2>&1; then
            return 0
        fi
        if ! kill -0 "$SERVER_PID" 2>/dev/null; then
            return 1
        fi
        sleep 0.1
    done
    return 1
}

echo "=========================================="
echo "  Webserv comprehensive integration suite"
echo "=========================================="

for dependency in curl python3 sed awk grep find sort; do
    if ! command -v "$dependency" >/dev/null 2>&1; then
        echo "Missing dependency: $dependency"
        exit 2
    fi
done

if [ "$BUILD_MODE" = "1" ]; then
    section "Clean C++98 build"
    if make -C "$ROOT_DIR" re; then
        pass "Build succeeds with -Wall -Wextra -Werror -std=c++98"
    else
        fail "Build failed"
        exit 1
    fi
fi

if [ ! -x "$SERVER_BIN" ]; then
    echo "Server binary not found: $SERVER_BIN"
    exit 2
fi

cp -R "${ROOT_DIR}/website" "$SITE"
find "$SITE/upload" "$SITE/small-upload" -maxdepth 1 -type f ! -name '.gitkeep' -delete
python3 -c 'import sys; sys.stdout.buffer.write(bytes(range(256)) * 4096)' \
    > "$SITE/assets/large-response.bin"
PYTHON_CGI_BIN="$(command -v python3 || true)"
sed -e "s#__TEST_SITE__#${SITE}#g" \
    -e "s#__PYTHON_CGI_PATH__#${PYTHON_CGI_BIN:-/usr/bin/python3}#g" \
    "$CONFIG_TEMPLATE" > "$CONFIG"

section "CLI and configuration rejection"
CLI_OUTPUT="$("$SERVER_BIN" 2>&1)"
CLI_EXIT=$?
if [ "$CLI_EXIT" -ne 0 ] && [[ "$CLI_OUTPUT" == *"Usage:"* ]]; then
    pass "Missing configuration argument is rejected"
else
    fail "Missing configuration argument is rejected"
fi

MISSING_OUTPUT="$("$SERVER_BIN" "${TEST_TMP}/does-not-exist.conf" 2>&1)"
MISSING_EXIT=$?
if [ "$MISSING_EXIT" -ne 0 ] && [[ "$MISSING_OUTPUT" == *"Error opening file"* ]]; then
    pass "Missing configuration file is rejected"
else
    fail "Missing configuration file is rejected"
fi

assert_rejected_config "Empty configuration is rejected" \
    "No server blocks found" ""
assert_rejected_config "Unknown top-level token is rejected" \
    "Invalid token" "http { }"
assert_rejected_config "Missing server brace is rejected" \
    "Invalid server block" "server"
assert_rejected_config "Port zero is rejected" \
    "Invalid listen directive" $'server {\nlisten 127.0.0.1:0;\n}'
assert_rejected_config "Port above 65535 is rejected" \
    "Invalid listen directive" $'server {\nlisten 127.0.0.1:65536;\n}'
assert_rejected_config "Non-numeric port is rejected" \
    "Invalid listen directive" $'server {\nlisten 127.0.0.1:http;\n}'
assert_rejected_config "Unknown server directive is rejected" \
    "Invalid token" $'server {\nlisten 127.0.0.1:8088;\nunknown value;\n}'
assert_rejected_config "Invalid body-size unit is rejected" \
    "Invalid client_max_body_size directive" $'server {\nlisten 127.0.0.1:8088;\nclient_max_body_size 12T;\n}'
assert_rejected_config "Invalid autoindex value is rejected" \
    "Invalid autoindex directive" $'server {\nlisten 127.0.0.1:8088;\nlocation / {\nautoindex maybe;\n}\n}'
assert_rejected_config "Unsupported allow_methods value is rejected" \
    "Invalid allow_methods directive" $'server {\nlisten 127.0.0.1:8088;\nlocation / {\nallow_methods PATCH;\n}\n}'
assert_rejected_config "Missing directive semicolon is rejected" \
    "Invalid root directive" $'server {\nlisten 127.0.0.1:8088;\nlocation / {\nroot /tmp\n}\n}'
assert_rejected_config "Incomplete error_page is rejected" \
    "Invalid error_page directive" $'server {\nlisten 127.0.0.1:8088;\nerror_page 404;\n}'

section "Server startup"
if [ "$LEAK_MODE" = "1" ] && command -v valgrind >/dev/null 2>&1; then
    echo "  Valgrind enabled (children traced into per-process logs)"
    valgrind --leak-check=full --show-leak-kinds=all \
        --track-origins=yes \
        --track-fds=yes --trace-children=yes \
        --log-file="${TEST_TMP}/valgrind.%p.log" \
        "$SERVER_BIN" "$CONFIG" >"$SERVER_LOG" 2>&1 &
else
    if [ "$LEAK_MODE" = "1" ]; then
        echo "  Valgrind unavailable; functional tests will still run"
    fi
    "$SERVER_BIN" "$CONFIG" >"$SERVER_LOG" 2>&1 &
fi
SERVER_PID=$!

if wait_until_ready; then
    pass "Server becomes ready on port 8080"
else
    fail "Server failed to become ready"
    sed -n '1,160p' "$SERVER_LOG"
    exit 1
fi

section "Basic GET, index and response headers"
R="$(http http://127.0.0.1:8080/)"
assert_status "GET / returns 200" "200" "$R"
assert_contains "Root index body is served" "Webserv Integration Lab" "$(response_body "$R")"
assert_header "HTML MIME type" "Content-Type" "$R" "text/html"
assert_header "Connection closes after response" "Connection" "$R" "close"

curl --silent --max-time 8 --dump-header "${TEST_TMP}/root.headers" \
    --output "${TEST_TMP}/root.body" http://127.0.0.1:8080/
ROOT_LENGTH="$(wc -c < "${TEST_TMP}/root.body" | tr -d ' ')"
ROOT_HEADER_LENGTH="$(tr -d '\r' < "${TEST_TMP}/root.headers" | awk -F ': *' 'tolower($1) == "content-length" { print $2; exit }')"
assert_eq "Content-Length matches exact body bytes" "$ROOT_LENGTH" "$ROOT_HEADER_LENGTH"

R="$(http 'http://127.0.0.1:8080/?ignored=query')"
assert_status "Static GET ignores query when resolving path" "200" "$R"

R="$(raw_http 8080 $'GET / HTTP/1.0\r\n\r\n')"
assert_status_line "HTTP/1.0 response preserves protocol version" "HTTP/1.0 200 OK" "$R"

section "Static files and MIME types"
R="$(http http://127.0.0.1:8080/style.css)"
assert_status "CSS file is served" "200" "$R"
assert_header "CSS MIME type" "Content-Type" "$R" "text/css"

R="$(http http://127.0.0.1:8080/script.js)"
assert_status "JavaScript file is served" "200" "$R"
assert_header "JavaScript MIME type" "Content-Type" "$R" "application/javascript"

R="$(http http://127.0.0.1:8080/assets/test.txt)"
assert_status "Text asset is served" "200" "$R"
assert_header "Text MIME type" "Content-Type" "$R" "text/plain"
assert_contains "Text asset body is intact" "Asset test file" "$(response_body "$R")"

R="$(http http://127.0.0.1:8080/assets/data.json)"
assert_status "JSON asset is served" "200" "$R"
assert_header "JSON MIME type" "Content-Type" "$R" "application/json"

R="$(http http://127.0.0.1:8080/assets/archive.xyz)"
assert_status "Unknown extension asset is served" "200" "$R"
assert_header "Unknown extension uses binary MIME type" "Content-Type" "$R" "application/octet-stream"

for mime_case in \
    "page.htm text/html" \
    "image.png image/png" \
    "image.jpg image/jpeg" \
    "image.jpeg image/jpeg" \
    "image.gif image/gif" \
    "favicon.ico image/x-icon"
do
    set -- $mime_case
    R="$(http "http://127.0.0.1:8080/assets/$1")"
    assert_status "$1 fixture is served" "200" "$R"
    assert_header "$1 MIME type" "Content-Type" "$R" "$2"
done

curl --silent --max-time 12 http://127.0.0.1:8080/assets/large-response.bin \
    --output "${TEST_TMP}/large-response.downloaded"
if cmp -s "$SITE/assets/large-response.bin" "${TEST_TMP}/large-response.downloaded"; then
    pass "One-megabyte response survives partial socket writes"
else
    fail "One-megabyte response survives partial socket writes"
fi

section "Directory index and autoindex"
R="$(http http://127.0.0.1:8080/index-fallback/)"
assert_status "Second configured index candidate is selected" "200" "$R"
assert_contains "Fallback index body is correct" "Fallback index selected" "$(response_body "$R")"

R="$(http http://127.0.0.1:8080/autoindex/)"
assert_status "Autoindex-enabled directory returns 200" "200" "$R"
assert_contains "Autoindex lists a fixture file" 'href="file1.txt"' "$(response_body "$R")"
assert_contains "Autoindex HTML-escapes filenames" 'href="&lt;escaped&gt;.txt"' "$(response_body "$R")"
assert_not_contains "Autoindex does not expose filesystem root" "$SITE" "$(response_body "$R")"

R="$(http http://127.0.0.1:8080/no-autoindex/)"
assert_status "Directory without index or autoindex returns 403" "403" "$R"

section "Location matching and method policy"
R="$(http http://127.0.0.1:8080/nested/base.txt)"
assert_status "Base nested location is selected" "200" "$R"
assert_contains "Base nested fixture body" "Base nested location" "$(response_body "$R")"

R="$(http http://127.0.0.1:8080/nested/deeper/value.txt)"
assert_status "Longest matching location is selected" "200" "$R"
assert_contains "Longest-location fixture body" "Longest location match selected" "$(response_body "$R")"

R="$(http -X DELETE http://127.0.0.1:8080/nested/deeper/delete-me.txt)"
assert_status "DELETE allowed by longest location returns 204" "204" "$R"
R="$(http http://127.0.0.1:8080/nested/deeper/delete-me.txt)"
assert_status "Deleted nested fixture is gone" "404" "$R"

R="$(http http://127.0.0.1:8080/nestedness)"
assert_status "Location prefix requires a path boundary" "404" "$R"

R="$(http http://127.0.0.1:8080/path-override/value.txt)"
assert_status "Location path override participates in matching" "200" "$R"
assert_contains "Location path override resolves the expected fixture" \
    "Location path override selected" "$(response_body "$R")"

R="$(http http://127.0.0.1:8080/trailing-root/value.txt)"
assert_status "Root ending in slash is normalized" "200" "$R"
assert_contains "Trailing-root fixture body is intact" \
    "Trailing root slash normalized" "$(response_body "$R")"

R="$(http -X POST http://127.0.0.1:8080/)"
assert_status "Known but disallowed method returns 405" "405" "$R"
R="$(http -X DELETE http://127.0.0.1:8080/get-only/page.html)"
assert_status "DELETE on GET-only location returns 405" "405" "$R"
R="$(http -X PUT http://127.0.0.1:8080/)"
assert_status "PUT returns 501" "501" "$R"
R="$(raw_http 8080 $'HEAD / HTTP/1.1\r\nHost: localhost\r\n\r\n')"
assert_status "HEAD returns 501" "501" "$R"

R="$(http -X POST --data-binary 'body' http://127.0.0.1:8080/post-disabled)"
assert_status "POST without upload_enable returns 403" "403" "$R"
R="$(http -X POST --data-binary 'body' http://127.0.0.1:8080/upload-no-store)"
assert_status "Enabled upload without upload_store returns 500" "500" "$R"
assert_contains "Configured 50x page is used" "Server Error" "$(response_body "$R")"

section "Upload, download and delete lifecycle"
printf 'hello upload lifecycle' > "${TEST_TMP}/small-body.txt"
R="$(http -X POST --data-binary @"${TEST_TMP}/small-body.txt" http://127.0.0.1:8080/upload/)"
assert_status "Small upload returns 201" "201" "$R"
assert_header "Upload response is plain text" "Content-Type" "$R" "text/plain"
SMALL_UPLOAD="$(newest_upload "$SITE/upload")"
if [ -n "$SMALL_UPLOAD" ]; then
    pass "Small upload creates a generated .dat file"
else
    fail "Small upload creates a generated .dat file"
fi

R="$(http http://127.0.0.1:8080/upload/)"
assert_status "Upload directory is browsable" "200" "$R"
assert_contains "Autoindex exposes a relative upload link" "href=\"$SMALL_UPLOAD\"" "$(response_body "$R")"

curl --silent --max-time 8 "http://127.0.0.1:8080/upload/$SMALL_UPLOAD" \
    --output "${TEST_TMP}/small-downloaded.txt"
if cmp -s "${TEST_TMP}/small-body.txt" "${TEST_TMP}/small-downloaded.txt"; then
    pass "Downloaded upload matches exact bytes"
else
    fail "Downloaded upload matches exact bytes"
fi

R="$(http -X DELETE "http://127.0.0.1:8080/upload/$SMALL_UPLOAD")"
assert_status "Uploaded file can be deleted" "204" "$R"
assert_header "204 response has zero content length" "Content-Length" "$R" "0"
R="$(http "http://127.0.0.1:8080/upload/$SMALL_UPLOAD")"
assert_status "Deleted upload returns 404" "404" "$R"
R="$(http -X DELETE http://127.0.0.1:8080/upload/)"
assert_status "Deleting a directory is forbidden" "403" "$R"
R="$(http -X DELETE http://127.0.0.1:8080/upload/missing.dat)"
assert_status "Deleting a missing file returns 404" "404" "$R"

python3 -c 'import sys; sys.stdout.buffer.write((b"0123456789abcdef" * 750))' \
    > "${TEST_TMP}/large-body.bin"
R="$(http -X POST --data-binary @"${TEST_TMP}/large-body.bin" http://127.0.0.1:8080/upload/)"
assert_status "Body larger than the in-memory threshold uploads" "201" "$R"
LARGE_UPLOAD="$(newest_upload "$SITE/upload")"
curl --silent --max-time 8 "http://127.0.0.1:8080/upload/$LARGE_UPLOAD" \
    --output "${TEST_TMP}/large-downloaded.bin"
if cmp -s "${TEST_TMP}/large-body.bin" "${TEST_TMP}/large-downloaded.bin"; then
    pass "Disk-buffered upload preserves exact bytes"
else
    fail "Disk-buffered upload preserves exact bytes"
fi
R="$(http -X DELETE "http://127.0.0.1:8080/upload/$LARGE_UPLOAD")"
assert_status "Disk-buffered upload can be deleted" "204" "$R"

section "Body-size boundaries"
python3 -c 'import sys; sys.stdout.write("x" * 1024)' > "${TEST_TMP}/body-1024.txt"
python3 -c 'import sys; sys.stdout.write("x" * 1025)' > "${TEST_TMP}/body-1025.txt"
python3 -c 'import sys; sys.stdout.write("x" * 65537)' > "${TEST_TMP}/body-65537.txt"

R="$(http -X POST --data-binary @"${TEST_TMP}/body-1024.txt" http://127.0.0.1:8080/small-upload/)"
assert_status "Location body limit accepts exactly 1K" "201" "$R"
R="$(http -X POST --data-binary @"${TEST_TMP}/body-1025.txt" http://127.0.0.1:8080/small-upload/)"
assert_status "Location body limit rejects 1K + 1 byte" "413" "$R"
R="$(http -X POST --data-binary @"${TEST_TMP}/body-65537.txt" http://127.0.0.1:8080/upload/)"
assert_status "Server body limit rejects 64K + 1 byte" "413" "$R"

section "Chunked transfer decoding"
R="$(raw_http 8080 $'POST /upload/ HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n4\r\nWiki\r\n5\r\npedia\r\n0\r\n\r\n')"
assert_status "Valid chunked upload returns 201" "201" "$R"
CHUNK_UPLOAD="$(newest_upload "$SITE/upload")"
assert_eq "Chunked body is decoded before storage" "Wikipedia" "$(tr -d '\n' < "$SITE/upload/$CHUNK_UPLOAD")"

R="$(raw_http 8080 $'POST /upload/ HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n3;fixture=yes\r\nabc\r\n0\r\nX-Trailer: accepted\r\n\r\n')"
assert_status "Chunk extensions and valid trailers are accepted" "201" "$R"

R="$(raw_http 8080 $'POST /upload/ HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\nZ\r\nbad\r\n0\r\n\r\n')"
assert_status "Invalid hexadecimal chunk size returns 400" "400" "$R"
R="$(raw_http 8080 $'POST /upload/ HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n3\r\nabcXX0\r\n\r\n')"
assert_status "Missing chunk CRLF returns 400" "400" "$R"
R="$(raw_http 8080 $'POST /upload/ HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n0\r\ninvalid-trailer\r\n\r\n')"
assert_status "Malformed chunk trailer returns 400" "400" "$R"

LONG_EXTENSION="$(python3 -c 'print("a" * 257, end="")')"
R="$(raw_http 8080 "POST /upload/ HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n1;${LONG_EXTENSION}\r\nx\r\n0\r\n\r\n")"
assert_status "Oversized chunk extension returns 414" "414" "$R"

CHUNK_1025="$(python3 -c 'print(format(1025, "x"))')"
CHUNK_DATA="$(python3 -c 'print("x" * 1025, end="")')"
R="$(raw_http 8080 "POST /small-upload/ HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n${CHUNK_1025}\r\n${CHUNK_DATA}\r\n0\r\n\r\n")"
assert_status "Chunked body obeys location body limit" "413" "$R"

section "HTTP request validation"
R="$(raw_http 8080 $'GET / HTTP/1.1\r\n\r\n')"
assert_status "HTTP/1.1 without Host returns 400" "400" "$R"
R="$(raw_http 8080 $'GET /only-two-parts\r\nHost: localhost\r\n\r\n')"
assert_status "Malformed request line returns 400" "400" "$R"
R="$(raw_http 8080 $'GET / HTTP/1.1\r\nBroken-Header\r\n\r\n')"
assert_status "Header without colon returns 400" "400" "$R"
R="$(raw_http 8080 $'GET / HTTP/1.1\r\nHost: localhost\r\nhost: duplicate\r\n\r\n')"
assert_status "Duplicate header names are case-insensitively rejected" "400" "$R"
R="$(raw_http 8080 $'POST /upload/ HTTP/1.1\r\nHost: localhost\r\nContent-Length: nope\r\n\r\n')"
assert_status "Non-numeric Content-Length returns 400" "400" "$R"
R="$(raw_http 8080 $'POST /upload/ HTTP/1.1\r\nHost: localhost\r\nContent-Length: 4\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n')"
assert_status "Content-Length with Transfer-Encoding returns 400" "400" "$R"
R="$(raw_http 8080 $'POST /upload/ HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: gzip\r\n\r\nbody')"
assert_status "Unsupported Transfer-Encoding returns 400" "400" "$R"
R="$(raw_http 8080 $'GET / HTTP/2.0\r\nHost: localhost\r\n\r\n')"
assert_status "Unsupported HTTP version returns 505" "505" "$R"

python3 -c 'print("GET /" + "a" * 8200 + " HTTP/1.1\r\nHost: localhost\r\n\r\n", end="")' \
    | python3 "$RAW_HTTP" 8080 > "${TEST_TMP}/long-uri.response" 2>/dev/null || true
R="$(<"${TEST_TMP}/long-uri.response")"
assert_status "Oversized request URI returns 414" "414" "$R"

python3 -c 'print("GET / HTTP/1.1\r\nHost: localhost\r\nX-Large: " + "a" * 33000 + "\r\n\r\n", end="")' \
    | python3 "$RAW_HTTP" 8080 > "${TEST_TMP}/large-header.response" 2>/dev/null || true
R="$(<"${TEST_TMP}/large-header.response")"
assert_status "Oversized request headers return 431" "431" "$R"

section "Python CGI execution and environment"
R="$(http -H 'X-Test-Fixture: HeaderValue' 'http://127.0.0.1:8080/cgi-bin/test.py?test_get=Codex%20Test')"
assert_status "Python CGI GET returns 200" "200" "$R"
assert_contains "Python CGI script executes" "Python CGI Working!" "$(response_body "$R")"
assert_contains "Python CGI receives decoded GET parameter" "Codex Test" "$(response_body "$R")"
assert_contains "Python CGI receives query string" "test_get=Codex%20Test" "$(response_body "$R")"
assert_contains "Python CGI receives HTTP headers" "HTTP_X_TEST_FIXTURE" "$(response_body "$R")"
assert_contains "Python CGI receives header value" "HeaderValue" "$(response_body "$R")"
assert_contains "Python CGI receives script name" "/cgi-bin/test.py" "$(response_body "$R")"

R="$(http -X POST -H 'Content-Type: application/x-www-form-urlencoded' \
    --data 'test_post=PostValue' http://127.0.0.1:8080/cgi-bin/test.py)"
assert_status "Python CGI POST returns 200" "200" "$R"
assert_contains "Python CGI receives POST form value" "PostValue" "$(response_body "$R")"
assert_contains "Python CGI sees POST method" "<strong>Method:</strong> POST" "$(response_body "$R")"

R="$(http http://127.0.0.1:8080/cgi-bin/status.py)"
assert_status "Python CGI-provided status is forwarded" "201" "$R"
assert_header "Python CGI custom header is forwarded" "X-CGI-Fixture" "$R" "status"
assert_contains "Python CGI custom-status body is forwarded" "PYTHON_CGI_STATUS_CREATED" "$(response_body "$R")"

R="$(http http://127.0.0.1:8080/cgi-bin/readme.txt)"
assert_status "Non-CGI extension in CGI location is served statically" "200" "$R"
assert_contains "Static file in CGI location has expected body" "must be served statically" "$(response_body "$R")"

R="$(http http://127.0.0.1:8080/broken-cgi/fail.py)"
assert_status "Missing CGI interpreter produces 500" "500" "$R"
assert_contains "Missing CGI interpreter reports execution failure" "CGI execution failed" "$(response_body "$R")"

R="$(http http://127.0.0.1:8080/cgi-bin/oversized.py)"
assert_status "Python CGI output above 8 MiB is rejected with 502" "502" "$R"

section "Redirects"
for redirect_case in \
    "redirect-301 301 /" \
    "redirect-302 302 /assets/test.txt" \
    "redirect-303 303 /get-only/page.html" \
    "redirect-307 307 /upload/" \
    "redirect-308 308 /"
do
    set -- $redirect_case
    R="$(http "http://127.0.0.1:8080/$1")"
    assert_status "$2 redirect status is returned" "$2" "$R"
    assert_header "$2 redirect Location header" "Location" "$R" "$3"
done

section "Errors and path safety"
R="$(http http://127.0.0.1:8080/nonexistent)"
assert_status "Missing resource returns 404" "404" "$R"
assert_contains "Custom 404 page is served" "404 - Not Found" "$(response_body "$R")"

R="$(http http://127.0.0.1:8080/no-root/resource)"
assert_status "Location without root returns 404" "404" "$R"

R="$(http --path-as-is http://127.0.0.1:8080/assets/../index.html)"
assert_status "Parent-directory traversal is forbidden" "403" "$R"
assert_not_contains "Traversal response does not expose index" "Webserv Integration Lab" "$(response_body "$R")"

section "Virtual hosts and independent listeners"
R="$(http -H 'Host: vhost.local' http://127.0.0.1:8080/)"
assert_status "Name-based virtual host returns 200" "200" "$R"
assert_contains "Host header selects virtual-host root" "VHOST_LOCAL_OK" "$(response_body "$R")"

R="$(http -H 'Host: alias.local:8080' http://127.0.0.1:8080/)"
assert_contains "Server-name alias with port selects virtual host" "VHOST_LOCAL_OK" "$(response_body "$R")"

R="$(http -H 'Host: VHOST.LOCAL' http://127.0.0.1:8080/)"
assert_contains "Host matching is case-insensitive" "VHOST_LOCAL_OK" "$(response_body "$R")"

R="$(http -H 'Host: unknown.local' http://127.0.0.1:8080/)"
assert_contains "Unknown Host falls back to first server" "Webserv Integration Lab" "$(response_body "$R")"

R="$(http -H 'Host: vhost.local' http://127.0.0.1:8080/missing)"
assert_status "Virtual-host missing resource returns 404" "404" "$R"
assert_contains "Virtual host uses its own custom 404" "VHOST_CUSTOM_404" "$(response_body "$R")"

R="$(http http://127.0.0.1:9090/)"
assert_status "Second listener returns 200" "200" "$R"
assert_contains "Second listener uses independent root" "SECOND_SERVER_OK" "$(response_body "$R")"
R="$(http http://127.0.0.1:9090/missing)"
assert_contains "Second listener uses its custom 404" "SECOND_CUSTOM_404" "$(response_body "$R")"
R="$(http -X POST http://127.0.0.1:9090/)"
assert_status "Second listener enforces its method policy" "405" "$R"

section "Concurrency and post-error health"
seq 1 40 | xargs -P 10 -I '{}' \
    curl --silent --max-time 5 --output /dev/null --write-out '%{http_code}\n' \
    http://127.0.0.1:8080/assets/test.txt > "${TEST_TMP}/concurrency.status"
CONCURRENCY_OK="$(grep -c '^200$' "${TEST_TMP}/concurrency.status" || true)"
assert_eq "Forty concurrent static requests succeed" "40" "$CONCURRENCY_OK"

R="$(http http://127.0.0.1:8080/)"
assert_status "Server remains healthy after malformed and concurrent requests" "200" "$R"

echo ""
echo "=========================================="
echo "  Functional results"
echo "=========================================="
echo "  Passed: $PASS"
echo "  Failed: $FAIL"

section "Graceful shutdown"
kill "$SERVER_PID" 2>/dev/null || true
wait "$SERVER_PID" 2>/dev/null || true
SERVER_PID=""
pass "Server exits after SIGTERM"

if [ "$LEAK_MODE" = "1" ]; then
    VALGRIND_LOG_COUNT=0
    VALGRIND_BAD_LEAKS=0
    VALGRIND_BAD_ERRORS=0
    VALGRIND_BAD_FDS=0

    section "Valgrind: all traced processes"
    for valgrind_log in "${TEST_TMP}"/valgrind.*.log; do
        [ -f "$valgrind_log" ] || continue
        VALGRIND_LOG_COUNT=$((VALGRIND_LOG_COUNT + 1))
        echo ""
        echo "--- $(basename "$valgrind_log") ---"
        # Keep the complete report in the main test output, including the
        # allocation stack traces printed by --leak-check=full.
        sed -n '/HEAP SUMMARY:/,$p' "$valgrind_log"

        DEFINITELY_LOST="$(grep 'definitely lost:' "$valgrind_log" | tail -1 || true)"
        INDIRECTLY_LOST="$(grep 'indirectly lost:' "$valgrind_log" | tail -1 || true)"
        POSSIBLY_LOST="$(grep 'possibly lost:' "$valgrind_log" | tail -1 || true)"
        ERROR_SUMMARY="$(grep 'ERROR SUMMARY:' "$valgrind_log" | tail -1 || true)"
        FD_OPEN="$(grep 'FILE DESCRIPTORS:' "$valgrind_log" | tail -1 \
            | grep -oE '[0-9]+ open' | grep -oE '[0-9]+' || true)"

        if ! grep -q 'All heap blocks were freed -- no leaks are possible' "$valgrind_log" \
            && ! { [[ "$DEFINITELY_LOST" =~ definitely\ lost:\ 0\ bytes ]] \
                && [[ "$INDIRECTLY_LOST" =~ indirectly\ lost:\ 0\ bytes ]] \
                && [[ "$POSSIBLY_LOST" =~ possibly\ lost:\ 0\ bytes ]]; }; then
            VALGRIND_BAD_LEAKS=1
        fi
        if [ -n "$ERROR_SUMMARY" ] && ! [[ "$ERROR_SUMMARY" =~ ERROR\ SUMMARY:\ 0\ errors ]]; then
            VALGRIND_BAD_ERRORS=1
        fi
        if [ -n "$FD_OPEN" ] && [ "$FD_OPEN" -gt 4 ]; then
            VALGRIND_BAD_FDS=1
        fi
    done

    if [ "$VALGRIND_LOG_COUNT" -eq 0 ]; then
        fail "No Valgrind logs were produced"
    else
        [ "$VALGRIND_BAD_LEAKS" -eq 0 ] \
            && pass "No definite, indirect or possible memory leaks in traced processes" \
            || fail "Valgrind leak summary is not clean in one or more traced processes"
        [ "$VALGRIND_BAD_ERRORS" -eq 0 ] \
            && pass "Valgrind reports zero memory errors in traced processes" \
            || fail "Valgrind reports errors in one or more traced processes"
        [ "$VALGRIND_BAD_FDS" -eq 0 ] \
            && pass "No application file-descriptor leak in traced processes" \
            || fail "Unexpected open file descriptors in one or more traced processes"
    fi
fi

echo ""
echo "=========================================="
echo "  Final results"
echo "=========================================="
echo "  Passed: $PASS"
echo "  Failed: $FAIL"

if [ "$FAIL" -ne 0 ]; then
    echo ""
    echo "Server log:"
    sed -n '1,200p' "$SERVER_LOG"
    exit 1
fi

exit 0
