--TEST--
phpinfo() reports the compiled-in attack modules
--FILE--
<?php
ob_start();
phpinfo(INFO_MODULES);
$info = ob_get_clean();
$expected = getenv('IG_EXPECT_MODULES') ?: 'sqli, xss';
$got = preg_match('/^Compiled-in modules => (.*)$/m', $info, $m) ? trim($m[1]) : '(missing)';
echo $got === $expected ? "ok\n" : "got '$got', expected '$expected'\n";
?>
--EXPECT--
ok
