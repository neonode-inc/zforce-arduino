/*
    NeoNode zForce sensor - Arduino Pro Micro i2c debug sketch

    Main goal: find the fix to allow setting more than only the 'enable' messge .. :/ -> DONE!
    => Now onto getting the most affined config possible in least amount of requests & streamlining the init workflow
    ( we'll see later after deviceConfig stuff if needed to totally replace zForce lib - hopefully not, since message handling seems quite neat ? ..
    
    22/08/2020 @StephaneAG
*/
#include <Zforce.h> // to borrow some touch notification handlers/parsers ? ;p

//#include <Wire.h> // R: TO TWEAK IN SEPARATE SUB DIR INSTEAD OF WITHIN ARDUINO APP PACKAGE !!
// ======> GOTCHA !! --> the Wire.h / twi.h 32 bytes i2c buffer was the thing causing the troubles !!
// at  {Arduino install}/hardware/arduino/avr/libraries/Wire/src/Wire.h
// ------> ALSO: for other uCs ( and the SAMD Arduino-compatible board ), the NeoNode-modded I2C lib should also up the limit .. (I2C.h MAX_BUFFER_SIZE 32 )
#//include "MYWire/MYWire.h"; // only mod: buffer maxed out at 128 instead of 32 ..
#include <MYWire.h> // other way than above ( only requires users to load via menu & select .zip/dir ) & can be included from zForce lib as well ;p

#include <AbsMouse.h> // Thx https://github.com/jonathanedgecombe/absmouse while at it, why not get back what I already had hackily yet very nicely ( smooth ) working ? :p

#define MAX_PAYLOAD 127
#define ZFORCE_I2C_ADDRESS 0x50

#define PIN_NN_DR 7 // INT6
#define PIN_NN_RST 5

uint8_t buffer1[MAX_PAYLOAD];

volatile bool newTouchDataFlag = false; // new data flag works with data ready pin ISR
void dataReadyISR() { newTouchDataFlag = true; }

// quick messages
// Enable's
uint8_t msg_enable[] = {0xEE, 0x0A, 0xEE, 0x08, 0x40, 0x02, 0x02, 0x00, 0x65, 0x02, 0x81, 0x00}; // Enable
uint8_t msg_disable[] = {0xEE, 0x0A, 0xEE, 0x08, 0x40, 0x02, 0x02, 0x00, 0x65, 0x02, 0x80, 0x00}; // Disable
uint8_t msg_reset[] = {0xEE, 0x0A, 0xEE, 0x08, 0x40, 0x02, 0x02, 0x00, 0x65, 0x02, 0x82, 0x00}; // Reset
/**/
// Device Config's
//uint8_t msg_reverseX[] = {0xEE, 0x0D, 0xEE, 0x0B, 0x40, 0x02, 0x02, 0x00, 0x73, 0x05, 0xA2, 0x03, 0x84, 0x01, 0xFF}; // ReversedX
uint8_t msg_notRevX[] = {0xEE, 0x0D, 0xEE, 0x0B, 0x40, 0x02, 0x02, 0x00, 0x73, 0x05, 0xA2, 0x03, 0x84, 0x01, 0x00}; // not ReversedX - dummy to get config
// from online docs example - not available in generator nor lib  ( since couldn't find a thorough list of available 'messages' -> but gotta better dig the docs or the .asn 'pdu' ? .. )
uint8_t msg_reflectiveEdgeFilter[] = {0xEE, 0x0B, 0xEE, 0x09, 0x40, 0x02, 0x02, 0x00, 0x73, 0x03, 0x85, 0x01, 0x80}; // Refletive Edge Filter ( last byte to 00 to set off )
/**/



// -- global params for quick access & later batch setup at once, as well as quick reminder ;) --
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

