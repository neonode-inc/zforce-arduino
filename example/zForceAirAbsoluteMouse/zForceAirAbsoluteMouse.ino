/*
    NeoNode zForce sensor - Arduino Pro Micro test sketch
    v0.1a - working fine as absolute mouse on Mac OS Catalina
    22/08/2020 @StephaneAG
*/

//#include <Mouse.h>
#include <AbsMouse.h> // Thx https://github.com/jonathanedgecombe/absmouse

//#include <Wire.h>
//#define USE_I2C_LIB 1
#include <Zforce.h> // R: includes modded I2C lib ..
#define PIN_NN_DR 7 // INT6
#define PIN_NN_RST 5

volatile bool newTouchDataFlag = false; // new data flag works with data ready pin ISR
void dataReadyISR() { newTouchDataFlag = true; }



void setup() {
  Serial.begin(115200);
  while(!Serial){}; // wait for serial conn

  AbsMouse.init(1790, 1119); // R: native resolution 16-inch (3072 x 1920) / 1,7152428811

  Serial.println("zforce start");

  pinMode(PIN_NN_DR, INPUT);
  pinMode(PIN_NN_RST, OUTPUT);            // setup Reset pin
  zforce.Start(PIN_NN_DR);
  
  digitalWrite(PIN_NN_RST, LOW);
  delay(10);
  digitalWrite(PIN_NN_RST, HIGH);         // Reset sensor

  delay(50);

  // Init / Boot complete
  Message* msg = zforce.GetMessage();
  do { msg = zforce.GetMessage(); } while (msg == NULL);
  if (msg != NULL){
    Serial.println("Received Boot Complete Notification");
    Serial.print("Message type is: ");
    Serial.println((int)msg->type);
    zforce.DestroyMessage(msg);
  }

  // Send and read Enable - mandatory
  Serial.println("sending enable ..");
  zforce.Enable(true);
  do { msg = zforce.GetMessage(); } while (msg == NULL);
  if (msg->type == MessageType::ENABLETYPE){
    Serial.print("Message type is: ");
    Serial.println((int)msg->type);
    Serial.println("Sensor is now enabled and will report touches.");
  }
  zforce.DestroyMessage(msg);

  // to use external interrupt instead of polling data ready through loop()
  attachInterrupt(digitalPinToInterrupt(PIN_NN_DR), dataReadyISR, RISING);
}

void loop() {
  
  // receives touch updates via polling - works fine :D
  /*
  Message* touch = zforce.GetMessage();
  if (touch != NULL){
    Serial.println("Touch Msg");
    if (touch->type == MessageType::TOUCHTYPE){
      for (uint8_t i = 0; i < ((TouchMessage*)touch)->touchCount; i++){
        Serial.print("X is: ");
        Serial.println(((TouchMessage*)touch)->touchData[i].x);
        Serial.print("Y is: ");
        Serial.println(((TouchMessage*)touch)->touchData[i].y);
        Serial.print("ID is: ");
        Serial.println(((TouchMessage*)touch)->touchData[i].id);
        Serial.print("Event is: ");
        Serial.println(((TouchMessage*)touch)->touchData[i].event);
      }
    }

    zforce.DestroyMessage(touch);
  }
  */
  
  // receives touch updates via ISR & external interrupt - also works fine :P
  /**/
  if(newTouchDataFlag== true){
    Message* touch = zforce.GetMessage();
    if (touch != NULL){
      Serial.println("Touch Msg");
      if (touch->type == MessageType::TOUCHTYPE){
        for (uint8_t i = 0; i < ((TouchMessage*)touch)->touchCount; i++){
          Serial.print("X is: ");
          Serial.println(((TouchMessage*)touch)->touchData[i].x);
          Serial.print("Y is: ");
          Serial.println(((TouchMessage*)touch)->touchData[i].y);
          Serial.print("ID is: ");
          Serial.println(((TouchMessage*)touch)->touchData[i].id);
          Serial.print("Event is: ");
          Serial.println(((TouchMessage*)touch)->touchData[i].event);
          // 0 -> touchstart, 1 -> touchmove, 2 -> touchend
          //if(((TouchMessage*)touch)->touchData[i].event == 1) Mouse.move(((TouchMessage*)touch)->touchData[i].x, ((TouchMessage*)touch)->touchData[i].y);
          // quick & dirty reverseXY & remap
          long xPos = ((TouchMessage*)touch)->touchData[i].x > 3448 ? 3448 : ((TouchMessage*)touch)->touchData[i].x;
          long yPos = ((TouchMessage*)touch)->touchData[i].y > 2147 ? 2147 : ((TouchMessage*)touch)->touchData[i].y;
          long mappedXpos = map(xPos, 0, 3448, 1791, 0);
          long mappedYpos = map(yPos, 0, 2147, 1119, 0) -25; // half-finger offset
          /* - 1st way -
          //if(((TouchMessage*)touch)->touchData[i].event == 1) Mouse.move(mappedXpos, mappedYpos);
          if(((TouchMessage*)touch)->touchData[i].event == 1) AbsMouse.move(mappedXpos, mappedYpos);
          // to act as std click on hover-out
          if(((TouchMessage*)touch)->touchData[i].event == 2){
            AbsMouse.press(MOUSE_LEFT);
            AbsMouse.release(MOUSE_LEFT);
          }
          */
          // other way
          if(((TouchMessage*)touch)->touchData[i].event == 1){
            AbsMouse.move(mappedXpos, mappedYpos);
            AbsMouse.press(MOUSE_LEFT);
          }
          if(((TouchMessage*)touch)->touchData[i].event == 2){
            AbsMouse.release(MOUSE_LEFT);
          }
        }
      }

      zforce.DestroyMessage(touch);
    }
    newTouchDataFlag = false;
  }
  /**/
}
