/*  Neonode zForce v7 interface library for Arduino

    Copyright (C) 2019-2023 Neonode Inc.

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
  this->remainingRawLength = 0;
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
  bool successfulTouchFormatRequest = this->TouchFormat();

  if (successfulTouchFormatRequest)
  {
    do
    {
      msg = this->GetMessage();
    } while (msg == nullptr);

    if (msg->type == MessageType::TOUCHFORMATTYPE && touchMetaInformation.touchDescriptor != nullptr)
    {
      this->touchDescriptorInitialized = true;
    }

    this->DestroyMessage(msg);
  }
    
  // Get Platform information
  this->GetPlatformInformation();
  do
  {
    msg = this->GetMessage();
  } while (msg == nullptr);

  if (msg->type == MessageType::PLATFORMINFORMATIONTYPE)
  {
    this->FirmwareVersionMajor = ((PlatformInformationMessage *)msg)->firmwareVersionMajor;
    this->FirmwareVersionMinor = ((PlatformInformationMessage *)msg)->firmwareVersionMinor;
    uint8_t length = ((PlatformInformationMessage *)msg)->mcuUniqueIdentifierLength;
    this->MCUUniqueIdentifier = new char[length + 1];
    strncpy(this->MCUUniqueIdentifier, ((PlatformInformationMessage *)msg)->mcuUniqueIdentifier, length);
    this->MCUUniqueIdentifier[length] = '\0';
  }
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
  Wire.requestFrom(this->i2cAddress, 2);
  payload[0] = Wire.read();
  payload[1] = Wire.read();
  
  int index = 2;
  Wire.requestFrom(this->i2cAddress, payload[1]);
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

/*
 * Send the octet array containing an ASN.1 command as is, without validation.
 * If you need to send more than 255 bytes, call it more times with new data until done.
 *
 * payload        Pointer to the octet buffer containing the ASN.1 payload to send.
 * payloadLength  Length of the data to send.
 *
 * Return Value   true for successful send and false if any error occured.
 *                Sadly, some Arduino i2c drivers do not signal errors.
 */
bool Zforce::SendRawMessage(uint8_t* payload, uint8_t payloadLength)
{
  if ((payload == nullptr) || (payloadLength == 0))
  {
    return false;
  }

  buffer[0] = 0xEE;
  buffer[1] = payloadLength;
  memcpy(&buffer[2], payload, payloadLength);

  return Write(buffer) == 0;
}

/*
 * Receive a raw ASN.1 message. The only validation done is decoding the initial ASN.1 payload length
 * to see if more data should follow.
 *
 * receivedLength   A pointer to where the size of the returned data should be placed.
 * remainingLength  A pointer to where the size of the remaining data should be placed, if any.
 *
 * Return value     If an error occured (see comment above for the return value of SendRawMessage),
 *                  the pointer will be nullptr.
 *                  A pointer to the ASN.1 payload of the received data is returned.
 *                  Note, that any subsequent read or write-operation, even notifications, touches, etc will
 *                  will overwrite the buffer, so make sure to copy any data you want to save.
 */
