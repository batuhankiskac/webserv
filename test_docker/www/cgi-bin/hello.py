#!/usr/bin/env python3
import os
import sys

print("Content-Type: text/html\r\n\r\n", end="")
print("<html><body>")
print("<h1>CGI Test</h1>")
print("<p>Method: {}</p>".format(os.environ.get("REQUEST_METHOD", "unknown")))
print("<p>Query: {}</p>".format(os.environ.get("QUERY_STRING", "")))
print("<p>Script: {}</p>".format(os.environ.get("SCRIPT_NAME", "")))
print("<p>Content-Length: {}</p>".format(os.environ.get("CONTENT_LENGTH", "0")))

if os.environ.get("REQUEST_METHOD") == "POST":
    cl = int(os.environ.get("CONTENT_LENGTH", 0))
    if cl > 0:
        body = sys.stdin.buffer.read(cl)
        print("<p>Body length received: {}</p>".format(len(body)))
        print("<p>Body: {}</p>".format(body.decode("utf-8", errors="replace")[:200]))

print("</body></html>")
