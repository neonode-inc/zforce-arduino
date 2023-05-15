/*  Neonode zForce v7 interface library for Arduino

    This example code is distributed freely.
    This is an exception from the rest of the library that is released
    under GNU Lesser General Public License.

    The purpose of this example code is to demonstrate parts of the 
    library's functionality and capabilities. It is free to use, copy
    and edit without restrictions.

*/

#include <Zforce.h>

// IMPORTANT: change "1" to assigned GPIO digital pin for dataReady signal in your setup:
#define DATA_READY 1

void setup()
{
  Serial.begin(115200);
  while(!Serial){};

  Serial.println("zforce start");
  zforce.Start(DATA_READY);
  init_sensor();
}

void loop()
{
  // Continuously read any messages available from the sensor and print
  // touch data to Serial interface. Normally there should be only 
  // touch notifications as long as no request messages are sent to the sensor.
  Message* touch = zforce.GetMessage();
  if (touch != nullptr)
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
    else if (touch->type == MessageType::BOOTCOMPLETETYPE)
    {
      // If we for some reason receive a boot complete
      // message, the sensor needs to be reinitialized:
      init_sensor(); 
    }

    zforce.DestroyMessage(touch);
  }
}


// Write some configuration parameters to sensor and enable sensor.
// The choice of parameters to configure here are just examples to show how to 
// use the library.
// NOTE: Sensor firmware versions 2.xx has persistent storage of configuration
// parameters while versions 1.xx does not. See the library documentation for
//  further info.
void init_sensor()
{
  Message *msg = nullptr;


  // Send and read ReverseX
  zforce.ReverseX(false);

  do
  {
    msg = zforce.GetMessage();
  } while (msg == nullptr);

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
  } while (msg == nullptr);

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
  } while (msg == nullptr);

  if (msg->type == MessageType::TOUCHACTIVEAREATYPE)
  {
    Serial.print("minX is: ");
    Serial.println(((TouchActiveAreaMessage *)msg)->minX);
    Serial.print("minY is: ");
    Serial.println(((TouchActiveAreaMessage *)msg)->minY);
    Serial.print("maxX is: ");
    Serial.println(((TouchActiveAreaMessage *)msg)->maxX);
    Serial.print("maxY is: ");
    Serial.println(((TouchActiveAreaMessage *)msg)->maxY);
  }

  zforce.DestroyMessage(msg);


  // Send and read Enable

  zforce.Enable(true);

  do
  {
    msg = zforce.GetMessage();
  } while (msg == nullptr);

  if (msg->type == MessageType::ENABLETYPE)
  {
    Serial.print("Message type is: ");
    Serial.println((int)msg->type);
    Serial.println("Sensor is now enabled and will report touches.");
  }

  zforce.DestroyMessage(msg);
}
