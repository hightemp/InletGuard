--TEST--
InletGuard\version() signature is the same on all PHP versions
--FILE--
<?php
$f = new ReflectionFunction('InletGuard\version');
var_dump($f->getNamespaceName());
var_dump($f->getShortName());
var_dump($f->getNumberOfParameters());
$type = $f->getReturnType();
// ReflectionNamedType::getName() exists since 7.1; __toString() is deprecated in 7.1-7.4.
var_dump(method_exists($type, 'getName') ? $type->getName() : (string) $type);
?>
--EXPECT--
string(10) "InletGuard"
string(7) "version"
int(0)
string(6) "string"
