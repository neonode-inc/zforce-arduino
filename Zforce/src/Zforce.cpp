#include "I2C/I2C.h"
#include "Zforce.h"
#include <string.h>

Zforce::Zforce()
{
}

void Zforce::Start(int dr)
{
  dataReady = dr;
  pinMode(dataReady, INPUT);
  I2c.setSpeed(1);
  I2c.begin();
}

int Zforce::Read(uint8_t * payload)
{
  int status = 0;
  
  I2c.read(ZFORCE_I2C_ADDRESS, 2);

  // Read the 2 I2C header bytes.
  payload[0] = I2c.receive();
  payload[1] = I2c.receive();
  
  I2c.read(ZFORCE_I2C_ADDRESS, payload[1], &payload[2]);

  return status; // return 0 if success, otherwise error code according to Atmel Data Sheet
}

/*
 * Sends a message in the form of a byte array.
 */
int Zforce::Write(uint8_t* payload)
{
  int len = payload[1] + 2;
  int status = I2c.write(ZFORCE_I2C_ADDRESS, payload[0], &payload[1], len);

  return status; // return 0 if success, otherwise error code according to Atmel Data Sheet
}

bool Zforce::Enable(bool isEnabled)
{
  bool failed = false;

  uint8_t enable[] = {0xEE, 0x0A, 0xEE, 0x08, 0x40, 0x02, 0x02, 0x00, 0x65, 0x02, (isEnabled ? 0x81 : 0x80), 0x00};
  
  if(GetDataReady() == HIGH)
  {
    Read(buffer);
    ClearBuffer(buffer);
  }

  if (Write(enable))
  {
    failed = true;
  }
  else
  {
    lastSentMessage = ENABLETYPE;
  }

  long ms = millis();
  while(GetDataReady() == LOW)
  {
    if((millis() - ms) > timeout)
    {
      failed = true;
      break;
    }
  }

  if(!failed)
  {
    if(Read(buffer))
    {
      failed = true;
    }
  }

  if(!failed)
  {
    VirtualParse(buffer);
    ClearBuffer(buffer);
  }

  return !failed;
}

bool Zforce::TouchActiveArea(uint16_t minX, uint16_t minY, uint16_t maxX, uint16_t maxY)
{
  bool failed = false;

  uint8_t length = 12;

  uint8_t firstMinX, secondMinX, firstMinY, secondMinY, firstMaxX, secondMaxX, firstMaxY, secondMaxY;

  if (minX > 127)
  {
    length++;
    if (minX > 255)
    {
      firstMinX = minX >> 7;
      secondMinX |= minX;
    }
    else
    {
      firstMinX = 0;
      secondMinX = minX;
    }
  }
  else
  {
    secondMinX = minX;
  }

  if (minY > 127)
  {
    length++;
    if (minY > 255)
    {
      firstMinY = minY >> 7;
      secondMinX |= minY;
    }
    else
    {
      firstMinY = 0;
      secondMinY = minY;
    }
  }
  else
  {
    secondMinY = minY;
  }

  if (maxX > 127)
  {
    length++;
    if (maxX > 255)
    {
      firstMaxX = maxX >> 7;
      secondMaxX |= maxX;
    }
    else
    {
      firstMaxX = 0;
      secondMaxX = maxX;
    }
  }
  else
  {
    secondMaxX = maxX;
  }

  if (maxY > 127)
  {
    length++;
    if (maxY > 255)
    {
      firstMaxY = maxY >> 7;
      secondMaxY |= maxY;
    }
    else
    {
      firstMaxY = 0;
      secondMaxY = maxY;
    }
  }
  else
  {
    secondMaxY = maxY;
  }

  Serial.println(maxY);
  Serial.println(firstMaxY);
  Serial.println(secondMaxY);

  Serial.println("Fixed all the second and firsts");

  uint8_t test[] = {firstMaxY, secondMaxY};


  uint8_t touchActiveArea[] = {0xEE, length + 9, 0xEE, length + 7, 0x40, 0x02, 0x02, 0x00, 0x73, length + 2, 0xA2, length, 
                               0x80, minX > 127 ? 0x02 : 0x01, minX > 127 ? firstMinX : secondMinX, minX < 127 ? : secondMinX,
                               0x81, minY > 127 ? 0x02 : 0x01, minY > 127 ? firstMinY : secondMinY, minY < 127 ? : secondMinY,
                               0x82, maxX > 127 ? 0x02 : 0x01, maxX > 127 ? firstMaxX : secondMaxX, maxX < 127 ? : secondMaxX,
                               0x83, maxY > 127 ? 0x02 : 0x01, maxY > 127 ? firstMaxY : secondMaxY, maxY < 127 ? : secondMaxY};

                               for (int i = 0; i < sizeof(touchActiveArea); i++)
                               {
                                 Serial.print(touchActiveArea[i], HEX);
                                 Serial.print(" ");
                               }

  if(GetDataReady() == HIGH)
  {
    Read(buffer);
    ClearBuffer(buffer);
  }

  if (Write(touchActiveArea))
  {
    failed = true;
  }
  else
  {
    lastSentMessage = TOUCHACTIVEAREATYPE;
  }

  long ms = millis();
  while(GetDataReady() == LOW)
  {
    if((millis() - ms) > timeout)
    {
      failed = true;
      break;
    }
  }

  Serial.print("failed is: ");
  Serial.print(failed);
  if(!failed)
  {
    if(Read(buffer))
    {
      failed = true;
    }
  }

  if(!failed)
  {
    VirtualParse(buffer);
    ClearBuffer(buffer);
  }

  return !failed;
}

