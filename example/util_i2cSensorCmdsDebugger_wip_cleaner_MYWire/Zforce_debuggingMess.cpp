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
//#include "I2C/I2C.h" // no need to add bloat for nothing
#include "Zforce.h"
//#if USE_I2C_LIB == 0
  //#include <Wire.h> // no need to add bloat for nothing ( replaced by #include "MYWire/MYWire.h"; within debugger sketch )
  #include <MYWire.h> // replacement with bigger buffer ( 32 -> 128 bytes )
  #if(ARDUINO >= 100)
    #include <Arduino.h>
  #else
    #include <WProgram.h>
  #endif
//#endif

Zforce::Zforce()
{
}

void Zforce::Start(int dr)
{
  dataReady = dr;
  pinMode(dataReady, INPUT);
#if USE_I2C_LIB == 1
  I2c.setSpeed(1);
  I2c.begin();
#else
  MYWire.begin();
#endif
}

int Zforce::Read(uint8_t * payload)
{
#if USE_I2C_LIB == 1
  int status = 0;

  status = I2c.read(ZFORCE_I2C_ADDRESS, 2);

  // Read the 2 I2C header bytes.
  payload[0] = I2c.receive();
  payload[1] = I2c.receive();

  status = I2c.read(ZFORCE_I2C_ADDRESS, payload[1], &payload[2]);

  return status; // return 0 if success, otherwise error code according to Atmel Data Sheet
#else
  MYWire.requestFrom(ZFORCE_I2C_ADDRESS, 2);
  payload[0] = MYWire.read();
  payload[1] = MYWire.read();

  int index = 2;
  MYWire.requestFrom(ZFORCE_I2C_ADDRESS, payload[1]);
  while (MYWire.available())
  {
    payload[index++] = MYWire.read();
  }

  return 0;
#endif
}

/*
 * Sends a message in the form of a byte array.
 */
int Zforce::Write2(uint8_t* payload)
{
#if USE_I2C_LIB == 1
  int len = payload[1] + 1;
  int status = I2c.write(ZFORCE_I2C_ADDRESS, payload[0], &payload[1], len);

  return status; // return 0 if success, otherwise error code according to Atmel Data Sheet
#else
  MYWire.beginTransmission(ZFORCE_I2C_ADDRESS);
  MYWire.write(payload, payload[1] + 2); // ORIGINAL CODE - makes sense: add 0xEE & Len to Len to get total data bytes length to sent over i2c
  //Wire.write(payload, payload[1]); // doesn't work
  //Wire.write(payload, payload[1] + 1); // doesn't work
  //Wire.write(payload, payload[1] + 3); // works - at least for enable, once ..
  //Wire.write(payload, payload[1] + 4); // didn't work ..
  //Wire.write(payload, payload[1] + 5); // not either
  //int len = sizeof(payload)/sizeof(payload[0]); // could it 'simply' be that ? ..
  //Serial.print("HACKY LEN:"); Serial.print(len);
  //Wire.write(payload, len); // should logically work ? ( if troubles came from here .. which it seemed .. )
  MYWire.endTransmission();

  return 0;
#endif
}

/*
 * Sends a message in the form of a byte array - Tef Mod
 */
