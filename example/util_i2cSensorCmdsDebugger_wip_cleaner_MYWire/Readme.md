This "debugger" sketch was written to investigate the trouble related to sending requests to the sensor module over I2C

If you can't ```Wire.requestFrom()``` or "Wire.write()" to the sensor module, it may come from the I2C clock & data lines needing a pull-up resistor to Vcc for both

If you can do so, are also able to intercept the "boot completed" notification as well as sending the "enable" request,
and possibly also receive further "touch" notification BUT CAN'T use any of the zforce lib api methods, then it may surely come from the size of the buffer i2c uses

To fix this trouble when not using the "local" sforce lib's I2C.h, a mod of "Wire.h" is used instead, called "MYWire.h"

Although the debuger sketch currently includes it, it's no longer needed if using the updated ( that 'll be proposed for merging ) zforce.cpp file,
that has been tweaked to use "MYWire.h" when not defining ```USE_I2C_LIB 1``` ( for non-SAMD2-based boards, other than the official NeoNode Prototyping Board like the Arduino Pro Micro I'm debugging on )

## Quick debug reminders ( extracted from the zforce docs & zforce.h/.cpp )

- to read stuff
```cpp
Zforce::Read(uint8_t * payload)
Wire.requestFrom(ZFORCE_I2C_ADDRESS, 2);
  payload[0] = Wire.read();
  payload[1] = Wire.read();
  
  int index = 2;
  Wire.requestFrom(ZFORCE_I2C_ADDRESS, payload[1]);
  while (Wire.available())
  {
    payload[index++] = Wire.read();
  }
```

- to write stuff
```cpp
Wire.beginTransmission(ZFORCE_I2C_ADDRESS);
Wire.write(payload, payload[1] + 2); // R: the +2 adds to the I2C LEN to get the toal number of bytes to be written
Wire.endTransmission();
```

## Reading the docs ..

Start with
- https://support.neonode.com/docs/display/AIRTSUsersGuide/I2C+Transport
- https://support.neonode.com/docs/display/AIRTSUsersGuide/Serialization+Protocol+Quick+Start

Then
- https://support.neonode.com/docs/display/AIRTSUsersGuide/Understanding+the+zForce+ASN.1+Serialization+Protocol#UnderstandingthezForceASN.1SerializationProtocol-RequestResponseNotification
- https://support.neonode.com/docs/display/AIRTSUsersGuide/zForce+Message+Specification#zForceMessageSpecification-Configuration

List of possible settings
- https://support.neonode.com/docs/display/SDKDOC/SDK+Function+Support+Matrix

Cmd+F & search for ‘Message’ on this to digg related I2C codes ?
- https://support.neonode.com/docs/display/SDKDOC/SDK+Complete+Function+Library

In brief,
- I2C adds is 0X50
- Module is always 40 02 02 00
- Module “sub-devices”: 65, 67, 68, 73

See content of the sketch for wayyyy more infos ;) ( careful - wip == not sure of everything, but mostly ok with my little mess ;p )


##  debug ~API

While very crude, it actually helped a lot to track down the trouble & find an ( easy indeed ) fix ( it became quite clear when logging the bytes len of received data )
See "Zforce_debuggingMess.cpp" in the current directory for hints on the quick fuzzing tests & the refined guess that led to finding the fix ;)

- ```readSensor(bool debug [, bool clearBuff])```:

Replaces ```Message* Zforce::GetMessage()``` , the main differences being its can log, keep buffer intact, & returns a different code based on the "type" of response or notification received

- ```writeSensor(uint8_t *bytesArr)```:

Replaces ```int Zforce::Write2(uint8_t* payload)```, no real difference ( the original one always returned 0 on non-SAMD21 boards )

- ```setupSensor(int mPIN_NN_RST, int mPIN_NN_DR)```:

Replaces ```void Zforce::Start(int dr)```, main difference is it checks the code returned by ```signed int configSensor()```,
after calling ```initSensor(mPIN_NN_RST, mPIN_NN_DR)```& ```initSensorInterrupt(mPIN_NN_DR)```, to know where something not expected happened while configuring the sensor module