bool Zforce::FlipXY(bool isFlipped)
{
  bool failed = false;

  uint8_t flipXY[] = {0xEE, 0x0D, 0xEE, 0x0B, 0x40, 0x02, 0x02, 0x00, 0x73, 0x05, 0xA2, 0x03, 0x86, 0x01, isFlipped ? 0xFF : 0x00};

  if(GetDataReady() == HIGH)
  {
    Read(buffer);
    ClearBuffer(buffer);
  }

  if (Write(flipXY))
  {
    failed = true;
  }
  else
  {
    lastSentMessage = FLIPXYTYPE;
  }

  long ms = millis();
  while(GetDataReady() == LOW)
  {
    if((millis() - ms) > timeout)
    {
      failed = true;
      break;
    }
  }

  if(!failed)
  {
    if(Read(buffer))
    {
      failed = true;
    }
  }

  if(!failed)
  {
    VirtualParse(buffer);
    ClearBuffer(buffer);
  }

  return !failed;
}

bool Zforce::ReverseX(bool isReversed)
{
  bool failed = false;

  uint8_t reverseX[] = {0xEE, 0x0D, 0xEE, 0x0B, 0x40, 0x02, 0x02, 0x00, 0x73, 0x05, 0xA2, 0x03, 0x84, 0x01, isReversed ? 0xFF : 0x00};

  if(GetDataReady() == HIGH)
  {
    Read(buffer);
    ClearBuffer(buffer);
  }

  if (Write(reverseX))
  {
    failed = true;
  }
  else
  {
    lastSentMessage = REVERSEXTYPE;
  }

  long ms = millis();
  while(GetDataReady() == LOW)
  {
    if((millis() - ms) > timeout)
    {
      failed = true;
      break;
    }
  }

  if(!failed)
  {
    if(Read(buffer))
    {
      failed = true;
    }
  }

  if(!failed)
  {
    VirtualParse(buffer);
    ClearBuffer(buffer);
  }

  return !failed;
}

bool Zforce::ReverseY(bool isReversed)
{
  bool failed = false;

  uint8_t reverseY[] = {0xEE, 0x0D, 0xEE, 0x0B, 0x40, 0x02, 0x02, 0x00, 0x73, 0x05, 0xA2, 0x03, 0x85, 0x01, isReversed ? 0xFF : 0x00};

  if(GetDataReady() == HIGH)
  {
    Read(buffer);
    ClearBuffer(buffer);
  }

  if (Write(reverseY))
  {
    failed = true;
  }
  else
  {
    lastSentMessage = REVERSEYTYPE;
  }

  long ms = millis();
  while(GetDataReady() == LOW)
  {
    if((millis() - ms) > timeout)
    {
      failed = true;
      break;
    }
  }

  if(!failed)
  {
    if(Read(buffer))
    {
      failed = true;
    }
  }

  if(!failed)
  {
    VirtualParse(buffer);
    ClearBuffer(buffer);
  }

  return !failed;
}