// -- global setup as 4 chunks/requests ( 3 for setup, 1 for enable ) --
// R => digg docs to find getters in order to use defaults & override only when needed ;p
// - OperationMode Group
// 'empty' placeholder
//uint8_t opModeBytes[] = {0xEE, 0x17, 0xEE, 0x15, 0x40, 0x02, 0x02, 0x00, 0x67, 0x0F, 0x80, 0x01, 0x00, 0x81, 0x01, 0x00, 0x82, 0x01, 0x00, 0x83, 0x01, 0xFF, 0x84, 0x01, 0x00}; // ok with added prefix !
uint8_t opModeBytes[] = {0xEE, 0x17,
                         0xEE, 0x15, 0x40, 0x02, 0x02, 0x00, 0x67, 0x0F, 
                         0x80, 0x01, (uint8_t)(opModeDetection ? 0xFF : 0x00),
                         0x81, 0x01, (uint8_t)(opModeSignal ? 0xFF : 0x00), 0x82, 0x01, 0x00,
                         0x83, 0x01, (uint8_t)(opModeDetectionHID ? 0xFF : 0x00), 0x84, 0x01, 0x00};
                         
// - Frequency Group
//'empty' placeholder
//uint8_t frequencyBytes[] = {0xEE, 0x0E, 0xEE, 0x0C, 0x40, 0x02, 0x02, 0x00, 0x68, 0x06, 0x80, 0x01, 0x00, 0x82, 0x01, 0x00}; //ok with added prefix
const uint8_t length8 = 8;
uint8_t frequencyBytes[] = {0xEE, length8 + 8,
                            0xEE, length8 + 6,
                            0x40, 0x02, 0x02, 0x00,
                            0x68, length8,
                              0x80, 0x02, (uint8_t)(fingerFrequency >> 8), (uint8_t)(fingerFrequency & 0xFF),
                              0x82, 0x02, (uint8_t)(idleFrequency   >> 8), (uint8_t)(idleFrequency   & 0xFF)};
                            
// - DeviceConfig Group
const uint8_t length16 = 16;
// default config
uint8_t deviceConfBytes[] = {0xEE, 0x45, 0xEE, 0x43, 0x40, 0x2, 0x2, 0x0, 0x73, 0x3D, 0xA2, 0x1D, 0x80, 0x1, 0x24, 0x81, 0x1, 0x0, 0x82, 0x2, 0xD, 0xA4, 0x83, 0x2, 0xC, 0xCD, 0x84, 0x1, 0x0, 0x85, 0x1, 0x0, 0x86, 0x1, 0x0, 0x87, 0x1, 0x0, 0x88, 0x1, 0x0, 0xA4, 0xC, 0x80, 0x1, 0x0, 0x81, 0x1, 0x0, 0x82, 0x1, 0x0, 0x83, 0x1, 0x0, 0x85, 0x1, 0x0, 0x86, 0x1, 0x2, 0xA7, 0x8, 0x80, 0x2, 0xD, 0x80, 0x81, 0x2, 0xC, 0xCD};
// 'empty' placeholder
// R: in the following, not added yet: mergeTouches/reflectiveEdgeFilter ( 0x85 ), reportedTouches ( 0x86 ) - 23 items currently
uint8_t lenPositions[] = {1, 3, 9, 11, 14, 16, 19, 22, 25, 28, 31, 34, 37, 40, 43, 45, 48, 51, 54, 57, 60, 62, 65}; // 'const'
uint8_t lenPositionsT[] = {1, 3, 9, 11, 14, 16, 19, 22, 25, 28, 31, 34, 37, 40, 43, 45, 48, 51, 54, 57, 60, 62, 65}; // 'll be messed with
uint8_t    lenLevels[] = {0, 1, 2,  3,  3,  4,  4,  4,  4,  4,  4,  4,  4,  4,  3,  4,  4,  4,  4,  4,  3,  4,  4};
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

void setup() {
  Serial.begin(115200);
  while(!Serial){}; // wait for serial conn

  AbsMouse.init(1790, 1119); // R: native resolution 16-inch (3072 x 1920) / 1,7152428811

  zforce.Start(PIN_NN_DR); // to borrow Touch parsing capabilities
  //MYWire.begin();
  setupSensor(PIN_NN_RST, PIN_NN_DR); // handles: init & interrupt setup / full config / start
  //zforce.Start(PIN_NN_DR); // to borrow Touch parsing capabilities
}

void loop() {
  //onSensorReceiveInt(); // using Interrupt pin for DataReady
  onSensorReceivedInt_zForce(); // same while borrowing zForce lib's touch parser
  //onSensorReceive(); // polling for DataReady HIGH
}


