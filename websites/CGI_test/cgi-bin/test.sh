#!/bin/bash

echo "Content-Type: text/html"
echo

echo "<html>"
echo "<body>"
echo "<h1>Bash CGI Test</h1>"
echo "<p>Query string: $QUERY_STRING</p>"
echo "</body>"
echo "</html>"