bool Zforce::ReportedTouches(uint8_t touches)
{
  bool failed = false;

  if(touches > 10)
  {
    touches = 10;
  }

  uint8_t reportedTouches[] = {0xEE, 0x0B, 0xEE, 0x09, 0x40, 0x02, 0x02, 0x00, 0x73, 0x03, 0x86, 0x01, touches};

  if(GetDataReady() == HIGH)
  {
    Read(buffer);
    ClearBuffer(buffer);
  }

  if (Write(reportedTouches))
  {
    failed = true;
  }
  else
  {
    lastSentMessage = REPORTEDTOUCHESTYPE;
  }

  long ms = millis();
  while(GetDataReady() == LOW)
  {
    if((millis() - ms) > timeout)
    {
      failed = true;
      break;
    }
  }

  if(!failed)
  {
    if(Read(buffer))
    {
      failed = true;
    }
  }

  if(!failed)
  {
    VirtualParse(buffer);
    ClearBuffer(buffer);
  }

  return !failed;
}


int Zforce::GetDataReady()
{
  return digitalRead(dataReady);
}

Message* Zforce::GetMessage()
{
  if(GetDataReady() == HIGH)
  {
    if(!Read(buffer))
    {
      VirtualParse(buffer);
      ClearBuffer(buffer);
    }
  }
  
  return Dequeue();
}

void Zforce::DestroyMessage(Message* msg)
{
  delete msg;
  msg = nullptr;
}

void Zforce::VirtualParse(uint8_t* payload)
{
  switch(payload[2]) // Check if the payload is a response to a request or if it's a notification.
  {
    case 0xEF:
      switch(lastSentMessage) //Assumes that we get the response we expect
      {
        case ENABLETYPE:
          EnableMessage* enable = new EnableMessage;
          enable->type = ENABLETYPE;
          ParseEnable(enable, payload);
          Enqueue(dynamic_cast<Message*>(enable));
        break;

        case TOUCHACTIVEAREATYPE:
          TouchActiveAreaMessage* touchActiveArea = new TouchActiveAreaMessage;
          touchActiveArea->type = TOUCHACTIVEAREATYPE;
          ParseTouchActiveArea(touchActiveArea, payload);
          Enqueue(dynamic_cast<Message*>(touchActiveArea));
        break;

        case REVERSEXTYPE:
          ReverseXMessage* reverseX = new ReverseXMessage;
          reverseX->type = REVERSEXTYPE;
          ParseReverseX(reverseX, payload);
          Enqueue(dynamic_cast<Message*>(reverseX));
        break;

        case REVERSEYTYPE:
          ReverseYMessage* reverseY = new ReverseYMessage;
          reverseY->type = REVERSEYTYPE;
          ParseReverseY(reverseY, payload);
          Enqueue(dynamic_cast<Message*>(reverseY));
        break;

        case FLIPXYTYPE:
          FlipXYMessage* flipXY = new FlipXYMessage;
          flipXY->type = FLIPXYTYPE;
          ParseFlipXY(flipXY, payload);
          Enqueue(dynamic_cast<Message*>(flipXY));
        break;

        case REPORTEDTOUCHESTYPE:
          ReportedTouchesMessage* reportedTouches = new ReportedTouchesMessage;
          reportedTouches->type = REPORTEDTOUCHESTYPE;
          ParseReportedTouches(reportedTouches, payload);
          Enqueue(dynamic_cast<Message*>(reportedTouches));
        break;

        default:
        break;
      }
    break;

    case 0xF0:
      if (payload[8] == 0xA0) // Check the identifier if this is a touch message or something else.
      {
        uint8_t touchCount = payload[9] / 11; // Calculate the amount of touch objects.
        for (uint8_t i = 0; i < touchCount; i++)
        {
          TouchMessage* touch = new TouchMessage;
          touch->type = TOUCHTYPE;
          ParseTouch(touch, payload, i);
          Enqueue(dynamic_cast<Message*>(touch));
        }
      }
    break;

    default:
    break;
  }
  lastSentMessage = NONE;
}

