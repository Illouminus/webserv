#!/bin/sh
# test.sh - Minimal shell CGI script for testing.
# Ensure the script is executable: chmod +x test.sh
# And that the server's cgi_pass points to /bin/sh (or the appropriate shell)

# 1) Read environment variables
METHOD="$REQUEST_METHOD"
CONTENT_LENGTH="$CONTENT_LENGTH"
QUERY_STRING="$QUERY_STRING"

# 2) Read POST/PUT body if available
BODY=""
if [ "$METHOD" = "POST" ] || [ "$METHOD" = "PUT" ]; then
    BODY=$(dd bs=1 count="$CONTENT_LENGTH" 2>/dev/null)
fi

# 3) Output response headers
echo "Content-Type: text/html"
echo ""

# 4) Output HTML response with request details
cat <<EOF
<!DOCTYPE html>
<html>
<head><title>Shell CGI Test</title></head>
<body>
<h1>Hello from Shell CGI!</h1>
<p>REQUEST_METHOD = $METHOD</p>
<p>QUERY_STRING = $QUERY_STRING</p>
<h2>Environment Variables</h2>
<pre>
REQUEST_METHOD=$REQUEST_METHOD
CONTENT_LENGTH=$CONTENT_LENGTH
QUERY_STRING=$QUERY_STRING
</pre>
EOF

if [ -n "$BODY" ]; then
  echo "<h2>Body</h2>"
  echo "<pre>$BODY</pre>"
fi

cat <<EOF
</body>
</html>
EOF