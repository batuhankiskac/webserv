<?php
header("Content-Type: text/html; charset=UTF-8");
echo "<!DOCTYPE html>\n";
echo "<html lang=\"en\">\n<head>\n";
echo "<meta charset=\"UTF-8\">\n";
echo "<title>PHP CGI Test</title>\n";
echo "<link rel=\"stylesheet\" href=\"/style.css\">\n";
echo "</head>\n<body>\n";
echo "<div class=\"container\">\n";
echo "<h1>PHP CGI Working!</h1>\n";

echo "<div class=\"section\">\n";
echo "<h2>Request Info</h2>\n";
echo "<p><strong>Method:</strong> " . htmlspecialchars($_SERVER['REQUEST_METHOD']) . "</p>\n";
echo "<p><strong>Query String:</strong> " . htmlspecialchars($_SERVER['QUERY_STRING']) . "</p>\n";
echo "<p><strong>Script Name:</strong> " . htmlspecialchars($_SERVER['SCRIPT_NAME']) . "</p>\n";
echo "<p><strong>Path Info:</strong> " . htmlspecialchars($_SERVER['PATH_INFO']) . "</p>\n";
echo "<p><strong>Path Translated:</strong> " . htmlspecialchars($_SERVER['PATH_TRANSLATED']) . "</p>\n";
echo "<p><strong>Server Protocol:</strong> " . htmlspecialchars($_SERVER['SERVER_PROTOCOL']) . "</p>\n";
echo "<p><strong>Server Name:</strong> " . htmlspecialchars($_SERVER['SERVER_NAME']) . "</p>\n";
echo "<p><strong>Server Port:</strong> " . htmlspecialchars($_SERVER['SERVER_PORT']) . "</p>\n";
echo "<p><strong>Gateway Interface:</strong> " . htmlspecialchars($_SERVER['GATEWAY_INTERFACE']) . "</p>\n";

if (!empty($_GET)) {
    echo "<h3>GET Parameters:</h3>\n<ul>\n";
    foreach ($_GET as $key => $value) {
        echo "<li><strong>" . htmlspecialchars($key) . ":</strong> " . htmlspecialchars($value) . "</li>\n";
    }
    echo "</ul>\n";
}

if (!empty($_POST)) {
    echo "<h3>POST Parameters:</h3>\n<ul>\n";
    foreach ($_POST as $key => $value) {
        echo "<li><strong>" . htmlspecialchars($key) . ":</strong> " . htmlspecialchars($value) . "</li>\n";
    }
    echo "</ul>\n";
}

echo "<h3>Environment Variables (HTTP_*):</h3>\n<ul>\n";
foreach ($_SERVER as $key => $value) {
    if (strpos($key, 'HTTP_') === 0) {
        echo "<li><strong>" . htmlspecialchars($key) . ":</strong> " . htmlspecialchars($value) . "</li>\n";
    }
}
echo "</ul>\n";

echo "</div>\n";

echo "<div class=\"section\">\n";
echo "<h2>Test Forms</h2>\n";
echo "<h3>GET Test</h3>\n";
echo "<form action=\"/cgi-bin/test.php\" method=\"GET\">\n";
echo "<input type=\"text\" name=\"test_get\" placeholder=\"GET parameter\">\n";
echo "<button type=\"submit\">Send GET</button>\n";
echo "</form>\n";

echo "<h3>POST Test</h3>\n";
echo "<form action=\"/cgi-bin/test.php\" method=\"POST\">\n";
echo "<input type=\"text\" name=\"test_post\" placeholder=\"POST parameter\">\n";
echo "<button type=\"submit\">Send POST</button>\n";
echo "</form>\n";
echo "</div>\n";

echo "<p><a href=\"/\">Back to home</a></p>\n";
echo "</div>\n</body>\n</html>\n";
?>