int Zforce::Write(uint8_t* payload, size_t arraySize)
//int Zforce::Write(byte* payload)
{
/*
//#if USE_I2C_LIB == 1 // R: errors when compiling anyway ..
  int len = payload[1] + 1;
  int status = I2c.write(ZFORCE_I2C_ADDRESS, payload[0], &payload[1], len);

  return status; // return 0 if success, otherwise error code according to Atmel Data Sheet
//#else
*/
  MYWire.beginTransmission(ZFORCE_I2C_ADDRESS);
  //Wire.write(payload, payload[1] + 2); // ORIGINAL CODE - makes sense: add 0xEE & Len to Len to get total data bytes length to sent over i2c
  //Wire.write(payload, payload[1]); // doesn't work
  //Wire.write(payload, payload[1] + 1); // doesn't work
  //Wire.write(payload, payload[1] + 3); // works - at least for enable, once ..
  //Wire.write(payload, payload[1] + 4); // didn't work ..
  //Wire.write(payload, payload[1] + 5); // not either
  //int len = sizeof(payload)/sizeof(payload[0]); // could it 'simply' be that ? ..
  //Wire.write(payload, len); // should logically work ? ( if troubles came from here .. which it seemed .. )
  //Wire.write(payload, sizeof(payload)); // should also logically work ?
  //int len = sizeof(payload);
  //Serial.print("HACKY LEN:"); Serial.print(len); // 2 ?! -> ok, something's pretty weird here ..
  /*
  int len = payload[1]+2;
  int len0 = sizeof(payload);
  int len1 = sizeof(payload)/sizeof(payload[0]);
  int len2 = sizeof(payload)/sizeof(uint8_t);
  Serial.print("payload[1]+2 ="); Serial.print(len);
  Serial.print("\tsizeof(payload) = "); Serial.print(len0);
  Serial.print("\tsizeof(payload)/sizeof(payload[0]) = "); Serial.print(len1);
  Serial.print("\tsizeof(payload)/sizeof(uint8_t) = "); Serial.print(len2);
  Serial.println(); // always equal to 2 .. OK I THINK I GOT IT: "pointer vs array" trouble ? :P
  */
  /*
  int len = payload[1]+2;
  int len0 = sizeof(&payload);
  int len1 = sizeof(&payload)/sizeof(&payload[0]);
  int len2 = sizeof(&payload)/sizeof(uint8_t);
  Serial.print("payload[1]+2 ="); Serial.print(len);
  Serial.print("\tsizeof(payload) = "); Serial.print(len0);
  Serial.print("\tsizeof(payload)/sizeof(payload[0]) = "); Serial.print(len1);
  Serial.print("\tsizeof(payload)/sizeof(uint8_t) = "); Serial.print(len2);
  Serial.println(); // always equal to 2 .. OK I THINK I GOT IT: "pointer vs array" trouble ? :P
  */
  //Serial.print("passed array size ="); Serial.print(arraySize); // sizeof is fine when not troubled via refs/ptrs ;p


  //Wire.write(payload, len);
  //Wire.write(payload, 12); // hardcoded should at least work ?! INDEED !
  MYWire.write(payload, arraySize); // ways cleaner !
  MYWire.endTransmission();

  return 0;
//#endif
}

/*
 * Sends a message in the form of a byte array Tef Mod
 */
bool Zforce::Enable(bool isEnabled)
{
  bool failed = false;

  uint8_t enable[] = {0xEE, 0x0A, 0xEE, 0x08, 0x40, 0x02, 0x02, 0x00, 0x65, 0x02, (uint8_t)(isEnabled ? 0x81 : 0x80), 0x00}; // original code
  //byte enable[] = {0xEE, 0x0A, 0xEE, 0x08, 0x40, 0x02, 0x02, 0x00, 0x65, 0x02, (uint8_t)(isEnabled ? 0x81 : 0x80), 0x00}; // passes fine
  //uint8_t enable[] = {0xEE, 0x0A, 0xEE, 0x08, 0x40, 0x02, 0x02, 0x00, 0x65, 0x02, 0x82, 0x00}; // Ten debug: trying to use the 'reset' request - received ok
  //uint8_t enable[] = {0xEE, 0x0B, 0xEE, 0x09, 0x40, 0x02, 0x02, 0x00, 0x65, 0x03, 0x81, 0x01, 0x00}; // Tef Debug: trying to specify n of sample touches - received ok
  //uint8_t enable[] = {0xEE, 0x0B, 0xEE, 0x09, 0x40, 0x02, 0x02, 0x00, 0x65, 0x03, 0x81, 0x01, 0x01}; // Tef Debug: trying to specify n of sample touches - received Zero touch ?
  //uint8_t enable[] = {0xEE, 0x0B, 0xEE, 0x09, 0x40, 0x02, 0x02, 0x00, 0x65, 0x03, 0x81, 0x01, 0x02}; // Tef Debug: trying to specify n of sample touches - received One touch ?
  //uint8_t enable[] = {0xEE, 0x0B, 0xEE, 0x09, 0x40, 0x02, 0x02, 0x00, 0x65, 0x03, 0x81, 0x01, 0x03}; // Tef Debug: trying to specify n of sample touches - received Two touch eats indeed

  //if (Write(enable)) // We assume that the end user has called GetMessage prior to calling this method
  if (Write(enable, sizeof(enable)/sizeof(enable[0]))) // We assume that the end user has called GetMessage prior to calling this method
  {
    failed = true;
  }
  else
  {
    lastSentMessage = MessageType::ENABLETYPE;
  }

  return !failed;
}