uint8_t* Zforce::ReceiveRawMessage(uint8_t* receivedLength, uint16_t *remainingLength)
{
  if ((GetDataReady() == HIGH) && !Read(buffer))
  {
    uint8_t i2cPayloadLength = buffer[1];
    // Check if this is a second or first call.
    if (this->remainingRawLength == 0)
    {
      // This is the first invocation.
      // Check if it's a short, 2, or 3 byte length encoding.
      uint8_t firstLengthByte = buffer[3];
      uint16_t asn1AfterHeaderLength;
      uint8_t asn1HeaderLength = 2; // EE/EF/F0 + First byte of length.
      if (firstLengthByte < 0x80)
      {
        // Short form. Lower 7 bits are the length, but since the high bit is 0, we don't need to & 0x7F to get the lower 7.
        asn1AfterHeaderLength = firstLengthByte;
      }
      else
      {
        // Long form. First byte's top bit is set. The lower 7 bits contain the number of length bytes.
        // The following 1 or 2 bytes contain the actual length, in Big Endian / Motorola Byte Order encoding.
        uint8_t numberOfLengthBytes = (firstLengthByte & 0x7F);
        asn1HeaderLength += numberOfLengthBytes;
        asn1AfterHeaderLength = buffer[4];
        if (numberOfLengthBytes == 2)
        {
          asn1AfterHeaderLength <<= 8;
          asn1AfterHeaderLength += buffer[5];
        }
        else if (numberOfLengthBytes != 1)
        {
          // The data is most likely corrupted. Valid numbers are 1 and 2.
          *receivedLength = 0;
          *remainingLength = 0;
          return nullptr;
        }
      }
      uint16_t fullAsn1MessageLength = asn1AfterHeaderLength + asn1HeaderLength;
      // Full is the full ASN.1 payload, which can be split over several i2c payloads.
      this->remainingRawLength = fullAsn1MessageLength - i2cPayloadLength;
      *remainingLength = this->remainingRawLength;
      *receivedLength = i2cPayloadLength;
    }
    else
    {
      // Since this is the second or later invocation, we do NOT parse the ASN.1, since it
      // may well be in the middle of some data, and we already know the lengths.
      this->remainingRawLength -= i2cPayloadLength;
      *remainingLength = this->remainingRawLength;
      *receivedLength = i2cPayloadLength;
    }
  }
  else
  {
    // Either Data Ready was not high, or the read failed.
    *remainingLength = 0;
    *receivedLength = 0;
    return nullptr;
  }

  return &buffer[2]; // Skipping the i2c header in the response.
}

