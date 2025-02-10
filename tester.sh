#!/bin/bash
# tester.sh — простой тестер для проверки работы вашего сервера по конфигурации.
# Используются только CGI для PHP и SH.
# Зеленым цветом выводится SUCCESS, красным — FAIL.

# ANSI escape-последовательности для цветов:
GREEN="\033[32m"
RED="\033[31m"
NC="\033[0m"  # сброс цвета

# Функция для тестирования GET-запроса
function test_get() {
    url="$1"
    host="$2"        # значение для заголовка Host (если нужно)
    expected="$3"    # ожидаемый HTTP-статус
    desc="$4"
    echo -n "GET $url [$desc]: "
    if [ -n "$host" ]; then
        code=$(curl -s -o /dev/null -w "%{http_code}" -H "Host: $host" "$url")
    else
        code=$(curl -s -o /dev/null -w "%{http_code}" "$url")
    fi
    if [ "$code" -eq "$expected" ]; then
        echo -e "${GREEN}SUCCESS${NC} (status: $code)"
    else
        echo -e "${RED}FAIL${NC} (status: $code, expected: $expected)"
    fi
}

# Функция для тестирования POST-запроса с данными (без chunked)
function test_post() {
    url="$1"
    host="$2"
    data="$3"
    expected="$4"
    desc="$5"
    echo -n "POST $url [$desc]: "
    if [ -n "$host" ]; then
        code=$(curl -s -X POST -H "Host: $host" -d "$data" -o /dev/null -w "%{http_code}" "$url")
    else
        code=$(curl -s -X POST -d "$data" -o /dev/null -w "%{http_code}" "$url")
    fi
    if [ "$code" -eq "$expected" ]; then
        echo -e "${GREEN}SUCCESS${NC} (status: $code)"
    else
        echo -e "${RED}FAIL${NC} (status: $code, expected: $expected)"
    fi
}

# Функция для тестирования POST-запроса с Transfer-Encoding: chunked
function test_post_chunked() {
    url="$1"
    host="$2"
    file="$3"
    expected="$4"
    desc="$5"
    echo -n "POST (chunked) $url with file $file [$desc]: "
    if [ -n "$host" ]; then
        code=$(curl -s -X POST -H "Transfer-Encoding: chunked" -H "Host: $host" --data-binary "@$file" -o /dev/null -w "%{http_code}" "$url")
    else
        code=$(curl -s -X POST -H "Transfer-Encoding: chunked" --data-binary "@$file" -o /dev/null -w "%{http_code}" "$url")
    fi
    if [ "$code" -eq "$expected" ]; then
        echo -e "${GREEN}SUCCESS${NC} (status: $code)"
    else
        echo -e "${RED}FAIL${NC} (status: $code, expected: $expected)"
    fi
}

# Функция для тестирования DELETE-запроса
function test_delete() {
    url="$1"
    host="$2"
    expected="$3"
    desc="$4"
    echo -n "DELETE $url [$desc]: "
    if [ -n "$host" ]; then
        code=$(curl -s -X DELETE -H "Host: $host" -o /dev/null -w "%{http_code}" "$url")
    else
        code=$(curl -s -X DELETE -o /dev/null -w "%{http_code}" "$url")
    fi
    if [ "$code" -eq "$expected" ]; then
        echo -e "${GREEN}SUCCESS${NC} (status: $code)"
    else
        echo -e "${RED}FAIL${NC} (status: $code, expected: $expected)"
    fi
}

echo "=== Testing Server on 127.0.0.1:8080 (server_name: localhost, root: www/site1) ==="
# GET / -> index.html
test_get "http://127.0.0.1:8080/" "" 200 "Site1 index"
# GET /ajax -> index.html from www/site1/ajax
test_get "http://127.0.0.1:8080/ajax" "" 200 "Site1 ajax index"
# GET /images -> autoindex (если файлы есть)
test_get "http://127.0.0.1:8080/images" "" 200 "Site1 images autoindex"

# Тест загрузки файла через /uploads (location /uploads, root www/site1/uploads)
echo "Test file for upload" > upload_test.txt
test_post "http://127.0.0.1:8080/uploads/upload_test.txt" "" "$(cat upload_test.txt)" 201 "File upload to /uploads"
test_delete "http://127.0.0.1:8080/uploads/upload_test.txt" "" 200 "File deletion from /uploads"
rm -f upload_test.txt

echo ""
echo "=== Testing Server on 127.0.0.1:8081 (server_name: localhost, root: www/site2) ==="
# GET / -> index.html
test_get "http://127.0.0.1:8081/" "" 200 "Site2 index"
# GET /secret -> secret page (если существует, ожидаем 200)
test_get "http://127.0.0.1:8081/secret" "" 200 "Site2 secret"

echo ""
echo "=== Testing 0 on 127.0.0.1:8082 (server_name: mydomain.com, root: www/site2, CGI for .sh) ==="
# Для этого сервера задаем Host: mydomain.com
# GET / -> index.php
test_get "http://127.0.0.1:8082/" "mydomain.com" 200 "Site2 index on mydomain.com"
# Тест CGI для .sh
# Создадим простой SH-скрипт в папке www/site2 (предполагается, что location ~ \.sh$ его обработает)
echo "#!/bin/sh
echo 'Content-Type: text/html'
echo ''
echo '<h2>Hello from CGI SH</h2>'" > www/site2/test.sh
chmod +x www/site2/test.sh
test_post "http://127.0.0.1:8082/test.sh" "mydomain.com" "data=foo" 200 "CGI .sh execution on mydomain.com"

echo ""
echo "=== Testing Server on 127.0.0.1:8080 (server_name: mydomain.com, root: www/site2) ==="
# Этот сервер (на 8080) обслуживает запросы для mydomain.com (по виртуальному хосту)
test_get "http://127.0.0.1:8080/" "mydomain.com" 200 "Site2 index on mydomain.com"

echo ""
echo "=== Testing POST /uploads with chunked encoding (max_body_size 200k) ==="
# Для тестирования POST /post_body используем файл, размер которого меньше лимита
head -c 100 /dev/urandom > small_body.txt
test_post_chunked "http://127.0.0.1:8080/uploads" "" "small_body.txt" 201 "POST /uploads within limit"
# Теперь тест, когда размер тела превышает лимит. Допустим, лимит в конфиге для /post_body = 200k, 
# но вы можете изменить тест, если хотите проверить поведение при превышении лимита.
# Здесь для примера отправляем 300k:
head -c 900 /dev/urandom > large_body.txt
test_post_chunked "http://127.0.0.1:8080/uploads" "" "large_body.txt" 413 "POST /uploads exceeding limit"
rm -f small_body.txt large_body.txt

echo ""
echo "All tests completed."
