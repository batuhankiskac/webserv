#!/usr/bin/env python3
import os
import sys
import html
import urllib.parse

print("Content-Type: text/html; charset=UTF-8")
print()
print("<!DOCTYPE html>")
print("<html lang=\"en\"><head>")
print("<meta charset=\"UTF-8\">")
print("<title>Python CGI Test</title>")
print("</head><body>")
print("<h1>Python CGI Working!</h1>")

print("<div class=\"section\">")
print("<h2>Request Info</h2>")
print("<p><strong>Method:</strong> " + html.escape(os.environ.get('REQUEST_METHOD', '')) + "</p>")
print("<p><strong>Query String:</strong> " + html.escape(os.environ.get('QUERY_STRING', '')) + "</p>")
print("<p><strong>Script Name:</strong> " + html.escape(os.environ.get('SCRIPT_NAME', '')) + "</p>")
print("<p><strong>Path Info:</strong> " + html.escape(os.environ.get('PATH_INFO', '')) + "</p>")
print("<p><strong>Path Translated:</strong> " + html.escape(os.environ.get('PATH_TRANSLATED', '')) + "</p>")
print("<p><strong>Server Protocol:</strong> " + html.escape(os.environ.get('SERVER_PROTOCOL', '')) + "</p>")
print("<p><strong>Server Name:</strong> " + html.escape(os.environ.get('SERVER_NAME', '')) + "</p>")
print("<p><strong>Server Port:</strong> " + html.escape(os.environ.get('SERVER_PORT', '')) + "</p>")
print("<p><strong>Gateway Interface:</strong> " + html.escape(os.environ.get('GATEWAY_INTERFACE', '')) + "</p>")

query = urllib.parse.parse_qs(os.environ.get('QUERY_STRING', ''))
if query:
    print("<h3>GET Parameters:</h3>\n<ul>")
    for key, values in query.items():
        print("<li><strong>" + html.escape(key) + ":</strong> " + html.escape(values[0]) + "</li>")
    print("</ul>")

try:
    content_length = int(os.environ.get('CONTENT_LENGTH', '0'))
except ValueError:
    content_length = 0
if content_length > 0:
    raw_body = sys.stdin.buffer.read(content_length).decode('utf-8', 'replace')
    form = urllib.parse.parse_qs(raw_body)
    if form:
        print("<h3>POST Parameters:</h3>\n<ul>")
        for key, values in form.items():
            print("<li><strong>" + html.escape(key) + ":</strong> " + html.escape(values[0]) + "</li>")
        print("</ul>")

print("<h3>Environment Variables (HTTP_*):</h3>\n<ul>")
for key in sorted(os.environ):
    if key.startswith('HTTP_'):
        print("<li><strong>" + html.escape(key) + ":</strong> " + html.escape(os.environ[key]) + "</li>")
print("</ul>")
print("</div>")
print("</body></html>")
