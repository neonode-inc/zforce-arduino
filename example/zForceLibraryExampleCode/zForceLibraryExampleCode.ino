/*  Neonode zForce v7 interface library for Arduino

    Copyright (C) 2019 Neonode Inc.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include <Zforce.h>
#define DATA_READY 4

/*
 * If you are using a SAMD board, uncomment the define below in order to print to the serial monitor.
 */
//#define Serial SerialUSB

void setup()
{
  Serial.begin(115200);
  while(!Serial){};
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
      for (uint8_t i = 0; i < ((TouchMessage*)touch)->touchCount; i++)
      {
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
}
