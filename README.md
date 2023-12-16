# 'UDPServerGeneral' - UDP<->TCP Bridge
_A simple Server in C for easy UDP processing via e.g. PHP_

Standard Servers (Apache, nginx, ..) only work with TCP connections.
Sometimes IoT devices use UDP. However, complete UDP Servers are complex.
But mapping UDP to e.g. PHP and back is a very simple solution.

For simple projects a Server (as System Service) could be written and run in e.g. PHP directly, 
but for development this solution might be easier: a small UDP Server in C sends all incomming 
UDP via Curl/libcurl to a Standard Server (e.g PHP) and the result back via UDP. 
Developed for use with **LTX Microcloud**. 

For simplicity, the complete server is in one single C Source ;-)

Compile it on the Server and run it as Service.

![Overview](./docu/ovinfo.png "Overview")
 
## A Minimum Script in PHP
```
	<?php
		header('Content-Type: text/plain');
		$hexplbe = @$_REQUEST['p'];
		if (!isset($hexplbe)) die("#ERROR: No Payload");
		$payload=@hex2bin($hexplbe);
		if(!strlen($payload)) die("#ERROR: Payload Format ('$hexplbe')"); // Odd size or wrong Chars?
		$payrep =  bin2hex(strrev($payload)); // Reverse
		echo $payrep;
	?>
```

## Installation/Test ##
- Tested with Visual Studio (Windows) and GCC
- For details see Sourcecode
- Here is HowTo install a System Service on UBUNTU Linux: see LTX-Microcloud -> sw/udp/notes.txt

## Links ##
- Curl/libcurl: https://curl.se
- LTX Microcloud: https://github.com/joembedded/LTX_server
***