bool Zforce::TouchActiveArea(uint16_t minX, uint16_t minY, uint16_t maxX, uint16_t maxY)
{
  bool failed = false;

  const uint8_t length = 16;

  uint8_t touchActiveArea[] = {0xEE, length + 10, 0xEE, length + 8,
                               0x40, 0x02, 0x02, 0x00, 0x73, length + 2, 0xA2, length,
                               0x80, 0x02, (uint8_t)(minX >> 8), (uint8_t)(minX & 0xFF),
                               0x81, 0x02, (uint8_t)(minY >> 8), (uint8_t)(minY & 0xFF),
                               0x82, 0x02, (uint8_t)(maxX >> 8), (uint8_t)(maxX & 0xFF),
                               0x83, 0x02, (uint8_t)(maxY >> 8), (uint8_t)(maxY & 0xFF)};

  //if (Write(touchActiveArea)) // We assume that the end user has called GetMessage prior to calling this method
  if (Write(touchActiveArea, sizeof(touchActiveArea)/sizeof(touchActiveArea[0])))
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


    //if (Write(frequency)) // We assume that the end user has called GetMessage prior to calling this method
    if (Write(frequency, sizeof(frequency)/sizeof(frequency[0])))
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

  //if (Write(flipXY)) // We assume that the end user has called GetMessage prior to calling this method
  if (Write(flipXY, sizeof(flipXY)/sizeof(flipXY[0])))
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

  //if (Write(reverseX)) // We assume that the end user has called GetMessage prior to calling this method
  if (Write(reverseX, sizeof(reverseX)/sizeof(reverseX[0])))
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

  //if (Write(reverseY)) // We assume that the end user has called GetMessage prior to calling this method
  if (Write(reverseY, sizeof(reverseY)/sizeof(reverseY[0])))
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

  //if (Write(reportedTouches)) // We assume that the end user has called GetMessage prior to calling this method
  if (Write(reportedTouches, sizeof(reportedTouches)/sizeof(reportedTouches[0])))
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

  //if (Write(detectionMode)) // We assume that the end user has called GetMessage prior to calling this method
  if (Write(detectionMode, sizeof(detectionMode)/sizeof(detectionMode[0])))
  {
    failed = true;
  }
  else
  {
    lastSentMessage = MessageType::DETECTIONMODETYPE;
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
    default:
    {
      (*(msg)) = new Message;
      (*(msg))->type = MessageType::NONE;
    }
    break;
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

void Zforce::ParseDetectionMode(DetectionModeMessage* msg, uint8_t* payload)
{
  uint8_t offset = payload[11] + 11; // 11 = Index for SubTouchActiveArea length
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
  msg->touchCount = payload[9] / 11; // Calculate the amount of touch objects.
  msg->touchData = new TouchData[msg->touchCount];

  for(uint8_t i = 0; i < msg->touchCount; i++)
  {
    msg->touchData[i].id = payload[12 + (i * 11)];
    msg->touchData[i].event = (TouchEvent)(payload[13 + (i * 11)]);
    msg->touchData[i].x = payload[14 + (i * 11)] << 8;
    msg->touchData[i].x |= payload[15 + (i * 11)];
    msg->touchData[i].y = payload[16 + (i * 11)] << 8;
    msg->touchData[i].y |= payload[17 + (i * 11)];
    msg->touchData[i].sizeX = payload[18 + (i * 11)] << 8;
    msg->touchData[i].sizeX |= payload[19 + (i * 11)];
  }
}

void Zforce::ClearBuffer(uint8_t* buffer)
{
  memset(buffer, 0, MAX_PAYLOAD);
}

Zforce zforce = Zforce();
