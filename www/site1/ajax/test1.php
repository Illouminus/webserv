#!/usr/bin/env php
<?php
// Указываем, что выводится HTML с кодировкой UTF-8
header("Content-Type: text/html; charset=UTF-8");

// Пример динамического контента
echo "<h2>Привет от PHP CGI!</h2>";
echo "<p>Сейчас: " . date("Y-m-d H:i:s") . "</p>";

// Можно добавить больше логики, например, обработку входных параметров
?>