bool Zforce::Enable(bool isEnabled)
{
  bool failed = false;
  int returnCode;

  uint8_t enable[] =  {0xEE, 0x0B, 0xEE, 0x09, 0x40, 0x02, 0x02, 0x00, 0x65, 0x03, 0x81, 0x01, 0x00};
  uint8_t disable[] = {0xEE, 0x0A, 0xEE, 0x08, 0x40, 0x02, 0x02, 0x00, 0x65, 0x02, 0x80, 0x00};
  uint8_t operationMode[] = {0xEE, 0x17, 0xEE, 0x15, 0x40, 0x02, 0x02, 0x00, 0x67, 0x0F, 0x80, 0x01, 0xFF, 0x81, 0x01, 0x00, 0x82, 0x01, 0x00, 0x83, 0x01, 0x00, 0x84, 0x01, 0x00};

  // We assume that the end user has called GetMessage prior to calling this method
  if (isEnabled)
  {
    returnCode = Write(operationMode);
    if (returnCode != 0)
    {
      failed = true;
    }
    else
    {
      Message* msg = nullptr;
      do
      {
        msg = this->GetMessage();
      } while (msg == nullptr);

      this->DestroyMessage(msg);

      returnCode = Write(enable);
    }
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

bool Zforce::TouchActiveArea(uint16_t minX, uint16_t minY, uint16_t maxX, uint16_t maxY)
{
  bool failed = false;

  uint8_t touchActiveAreaPayloadLength = 2 * 4;
  // 2 bytes * 4 entries. Not counting the actual values, which are 1 or 2 bytes.

  // Each value that is >127 gets an extra byte.

  uint8_t minXValue[2];
  uint8_t minYValue[2];
  uint8_t maxXValue[2];
  uint8_t maxYValue[2];

  uint8_t minXLength = SerializeInt(minX, minXValue);
  uint8_t minYLength = SerializeInt(minY, minYValue);
  uint8_t maxXLength = SerializeInt(maxX, maxXValue);
  uint8_t maxYLength = SerializeInt(maxY, maxYValue);

  touchActiveAreaPayloadLength += minXLength;
  touchActiveAreaPayloadLength += minYLength;
  touchActiveAreaPayloadLength += maxXLength;
  touchActiveAreaPayloadLength += maxYLength;

#define TAA_ALLHEADERSSIZE (2 + 2 + 4 + 4)

  uint8_t totalLength = TAA_ALLHEADERSSIZE + touchActiveAreaPayloadLength;
  // 2 bytes I2C header: 0xEE + 1 byte for Length. Length: totalLength - 2.
  // 2 bytes for Request: 0xEE + 1 byte for Length. Length: totalLength - 4.
  // 4 bytes for Address: 0x40, 0x02, 0x02, 0x00.
  // 4 bytes for touchActiveArea payload headers: 0x73, touchActiveAreaPayloadLength + 2, 0xA2, touchActiveAreaPayloadLength.
  // X bytes for the actual payload (calculated above).

  uint8_t touchActiveArea[totalLength] = { 0xEE, (uint8_t)(totalLength - 2),
                                           0xEE, (uint8_t)(totalLength - 4),
                                           0x40, 0x02, 0x02, 0x00,
                                           0x73, (uint8_t)(touchActiveAreaPayloadLength + 2), 0xA2, touchActiveAreaPayloadLength };

  size_t offset = TAA_ALLHEADERSSIZE;

  // Each value <= 127 is 1 byte, above is 2 bytes.

  // MinX.
  touchActiveArea[offset++] = 0x80; // MinX identifier.
  touchActiveArea[offset++] = minXLength;
  touchActiveArea[offset++] = (uint8_t)minXValue[0];
  if (minXLength == 2)
  {
    touchActiveArea[offset++] = (uint8_t)minXValue[1];
  }

  // MinY.
  touchActiveArea[offset++] = 0x81; // MinY identifier.
  touchActiveArea[offset++] = minYLength;
  touchActiveArea[offset++] = (uint8_t)minYValue[0];
  if (minYLength == 2)
  {
    touchActiveArea[offset++] = (uint8_t)minYValue[1];
  }

  // MaxX.
  touchActiveArea[offset++] = 0x82; // MaxX identifier.
  touchActiveArea[offset++] = maxXLength;
  touchActiveArea[offset++] = (uint8_t)maxXValue[0];
  if (maxXLength == 2)
  {
    touchActiveArea[offset++] = (uint8_t)maxXValue[1];
  }

  // MaxY.
  touchActiveArea[offset++] = 0x83; // MaxY identifier.
  touchActiveArea[offset++] = maxYLength;
  touchActiveArea[offset++] = (uint8_t)maxYValue[0];
  if (maxYLength == 2)
  {
    touchActiveArea[offset++] = (uint8_t)maxYValue[1];
  }

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

  uint8_t frequencyPayloadLength = 2 * 2;
  // 2 bytes * 2 entries. Not counting the actual values, which are 1 or 2 bytes.

  // Each value that is >127 gets an extra byte.

  uint8_t fingerFrequencyValue[2];
  uint8_t idleFrequencyValue[2];

  uint8_t fingerFrequencyLength = SerializeInt(fingerFrequency, fingerFrequencyValue);
  uint8_t idleFrequencyLength = SerializeInt(idleFrequency, idleFrequencyValue);

  frequencyPayloadLength += fingerFrequencyLength;
  frequencyPayloadLength += idleFrequencyLength;

#define FREQ_ALLHEADERSSIZE (2 + 2 + 4 + 2)

  uint8_t totalLength = FREQ_ALLHEADERSSIZE + frequencyPayloadLength;
  // 2 bytes I2C header: 0xEE + 1 byte for Length. Length: totalLength - 2.
  // 2 bytes for Request: 0xEE + 1 byte for Length. Length: totalLength - 4.
  // 4 bytes for Address: 0x40, 0x02, 0x02, 0x00.
  // 2 bytes for Frequency payload headers: 0x68, frequencyPayloadLength.
  // X bytes for the actual payload (calculated above).

  uint8_t frequency[totalLength] = { 0xEE, (uint8_t)(totalLength - 2),
                                     0xEE, (uint8_t)(totalLength - 4),
                                     0x40, 0x02, 0x00, 0x00,
                                     0x68, frequencyPayloadLength };

  size_t offset = FREQ_ALLHEADERSSIZE;

  // Each value <= 127 is 1 byte, above is 2 bytes.

  // Finger Frequency.
  frequency[offset++] = 0x80; // Finger Frequency identifier.
  frequency[offset++] = fingerFrequencyLength;
  frequency[offset++] = (uint8_t)fingerFrequencyValue[0];
  if (fingerFrequencyLength == 2)
  {
    frequency[offset++] = (uint8_t)fingerFrequencyValue[1];
  }

  // Idle Frequency.
  frequency[offset++] = 0x82; // Idle Frequency identifier.
  frequency[offset++] = idleFrequencyLength;
  frequency[offset++] = (uint8_t)idleFrequencyValue[0];
  if (idleFrequencyLength == 2)
  {
    frequency[offset++] = (uint8_t)idleFrequencyValue[1];
  }

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

bool Zforce::GetPlatformInformation()
{
  bool failed = false;
  uint8_t platformInformation[] = {0xEE, 0x08, 0xEE, 0x06, 0x40, 0x02, 0x00, 0x00, 0x6C, 0x00};

  if (Write(platformInformation))
  {
    failed = true;
  }
  else
  {
    lastSentMessage = MessageType::PLATFORMINFORMATIONTYPE;
  }

  return !failed;
}

bool Zforce::TouchMode(uint8_t mode, int16_t clickOnTouchRadius, int16_t clickOnTouchTime)
{
  bool failed = false;
  uint8_t serializedTime[2];
  uint8_t serializedRadius[2];
    
  uint8_t timeLength = SerializeInt(clickOnTouchTime, serializedTime);
  uint8_t radiusLength = SerializeInt(clickOnTouchRadius, serializedRadius);
  uint8_t combinedLength = timeLength + radiusLength;
  uint8_t touchMode[18 + combinedLength] = {0xEE, (uint8_t)(16 + combinedLength), 0xEE, (uint8_t)(14 + combinedLength), 0x40, 
                           0x02, 0x02, 0x00, 0x7F, 0x24, (uint8_t)(7 + combinedLength), 
                           0x80, 0x01, mode, 0x81, timeLength, serializedTime[0] };

  uint8_t index = 17;
  if (timeLength == 2)
  {
    touchMode[index++] = serializedTime[1];
  }

  touchMode[index++] = 0x82;
  touchMode[index++] = radiusLength;
  touchMode[index++] = serializedRadius[0];

  if (radiusLength == 2)
  {
    touchMode[index] = serializedRadius[1];
  }

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
  uint8_t serializedTime[2];
  uint8_t timeLength = SerializeInt(time, serializedTime);
  uint8_t floatingProtection[17 + timeLength] = {0xEE, (uint8_t)(15 + timeLength), 0xEE, (uint8_t)(13 + timeLength), 0x40, 0x02, 0x02, 0x00, 0x73,
                                  (uint8_t)(7 + timeLength), 0xA8, (uint8_t)(5 + timeLength), 0x80, 0x01, (uint8_t)(enabled ? 0xFF : 0x00), 0x81,
                                  timeLength, serializedTime[0]};

  if (timeLength == 2)
  {
    floatingProtection[18] = serializedTime[1];
  }

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
        if (this->touchDescriptorInitialized)
        {
          msg = new TouchMessage;
          msg->type = MessageType::TOUCHTYPE;
          ParseTouch((TouchMessage*)msg, payload);
        }
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
    case MessageType::PLATFORMINFORMATIONTYPE:
    {
      (*(msg)) = new PlatformInformationMessage;
      (*(msg))->type = MessageType::PLATFORMINFORMATIONTYPE;
      ParsePlatformInformation((PlatformInformationMessage*)(*(msg)), &payload[2], payload[1] - 1);
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

void Zforce::ParsePlatformInformation(PlatformInformationMessage *msg, uint8_t *rawData, uint32_t length)
{
  (void)length;
  uint16_t value = 0;
  uint16_t valueLength = 0;

  rawData += GetNumLengthBytes(&rawData[1]) + 5; // Skip response byte + ASN.1 length + address
  rawData += GetNumLengthBytes(&rawData[1]) + 1; // Skip ASN.1 Device Information + length
  
  uint32_t platformInformationLength = GetLength(&rawData[1]);
  rawData += GetNumLengthBytes(&rawData[1]) + 1; // PlatformInformation length + PlatformInformation application identifier

  uint32_t position = 0;

  while (position < platformInformationLength)
  {
    switch (rawData[position++])
    {
      case 0x84: // FirmwareVersionMajor
      {
        valueLength = rawData[position++];
        if (valueLength == 2)
        {
            value = rawData[position++] << 8;
            value |= rawData[position++];
        }
        else
        {
            value = rawData[position++];
        }

        msg->firmwareVersionMajor = value;
        break;
      case 0x85: // FirmwareVersionMinor
        valueLength = rawData[position++];
        if (valueLength == 2)
        {
            value = rawData[position++] << 8;
            value |= rawData[position++];
        }
        else
        {
            value = rawData[position++];
        }

        msg->firmwareVersionMinor = value;
        break;
      }
      case 0x8A: // MCUUniqueIdentifier
      {
        uint8_t *MCUUniqueIdentifier = nullptr;
        uint32_t MCUUniqueIdentifierLength;
        DecodeOctetString(rawData, &position, &MCUUniqueIdentifierLength, &MCUUniqueIdentifier);

        // Each byte gets converted into its hex representation, which takes 2 bytes, then we add space for the null byte.
        const uint32_t bufferLength = (MCUUniqueIdentifierLength * 2) + 1;

        char* mcuIdBuffer = (char *)malloc(bufferLength);
        mcuIdBuffer[MCUUniqueIdentifierLength - 1] = 0; // Add null byte.
        int writeSize = 0;
        for (size_t i = 0; i < MCUUniqueIdentifierLength; i++)
        {
            writeSize += snprintf(mcuIdBuffer + writeSize, bufferLength - writeSize, "%02X", MCUUniqueIdentifier[i]);
        }
        free(MCUUniqueIdentifier);
        msg->mcuUniqueIdentifier = mcuIdBuffer;
        msg->mcuUniqueIdentifierLength = writeSize;
        break;
      }
      default:
      {
        position += rawData[position];
        position++;
        break;
      }
    }
  }
}

void Zforce::ParseTouchMode(TouchModeMessage* msg, uint8_t* payload)
{
  const uint8_t offset = 9;
  uint16_t valueLength = 0;

  msg->clickOnTouchTime = -1;   // FW <=1.55 doesn't reply properly when mode = Disabled.
  msg->clickOnTouchRadius = -1; // We set them to -1, signaling the data is invalid.

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
    msg->timestamp = 0;
    
    if ((payload[1] + 2) > (payloadOffset + (expectedTouchLength * msg->touchCount))) // Check for timestamp
    {
      uint8_t timestampIndex = payloadOffset + (expectedTouchLength * msg->touchCount) - 2;
      if (payload[timestampIndex] == 0x58) // Check for timestamp identifier
      {
        uint8_t timestampLength = payload[timestampIndex + 1];
        for (int index = (timestampIndex + 2); index < (timestampIndex + 2 + timestampLength); index++)
        {
          msg->timestamp <<= 8;
          msg->timestamp |= payload[index];
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
  memset(buffer, 0, BUFFER_SIZE);
}

uint8_t Zforce::SerializeInt(int32_t value, uint8_t* serialized)
{
  if (value < 128)
  {
    serialized[0] = (uint8_t)(value & 0xFF);
    return 1;
  }
  else
  {
    serialized[0] = (uint8_t)(value >> 8);
    serialized[1] = (uint8_t)(value & 0xFF);
    return 2;
  }
}

void Zforce::DecodeOctetString(uint8_t* rawData, uint32_t* position, uint32_t* destinationLength, uint8_t** destination)
{
    uint32_t length = rawData[(*position)++];
    *destinationLength = length;
    *destination = (uint8_t*)malloc(length);
    memcpy(*destination, &rawData[(*position)], length);
    (*position) += length;
}

uint16_t Zforce::GetLength(uint8_t* rawData)
{
    int numLengthBytes = 0;
    int length = 0;
    if (rawData[0] & 0x80) // We have long length form
    {
        numLengthBytes = rawData[0] - 0x80;
        for (int i = 0; i < numLengthBytes; i++)
        {
            length += rawData[i + 1];
        }
    }
    else
    {
        length = rawData[0];
    }

    return length;
}

uint8_t Zforce::GetNumLengthBytes(uint8_t* rawData)
{
    uint8_t numLengthBytes = 1;
    if (rawData[0] & 0x80) // We have long length form
    {
        numLengthBytes = (rawData[0] - 0x80) + 1;
    }

    return numLengthBytes;
}

Zforce zforce = Zforce();
