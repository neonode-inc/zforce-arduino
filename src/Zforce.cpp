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


#include <string.h>
#include <inttypes.h>
#include "I2C/I2C.h"
#include "Zforce.h"
#if USE_I2C_LIB == 0
  #include <Wire.h>
  #if(ARDUINO >= 100)
    #include <Arduino.h>
  #else
    #include <WProgram.h>
  #endif
#endif

Zforce::Zforce()
{
}

void Zforce::Start(int dr)
{
  Start(dr, ZFORCE_DEFAULT_I2C_ADDRESS);
}

void Zforce::Start(int dr, int i2cAddress)
{
  this->i2cAddress = i2cAddress;
  dataReady = dr;
  pinMode(dataReady, INPUT);
#if USE_I2C_LIB == 1
  I2c.setSpeed(1);
  I2c.begin();
#else
  Wire.begin();
#endif

  /* Reading of boot complete and sending/reading of touchformat 
   * can be moved to user side but is by default 
   * kept here for simplicity for the end user. */

  // Read out potential boot complete notification
  auto msg = this->GetMessage();
  if (msg != nullptr)
  {
    this->DestroyMessage(msg);
  }

  // Get the touch descriptor from the sensor in order to deserialize the touch notifications
  this->TouchFormat();
  do
  {
    msg = this->GetMessage();
  } while (msg == nullptr);

  this->DestroyMessage(msg);
 
}

int Zforce::Read(uint8_t * payload)
{
#if USE_I2C_LIB == 1
  int status = 0;

  status = I2c.read(this->i2cAddress, 2);

  // Read the 2 I2C header bytes.
  payload[0] = I2c.receive();
  payload[1] = I2c.receive();

  status = I2c.read(this->i2cAddress, payload[1], &payload[2]);

  return status; // return 0 if success, otherwise error code according to Atmel Data Sheet
#else
  Wire.requestFrom(ZFORCE_I2C_ADDRESS, 2);
  payload[0] = Wire.read();
  payload[1] = Wire.read();
  
  int index = 2;
  Wire.requestFrom(ZFORCE_I2C_ADDRESS, payload[1]);
  while (Wire.available())
  {
    payload[index++] = Wire.read();
  }
  
  return 0;
#endif
}

/*
 * Sends a message in the form of a byte array.
 */
int Zforce::Write(uint8_t* payload)
{
#if USE_I2C_LIB == 1
  int len = payload[1] + 1;
  int status = I2c.write(this->i2cAddress, payload[0], &payload[1], len);

  return status; // return 0 if success, otherwise error code according to Atmel Data Sheet
#else
  Wire.beginTransmission(this->i2cAddress);
  Wire.write(payload, payload[1] + 2);
  Wire.endTransmission();

  return 0;
#endif
}

bool Zforce::Enable(bool isEnabled)
{
  bool failed = false;
  int returnCode;

  uint8_t enable[] =  {0xEE, 0x0B, 0xEE, 0x09, 0x40, 0x02, 0x02, 0x00, 0x65, 0x03, 0x81, 0x01, 0x00};
  uint8_t disable[] = {0xEE, 0x0A, 0xEE, 0x08, 0x40, 0x02, 0x02, 0x00, 0x65, 0x02, 0x80, 0x00};

  if (isEnabled)
  {
    returnCode = Write(enable); // We assume that the end user has called GetMessage prior to calling this method
  }
  else 
  {
    returnCode = Write(disable);
  }

  if (returnCode != 0)
  {
    failed = true;
  }
  else
  {
    lastSentMessage = MessageType::ENABLETYPE;
  }

  return !failed;
}

bool Zforce::GetEnable()
{
  bool failed = false;
  uint8_t enable[] = {0xEE, 0x08, 0xEE, 0x06, 0x40, 0x02, 0x02, 0x00, 0x65, 0x00};

  if (Write(enable)) // We assume that the end user has called GetMessage prior to calling this method
  {
    failed = true;
  }
  else
  {
    lastSentMessage = MessageType::ENABLETYPE;
  }

  return !failed;
}

