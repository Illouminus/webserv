#!/bin/bash
# tester.sh — Simple tester for verifying your server configuration.
# Only CGI for PHP and Shell is supported.
# Successful tests will be marked with a green check (✔) and failed ones with a red cross (✖).

# ANSI escape sequences for colors:
GREEN="\e[0;32m"
RED="\033[31m"
YELLOW="\033[33m"
CYAN="\033[36m"
NC="\033[0m"  # No Color (reset)

# Unicode symbols
CHECK_MARK="✔"
CROSS_MARK="✖"

# Function to print a header with a separator
function print_header() {
    echo -e "\n${CYAN}========== $1 ==========${NC}"
}

# Function to test GET requests.
# Arguments:
#   $1 - URL
#   $2 - Host header value (if needed)
#   $3 - Expected HTTP status code
#   $4 - Test description
function test_get() {
    url="$1"
    host="$2"
    expected="$3"
    desc="$4"
    echo -e "${YELLOW}GET${NC} $url [$desc]: "
    if [ -n "$host" ]; then
        code=$(curl -s -H "Host: $host" -H "Connection: close" -o /dev/null -w "%{http_code}" "$url")
    else
        code=$(curl -s -H "Connection: close" -o /dev/null -w "%{http_code}" "$url")
    fi
    if [ "$code" -eq "$expected" ]; then
        echo -e "${GREEN}${CHECK_MARK} SUCCESS${NC} (status: $code)"
    else
        echo -e "${RED}${CROSS_MARK} FAIL${NC} (status: $code, expected: $expected)"
    fi
}


function test_unknow() {
    url="$1"
    host="$2"
    expected="$3"
    desc="$4"
    echo -e "${YELLOW}UNKNOW${NC} $url [$desc]: "
    if [ -n "$host" ]; then
        code=$(curl -s -H "Host: $host" -X UNKNOW -H "Connection: close" -o /dev/null -w "%{http_code}" "$url")
    else
        code=$(curl -s -H "Connection: close" -X UNKNOW -o /dev/null -w "%{http_code}" "$url")
    fi
    if [ "$code" -eq "$expected" ]; then
        echo -e "${GREEN}${CHECK_MARK} SUCCESS${NC} (status: $code)"
    else
        echo -e "${RED}${CROSS_MARK} FAIL${NC} (status: $code, expected: $expected)"
    fi
}


# Function to test POST requests (non-chunked).
# Arguments:
#   $1 - URL
#   $2 - Host header value (if needed)
#   $3 - POST data (e.g., form-urlencoded string)
#   $4 - Expected HTTP status code
#   $5 - Test description
function test_post() {
    url="$1"
    host="$2"
    data="$3"
    expected="$4"
    desc="$5"
    echo -e "${YELLOW}POST${NC} $url [$desc]: "
    if [ -n "$host" ]; then
        code=$(curl -s -X POST -H "Host: $host" -H "Connection: close" -d "$data" -o /dev/null -w "%{http_code}" "$url")
    else
        code=$(curl -s -X POST -H "Connection: close" -d "$data" -o /dev/null -w "%{http_code}" "$url")
    fi
    if [ "$code" -eq "$expected" ]; then
        echo -e "${GREEN}${CHECK_MARK} SUCCESS${NC} (status: $code)"
    else
        echo -e "${RED}${CROSS_MARK} FAIL${NC} (status: $code, expected: $expected)"
    fi
}

# Function to test POST requests with Transfer-Encoding: chunked.
# Arguments:
#   $1 - URL
#   $2 - Host header value (if needed)
#   $3 - File to send as POST body
#   $4 - Expected HTTP status code
#   $5 - Test description
function test_post_chunked() {
    url="$1"
    host="$2"
    file="$3"
    expected="$4"
    desc="$5"
    echo -e "${YELLOW}POST (chunked)${NC} $url with file $file [$desc]: "
    if [ -n "$host" ]; then
        code=$(curl -s -X POST -H "Transfer-Encoding: chunked" -H "Host: $host" -H "Connection: close" --data-binary "@$file" -o /dev/null -w "%{http_code}" "$url")
    else
        code=$(curl -s -X POST -H "Transfer-Encoding: chunked" -H "Connection: close" --data-binary "@$file" -o /dev/null -w "%{http_code}" "$url")
    fi
    if [ "$code" -eq "$expected" ]; then
        echo -e "${GREEN}${CHECK_MARK} SUCCESS${NC} (status: $code)"
    else
        echo -e "${RED}${CROSS_MARK} FAIL${NC} (status: $code, expected: $expected)"
    fi
}