// only helper borrowed from zForce lib ( for now .. )
void onSensorReceivedInt_zForce(){
  Message* touch = zforce.GetMessage();
    if (touch != NULL){
      //Serial.println("Touch Msg");
      if (touch->type == MessageType::TOUCHTYPE){
        for (uint8_t i = 0; i < ((TouchMessage*)touch)->touchCount; i++){
          /* - not printed: too many ?
          Serial.print("X is: ");
          Serial.println(((TouchMessage*)touch)->touchData[i].x);
          Serial.print("Y is: ");
          Serial.println(((TouchMessage*)touch)->touchData[i].y);
          Serial.print("ID is: ");
          Serial.println(((TouchMessage*)touch)->touchData[i].id);
          Serial.print("Event is: ");
          Serial.println(((TouchMessage*)touch)->touchData[i].event);
          */
          // 0 -> touchstart, 1 -> touchmove, 2 -> touchend
          //if(((TouchMessage*)touch)->touchData[i].event == 1) Mouse.move(((TouchMessage*)touch)->touchData[i].x, ((TouchMessage*)touch)->touchData[i].y);
          // quick & dirty reverseXY & remap
          long xPos = ((TouchMessage*)touch)->touchData[i].x > 3448 ? 3448 : ((TouchMessage*)touch)->touchData[i].x;
          long yPos = ((TouchMessage*)touch)->touchData[i].y > 2147 ? 2147 : ((TouchMessage*)touch)->touchData[i].y;
          long mappedXpos = map(xPos, 0, 3448, 1791, 0);
          //long mappedYpos = map(yPos, 0, 2147, 1119, 0) -25; // half-finger offset
          long mappedYpos = map(yPos, 0, 2147, 1119, 0) -75; // full-finger offset ( to get cursor right above )
          /* - 1st way -
            'move cursor while moving & left click on touchend' ( aka use hold ctrl for right click .. )
          */
          //if(((TouchMessage*)touch)->touchData[i].event == 1) Mouse.move(mappedXpos, mappedYpos);
          if(((TouchMessage*)touch)->touchData[i].event == 1) AbsMouse.move(mappedXpos, mappedYpos);
          // to act as std click on hover-out
          if(((TouchMessage*)touch)->touchData[i].event == 2){
            AbsMouse.press(MOUSE_LEFT);
            AbsMouse.release(MOUSE_LEFT);
          }
          /**/
          // other way
          /*
          if(((TouchMessage*)touch)->touchData[i].event == 1){
            AbsMouse.move(mappedXpos, mappedYpos);
            AbsMouse.press(MOUSE_LEFT);
          }
          if(((TouchMessage*)touch)->touchData[i].event == 2){
            AbsMouse.release(MOUSE_LEFT);
          }
          */
        }
      } else {
        Serial.print("Msg received Not of TouchType");
        Serial.println((uint8_t)touch->type);
      }

      zforce.DestroyMessage(touch);
    }
}


// ---- My Funky Stuff ----

void onSensorReceiveInt(){
  if(newTouchDataFlag == true){ // set via ISR from interrupt change ( rising edge )
    // ..
    int retV = readSensor(false); // R: disable inner debug logs or interrupt will choke ;p
    Serial.print("retVal: 0x"); Serial.println(retV, HEX);
    Serial.println();
    // ..
    newTouchDataFlag = false;
  }
}

void onSensorReceive(){
  int retV = readSensor(false); // R: disable inner debug logs or interrupt will choke ;p
  Serial.print("retVal: 0x"); Serial.println(retV, HEX);
  Serial.println();
}

void initSensor(int mPIN_NN_RST, int mPIN_NN_DR){
  // pinMode & init sequence: pull reset low then back high
  pinMode(mPIN_NN_DR, INPUT); // setup Data Ready pin
  pinMode(mPIN_NN_RST, OUTPUT); // setup Reset pin
  // Reset sensor
  digitalWrite(mPIN_NN_RST, LOW);
  delay(10);
  digitalWrite(mPIN_NN_RST, HIGH);
  delay(50);
}

void initSensorInterrupt(int mPIN_NN_DR){
  attachInterrupt(digitalPinToInterrupt(mPIN_NN_DR), dataReadyISR, RISING);
}

