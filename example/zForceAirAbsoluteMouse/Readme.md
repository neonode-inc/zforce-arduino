This is a quick 'n dirty implementation of an "absolute" mouse.

Nb: the main reason the code is not optimized & using "native sensor settings" ( operation mode, frequency, device config ) instead of using 'map()' & stuff
is because i2c was being debugged / investigated at the same time
( to find the reason of the "locking" problem when using one of the "native sensor setting" request on a non-SAMD21-based board like Arduino Pro Micro )

Spoiler: the reason was a too small buffer, resulting on an overflow & lock ( no longer being able to interact with the sensor module )

Teaser: Once the "i2c debugger" sketch 'll be "finished", a 'hybrid" behavior 'll be implemented upon the one built in this test file ;)
