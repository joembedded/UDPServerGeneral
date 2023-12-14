<?php
/* Payload-Script ***Minimal_for_Tests*** 'p': Payload
* Payload must be a HEX String in BE Order (for easier debugging) 
* e.g. 202122 represents 32,33,33
* Call e.g. : http://localhost/wrk/udplog/payload_minimal.php?p=0011223344556677
* Reply is an Hex string in BE Order
* e.g. here: 08657b462b (08: 8 Bytes Payload, 657b462b: Unix-Timestamp for 14.12.2023)
*/

header('Content-Type: text/plain');

$hexplbe = @$_REQUEST['p'];
if (!isset($hexplbe)) die("#ERROR: No Payload");
$paybytes = @unpack('C*', hex2bin($hexplbe));
$paycount = count($paybytes);
if(!$paycount) die("#ERROR: Payload Format ('$hexplbe')"); // Odd size or wrong Chars?
$payrep =  bin2hex(pack("CN", $paycount,time()));
echo $payrep;
// --- END ---
