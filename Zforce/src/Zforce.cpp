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

  for (int i = 0; i < payload[1] + 2; i++)
  {
    Serial.print(payload[i], HEX);
    Serial.print(" ");
  }

  Serial.println("");

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
    Serial.println("clear buffer innan skick av enable.");
    ClearBuffer(buffer);
  }

  if (Write(enable))
  {
    failed = true;
  }
  else
  {
    lastSentMessage = ENABLE;
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
    Serial.println("DataReady e high");
    if(!Read(buffer))
    {
      Serial.println("Read Buffer lyckades");
    }
      VirtualParse(buffer);
      Serial.println("Vi har kört virtualparse");
      ClearBuffer(buffer);
      Serial.println("Vi har clearat buffern");
  }
  
  return Dequeue();
}

void Zforce::DestroyMessage(Message* msg)
{
  delete msg;
  msg = nullptr;
}

void Zforce::DestroyEnableMessage(EnableMessage* msg)
{
  delete msg;
  msg = nullptr;
}

void Zforce::DestroyTouchMessage(TouchMessage* msg)
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
        case ENABLE:
          EnableMessage* enable = new EnableMessage;
          enable->type = ENABLETYPE;
          ParseEnable(enable, payload);
          Enqueue(static_cast<Message*>(enable));
        break;

        case TOUCHACTIVEAREA:
          TouchActiveAreaMessage* touchActiveArea = new TouchActiveAreaMessage;
          touchActiveArea->type = TOUCHACTIVEAREATYPE;
          ParseTouchActiveArea(touchActiveArea, payload);
          Enqueue(static_cast<Message*>(touchActiveArea));
        break;

        case REVERSEX:
          ReverseXMessage* reverseX = new ReverseXMessage;
          reverseX->type = REVERSEXTYPE;
          ParseReverseX(reverseX, payload);
          Enqueue(static_cast<Message*>(reverseX));
        break;

        case REVERSEY:
          ReverseYMessage* reverseY = new ReverseYMessage;
          reverseY->type = REVERSEYTYPE;
          ParseReverseY(reverseY, payload);
          Enqueue(static_cast<Message*>(reverseY));
        break;

        case FLIPXY:
          FlipXYMessage* flipXY = new FlipXYMessage;
          flipXY->type = FLIPXYTYPE;
          ParseFlipXY(flipXY, payload);
          Enqueue(static_cast<Message*>(flipXY));
        break;

        case REPORTEDTOUCHES:
          ReportedTouchesMessage* reportedTouches = new ReportedTouchesMessage;
          reportedTouches->type = REPORTEDTOUCHESTYPE;
          ParseReportedTouches(reportedTouches, payload);
          Enqueue(static_cast<Message*>(reportedTouches));
        break;

        default:
        break;
      }
    break;

    case 0xF0:
      if (payload[8] == 0xA0) // Check the identifier if this is a touch message or something else.
      {
        uint8_t touchCount = 1;//payload[9] / 11; // Calculate the amount of touch objects.
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
  lastSentMessage = UNKNOWN;
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
  Message* linkerNode = nullptr;

  if (nullptr == headNode)
  {
    headNode = msg;
  }
  else
  {
    linkerNode = headNode;
    while(nullptr != linkerNode->next)
    {
      linkerNode = linkerNode->next;
    }
    linkerNode->next = msg;
  }
}

Message* Zforce::Dequeue()
{
  Message* message = nullptr;
  Message* temp;

  if(nullptr != headNode)
  {
    switch (headNode->type)
    {
      case ENABLETYPE:
      message = dynamic_cast<Message*>(new EnableMessage);
      break;
      case TOUCHACTIVEAREATYPE:
      break;
      case REVERSEXTYPE:
      break;
      case REVERSEYTYPE:
      break;
      case FLIPXYTYPE:
      break;
      case REPORTEDTOUCHESTYPE:
      break;
      case TOUCHTYPE:
      message = dynamic_cast<Message*>(new TouchMessage);
      break;
      default:
      break;
    }
  }

  if (nullptr != headNode)
  {
    memcpy(message, headNode, sizeof(headNode));
    temp = headNode;

    if (nullptr != headNode->next)
    {
      headNode = headNode->next;
    }

    delete temp;
    temp = nullptr;
  }

  return message;
}

void Zforce::ClearBuffer(uint8_t* buffer)
{
  memset(buffer, 0, sizeof(buffer));
}

/*
 * Get the first two "I2C bytes" of the message in order to get the length of the whole message.
 */

Zforce zforce = Zforce();