--TEST--
HTML entities
--POST--
--GET--
--FILE--
<?php 
setlocale ("LC_CTYPE", "C");
echo htmlspecialchars ("<>\"&��\n");
echo htmlentities ("<>\"&��\n");
?>
--EXPECT--

&lt;&gt;&quot;&amp;��
&lt;&gt;&quot;&amp;&aring;&Auml;
