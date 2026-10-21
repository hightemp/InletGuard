--TEST--
InletGuard\version() returns the extension version
--FILE--
<?php
$version = InletGuard\version();
var_dump(is_string($version));
var_dump($version === phpversion('inletguard'));
var_dump((bool) preg_match('/^\d+\.\d+\.\d+(-[0-9A-Za-z.-]+)?$/', $version));
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