bool Zforce::TouchActiveArea(uint16_t lowerBoundX, uint16_t lowerBoundY, uint16_t upperBoundX, uint16_t upperBoundY)
{
  bool failed = false;

  const uint8_t length = 16;

  uint8_t touchActiveArea[] = {0xEE, length + 10, 0xEE, length + 8,
                               0x40, 0x02, 0x02, 0x00, 0x73, length + 2, 0xA2, length,
                               0x80, 0x02, (uint8_t)(lowerBoundX >> 8), (uint8_t)(lowerBoundX & 0xFF),
                               0x81, 0x02, (uint8_t)(lowerBoundY >> 8), (uint8_t)(lowerBoundY & 0xFF),
                               0x82, 0x02, (uint8_t)(upperBoundX >> 8), (uint8_t)(upperBoundX & 0xFF),
                               0x83, 0x02, (uint8_t)(upperBoundY >> 8), (uint8_t)(upperBoundY & 0xFF)};

  if (Write(touchActiveArea)) // We assume that the end user has called GetMessage prior to calling this method
  {
    failed = true;
  }
  else
  {
    lastSentMessage = MessageType::TOUCHACTIVEAREATYPE;
  }

  return !failed;
}

bool Zforce::Frequency(uint16_t idleFrequency, uint16_t fingerFrequency)
{
    bool failed = false;

    const uint8_t length = 8;

    uint8_t frequency[] = { 0xEE, length + 8, 0xEE, length + 6,
                            0x40, 0x02, 0x02, 0x00, 0x68, length,
                            0x80, 0x02, (uint8_t)(fingerFrequency >> 8), (uint8_t)(fingerFrequency & 0xFF),
                            0x82, 0x02, (uint8_t)(idleFrequency   >> 8), (uint8_t)(idleFrequency   & 0xFF)};


    if (Write(frequency)) // We assume that the end user has called GetMessage prior to calling this method
    {
        failed = true;
    }
    else
    {
        lastSentMessage = MessageType::FREQUENCYTYPE;
    }

    return !failed;
}


bool Zforce::FlipXY(bool isFlipped)
{
  bool failed = false;

  uint8_t flipXY[] = {0xEE, 0x0D, 0xEE, 0x0B, 0x40, 0x02, 0x02, 0x00, 0x73, 0x05, 0xA2, 0x03, 0x86, 0x01, (uint8_t)(isFlipped ? 0xFF : 0x00)};

  if (Write(flipXY)) // We assume that the end user has called GetMessage prior to calling this method
  {
    failed = true;
  }
  else
  {
    lastSentMessage = MessageType::FLIPXYTYPE;
  }

  return !failed;
}

bool Zforce::ReverseX(bool isReversed)
{
  bool failed = false;

  uint8_t reverseX[] = {0xEE, 0x0D, 0xEE, 0x0B, 0x40, 0x02, 0x02, 0x00, 0x73, 0x05, 0xA2, 0x03, 0x84, 0x01, (uint8_t)(isReversed ? 0xFF : 0x00)};

  if (Write(reverseX)) // We assume that the end user has called GetMessage prior to calling this method
  {
    failed = true;
  }
  else
  {
    lastSentMessage = MessageType::REVERSEXTYPE;
  }

  return !failed;
}

