#!/bin/bash
set -e

echo "=== Building webserv in Docker ==="
docker run --rm \
  -v /Users/batuhankiskac/Desktop/webserv:/workspace \
  -w /workspace \
  ubuntu:24.04 \
  bash -c "apt-get update -qq && apt-get install -y -qq g++ make 2>&1 | tail -1 && make clean && make 2>&1"

echo ""
echo "=== Running tests in Docker ==="
docker run --rm \
  -v /Users/batuhankiskac/Desktop/webserv:/workspace \
  -w /workspace \
  -p 8080:8080 \
  -p 9090:9090 \
  ubuntu:24.04 \
  bash -c '
    set -e
    apt-get update -qq 2>/dev/null
    apt-get install -y -qq curl python3 netcat-openbsd 2>/dev/null 1>/dev/null

    chmod +x /workspace/test_docker/www/cgi-bin/*.py

    echo "=== Starting webserv ==="
    /workspace/webserv /workspace/test_docker/test.conf &
    SERVER_PID=$!
    sleep 2

    if ! kill -0 $SERVER_PID 2>/dev/null; then
        echo "FAILED: Server did not start!"
        exit 1
    fi
    echo "Server started (PID $SERVER_PID)"
    echo ""

    bash /workspace/test_docker/run_tests.sh
    TEST_EXIT=$?

    echo ""
    echo "=== Cleaning up ==="
    kill $SERVER_PID 2>/dev/null || true
    wait $SERVER_PID 2>/dev/null || true

    exit $TEST_EXIT
  '
