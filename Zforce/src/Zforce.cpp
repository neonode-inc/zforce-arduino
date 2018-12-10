#include "I2C/I2C.h"
#include "Zforce.h"

Zforce::Zforce()
{
}

Zforce::Start(int dr)
{
  dataReady = dr;
  pinMode(dataReady, INPUT);
  I2c.setSpeed(1);
  I2c.begin();
}

void Zforce::ReadMessage()
{
  int status = 0;
  status = I2c.read(ZFORCE_I2C_ADDRESS, 2);

  if (status)
  {
    return status;
  }

  payload[0] = I2c.receive();
  payload[1] = I2c.receive();

  length = payload[1] + 1; // +1 in order to include the i2c header.
  
  status = I2c.read(ZFORCE_I2C_ADDRESS, payload[1], &payload[2]);

  if (status)
  {
    return status;
  }

  return status;
}

/*
 * Sends a message in the form of a byte array.
 */
int Zforce::WriteMessage(uint8_t* data)
{
  int len = data[1] + 2;
  int status = I2c.write(ZFORCE_I2C_ADDRESS, data[0], &data[1], len);

  return status; // return 0 if success, otherwise error code according to Atmel Data Sheet
}

int Zforce::GetDataReady()
{
  return digitalRead(dataReady);
}

/*
 * Get the first two "I2C bytes" of the message in order to get the length of the whole message.
 */
int Zforce::GetLength()
{
  return length;
}

uint8_t * Zforce::GetPayload()
{
  return payload;
}

Zforce zforce = Zforce();