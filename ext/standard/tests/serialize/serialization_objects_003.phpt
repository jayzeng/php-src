--TEST--
Test serialize() & unserialize() functions: objects (abstract classes)
--FILE--
<?php 
/* Prototype  : proto string serialize(mixed variable)
 * Description: Returns a string representation of variable (which can later be unserialized) 
 * Source code: ext/standard/var.c
 * Alias to functions: 
 */
/* Prototype  : proto mixed unserialize(string variable_representation)
 * Description: Takes a string representation of variable and recreates it 
 * Source code: ext/standard/var.c
 * Alias to functions: 
 */

echo "\n--- Testing Abstract Class ---\n";
// abstract class
abstract class Name 
{
  public function Name() {
    $this->a = 10;
    $this->b = 12.222;
    $this->c = "string";
  }
  abstract protected function getClassName();
  public function printClassName () {
    return $this->getClassName();
  } 
}
// implement abstract class
class extendName extends Name 
{
  var $a, $b, $c;

  protected function getClassName() {
    return "extendName";
  }
}

$obj_extendName = new extendName();
$serialize_data = serialize($obj_extendName);
var_dump( $serialize_data );
$unserialize_data = unserialize($serialize_data);
var_dump( $unserialize_data );

$serialize_data = serialize($obj_extendName->printClassName());
var_dump( $serialize_data );
$unserialize_data = unserialize($serialize_data);
var_dump( $unserialize_data );

echo "\nDone";
?>
--EXPECTF--
--- Testing Abstract Class ---
unicode(119) "O:10:"extendName":3:{U:1:"a";i:10;U:1:"b";d:12.2219999999999995310417943983338773250579833984375;U:1:"c";U:6:"string";}"
object(extendName)#%d (3) {
  [u"a"]=>
  int(10)
  [u"b"]=>
  float(12.222)
  [u"c"]=>
  unicode(6) "string"
}
unicode(18) "U:10:"extendName";"
unicode(10) "extendName"

Done