bool Zforce::ReverseY(bool isReversed)
{
  bool failed = false;

  uint8_t reverseY[] = {0xEE, 0x0D, 0xEE, 0x0B, 0x40, 0x02, 0x02, 0x00, 0x73, 0x05, 0xA2, 0x03, 0x85, 0x01, (uint8_t)(isReversed ? 0xFF : 0x00)};

  if (Write(reverseY)) // We assume that the end user has called GetMessage prior to calling this method
  {
    failed = true;
  }
  else
  {
    lastSentMessage = MessageType::REVERSEYTYPE;
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

  if (Write(reportedTouches)) // We assume that the end user has called GetMessage prior to calling this method
  {
    failed = true;
  }
  else
  {
    lastSentMessage = MessageType::REPORTEDTOUCHESTYPE;
  }

  return !failed;
}

bool Zforce::DetectionMode(bool mergeTouches, bool reflectiveEdgeFilter)
{
  bool failed = false;

  uint8_t detectionModeValue = 0x00;
  detectionModeValue |= mergeTouches ? 0x20 : 0x00; // 0x20 as defined in the ASN.1 protocol
  detectionModeValue |= reflectiveEdgeFilter ? 0x80 : 0x00; // 0x80 as defined in the ASN.1 protocol
  uint8_t detectionMode[] = {0xEE, 0x0C, 0xEE, 0x0A, 0x40, 0x02, 0x02, 0x00, 0x73, 0x04, 0x85, 0x02, 0x00, detectionModeValue};

  if (Write(detectionMode)) // We assume that the end user has called GetMessage prior to calling this method
  {
    failed = true;
  }
  else
  {
    lastSentMessage = MessageType::DETECTIONMODETYPE;
  }

  return !failed;
}

bool Zforce::TouchFormat()
{
  bool failed = false;
  uint8_t touchFormat[] = {0xEE, 0x08, 0xEE, 0x06, 0x40, 0x02, 0x02, 0x00, 0x66, 0x00};

  if (Write(touchFormat))
  {
    failed = true;
  }
  else
  {
    lastSentMessage = MessageType::TOUCHFORMATTYPE;
  }

  return !failed;
}

bool Zforce::TouchMode(uint8_t mode, int16_t clickOnTouchRadius, int16_t clickOnTouchTime)
{
  bool failed = false;
  uint8_t touchMode[] = {0xEE, 0x14, 0xEE, 0x12, 0x40, 
                           0x02, 0x02, 0x00, 0x7F, 0x24, 0x0B, 
                           0x80, 0x01, mode, 0x81, 0x02, 
                           (uint8_t)(clickOnTouchTime << 8), 
                           (uint8_t)(clickOnTouchTime & 0xFF), 
                           0x82, 0x02, (uint8_t)(clickOnTouchRadius << 8), 
                           (uint8_t)(clickOnTouchRadius & 0xFF) };

  if (Write(touchMode))
  {
    failed = true;
  }
  else
  {
    lastSentMessage = MessageType::TOUCHMODETYPE;
  }

  return !failed;
}

bool Zforce::FloatingProtection(bool enabled, uint16_t time)
{
  bool failed = false;
  uint8_t floatingProtection[] = {0xEE, 0x11, 0XEE, 0x0F, 0x40, 0x02, 0x02, 0x00, 0x73,
                                  0x09, 0xA8, 0x07, 0x80, 0x01, (uint8_t)(enabled ? 0xFF : 0x00), 0x81,
                                  0x02, (uint8_t)(time << 8), (uint8_t)(time & 0xFF)};

  if (Write(floatingProtection))
  {
    failed = true;
  }
  else
  {
    lastSentMessage = MessageType::FLOATINGPROTECTIONTYPE;
  }

  return !failed;
}

int Zforce::GetDataReady()
{
  return digitalRead(dataReady);
}

Message* Zforce::GetMessage()
{
  Message* msg = nullptr;
  if(GetDataReady() == HIGH)
  {
    if(!Read(buffer))
    {
      msg = VirtualParse(buffer);
      ClearBuffer(buffer);
    }
  }

  return msg;
}

void Zforce::DestroyMessage(Message* msg)
{
  delete msg;
  msg = nullptr;
}

Message* Zforce::VirtualParse(uint8_t* payload)
{
  Message* msg = nullptr;

  switch(payload[2]) // Check if the payload is a response to a request or if it's a notification.
  {
    case 0xEF:
    {
      ParseResponse(payload, &msg);
    }
    break;
    case 0xF0:
    {
      if (payload[8] == 0xA0) // Check the identifier if this is a touch message or something else.
      {
        msg = new TouchMessage;
        msg->type = MessageType::TOUCHTYPE;
        ParseTouch((TouchMessage*)msg, payload);
      }
      else if (payload[8] == 0x63)
      {
        msg = new Message;
        msg->type = MessageType::BOOTCOMPLETETYPE;
      }
    }
    break;

    default:
    break;
  }

  lastSentMessage = MessageType::NONE;
  return msg;
}

void Zforce::ParseResponse(uint8_t* payload, Message** msg)
{
  switch(lastSentMessage)
  {
    case MessageType::REVERSEYTYPE:
    {
      (*(msg)) = new ReverseYMessage;
      (*(msg))->type = MessageType::REVERSEYTYPE;
      ParseReverseY((ReverseYMessage*)(*(msg)), payload);
    }
    break;
    case MessageType::ENABLETYPE:
    {
      (*(msg)) = new EnableMessage;
      (*(msg))->type = MessageType::ENABLETYPE;
      ParseEnable((EnableMessage*)(*(msg)), payload);
    }
    break;
    case MessageType::TOUCHACTIVEAREATYPE:
    {
      (*(msg)) = new TouchActiveAreaMessage;
      (*(msg))->type = MessageType::TOUCHACTIVEAREATYPE;
      ParseTouchActiveArea((TouchActiveAreaMessage*)(*(msg)), payload);
    }
    break;
    case MessageType::REVERSEXTYPE:
    {
      (*(msg)) = new ReverseXMessage;
      (*(msg))->type = MessageType::REVERSEXTYPE;
      ParseReverseX((ReverseXMessage*)(*(msg)), payload);
    }
    break;
    case MessageType::FLIPXYTYPE:
    {
      (*(msg)) = new FlipXYMessage;
      (*(msg))->type = MessageType::FLIPXYTYPE;
      ParseFlipXY((FlipXYMessage*)(*(msg)), payload);
    }
    break;
    case MessageType::REPORTEDTOUCHESTYPE:
    {
      (*(msg)) = new ReportedTouchesMessage;
      (*(msg))->type = MessageType::REPORTEDTOUCHESTYPE;
      ParseReportedTouches((ReportedTouchesMessage*)(*(msg)), payload);
    }
    break;
    case MessageType::FREQUENCYTYPE:
    {
      (*(msg)) = new FrequencyMessage;
      (*(msg))->type = MessageType::FREQUENCYTYPE;
      ParseFrequency((FrequencyMessage*)(*(msg)), payload);
    }
    break;
    case MessageType::DETECTIONMODETYPE:
    {
      (*(msg)) = new DetectionModeMessage;
      (*(msg))->type = MessageType::DETECTIONMODETYPE;
      ParseDetectionMode((DetectionModeMessage*)(*(msg)), payload);
    }
    break;
    case MessageType::TOUCHFORMATTYPE:
    {
      (*(msg)) = new TouchDescriptorMessage;
      (*(msg))->type = MessageType::TOUCHFORMATTYPE;
      ParseTouchDescriptor((TouchDescriptorMessage*)(*(msg)), payload);
    }
    break;
    case MessageType::TOUCHMODETYPE:
    {
      (*(msg)) = new TouchModeMessage;
      (*(msg))->type = MessageType::TOUCHMODETYPE;
      ParseTouchMode((TouchModeMessage*)(*(msg)), payload);
    }
    break;
    case MessageType::FLOATINGPROTECTIONTYPE:
    {
      (*(msg)) = new FloatingProtectionMessage;
      (*(msg))->type = MessageType::FLOATINGPROTECTIONTYPE;
      ParseFloatingProtection((FloatingProtectionMessage *)(*(msg)), payload);
    }
    break;
    default:
    {
      (*(msg)) = new Message;
      (*(msg))->type = MessageType::NONE;
    }
    break;
  }
}

void Zforce::ParseTouchDescriptor(TouchDescriptorMessage* msg, uint8_t* payload)
{
  uint8_t amountBits = ((payload[11] - 1) * 8) - payload[12];

  uint32_t descr = 0;
  descr |= (uint32_t)payload[13] << 24;
  descr |= (uint32_t)payload[14] << 16;
  descr |= (uint32_t)payload[15] << 8;

  msg->descriptor = new TouchDescriptor[(int)TouchDescriptor::MaxValue];
  uint8_t bitIndex = 0;
  uint8_t descIndex = 0;
  while (bitIndex <= amountBits)
  {
    if (descr & (0x80000000 >> bitIndex))
    {
      msg->descriptor[descIndex++] = (TouchDescriptor)bitIndex;
    }
    bitIndex++;
  }

  touchMetaInformation.touchDescriptor = new TouchDescriptor[descIndex];
  touchMetaInformation.touchByteCount = descIndex;
  for (int i = 0; i < touchMetaInformation.touchByteCount; i++)
  {
    touchMetaInformation.touchDescriptor[i] = msg->descriptor[i];
  }
}

void Zforce::ParseTouchMode(TouchModeMessage* msg, uint8_t* payload)
{
  const uint8_t offset = 9;
  uint16_t valueLength = 0;

  for (int i = offset; i < payload[10] + offset; i++)
  {
    switch (payload[i])
    {
      case 0x80: // TouchMode
        msg->mode = (TouchModes)payload[i + 2];
      break;
      case 0x81: // ClickOnTouchTime
        valueLength = payload[i + 1];
        if (valueLength == 2)
        {
          msg->clickOnTouchTime = payload[i + 2] << 8;
          msg->clickOnTouchTime |= payload[i + 3];
        }
        else
        {
          msg->clickOnTouchTime = payload[i + 2];
        }
      break;
      case 0x82: // ClickOnTouchRadius
        valueLength = payload[i + 1];
        if (valueLength == 2)
        {
          msg->clickOnTouchRadius = payload[i + 2] << 8;
          msg->clickOnTouchRadius |= payload[i + 3];
        }
        else
        {
          msg->clickOnTouchRadius = payload[i + 2];
        }
      break;
      default:
      break;
    }
  }
}

void Zforce::ParseFrequency(FrequencyMessage* msg, uint8_t* payload)
{
    const uint8_t offset = 8;
    uint16_t value = 0;
    uint16_t valueLength = 0;

    for (int i = offset; i < payload[9] + offset; i++)
    {
        switch (payload[i])
        {
        case 0x80: // Finger Frequency
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
            msg->fingerFrequency = value;
            break;

        case 0x82: //Idle Frequency
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
            msg->idleFrequency = value;
            break;

        default:
            break;
        }
    }
}

void Zforce::ParseTouchActiveArea(TouchActiveAreaMessage* msg, uint8_t* payload)
{
  const uint8_t offset = 10;
  uint16_t value = 0;
  uint16_t valueLength = 0;

  for (int i = offset; i < payload[11] + offset; i++) // 10 = index for TouchActiveArea struct, 11 index for length of TouchActiveArea struct
  {
    switch (payload[i])
    {
      case 0x80: // LowerBoundX
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
        msg->lowerBoundX = value;
      break;

      case 0x81: // LowerBoundY
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
        msg->lowerBoundY = value;
      break;

      case 0x82: // UpperBoundX
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
        msg->upperBoundX = value;
      break;

      case 0x83: // UpperBoundY
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
        msg->upperBoundY = value;
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

void Zforce::ParseFloatingProtection(FloatingProtectionMessage* msg, uint8_t* payload)
{
  const uint8_t offset = 10;
  uint8_t valueLength = 0;
  msg->time = 0;

  for(int i = offset + payload[11]; i < payload[9] + offset; i++)
  {
    if (payload[i] == 0xA8) // Identifier floating protection
    {
      for (int j = 0; j < payload[i + 1]; j++)
      {
        if (payload[i + j] == 0x80) // Identifier floating protection enabled
        {
          msg->enabled = (bool)payload[i + j + 2];
        }
        if (payload[i + j] == 0x81) // Identifier for time
        {
          valueLength = payload[i + j + 1];
          if (valueLength == 2)
          {
            msg->time = payload[i + j + 2] << 8;
            msg->time |= payload[i + j + 3];
          }
          else
          {
            msg->time = payload[i + j + 2];
          }
          break;
        }
      }
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

void Zforce::ParseDetectionMode(DetectionModeMessage* msg, uint8_t* payload)
{
  uint8_t offset = payload[11] + 11; // 11 = Index for TouchActiveArea length
  offset += payload[offset + 2]; // Add the length of the following Sequence to the offset
  const uint8_t length = payload[1] + 2;

  for (int i = offset; i < length; i++)
  {
    if (payload[i] == 0x85) // Primitive type = 0x60, context specific = 0x20, tag id = 0x05 => 0x85
    {
      // We found it
      uint8_t valueLength = payload[i + 1];
      msg->mergeTouches = (payload[i + valueLength + 1] & 0x20) != 0; 
      msg->reflectiveEdgeFilter = (payload[i + valueLength + 1] & 0x80) != 0;
      break;
    }
    else
    {
      // Keep looking 
      i += payload[i + 1] + 1;
    }
  }
}

void Zforce::ParseTouch(TouchMessage* msg, uint8_t* payload)
{

  if (touchMetaInformation.touchDescriptor == nullptr)
  {
    return;
  } 
  else
  {
    const uint8_t payloadOffset = 12;
    const uint8_t expectedTouchLength = touchMetaInformation.touchByteCount + 2;
    msg->touchCount = payload[9] / expectedTouchLength;
    msg->touchData = new TouchData[msg->touchCount];
    
    if ((payload[1] + 2) > (payloadOffset + (expectedTouchLength * msg->touchCount))) // Check for timestamp
    {
      uint8_t timestampIndex = payloadOffset + (touchMetaInformation.touchByteCount * msg->touchCount);
      if (payload[timestampIndex] == 0x58) // Check for timestamp identifier
      {
        uint8_t timestampLength = payload[timestampIndex + 1];
        uint8_t valueIndex = timestampIndex + timestampLength + 1;
        for (int8_t i = 0; i < timestampLength; i++)
        {
          msg->timestamp |= payload[valueIndex - i] << (8 * i);
        }
      }
    }

    for (uint8_t i = 0; i < msg->touchCount; i++)
    {
      for (uint8_t j = 0; j < touchMetaInformation.touchByteCount; j++)
      {
        uint8_t index = payloadOffset + j + (i * expectedTouchLength);
        switch (touchMetaInformation.touchDescriptor[j])
        {
          case TouchDescriptor::Id:
          {
            msg->touchData[i].id = payload[index];
            break;
          }
          case TouchDescriptor::Event:
          {
            msg->touchData[i].event = (TouchEvent)payload[index];
            break;
          }
          case TouchDescriptor::LocXByte1:
          {
            msg->touchData[i].x = payload[index];
            break;
          }
          case TouchDescriptor::LocXByte2:
          {
            msg->touchData[i].x <<= 8;
            msg->touchData[i].x |= payload[index];
            break;
          }
          case TouchDescriptor::LocXByte3:
          {
            msg->touchData[i].x <<= 8;
            msg->touchData[i].x |= payload[index];
            break;
          }
          case TouchDescriptor::LocYByte1:
          {
            msg->touchData[i].y = payload[index];
            break;
          }
          case TouchDescriptor::LocYByte2:
          {
            msg->touchData[i].y <<= 8;
            msg->touchData[i].y |= payload[index];
            break;
          }
          case TouchDescriptor::LocYByte3:
          {
            msg->touchData[i].y <<= 8;
            msg->touchData[i].y |= payload[index];
            break;
          }
          case TouchDescriptor::LocZByte1:
          {
            // No support for Z in Neonode AIR Touch sensor
            break;
          }
          case TouchDescriptor::LocZByte2:
          {
            // No support for Z in Neonode AIR Touch sensor
            break;
          }
          case TouchDescriptor::LocZByte3:
          {
            // No support for Z in Neonode AIR Touch sensor
            break;
          }
          case TouchDescriptor::SizeXByte1:
          {
            msg->touchData[i].sizeX = payload[index];
            break;
          }
          case TouchDescriptor::SizeXByte2:
          {
            msg->touchData[i].sizeX <<= 8;
            msg->touchData[i].sizeX |= payload[index];
            break;
          }
          case TouchDescriptor::SizeXByte3:
          {
            msg->touchData[i].sizeX <<= 8;
            msg->touchData[i].sizeX |= payload[index];
            break;
          }
          case TouchDescriptor::SizeYByte1:
          {
            // No support for size Y in Neonode AIR Touch sensor
            break;
          }
          case TouchDescriptor::SizeYByte2:
          {
            // No support for size Y in Neonode AIR Touch sensor
            break;
          }
          case TouchDescriptor::SizeYByte3:
          {
            // No support for size Y in Neonode AIR Touch sensor
            break;
          }
          case TouchDescriptor::SizeZByte1:
          {
            // No support for size Z in Neonode AIR Touch sensor
            break;
          }
          case TouchDescriptor::SizeZByte2:
          {
            // No support for size Z in Neonode AIR Touch sensor
            break;
          }
          case TouchDescriptor::SizeZByte3:
          {
            // No support for size Z in Neonode AIR Touch sensor
            break;
          }
          case TouchDescriptor::Orientation:
          {
            // No support for Orientation in Neonode AIR Touch sensor
            break;
          }
          case TouchDescriptor::Confidence:
          {
            // Confidence is reported but should not be used as it is always reported as 100%
            break;
          }
          case TouchDescriptor::Pressure:
          {
             // There iws no support for pressure in Neonode AIR Touch sensor
            break;
          }
          default:
          break;
        }
      }
    }
  }
}

void Zforce::ClearBuffer(uint8_t* buffer)
{
  memset(buffer, 0, MAX_PAYLOAD);
}

Zforce zforce = Zforce();
