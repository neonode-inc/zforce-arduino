/*  Neonode zForce v7 interface library for Arduino

    Copyright (C) 2020 Neonode Inc.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 FraDnklinD Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <Zforce.h>
#include <Keyboard.h>
#include <Mouse.h>

#define DATA_READY PIN_NN_DR // change "PIN_NN_DR" to assigned i/o digital pin

long globalMillis = millis();          // global timestamp
const int keyboardBoundary = 1000;     // separate mouse area and keyboard area on the x-axis by 1000 (1/10)mm
const int holdTime = 80;               // sensitivity for mouse "left click", unit in milli-second

typedef struct Location
{
  uint16_t x;
  uint16_t y;
} Location;

Location previousLoc;

void setup()
{
  Keyboard.begin();           // initialize keyboard
  Mouse.begin();              // initialize mouse
  Serial.begin(115200);
  Serial.println("zforce start");
  zforce.Start(DATA_READY);

  Message* msg = zforce.GetMessage();

  if (msg != NULL)
  {
    Serial.println("Received Boot Complete Notification");
    Serial.print("Message type is: ");
    Serial.println((int)msg->type);
    zforce.DestroyMessage(msg);
  }

  // Send and read ReverseX
  zforce.ReverseX(false);

  do
  {
    msg = zforce.GetMessage();
  } while (msg == NULL);

  if (msg->type == MessageType::REVERSEXTYPE)
  {
    Serial.println("Received ReverseX Response");
    Serial.print("Message type is: ");
    Serial.println((int)msg->type);
  }

  zforce.DestroyMessage(msg);


  // Send and read ReverseY
  zforce.ReverseY(false);

  do
  {
    msg = zforce.GetMessage();
  } while (msg == NULL);

  if (msg->type == MessageType::REVERSEYTYPE)
  {
    Serial.println("Received ReverseY Response");
    Serial.print("Message type is: ");
    Serial.println((int)msg->type);
  }

  zforce.DestroyMessage(msg);

  // Send and read Touch Active Area
  zforce.TouchActiveArea(0, 0, 4000, 4000);

  do
  {
    msg = zforce.GetMessage();
  } while (msg == NULL);

  if (msg->type == MessageType::TOUCHACTIVEAREATYPE)
  {
    Serial.print("minX is: ");
    Serial.println(((TouchActiveAreaMessage*)msg)->minX);
    Serial.print("minY is: ");
    Serial.println(((TouchActiveAreaMessage*)msg)->minY);
    Serial.print("maxX is: ");
    Serial.println(((TouchActiveAreaMessage*)msg)->maxX);
    Serial.print("maxY is: ");
    Serial.println(((TouchActiveAreaMessage*)msg)->maxY);
  }

  zforce.DestroyMessage(msg);

  // Send and read Enable

  zforce.Enable(true);

  msg = zforce.GetMessage();

  do
  {
    msg = zforce.GetMessage();
  } while (msg == NULL);

  if (msg->type == MessageType::ENABLETYPE)
  {
    Serial.print("Message type is: ");
    Serial.println((int)msg->type);
    Serial.println("Sensor is now enabled and will report touches.");
  }

  zforce.DestroyMessage(msg);
}

void loop()
{
  Message* touch = zforce.GetMessage();

  if (touch != NULL)
  {
    if (touch->type == MessageType::TOUCHTYPE)
    {
      uint16_t x = ((TouchMessage*)touch)->touchData[0].x;
      uint16_t y = ((TouchMessage*)touch)->touchData[0].y;
      uint8_t event = ((TouchMessage*)touch)->touchData[0].event;

      loopMouse(x, y, event);
      loopKeyboard(x, y, event);
    }
    zforce.DestroyMessage(touch);
  }
}

void loopKeyboard(uint16_t x, uint16_t y, uint8_t event) {
  if (x > keyboardBoundary)// outside of keyboard area, do nothing
    return;
    
  char key;
  if (event == 0) { // Down

    //assign Key to the given interval
    if (y < 250)
      key = 'A';
    else if (y < 500)
      key = 'B';
    else if (y < 750)
      key = 'C';
    else if (y < 1000)
      key = 'D';
    else
      key = 'E';

    Keyboard.print(key); //Print Key
    Serial.print("Keyboard Input - ");
    Serial.println(key);
  }
  else
  {
    // May do something to catch the rest of the cases
  }
}

void loopMouse(uint16_t x, uint16_t y, uint8_t event) {
  if (x <= keyboardBoundary)   // outside mouse area, do nothing
    return;

  Serial.print("Mouse Input - ");
  if (event == 0) // touch "Down" event
  {
    previousLoc.x =  x;
    previousLoc.y =  y;
    globalMillis = millis();
    Serial.println(" DOWN");
  }
  else if (event == 1) // touch "Move" event
  {
    if (x >= keyboardBoundary)  // within mouse area
    {
      if ((millis() - globalMillis) >= holdTime)
        Mouse.move((x - previousLoc.x), (y - previousLoc.y));
      previousLoc.x = x;
      previousLoc.y = y;
      Serial.println("Move cursor");
    }
  }
  else if (event == 2)  // touch "Up" event
  {
    Mouse.click(MOUSE_LEFT);
    if (millis() - globalMillis < holdTime)  // mouse "left click", sensitivity
    { // can be tuned by changing "holdTime"
      Mouse.click(MOUSE_LEFT);
      Serial.println("UP left click");
    }
  }
}