void setDeviceConfigWIP(){
  // Debug: log the std deviceConfig for better compare
  for(int i=0; i < sizeof(deviceConfBytesT)/sizeof(deviceConfBytesT[0]); i++){
    //Serial.print(buffer1[i], HEX); // print the byte as hex
    //Serial.print("\t"); // print spacer
    //Serial.print("0x");
    Serial.print(deviceConfBytesT[i], HEX); // print the byte as hex
    Serial.print(", "); // print spacer
  }
  Serial.print("\ttotal bytes: "); Serial.println( sizeof(deviceConfBytesT)/sizeof(deviceConfBytesT[0]) );

  // 0 cleanup buffer1 before borrowing it ( may have been filled previously if not clearing it while getting boot complete stuff )
  memset(buffer1, 0, MAX_PAYLOAD); //clearBuffer since we 'borrowed' it to generate our device config
  
  // 1: write template to buffer 'as is'
  //memcpy(buffer1, deviceConfBytesT, sizeof deviceConfBytesT); // untested yet
  memcpy(buffer1, deviceConfBytesT, sizeof(deviceConfBytesT)/sizeof(deviceConfBytesT[0])); // logically should work fine ?

  // Debug log buffer to make sure it contains our full config
  for(int i=0; i < 128; i++){
  //for(int i=0; i < sizeof(deviceConfBytesT)/sizeof(deviceConfBytesT[0]); i++){
    //Serial.print(buffer1[i], HEX); // print the byte as hex
    //Serial.print("\t"); // print spacer
    //Serial.print("0x");
    Serial.print(buffer1[i], HEX); // print the byte as hex
    Serial.print(", "); // print spacer
  }
  //Serial.print("\ttotal bytes: "); Serial.println( sizeof(deviceConfBytesT)/sizeof(deviceConfBytesT[0]) );
  Serial.print("\ttotal buffer, aka bytes: "); Serial.println(128);

  int templateCurrLen = sizeof(deviceConfBytesT)/sizeof(deviceConfBytesT[0]);// store current known template len to be able to increase it while looping as well as usable to 'relocate' stuff
  // 2: loop over known len's positions while skipping beginning of deviceConfBytesT up to the 1st item of interest
  for(int i= 3; i < sizeof(lenPositions)/sizeof(lenPositions[0]); i++){
    // Debug IDs & Lens
    uint8_t ID = deviceConfBytesT[lenPositions[i]-1];
    //int ID = (int)deviceConfBytesT[lenPositions[i]-1];
    uint8_t len = deviceConfBytesT[lenPositions[i]];
//    Serial.print("ID: 0x"); Serial.print((byte)ID, HEX); Serial.print("\tLen: 0x"); Serial.print((byte)len, HEX);
//    Serial.print("\tLen position:"); Serial.print(lenPositions[i]); Serial.print("\tlvl:"); Serial.println(lenLevels[i]);
    
    // if it's one of those we're interested in ( that is, params ), currently using the 
    int ib = lenPositionsT[i]; // get the position of the len item we're currently looping over - R: are being update as we go to reflect the changes in the buffer
    bool needUpdate = false; // whether or not to rollback after update to recalculate lvl-n lengths
    int paramBytesUpdate = 0; // to be set to 1 or 2 depending on what's to be done with tmp bytes & further mod ( push, rollback, .. )
    byte tmpByte1 = 0;
    byte tmpByte2 = 0;
    switch(ib){
      case 11: // trackedTouches: will fit in byte
//        Serial.println("trackedTouches ..");
        //buffer1[lenPositions[i]+1] = trackedTouches;
        paramBytesUpdate++;
        tmpByte1 = trackedTouches;
        break;
      case 16: // minX
//        Serial.println("minX ..");
        //if(minX < 127) buffer1[lenPositions[i]+1] = minX; // was previously checking with 255 ( 'minX & 0xFF == minX' )
        paramBytesUpdate++; // we have at least one sure byte
        if(minX < 127) tmpByte1 = minX; // was previously checking with 255 ( 'minX & 0xFF == minX' )
        else {
          paramBytesUpdate++; // we have also a second one
          //buffer1[lenPositions[i]] = 2; // update len of current param
          //templateCurrLen++; // increase current total buffer bytes counter
          //for(int x= templateCurrLen; x > lenPositions[i]+1; x--){ buffer1[x] = buffer1[x-1]; } // increase by 1 the position of everything in buffer after the current position
          //buffer1[lenPositions[i]+1] = minX >> 8; // store 1st half of split byte ( replacing default single-byte value )
          //buffer1[lenPositions[i]+2] = minX & 0xFF; // store 2nd half of split byte ( in the space made available for ovewrite 2 lines above by pushing stuff after to 'free' one byte )
          tmpByte1 = minX >> 8; // store 1st half of split byte ( replacing default single-byte value )
          tmpByte2 = minX & 0xFF; // store 2nd half of split byte ( in the space made available for ovewrite 2 lines above by pushing stuff after to 'free' one byte )
//          Serial.print("new value: "); Serial.println( (tmpByte1 << 8)| tmpByte2 ); // ok merging it back ;p
          //needUpdate = true; // indicate we need to 'rollback & adjust' things ..
        }
        break;
       case 19: // minY
         paramBytesUpdate++;
         if(minY < 127) tmpByte1 = minY;
         else { paramBytesUpdate++; tmpByte1 = minY >> 8; tmpByte2 = minY & 0xFF; }
         break;
       case 22: // maxX
         paramBytesUpdate++;
         if(maxX < 127) tmpByte1 = maxX;
         else { paramBytesUpdate++; tmpByte1 = maxX >> 8; tmpByte2 = maxX & 0xFF; }
         break;
       case 25: // maxY
         paramBytesUpdate++;
         if(maxY < 127) tmpByte1 = maxY;
         else { paramBytesUpdate++; tmpByte1 = maxY >> 8; tmpByte2 = maxY & 0xFF; }
         break;
       case 28: // reverseX
         paramBytesUpdate++; tmpByte1 = (reverseX == true)? 0xFF : 0x00;
         break;
       case 31: // reverseY
         paramBytesUpdate++; tmpByte1 = (reverseY == true)? 0xFF : 0x00;
         break;
       case 34: // flipXY
         paramBytesUpdate++; tmpByte1 = (flipXY == true)? 0xFF : 0x00;
         break;
       case 37: // offsetX
         paramBytesUpdate++;
         if(offsetX < 127) tmpByte1 = offsetX;
         else { paramBytesUpdate++; tmpByte1 = offsetX >> 8; tmpByte2 = offsetX & 0xFF; }
         break;
       case 40: // offsetY
         paramBytesUpdate++;
         if(offsetY < 127) tmpByte1 = offsetY;
         else { paramBytesUpdate++; tmpByte1 = offsetY >> 8; tmpByte2 = offsetY & 0xFF; }
         break;
       case 45: // maxSizeEn
         paramBytesUpdate++; tmpByte1 = (maxSizeEn == true)? 0xFF : 0x00;
         break;
       case 48: // maxSize
         paramBytesUpdate++;
         if(maxSize < 127) tmpByte1 = maxSize;
         else { paramBytesUpdate++; tmpByte1 = maxSize >> 8; tmpByte2 = maxSize & 0xFF; }
         break;
       case 51: // minSizeEn
         paramBytesUpdate++; tmpByte1 = (minSizeEn == true)? 0xFF : 0x00;
         break;
       case 54: // minSize
         paramBytesUpdate++;
         if(minSize < 127) tmpByte1 = minSize;
         else { paramBytesUpdate++; tmpByte1 = minSize >> 8; tmpByte2 = minSize & 0xFF; }
         break;
       case 57: // reportedTouches R: unsure ==> digg where it stands ( whether at root of device conf or within 0xA4 )
         paramBytesUpdate++; tmpByte1 = (reportedTouches == true)? 0xFF : 0x00;
         break;
       case 62: // displaySizeX
         paramBytesUpdate++;
         if(displaySizeX < 127) tmpByte1 = displaySizeX;
         else { paramBytesUpdate++; tmpByte1 = displaySizeX >> 8; tmpByte2 = displaySizeX & 0xFF; }
         break;
       case 65: // displaySizeY
         paramBytesUpdate++;
         if(displaySizeY < 127) tmpByte1 = displaySizeY;
         else { paramBytesUpdate++; tmpByte1 = displaySizeY >> 8; tmpByte2 = displaySizeY & 0xFF; }
         break;
      default:
        break;
    }
    if(paramBytesUpdate > 0){ // at least one param byte has been modified: need buffer update
      buffer1[lenPositions[i]+1] = tmpByte1; // write 1st byte in all cases
      if(paramBytesUpdate > 1){ // more than one param byte has been modified: need 'beefy' buffer update ;P ( 'beefer buffy' update ^^ )
        buffer1[lenPositions[i]] = 2; // update len of current param
        templateCurrLen++; // increase current total buffer bytes counter
        for(int x= templateCurrLen; x > lenPositionsT[i]+1; x--){ buffer1[x] = buffer1[x-1]; } // increase by 1 the position of everything in buffer after the current position
        for(int j= i; j < sizeof(lenPositions)/sizeof(lenPositions[0]); j++){ lenPositionsT[j]++; } // increase by 1 the tmp position of everything in buffer after the current position
        buffer1[lenPositions[i]+2] = tmpByte2; // write 2nd byte
        // -- start rollback
        // 'rollback' increase by 1 all of its 'previous 1st level siblings' len in buffer ( look for current item's level-1, once found, +1 their length then look for -level-2 .. until level == 0 )
        int rollbackLvlStart = lenLevels[i];
        Serial.print("Rollback start level: "); Serial.println(rollbackLvlStart);
        for(int y = i; y >= 0; y--){
          Serial.print("len level of previous sibling "); Serial.println(lenLevels[y]);
          if(lenLevels[y] < rollbackLvlStart){
            rollbackLvlStart--; // decrement current deepness it tree to later increment only 'ascendant' tree elements
            buffer1[lenPositions[y]]++; // increment that element's length since it emcompasses the item we're originally looping on ( a 'descendant' of the same 'tree' )
          }
        }
        // -- end rollback
      }
    }
  }
  // debug / log it
  for(int i=0; i < 128; i++){
  //for(int i=0; i < sizeof(deviceConfBytesT)/sizeof(deviceConfBytesT[0]); i++){
    //Serial.print(buffer1[i], HEX); // print the byte as hex
    //Serial.print("\t"); // print spacer
    //Serial.print("0x");
    Serial.print(buffer1[i], HEX); // print the byte as hex
    Serial.print(", "); // print spacer
  }
  //Serial.print("\ttotal bytes: "); Serial.println( sizeof(deviceConfBytesT)/sizeof(deviceConfBytesT[0]) );
  Serial.print("\ttotal buffer, aka bytes: "); Serial.println(128);
  // write it & then clear it
  // ..
  memset(buffer1, 0, MAX_PAYLOAD); //clearBuffer since we 'borrowed' it to generate our device config
}

