--TEST--
Phar object: add file
--SKIPIF--
<?php if (!extension_loaded('phar')) die('skip'); ?>
--FILE--
<?php

$pharconfig = 0;

require_once 'phar_oo_test.inc';

$phar = new Phar($fname);

$phar['f.php'] = 'hi';
var_dump(isset($phar['f.php']));
echo $phar['f.php'];
echo "\n";

?>
===DONE===
--CLEAN--
<?php 
unlink(dirname(__FILE__) . '/phar_oo_test.phar.php');
__halt_compiler();
?>
--EXPECT--
bool(true)
hi
===DONE===