void Zforce::ParseTouchActiveArea(TouchActiveAreaMessage* msg, uint8_t* payload)
{
  const uint8_t offset = 10;
  uint16_t value = 0;
  uint16_t valueLength = 0;
  for (int i = offset; i < payload[11] + offset; i++) // 10 = index for SubTouchActiveArea struct, 11 index for length of SubTouchActiveArea struct
  {
    switch (payload[i])
    {
      case 0x80: // MinX
        valueLength = payload[i + 1];
        if (valueLength == 2)
        {
          value = payload[i + 2] << 8;
          value |= payload[i + 3];
        }
        else
        {
          value = payload[i + 2];
        }
        msg->minX = value;
      break;

      case 0x81: // MinY
        valueLength = payload[i + 1];
        if (valueLength == 2)
        {
          value = payload[i + 2] << 8;
          value |= payload[i + 3];
        }
        else
        {
          value = payload[i + 2];
        }
        msg->minY = value;
      break;

      case 0x82: // MaxX
        valueLength = payload[i + 1];
        if (valueLength == 2)
        {
          value = payload[i + 2] << 8;
          value |= payload[i + 3];
        }
        else
        {
          value = payload[i + 2];
        }
        msg->maxX = value;
      break;

      case 0x83: // MaxY
        valueLength = payload[i + 1];
        if (valueLength == 2)
        {
          value = payload[i + 2] << 8;
          value |= payload[i + 3];
        }
        else
        {
          value = payload[i + 2];
        }
        msg->maxY = value;
      break;

      default:
      break;
    }
  }
}

void Zforce::ParseEnable(EnableMessage* msg, uint8_t* payload)
{
  switch (payload[10])
  {
    case 0x80:
      msg->enabled = false;
    break;

    case 0x81:
      msg->enabled = true;
    break;

    default:
    break;
  }
}

void Zforce::ParseReportedTouches(ReportedTouchesMessage* msg, uint8_t* payload)
{
  const uint8_t offset = 10;
  for(int i = offset + payload[11]; i < payload[9] + offset; i++)
  {
    if(payload[i] == 0x86)
    {
      msg->reportedTouches = payload[i + 2];
      break;
    }
  }
}

void Zforce::ParseReverseX(ReverseXMessage* msg, uint8_t* payload)
{
  const uint8_t offset = 10;
  for(int i = offset; i < payload[11] + offset; i++)
  {
    if(payload[i] == 0x84)
    {
      msg->reversed = (bool)payload[i + 2];
      break;
    }
  }
}

void Zforce::ParseReverseY(ReverseYMessage* msg, uint8_t* payload)
{
  const uint8_t offset = 10;
  for(int i = offset; i < payload[11] + offset; i++)
  {
    if(payload[i] == 0x85)
    {
      msg->reversed = (bool)payload[i + 2];
      break;
    }
  }
}

void Zforce::ParseFlipXY(FlipXYMessage* msg, uint8_t* payload)
{
  const uint8_t offset = 10;
  for(int i = offset; i < payload[11] + offset; i++)
  {
    if(payload[i] == 0x86)
    {
      msg->flipXY = (bool)payload[i + 2];
      break;
    }
  }
}

void Zforce::ParseTouch(TouchMessage* msg, uint8_t* payload, uint8_t i)
{
  msg->id = payload[12 + (i * 11)];
  msg->event = (TouchEvent)(payload[13 + (i * 11)]);
  msg->x = payload[14 + (i * 11)] << 8;
  msg->x |= payload[15 + (i * 11)];
  msg->y = payload[16 + (i * 11)] << 8;
  msg->y |= payload[17 + (i * 11)];
}

void Zforce::Enqueue(Message* msg)
{
  queue.push_back(msg);
}

Message* Zforce::Dequeue()
{
  Message* message = nullptr;

  if(!queue.empty())
  {
    for(uint8_t i = 0; i < queue.size(); i++)
    {
      if(queue.at(i)->type != TOUCHTYPE)
      {
        message = queue.at(i);
        queue.erase(queue.begin() + i);
        break;
      }
    }
  }

  if(nullptr == message)
  {
    if(!queue.empty())
    {
      message = queue.front();
      queue.pop_front();
    }
  }

  /* TODO Requirements for our own made queue

  1. Need to know if queue is empty or not
  2. Need to be able to access individual objects in queue
  3. Need push
  4. Need pop

  */

  return message;
}

void Zforce::ClearBuffer(uint8_t* buffer)
{
  memset(buffer, 0, sizeof(buffer));
}

Zforce zforce = Zforce();