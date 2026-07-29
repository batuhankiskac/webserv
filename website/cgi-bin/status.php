<?php
http_response_code(201);
header("Content-Type: text/plain");
header("X-CGI-Fixture: status");
echo "CGI_STATUS_CREATED\n";
?>
