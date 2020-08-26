/*
    NeoNode zForce sensor - Arduino Pro Micro test sketch
    v0.1a 
    22/08/2020 @StephaneAG
*/

//#define USE_I2C_LIB 1 // comment-out if using official SAMD21-based NeoNode Prototyping board
#include <Zforce.h> // R: modded version including I2C lib or modified "MYWire" lib
#define PIN_NN_DR 7 // INT6
#define PIN_NN_RST 5

volatile bool newTouchDataFlag = false; // new data flag works with data ready pin ISR
void dataReadyISR() { newTouchDataFlag = true; } // Interrupt Service Routine - R: keep as small/short/quick as possible !

void setup() {
  Serial.begin(115200);
  while(!Serial){}; // wait for serial conn

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
  // receives touch updates via ISR & external interrupt
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
        }
      }

      zforce.DestroyMessage(touch);
    }
    newTouchDataFlag = false;
  }
}
