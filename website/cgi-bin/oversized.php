<?php
header("Content-Type: text/plain");
echo str_repeat("x", 8 * 1024 * 1024 + 1);
?>
