#!/bin/bash
set -e

echo "============================================"
echo "  webserv Docker Test Suite"
echo "============================================"
echo ""

PASS=0
FAIL=0
FAIL_DETAILS=""

check() {
    local name="$1"
    local expected="$2"
    local actual="$3"
    if echo "$actual" | grep -q "$expected"; then
        echo "[PASS] $name"
        PASS=$((PASS + 1))
    else
        echo "[FAIL] $name"
        echo "       Expected: $expected"
        echo "       Got:      $(echo "$actual" | head -1)"
        FAIL=$((FAIL + 1))
        FAIL_DETAILS="$FAIL_DETAILS\n  - $name"
    fi
}

# Wait for server to start
sleep 1

# Clean up old uploads
rm -f /workspace/test_docker/www/upload/temp/* 2>/dev/null
rm -f /workspace/test_docker/www/uploads/temp/* 2>/dev/null

echo "--- 1. GET Static File (index.html) ---"
RESP=$(curl -s -w "\nHTTP_CODE:%{http_code}" http://localhost:8080/)
check "GET index.html returns 200" "HTTP_CODE:200" "$RESP"
check "GET index.html has content" "Hello from webserv" "$RESP"

echo ""
echo "--- 2. GET Static File (test.txt) ---"
RESP=$(curl -s -w "\nHTTP_CODE:%{http_code}" http://localhost:8080/test.txt)
check "GET test.txt returns 200" "HTTP_CODE:200" "$RESP"
check "GET test.txt has content" "plain text file" "$RESP"

echo ""
echo "--- 3. GET 404 (non-existent file) ---"
RESP=$(curl -s -w "\nHTTP_CODE:%{http_code}" http://localhost:8080/nonexistent.html)
check "GET nonexistent returns 404" "HTTP_CODE:404" "$RESP"

echo ""
echo "--- 4. GET Autoindex (directory listing) ---"
RESP=$(curl -s -w "\nHTTP_CODE:%{http_code}" http://localhost:8080/assets/)
check "GET autoindex returns 200" "HTTP_CODE:200" "$RESP"
check "GET autoindex has listing" "Index of" "$RESP"

echo ""
echo "--- 5. Redirect (301) ---"
RESP=$(curl -s -w "\nHTTP_CODE:%{http_code}" -D - http://localhost:8080/old-page 2>&1)
check "GET /old-page returns 301" "301" "$RESP"
check "Redirect has Location header" "Location: /new-page" "$RESP"

echo ""
echo "--- 6. Method Not Allowed ---"
RESP=$(curl -s -w "\nHTTP_CODE:%{http_code}" -X DELETE http://localhost:8080/)
check "DELETE on / returns 405" "HTTP_CODE:405" "$RESP"

echo ""
echo "--- 7. POST Upload ---"
RESP=$(curl -s -w "\nHTTP_CODE:%{http_code}" -X POST -d "test upload data" http://localhost:8080/upload)
check "POST upload returns 201" "HTTP_CODE:201" "$RESP"
check "Upload success message" "Upload successful" "$RESP"

echo ""
echo "--- 8. DELETE uploaded file ---"
UPLOADED=$(ls /workspace/test_docker/www/upload/temp/ | head -1)
if [ -n "$UPLOADED" ]; then
    RESP=$(curl -s -w "\nHTTP_CODE:%{http_code}" -X DELETE "http://localhost:8080/upload/temp/$UPLOADED")
    check "DELETE uploaded file returns 204" "HTTP_CODE:204" "$RESP"
else
    echo "[FAIL] No uploaded file to delete"
    FAIL=$((FAIL + 1))
fi

echo ""
echo "--- 9. CGI GET (hello.py) ---"
RESP=$(curl -s -w "\nHTTP_CODE:%{http_code}" "http://localhost:8080/cgi-bin/hello.py?name=test")
check "CGI GET returns 200" "HTTP_CODE:200" "$RESP"
check "CGI GET has method" "Method: GET" "$RESP"
check "CGI GET has query string" "name=test" "$RESP"

echo ""
echo "--- 10. CGI POST (hello.py) ---"
RESP=$(curl -s -w "\nHTTP_CODE:%{http_code}" -X POST -d "post body data" "http://localhost:8080/cgi-bin/hello.py")
check "CGI POST returns 200" "HTTP_CODE:200" "$RESP"
check "CGI POST has method" "Method: POST" "$RESP"
check "CGI POST received body" "post body data" "$RESP"

echo ""
echo "--- 11. CGI POST with large body (>8KB, temp file path) ---"
LARGE_BODY=$(python3 -c "print('A' * 20000)")
RESP=$(curl -s -w "\nHTTP_CODE:%{http_code}" -X POST -d "$LARGE_BODY" "http://localhost:8080/cgi-bin/hello.py")
check "CGI POST large body returns 200" "HTTP_CODE:200" "$RESP"
check "CGI POST large body received" "Body length received: 20000" "$RESP"

echo ""
echo "--- 12. CGI with custom status ---"
RESP=$(curl -s -w "\nHTTP_CODE:%{http_code}" "http://localhost:8080/cgi-bin/echo.py?code=201")
check "CGI custom status returns 201" "HTTP_CODE:201" "$RESP"

echo ""
echo "--- 13. Second server (port 9090) ---"
RESP=$(curl -s -w "\nHTTP_CODE:%{http_code}" http://localhost:9090/)
check "GET port 9090 returns 200" "HTTP_CODE:200" "$RESP"
check "GET port 9090 has content" "Second app" "$RESP"

echo ""
echo "--- 14. Chunked Transfer Encoding ---"
RESP=$(printf 'POST /cgi-bin/hello.py HTTP/1.1\r\nHost: localhost:8080\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n5\r\nhello\r\n6\r\n world\r\n0\r\n\r\n' | nc -w 5 localhost 8080)
check "Chunked POST returns 200" "200" "$RESP"
check "Chunked POST body received" "hello world" "$RESP"

echo ""
echo "--- 15. Large POST Upload (>8KB, uses temp file) ---"
LARGE_UPLOAD=$(python3 -c "print('B' * 20000)")
RESP=$(curl -s -w "\nHTTP_CODE:%{http_code}" -X POST -d "$LARGE_UPLOAD" http://localhost:8080/upload)
check "Large upload returns 201" "HTTP_CODE:201" "$RESP"

echo ""
echo "--- 15b. Verify large upload file content ---"
UPLOADED_FILE=$(ls -t /workspace/test_docker/www/upload/temp/ | head -1)
if [ -n "$UPLOADED_FILE" ]; then
    FILE_SIZE=$(wc -c < "/workspace/test_docker/www/upload/temp/$UPLOADED_FILE")
    check "Large upload file size is 20000" "20000" "$FILE_SIZE"
else
    echo "[FAIL] No large uploaded file found"
    FAIL=$((FAIL + 1))
fi

echo ""
echo "--- 16. Directory traversal protection ---"
RESP=$(printf 'GET /../../../etc/passwd HTTP/1.1\r\nHost: localhost:8080\r\nConnection: close\r\n\r\n' | nc -w 5 localhost 8080)
check "Directory traversal blocked" "403" "$RESP"

echo ""
echo "============================================"
echo "  Results: $PASS passed, $FAIL failed"
echo "============================================"
if [ $FAIL -gt 0 ]; then
    echo -e "Failed tests:$FAIL_DETAILS"
    exit 1
fi
