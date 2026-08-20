#!/usr/bin/env python3
import html
import os
import sys
import urllib.parse


def escaped_environment(name):
    return html.escape(os.environ.get(name, ""))


print("Content-Type: text/html; charset=UTF-8")
print()
print("<!DOCTYPE html>")
print('<html lang="en">')
print("<head>")
print('<meta charset="UTF-8">')
print("<title>Python CGI Test</title>")
print('<link rel="stylesheet" href="/style.css">')
print("</head>")
print("<body>")
print('<div class="container">')
print("<h1>Python CGI Working!</h1>")

print('<div class="section">')
print("<h2>Request Info</h2>")
for label, name in (
    ("Method", "REQUEST_METHOD"),
    ("Query String", "QUERY_STRING"),
    ("Script Name", "SCRIPT_NAME"),
    ("Path Info", "PATH_INFO"),
    ("Path Translated", "PATH_TRANSLATED"),
    ("Server Protocol", "SERVER_PROTOCOL"),
    ("Server Name", "SERVER_NAME"),
    ("Server Port", "SERVER_PORT"),
    ("Gateway Interface", "GATEWAY_INTERFACE"),
):
    print("<p><strong>" + label + ":</strong> " + escaped_environment(name) + "</p>")

query = urllib.parse.parse_qs(os.environ.get("QUERY_STRING", ""))
if query:
    print("<h3>GET Parameters:</h3>")
    print("<ul>")
    for key, values in query.items():
        print("<li><strong>" + html.escape(key) + ":</strong> "
              + html.escape(values[0]) + "</li>")
    print("</ul>")

try:
    content_length = int(os.environ.get("CONTENT_LENGTH", "0"))
except ValueError:
    content_length = 0

if content_length > 0:
    raw_body = sys.stdin.buffer.read(content_length).decode("utf-8", "replace")
    form = urllib.parse.parse_qs(raw_body)
    if form:
        print("<h3>POST Parameters:</h3>")
        print("<ul>")
        for key, values in form.items():
            print("<li><strong>" + html.escape(key) + ":</strong> "
                  + html.escape(values[0]) + "</li>")
        print("</ul>")

print("<h3>Environment Variables (HTTP_*):</h3>")
print("<ul>")
for name in sorted(os.environ):
    if name.startswith("HTTP_"):
        print("<li><strong>" + html.escape(name) + ":</strong> "
              + html.escape(os.environ[name]) + "</li>")
print("</ul>")
print("</div>")

print('<div class="section">')
print("<h2>Test Forms</h2>")
print("<h3>GET Test</h3>")
print('<form action="/cgi-bin/test.py" method="GET">')
print('<input type="text" name="test_get" placeholder="GET parameter">')
print('<button type="submit">Send GET</button>')
print("</form>")
print("<h3>POST Test</h3>")
print('<form action="/cgi-bin/test.py" method="POST">')
print('<input type="text" name="test_post" placeholder="POST parameter">')
print('<button type="submit">Send POST</button>')
print("</form>")
print("</div>")
print('<p><a href="/">Back to home</a></p>')
print("</div>")
print("</body>")
print("</html>")