signed int configSensor(){
  int retV = readSensor(true);
  Serial.print("retVal: 0x"); Serial.println(retV, HEX);
  Serial.println();
  if(retV != 0x63) return -1; // not Boot completed payload -> falsy / some error happened
  // - opMode
  /*
  writeSensor(opModeBytes);
  retV = readSensor(true); 
  Serial.print("retVal: 0x"); Serial.println(retV, HEX);
  Serial.println();
  if(retV != 0x67) return -2; // not OperationMode Group payload -> falsy / some error happened
  */
  // - freq
  /*
  writeSensor(frequencyBytes);
  retV = readSensor(true); 
  Serial.print("retVal: 0x"); Serial.println(retV, HEX);
  Serial.println();
  if(retV != 0x68) return -3; // not Frequency Group payload -> falsy / some error happened
  */
  // - device conf
  /*
  writeSensor(deviceConfBytes); // R: the one backup up from <to be determined>
  retV = readSensor(true); 
  Serial.print("retVal: 0x"); Serial.println(retV, HEX);
  Serial.println();
  if(retV != 0x73) return -4; // not DeviceConfig Group payload -> falsy / some error happened
  */

  /**/
  // -> to come: full device conf ! => nearly there ! ;P
  setDeviceConfigWIP();
  // dummy write to get current device config
  // Size differs: template one is 76 bytes, device one is 71
  writeSensor(msg_notRevX);
  retV = readSensor(true); 
  Serial.print("retVal: 0x"); Serial.println(retV, HEX);
  Serial.println();
  if(retV != 0x73) return -4; // not DeviceConfig Group payload -> falsy / some error happened
  /**/
  
  // - enable
  writeSensor(msg_enable);
  retV = readSensor(true); 
  Serial.print("retVal: 0x"); Serial.println(retV, HEX);
  Serial.println();
  if(retV != 0x65) return -5; // not Enable Group payload -> falsy / some error happened
  //else return 1; // enabled ok
  return 1;
}

