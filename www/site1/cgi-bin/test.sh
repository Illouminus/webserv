#!/bin/bash
# test.sh — минимальный CGI-скрипт на shell
# Убедитесь, что chmod +x test.sh
# И что loc->cgi_pass = "/bin/sh" (или '/usr/bin/sh')

# 1) Прочитать нужные переменные окружения
METHOD="$REQUEST_METHOD"
LEN="$CONTENT_LENGTH"
QUERY_STRING="$QUERY_STRING"

# 2) Считать body, если есть
BODY=""
if [ "$METHOD" = "POST" ] || [ "$METHOD" = "PUT" ]; then
    # Считываем ровно $CONTENT_LENGTH байт
    # shell прочитает их из stdin
    BODY=$(dd bs=1 count=$LEN 2>/dev/null)
fi

# 3) Печатаем заголовок
echo "Content-Type: text/html"
echo ""

# 4) Печатаем HTML
cat <<EOF
<!DOCTYPE html>
<html>
<head><title>Shell CGI Test</title></head>
<body>
<h1>Hello from Shell CGI!</h1>
<p>REQUEST_METHOD = $METHOD</p>
<p>QUERY_STRING = $QUERY_STRING</p>
EOF

if [ -n "$BODY" ]; then
  echo "<h3>Body</h3>"
  echo "<pre>$BODY</pre>"
fi

cat <<EOF
</body>
</html>
EOF
