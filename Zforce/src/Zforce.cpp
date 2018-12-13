#include "I2C/I2C.h"
#include "Zforce.h"

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
  status = I2c.read(ZFORCE_I2C_ADDRESS, 2);

  if (status)
  {
    return status;
  }

  // Read the 2 I2C header bytes.
  payload[0] = I2c.receive();
  payload[1] = I2c.receive();
  
  status = I2c.read(ZFORCE_I2C_ADDRESS, payload[1], &payload[2]);

  if (status)
  {
    return status;
  }

  Enqueue(payload);

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
    Read(payload);
  }

  if (Write(enable))
  {
    failed = true;
  }
  else
  {
    lastSentMessage = enable;
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

  if(!Read(payload) && !failed)
  {
    failed = true;
  }

  if(!failed)
  {
    VirtualParse(payload);
  }

  return !failed;
}

Message Zforce::GetMessage()
{
  Message msg;
  Dequeue(&msg);
  return (Message)msg;
}

void Zforce::VirtualParse(uint8_t* payload)
{
  Message msg;
  msg.type = payload[2];

  switch(msg.type)
  {
    case RESPONSE:
      switch(payload[8])
      {
        case 0x65: // It's an enable
          Enable enable = (Enable)msg;
          switch (payload[10])
          {
            case 0x80: // It's disabled
              enable.enabled = false;
            break;

            case 0x81: // It's enabled
              enable.enabled = true;
            break;

            default:
            break;
          }
        break;
        case 0x73: // It's a device configuration
        break;
        default:
        break;
      }
      
    break;

    case NOTIFICATION:
      if (payload[8] == 0xA0) // Check the identifier if this is a touch message or something else.
      {
        uint8_t touchCount = payload[9] / 11; // Calculate the amount of touch objects.
        for (uint8_t i = 0; i < touchCount; i++)
        {
          Touch touch = (Touch)msg;

          touch.id = payload[12 + (i * 11)];
          touch.event = (TouchEvent)(payload[13 + (i * 11)];
          touch.x = payload[14 + (i * 11)] << 8;
          touch.x |= payload[15 + (i * 11)];
          touch.y = payload[16 + (i * 11)] << 8;
          touch.y |= payload[17 + (i * 11)];

          Enqueue(msg);
        }
      }
    break;

    default:
    break;
  }

}

int Zforce::GetDataReady()
{
  return digitalRead(dataReady);
}

void Zforce::Enqueue(Message msg)
{
  
}

void Zforce::Dequeue(Message* msg)
{

}

/*
 * Get the first two "I2C bytes" of the message in order to get the length of the whole message.
 */
int Zforce::GetLength()
{
  return length;
}


Zforce zforce = Zforce();