"MYWire" is a very quick mod of the official "Wire" Arduino library ( handling I2C communication ).

Its only purpose is to provide a bigger buffer than the default 32 bytes one ( too little to allow the zForce lib to work correctly on non-SAMD21-based Arduinos )
while not forcing the coder to mod the version of the lib that comes packaged within the Arduino IDE Application bundle ( and loose the said modifications
each time the Arduino IDE is upgraded ).

As a reminder, the location of the said library files depends on the OS used.
For example, On Mac OS, its located at /Applications/Arduino.app/Contents/Resources/Java/libraries/

Some renaming was also done to avoid conflict with the "unmodded" library ( Wire.h & twi.h )

To use the libray:
- 1st, add it to the Arduino managed libraries ( Arduino > Sketch > Include library > Add .zip library )
- then, whenever needed, the modded lib should be included as ```#include <MYWire.h>``` ( in other words, using an "absolute" path )

Then, use just as if it was the official Wire lib:
```
// ..
MYWire.begin()
// ..
MYWire.beginTransmission(<addr>);
MYWire.write(<bytesArr>,<len>);
MYWire.endTransmission();
// ..
MYWire.requestFrom(<addr>, <len>);
while( MYWire.available() ) MYWire.read();
```
