#include <Zforce.h>
#define DATA_READY 13

void setup() 
{  
  Serial.begin(115200);
  zforce.Start(DATA_READY);

  Message* msg = zforce.GetMessage();

  if(msg != NULL)
  {
    Serial.print("Message type is: ");
    Serial.println((int)msg->type);
    zforce.DestroyMessage(msg);
  }

  // Send and read Enable

  zforce.Enable(true);

  msg = zforce.GetMessage();

  do
  {
    msg = zforce.GetMessage();
  } while (msg == NULL);

  if(msg->type == MessageType::ENABLETYPE)
  {
    Serial.print("Message type is: ");
    Serial.println((int)msg->type);
  }
  
  zforce.DestroyMessage(msg);

  // Send and read ReverseX
  zforce.ReverseX(true);

  do
  {
    msg = zforce.GetMessage();
  } while (msg == NULL);

  if(msg->type == MessageType::REVERSEXTYPE)
  {
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

  if(msg->type == MessageType::REVERSEYTYPE)
  {
    Serial.print("Message type is: ");
    Serial.println((int)msg->type);
  }

  zforce.DestroyMessage(msg);

  // Send and read Touch Active Area
  zforce.TouchActiveArea(50,50,2000,4000);

  do
  {
    msg = zforce.GetMessage();
  } while (msg == NULL);

  if(msg->type == MessageType::TOUCHACTIVEAREATYPE)
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
}

void loop() 
{
  Message* touch = zforce.GetMessage();
  if(touch != NULL && touch->type == MessageType::TOUCHTYPE)
  {
    for (uint8_t i = 0; i < ((TouchMessage*)touch)->touchCount; i++)
    {
      Serial.print("X is: ");
      Serial.println(((TouchMessage*)touch)->touchData[i].x);
      Serial.print("Y is: ");
      Serial.println(((TouchMessage*)touch)->touchData[i].y);
      Serial.print("ID is: ");
      Serial.println(((TouchMessage*)touch)->touchData[i].id);
    }
    zforce.DestroyMessage(touch);
  }
}