void setupSensor(int mPIN_NN_RST, int mPIN_NN_DR){
  //MYWire.begin(); // R: moved out when using zForce ( digg what calling twice begin() could do 1st .. )
  initSensor(mPIN_NN_RST, mPIN_NN_DR);
  initSensorInterrupt(mPIN_NN_DR);
  signed int confStartRet = configSensor();
  if(confStartRet < 0 ){ Serial.print("Error while configuring :/ : "); Serial.println(confStartRet); }
  else if(confStartRet == 1){ Serial.print("Ready to receive notifications from sensor ;P"); }
}

void writeSensor(uint8_t *bytesArr){
  MYWire.beginTransmission(ZFORCE_I2C_ADDRESS);
  //MYWire.write(bytesArr, sizeof(bytesArr)/sizeof(bytesArr[0]) );
  MYWire.write(bytesArr, bytesArr[1]+2 );
  MYWire.endTransmission();
}

int readSensor(bool debug){ return readSensor(debug, true); } // helper to clear buffer by default if not specified to keep it intact
int readSensor(bool debug, bool clearBuff){
  int ok = 0;
  uint16_t timeout = 500;
  do {
    if(digitalRead(PIN_NN_DR) == HIGH){
      MYWire.requestFrom(ZFORCE_I2C_ADDRESS, 2);
      buffer1[0] = MYWire.read();
      buffer1[1] = MYWire.read();
  
      int index = 2;
      MYWire.requestFrom(ZFORCE_I2C_ADDRESS, buffer1[1]);
      while (MYWire.available()){ buffer1[index++] = MYWire.read(); }
      ok = 2; // in case parsing didn't support it yet
      //msg = virtualParse(buff)
      switch(buffer1[2]){
        case 0xEF:
        {
          Serial.println("Response ( from request ) payload");
          ok = 0xEF; // R: type depends on what was last sent from host
          // BUT ! we can at least differentiate 'domains of concerns' ( somewhat close to how 'workbench' generator splits stuff .. )
          if (buffer1[8] == 0x65){
            ok = 0x65;
            if(debug) Serial.println("Enable Group payload");
          } else if (buffer1[8] == 0x67){
            ok = 0x67;
            if(debug) Serial.println("OperationMode Group payload");
          } else if (buffer1[8] == 0x68){
            ok = 0x68;
            if(debug) Serial.println("Frequency Group payload");
          } else if (buffer1[8] == 0x73){
            ok = 0x73;
            if(debug) Serial.println("DeviceConfig Group payload");
          }
          // else, go for the 'generic' 0xEF ( & check what we sent for more ;p )
        }
        break;
        case 0xF0:
        {
          if (buffer1[8] == 0xA0){
            ok = 0xA0;
            if(debug) Serial.println("Touch payload");
          } else if (buffer1[8] == 0x63){
            ok = 0x63;
            if(debug) Serial.println("Boot completed payload");
          }
        }
        break;

        default:
        break;
      }
      if(debug){
        for(int i=0; i < index; i++){
          //Serial.print(buffer1[i], HEX); // print the byte as hex
          //Serial.print("\t"); // print spacer
          Serial.print("0x");
          Serial.print(buffer1[i], HEX); // print the byte as hex
          Serial.print(", "); // print spacer
        }
        Serial.print("\ttotal bytes: "); Serial.println(index);
      }
      if(clearBuff) memset(buffer1, 0, MAX_PAYLOAD); //clearBuffer
      //
    }
    delay(1);
  //} while(ok == 0);
  } while (ok == 0 && --timeout >= 0);
  if(timeout <= 0) ok = 1;
  
  return ok;
}