# Function to test DELETE requests.
# Arguments:
#   $1 - URL
#   $2 - Host header value (if needed)
#   $3 - Expected HTTP status code
#   $4 - Test description
function test_delete() {
    url="$1"
    host="$2"
    expected="$3"
    desc="$4"
    echo -e "${YELLOW}DELETE${NC} $url [$desc]: "
    if [ -n "$host" ]; then
        code=$(curl -s -X DELETE -H "Host: $host" -H "Connection: close" -o /dev/null -w "%{http_code}" "$url")
    else
        code=$(curl -s -X DELETE -H "Connection: close" -o /dev/null -w "%{http_code}" "$url")
    fi
    if [ "$code" -eq "$expected" ]; then
        echo -e "${GREEN}${CHECK_MARK} SUCCESS${NC} (status: $code)"
    else
        echo -e "${RED}${CROSS_MARK} FAIL${NC} (status: $code, expected: $expected)"
    fi
}

###########################################
# Begin Tests
###########################################

print_header "Testing Server on 127.0.0.1:8080 (server_name: localhost, root: www/site1)"

# GET tests
test_get "http://127.0.0.1:8080/" "localhost" 200 "Site1 index"
test_get "http://127.0.0.1:8080/ajax" "localhost" 200 "Site1 ajax index"
test_get "http://127.0.0.1:8080/images" "localhost" 200 "Site1 images autoindex"
test_get "http://127.0.0.1:8080/oldpath" "localhost" 301 "Old path redirect"


# UNKNOW tests

test_unknow "http://127.0.01:8080/" "localhost" 405 "Site1 index"

# POST/DELETE tests (uploads)
echo "Test file content for upload" > upload_test.txt
test_post "http://127.0.0.1:8080/uploads/upload_test.txt" "localhost" "$(cat upload_test.txt)" 201 "File upload to /uploads (non-chunked)"
test_delete "http://127.0.0.1:8080/uploads/upload_test.txt" "localhost" 200 "File deletion from /uploads"
# Test: DELETE already deleted file (should return 404)
test_delete "http://127.0.0.1:8080/uploads/upload_test.txt" "localhost" 404 "Deleting non-existent file"
rm -f upload_test.txt

# Test: POST with body size exceeding limit (non-chunked)
# /uploads on site1 has max_body_size set to 200 bytes.
long_data=$(head -c 300 /dev/urandom | base64)
test_post "http://127.0.0.1:8080/uploads/exceed.txt" "localhost" "$long_data" 413 "POST with body exceeding max_body_size (non-chunked)"

# Test: POST to a non-existent directory (should result in error, e.g., 404)
test_post "http://127.0.0.1:8080/nonexistent" "localhost" "data=test" 404 "POST to non-existent directory"

# Test: GET non-existent file (should return 404)
test_get "http://127.0.0.1:8080/nonexistent.txt" "localhost" 404 "GET non-existent file"

print_header "Testing Server on 127.0.0.1:8081 (server_name: localhost, root: www/site2)"

test_get "http://127.0.0.1:8081/" "localhost" 200 "Site2 index"
test_get "http://127.0.0.1:8081/secret" "localhost" 200 "Site2 secret"

print_header "Testing Server on 127.0.0.1:8082 (server_name: mydomain.com, root: www/site2, CGI for .sh)"

test_get "http://127.0.0.1:8082/" "mydomain.com" 200 "Site2 index on mydomain.com"

# CGI test for .sh
echo "#!/bin/sh
echo 'Content-Type: text/html'
echo ''
echo '<h2>Hello from CGI SH</h2>'" > www/site2/test.sh
chmod +x www/site2/test.sh
test_post "http://127.0.0.1:8082/test.sh" "mydomain.com" "data=foo" 200 "CGI .sh execution on mydomain.com"
rm -f www/site2/test.sh

print_header "Testing Server on 127.0.0.1:8080 (server_name: mydomain.com, root: www/site2)"

test_get "http://127.0.0.1:8080/" "mydomain.com" 200 "Site2 index on mydomain.com"
test_get "http://127.0.0.1:8080/secret" "mydomain.com" 200 "Site2 secret on mydomain.com"

print_header "Testing POST /uploads with chunked encoding (max_body_size from location)"

# For /uploads on server on 8080 (server_name: localhost) max_body_size is set to 200 (bytes)
head -c 100 /dev/urandom > small_body.txt
test_post_chunked "http://127.0.0.1:8080/uploads" "localhost" "small_body.txt" 201 "POST /uploads within limit (chunked)"
head -c 900 /dev/urandom > large_body.txt
test_post_chunked "http://127.0.0.1:8080/uploads" "localhost" "large_body.txt" 413 "POST /uploads exceeding limit (chunked)"
rm -f small_body.txt large_body.txt

print_header "All tests completed"
