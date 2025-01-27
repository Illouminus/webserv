#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
import sys

def main():
    # Read basic CGI environment variables
    method = os.environ.get("REQUEST_METHOD", "")
    content_length = os.environ.get("CONTENT_LENGTH", "0")
    try:
        content_length = int(content_length)
    except ValueError:
        content_length = 0

    # Read the request body (only if POST or PUT, typically)
    body = ""
    if method == "POST" or method == "PUT":
        body = sys.stdin.read(content_length)

    # Print mandatory HTTP headers for CGI response
    # At least Content-Type and an empty line
    print("Content-Type: text/html")
    print()  # empty line separating headers from body

    print("<!DOCTYPE html>")
    print("<html><head><title>Test Python CGI</title></head>")
    print("<body>")
    print("<h1>Hello from Python CGI!</h1>")

    # Show request method
    print(f"<p>REQUEST_METHOD = {method}</p>")

    # Show the body if any
    if body:
        print("<h2>Request Body</h2>")
        print(f"<pre>{body}</pre>")

    print("</body></html>")

if __name__ == "__main__":
    main()