- ```void initSensor(int mPIN_NN_RST, int mPIN_NN_DR)```: sets up pinMode & triggering hardware reset of sensor module

- ```void initSensorInterrupt(int mPIN_NN_DR)```: attaches an interrupt that sets a flag for the related handler

- ```void onSensorReceive()```: used to poll for DataReady HIGH within ```loop()```

- ```void onSensorReceiveInt()```: handler for the flag set via the ISR ```void dataReadyISR()```

- ```void onSensorReceivedInt_zForce()```: handler for the flag set via the ISR ```void dataReadyISR()``` that "borrows" ```Message* touch = zforce.GetMessage()```'s touch parser for quick testing

Nb: it seemed logging was too much while using it, but "Absolute mouse behavior was working fine nontheless :) ( and I didn't have the time [yet ? .. or the will ? ..] to write a quick parser myself ;) )

- ```signed int configSensor()```: configures the sensor module by setting every possible parameter in the least amount of requests ( one for each “sub-devices”: [65,] 67, 68, 73, 65 - the former 65 for a possible software reset request - not currently done since we do a hardware reset on boot - and the latter for enabling, for example )

More specifically, it handles getting a "boot completed" notification, & then goes on to specifying operation mode, frequency settings, and device config before issuing an enable request ( Nb: doesn't currently support sending the "debug touches" additional param/value for "enable" requests )

Even more specifically, when setting the "device config" part, it internally calls ```void setDeviceConfigWIP()``` ( hugely wip as the name implies ), which handles setting all the bytes related to every aspect of the device config from sketch params ( near the beginning of the sketch, sorted by "sub-devices" )

- ```void setDeviceConfigWIP()```: the neety greedy ^^

Re-using a known, working device config is fine ( best generated by device - by storing the logged full device config in response received - than by workbench generator - which is windows only :/ - but it helped a lot since I'm very new to PDU stuff .. and couldn't get the .asn to get hints ( for now I guess ;p ) ),
can be stored to progmem BUT being able to do quick fiddling with params at the top of a sketch is quite handy ( and really in the Arduino feeling ;p , plus re-implementing the "generator" lenghts recalculation based on changes & "levels" was quite funny to implement ( if I finally got it right ? .. ), and gave me less a headache ( .. and there goe my weekend .. ) than tracking down the I2C-related bug ( only partial brain hiccup while trying to come up with a quick-to-implement-and-not-so-dirty implementation of the needed "rollback changed upward" behavior ^^ )

This part is a huge wip ( and could be well better implemented - even more after getting my head around the PDU stuff & cie ), but this is my own 2 cents on getting it working independently of the OS-available software/drivers :)

( Also, I needed something fun to code as a reward for identifying & fixing the I2C bug successfully ;D )

## Params available & "device config study"
( I think up-to-date with what's available -> aka already added to the zforce lib, but I really get my head around the PDU stuff to be able to set the tiniest bit of config that neat hardware can give :P )
```cpp
// /!\ some types are wrong -> gotta fix that ( some uint8_t -> uint16_t ) & values are for testing purposes ( of the "rollback length calculation & buffer/position update" part ) 
// - 0x65 - Enable Group -
  // 0x80 -> Disable
  // 0x81 -> Enable [ R: can include number of sample touch for debug purposes ]
  // 0x82 -> Reset
// - 0x67 - OperationMode Group - R: ONE at a time !
  bool opModeDetection = true; // 0x80 - Detection => getting messages after enable ok
  bool opModeSignal = false; // 0x80 - Signal => NOT getting messages after enable ok ==> not supported ?
  bool opModeDetectionHID = false; // 0x80 - DetectionHID => getting messages after enable ok
// - 0x68 - Frequency Group -
  uint16_t idleFrequency = 200; // Hz
  uint16_t fingerFrequency = 64; // Hz
// - 0x73 - DeviceConfig Group -
  uint8_t trackedTouches = 2; // 0x80
  bool mergeTouches = false; // 0x85 - detection mode
  bool reflectiveEdgeFilter = false; // 0x85 - detection mode
  uint8_t reportedTouches = 2; // 0x86
  // 0xA2 - Touch Active Area
    uint16_t minX = 127; // 0x80
    uint16_t minY = 127; // 0x81
    uint16_t maxX = 128; // 0x82
    uint16_t maxY = 128; // 0x83
    bool reverseX = true; // 0x84
    bool reverseY = true; // 0x85
    bool flipXY = true; // 0x86
    uint8_t offsetX = 128; // 0x87
    uint8_t offsetY = 128; // 0x88
  // 0xA4 - Restricts
    bool maxSizeEn = true; // 0x80
    uint16_t maxSize = 0; // 0x81
    bool minSizeEn = true; // 0x82
    uint16_t minSize = 0; // 0x83
  // 0xA7 - Display
    uint16_t displaySizeX = 0; // 0x80
    uint16_t displaySizeY = 0; // 0x81
```

So-called "study" of a "map" of the available settings for the "device config" part (I whished I had found such cheatsheet earlier -> very helpful when not capable of using the "PDU" stuff :| .. )
```cpp
// /!\ [not that ? ] many things wrong -> gotta fix/update that using device-gotten one ( when I manage time to do so ;p )
// Also, I may ( surely ? ) be hugely wrong about the device config that I generated ( doing so without the AirBar sensor module connected - don't know yet if that influences what appears / can be set using the workbench generator )
uint8_t deviceConfBytesT[] = {0xEE, 0x41,            // lvl 0, Len @[1],  idx 0  = I2C payload
                             0xEE, 0x3F,             // lvl 1, Len @[3],  idx 1  = ASN.1 payload
                             0x40, 0x02, 0x02, 0x00, //                   idx 2  - Const device identifier
                             0x73,0x39,              // lvl 2, Len @[9],  idx 3  = DeviceConfig Group
                               0x80, 0x01, 0x00,     // lvl 3, Len @[11], idx 4 - trackedTouches R: not present in device response yet digged from workgench generator
                               0xA2, 0x1B,           // lvl 3, Len @[14], idx 5 = Touch Active Area
                                 0x80, 0x01, 0x00,   // lvl 4, Len @[16], idx 6 - minX
                                 0x81, 0x01, 0x00,   // lvl 4, Len @[19], idx 7 - minY
                                 0x82, 0x01, 0x00,   // lvl 4, Len @[22], idx 8 - maxX
                                 0x83, 0x01, 0x00,   // lvl 4, Len @[25], idx 9 - maxY
                                 0x84, 0x01, 0x00,   // lvl 4, Len @[28], idx 10 - reverseX
                                 0x85, 0x01, 0x00,   // lvl 4, Len @[31], idx 11 - reverseY
                                 0x86, 0x01, 0x00,   // lvl 4, Len @[34], idx 12 - flipXY
                                 0x87, 0x01, 0x00,   // lvl 4, Len @[37], idx 13 - offsetX
                                 0x88, 0x01, 0x00,   // lvl 4, Len @[40], idx 14 - offsetY
                               0xA4, 0x0C,           // lvl 3, Len @[43], idx 15 = Restricts
                                 0x80, 0x01, 0x00,   // lvl 4, Len @[45], idx 16 - maxSizeEn
                                 0x81, 0x01, 0x00,   // lvl 4, Len @[48], idx 17 - maxSize
                                 0x82, 0x01, 0x00,   // lvl 4, Len @[51], idx 18 - minSizeEn
                                 0x83, 0x01, 0x00,   // lvl 4, Len @[54], idx 19 - minSize
                                 // R: mergedTouches & reflective filtering should probably stand here at 0x85 ?
                                 // digged from Zforce::DetectionMode(bool mergeTouches, bool reflectiveEdgeFilter) & present on device response 
                                 0x86, 0x01, 0x00,   // lvl 4, Len @[57], idx 20 - reportedTouches ( digged from Zforce::ReportedTouches & present on device response )
                               0xA7, 0x06,           // lvl 3, Len @[60], idx 21 = Display
                                 0x80, 0x01, 0x00,   // lvl 4, Len @[62], idx 22 - displaySizeX
                                 0x81, 0x01, 0x00};  // lvl 4, Len @[65], idx 23 - displaySizeY
/* - my model ,documented & kept intact from my ongoing mess ;p - R: gotten from workbench generator OR device ? => send dummy conf to device & re-ask it ;p
uint8_t deviceConfBytes[] = {0xEE, 0x41,             // lvl 0, Len @[1]  = I2C payload
                             0xEE, 0x3F,             // lvl 1, Len @[3]  = ASN.1 payload
                             0x40, 0x02, 0x02, 0x00, //                  - Const device identifier
                             0x73,0x39,              // lvl 2, Len @[9]  = DeviceConfig Group
                               0x80, 0x01, 0x00,     // lvl 3, Len @[11] - trackedTouches
                               0xA2, 0x1B,           // lvl 3, Len @[14] = Touch Active Area
                                 0x80, 0x01, 0x00,   // lvl 4, Len @[16] - minX
                                 0x81, 0x01, 0x00,   // lvl 4, Len @[19] - minY
                                 0x82, 0x01, 0x00,   // lvl 4, Len @[22] - maxX
                                 0x83, 0x01, 0x00,   // lvl 4, Len @[25] - maxY
                                 0x84, 0x01, 0x00,   // lvl 4, Len @[28] - reverseX
                                 0x85, 0x01, 0x00,   // lvl 4, Len @[31] - reverseY
                                 0x86, 0x01, 0x00,   // lvl 4, Len @[34] - flipXY
                                 0x87, 0x01, 0x00,   // lvl 4, Len @[37] - offsetX
                                 0x88, 0x01, 0x00,   // lvl 4, Len @[40] - offsetY
                               0xA4, 0x0C,           // lvl 3, Len @[43] = Restricts
                                 0x80, 0x01, 0x00,   // lvl 4, Len @[45] - maxSizeEn
                                 0x81, 0x01, 0x00,   // lvl 4, Len @[48] - maxSize
                                 0x82, 0x01, 0x00,   // lvl 4, Len @[51] - minSizeEn
                                 0x83, 0x01, 0x00,   // lvl 4, Len @[54] - minSize
                                 // 84 not present
                                 // 85 not accessible from workbench generator yet present from device response ( below )
                                 0x86, 0x01, 0x00,   // lvl 4, Len @[57] - reportedTouches ? ==> unsure --> re-digg ..
                               0xA7, 0x06,           // lvl 3, Len @[60] = Display
                                 0x80, 0x01, 0x00,   // lvl 4, Len @[62] - displaySizeX
                                 0x81, 0x01, 0x00};  // lvl 4, Len @[65] - displaySizeY
*/
/* - other model, from device ( using notRerverseX as a dummy config write to get full config response
uint8_t deviceConfByteD[] = {0xEE, 0x45,
                             0xEF, 0x43,
                             0x40, 0x2, 0x2, 0x0,
                             0x73, 0x3D,
                               0xA2, 0x1D,
                                 0x80, 0x1, 0x24, // 0x24 == 36 minX
                                 0x81, 0x1, 0x0,
                                 0x82, 0x2, 0xD, 0xA4, // (0xD << 8 ) | 0xA4 == 3492 maxX --> hence (3492 - 36 == 3456 ) for (maxX - minX == displaySizeX )
                                 0x83, 0x2, 0xC, 0xCD, // (0xC << 8 ) | 0xCD == 3277 maxY
                                 0x84, 0x1, 0x0,
                                 0x85, 0x1, 0x0,
                                 0x86, 0x1, 0x0,
                                 0x87, 0x1, 0x0,
                                 0x88, 0x1, 0x0,
                               0xA4, 0xC, 
                                 0x80, 0x1, 0x0,
                                 0x81, 0x1, 0x0,
                                 0x82, 0x1, 0x0,
                                 0x83, 0x1, 0x0,
                                 // 84 not present
                                 0x85, 0x1, 0x0,
                                 0x86, 0x1, 0x2, // 2 reported touches
                               0xA7, 0x8,
                                 0x80, 0x2, 0xD, 0x80, // (0xD << 8 ) | 0x80 == 3456 displaySizeX: joke ! => "suite in 4 & 90° laughing rocket lifting-off" using pairs of 0,x,8,< + a D and an | ? 0xD<<8|0x80
                                 0x81, 0x2, 0xC, 0xCD // (0xC << 8 ) | 0xCD == 3277 displaySizeY
*/
```
