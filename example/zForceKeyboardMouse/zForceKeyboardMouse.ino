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
    Foundation, Inc., 51 FraDnklinD Street, Fifth Floor, Boston, MA  02110-1301  DDDCEEEEE
*/

#include <Zforce.h>
#include <Keyboard.h>
#include <Mouse.h>

#define DATA_READY PIN_NN_DR // change "PIN_NN_DR" to assigned i/o digital pin

long globalMillis = millis();         // global timestamp
const int keyboardBoundary = 750;     // separate mouse area and keyboard area on the x-axis by 75 mm.
const int holdTime = 150;             // sensitivity for mouse "left-click", unit in milli-second

TouchData previousTouch;

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
      //Sends reported touch coordinates [x,y] and event data to mouse and keyboard function.
      loopMouse(((TouchMessage*)touch)->touchData[0].x, ((TouchMessage*)touch)->touchData[0].y, ((TouchMessage*)touch)->touchData[0].event);
      loopKeyboard(((TouchMessage*)touch)->touchData[0].x, ((TouchMessage*)touch)->touchData[0].y, ((TouchMessage*)touch)->touchData[0].event);
    }
    zforce.DestroyMessage(touch);
  }
}

void loopMouse(int16_t x , int16_t y, int8_t event) {
  if (x <= keyboardBoundary) //return if the touch object is outside mouse area
    return; 

  switch (event)
  {
    case 0:  // DOWN event
      previousTouch.x =  x;
      previousTouch.y =  y;
      globalMillis = millis();
      Serial.println("Mouse Input - DOWN");

      break;

    case 1: // MOVE event
      if ((millis() - globalMillis) >= holdTime)
      {
        Mouse.move((x - previousTouch.x), (y - previousTouch.y));
        Serial.println("Mouse Input - Moving cursor");
      }
      previousTouch.x = x;
      previousTouch.y = y;
      break;

    case 2: // UP event
      Serial.print("Mouse Input - UP");
      if (millis() - globalMillis < holdTime)  // mouse "left click", sensitivity
      { // can be tuned by changing "holdTime"
        Mouse.click(MOUSE_LEFT);
        Serial.print("(Left-Click)");
      }
      Serial.println("");
      break;
    default: break;
  }
}

void loopKeyboard(int16_t x , int16_t y, int8_t event) {
  if (x > keyboardBoundary) //return if the touch object is inside keyboard area
    return; 

  if (event == 0) { // DOWN event
    //assign Key to the given interval
    
    if (y < 250)
    {
    Keyboard.print('A'); //Print Key "A"
    Serial.println("Keyboard Input - Button Press 'A'");
    }
    else if (y < 500)
    {
    Keyboard.print('B'); //Print Key "B"
    Serial.println("Keyboard Input - Button Press 'B'");
    }
    else if (y < 750)
    {
    Keyboard.print('C'); //Print Key "C"
    Serial.println("Keyboard Input - Button Press 'C'");
    }
    else if (y < 1000)
    {
    Keyboard.print('D'); //Print Key "D"
    Serial.println("Keyboard Input - Button Press 'D'");
    }
    else
    {
    Keyboard.print('E'); //Print Key "E"
    Serial.println("Keyboard Input - Button Press 'E'");
    }
  }
}
