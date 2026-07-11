#!/usr/bin/env python3
import os
import sys

status = os.environ.get("QUERY_STRING", "")
if "=" in status:
    parts = status.split("=", 1)
    code = parts[1]
else:
    code = "200"

print("Status: {} OK\r\n".format(code), end="")
print("Content-Type: text/plain\r\n\r\n", end="")
print("Custom status test: {}".format(code))
