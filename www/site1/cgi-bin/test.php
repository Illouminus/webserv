#!/usr/bin/env php
<?php
/**
 * test.php — минимальный PHP-скрипт для проверки CGI
 * Перед запуском убедитесь, что loc->cgi_pass = "/usr/bin/php-cgi" (или другой путь)
 * И что файл исполняемый: chmod +x test.php
 */

// 1) Считаем метод и длину
$method = getenv("REQUEST_METHOD");
$content_length = getenv("CONTENT_LENGTH");

// 2) Если POST/PUT, прочитаем тело
$body = "";
if ($method === "POST" || $method === "PUT") {
    $body = file_get_contents("php://stdin", false, null, 0, (int)$content_length);
}

// 3) Выведем заголовки (обязательно!)
echo "Content-Type: text/html\r\n";
echo "\r\n";

// 4) Выводим HTML
echo "<!DOCTYPE html>\n";
echo "<html><head><title>PHP CGI Test</title></head><body>\n";
echo "<h1>Hello from PHP CGI!</h1>\n";
echo "<p>REQUEST_METHOD = $method</p>\n";

// Покажем содержимое body
if (!empty($body)) {
    echo "<h3>Body</h3>\n";
    echo "<pre>" . htmlspecialchars($body) . "</pre>\n";
}

echo "</body></html>\n";
