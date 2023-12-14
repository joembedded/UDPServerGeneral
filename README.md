# 'UDPServerGeneral' - UDP<->TCP Bridge
_A simple Server in C for easy UDP processing via e.g. PHP_

Standard Servers (Apache, nginx, ..) only work with TCP connections.
Sometimes IoT devices use UDP. However, complete UDP Servers are complex.
But mapping UDP to e.g. PHP and back is a very simple solution.

This small UDP Server in C sends all incomming UDP via Curl/libcurl 
to PHP and the result back via UDP.

For simplicity, the complete server is in one single C Source ;-)

Compile it on the Server and run it as Service.

## Installation/Test ##
- Tested with Visual Studio (Windows) and GCC

## Links ##
- Curl/libcurl: https://curl.se
***
