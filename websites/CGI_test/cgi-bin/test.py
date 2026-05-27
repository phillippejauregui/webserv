#!/usr/bin/env python3

print("Content-Type: text/html")
print()

print("<html>")
print("<body>")
print("<h1>Python CGI Test</h1>")
print("<p>Environment</p>")
import os
for k, v in os.environ.items():
    print(f"{k} = {v}<br>")
print("</body>")
print("</html>")
