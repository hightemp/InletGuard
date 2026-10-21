--TEST--
The inletguard extension is loaded
--FILE--
<?php
var_dump(extension_loaded('inletguard'));
?>
--EXPECT--
bool(true)
