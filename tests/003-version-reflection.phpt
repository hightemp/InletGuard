--TEST--
InletGuard\version() signature is the same on all PHP versions
--FILE--
<?php
$f = new ReflectionFunction('InletGuard\version');
var_dump($f->getNamespaceName());
var_dump($f->getShortName());
var_dump($f->getNumberOfParameters());
var_dump((string) $f->getReturnType());
?>
--EXPECT--
string(10) "InletGuard"
string(7) "version"
int(0)
string(6) "string"
