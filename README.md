# Neonode zForce AIR interface library for Arduino

An Arduino library for communicating with the [Neonode zForce Air Module] optical touch sensor. Handles the fundamental BER encoded ASN.1 messages.

[Neonode zForce Air Module]: https://www.digikey.com/short/p3374r

# Introduction
The library offers an easy way to communicate with the sensor as well as some primitive parsing of the ASN.1 serialized messages. This makes it easy to get x and y coordinates from touch notifications or set different settings in the sensor. The library does not have support for all messages available in the ASN.1 protocol, however the I2C read and write functions are public and can be used if any additional settings or requests need to be sent/read from the sensor.

# Support Questions
For support questions please submit a request to [our helpcenter](https://helpcenter.neonode.com/hc/en-us/requests/new).

# How to use the library

## Main Loop
The library is built around using zforce.GetMessage() as the main loop for reading messages from the sensor. GetMessage checks if the data ready pin is high and if it is, the function zforce.Read() will be called. The read function takes a buffer parameter which is used to store the data from the sensor.
```C++
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
```
When GetMessage has been called it is up to the end user to destroy the message by calling zforce.DestroyMessage() and passing the message pointer as a parameter.

## Send and Read Messages
The library has support for some basic settings in the sensor, for example zforce.SetTouchActiveArea(). When writing a message to the sensor the end user has to make sure that data ready is not high before writing. This is done by calling GetMessage and reading whatever might be in the I2C buffer.

When a message has been sent, the sensor always creates a response that has to be read by the host. It could take some time for the sensor to create the response and put it on the I2C buffer, which is why it is recommended to call the GetMessage function in a do while loop after sending a request.
```C++
// Make sure that there is nothing in the I2C buffer before writing to the sensor
Message* msg = zforce.GetMessage();
if(msg != NULL)
{
  // Here you can read whatever is in the message or just destroy it.
  zforce.DestroyMessage(msg);
}
 
// Send the Touch Active Area request
zforce.TouchActiveArea(50,50,2000,4000);
 
// Wait for the response to arrive
do
{
  msg = zforce.GetMessage();
} while (msg == NULL);
 
// See what the response contains
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
```

# Method Overview


## Public Methods

| Data Type   | Method          | Parameter                                               | Description                                                                                                                                                                                      | Return                                                                                   |
|-------------|-----------------|---------------------------------------------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|------------------------------------------------------------------------------------------|
| Constructor | Zforce          | None                                                    | Not used.                                                                                                                                                                                        | N/A                                                                                      |
| void        | Start           | int dataReady                                           | Used to initiate the I2C connection and set the current dataReady pin.                                                                                                                           | N/A                                                                                      |
| int         | Read            | uint8_t* payload                                        | Initiates an I2C read sequence by calling the read method in the I2C library.  This can also be used externally to read the ASN.1 serialized messages without parsing them.                      | Error code according to the atmel data sheet  for the corresponding mcu . 0 for success. |
| int         | Write           | uint8_t* payload                                        | Initiates  an I2C write sequence by calling the write method in the I2C library.  This can also be used externally to write ASN.1 serialized messages that are not yet supported by the library. | Error code according to the atmel data sheet  for the corresponding mcu . 0 for success. |
| bool        | Enable          | bool isEnabled                                          | Writes an enable message to the sensor and depending on the parameter either sends enable or disable.                                                                                            | True if the write succeeded.                                                             |
| bool        | TouchActiveArea | uint16_t minX uint16_t minY uint16_t maxX uint16_t maxY | Writes a touch active area message to the sensor with the passed parameters.                                                                                                                     | True if the write succeeded.                                                             |
| bool        | FlipXY          | bool isFlipped                                          | Writes a flip xy message to the sensor with the passed parameters.                                                                                                                               | True if the write succeeded.                                                             |
| bool        | ReverseX        | bool isReversed                                         | Writes a reverse x message to the sensor with the passed parameters.                                                                                                                             | True if the write succeeded.                                                             |
| bool        | ReverseY        | bool isReversed                                         | Writes a reverse y message to the sensor with the passed parameters.                                                                                                                             | True if the write succeeded.                                                             |
| bool        | ReportedTouches | uint8_t touches                                         | Writes a reported touches message to the sensor with the passed parameters.                                                                                                                      | True if the write succeeded.                                                             |
| int         | GetDataReady    | None                                                    | Performs a digital read on the data ready pin.                                                                                                                                                   | The current status of the data ready pin.                                                |
| Message*    | GetMessage      | None                                                    | Checks if the data ready pin is HIGH and calls the method VirtualParse if it is.                                                                                                                 | A message pointer which will be NULL if the data ready pin is LOW.                       |
| void        | DestroyMessage  | Message* msg                                            | Deletes the passed message pointer and sets it to null.                                                                                                                                          | N/A                                                                                      |

## Private Methods

| Data Type | Method               | Parameter                                    | Description                                                                                                                                              | Return             |
|-----------|----------------------|----------------------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------|--------------------|
| Message*  | VirtualParse         | uint8_t* payload                             | Checks if the payload contains a response or if it contains a notification and calls the appropriate method to parse the payload and populate a message. | A message pointer. |
| void      | ParseTouchActiveArea | TouchActiveAreaMessage* msg uint8_t* payload | Parsing of a touch active area response.                                                                                                                 | N/A                |
| void      | ParseEnable          | EnableMessage* msg uint8_t* payload          | Parsing of an enable response.                                                                                                                           | N/A                |
| void      | ParseReportedTouches | ReportedTouchesMessage* msg uint8_t* payload | Parsing of a reported touches response.                                                                                                                  | N/A                |
| void      | ParseReverseX        | ReverseXMessage* msg uint8_t* payload        | Parsing of a reverse x response.                                                                                                                         | N/A                |
| void      | ParseReverseY        | ReverseYMessage* msg uint8_t* payload        | Parsing of a reverse y response.                                                                                                                         | N/A                |
| void      | ParseFlipXY          | FlipXYMessage* msg uint8_t* payload          | Parsing of a flip xy response.                                                                                                                           | N/A                |
| void      | ParseTouch           | TouchMessage* msg uint8_t* payload           | Parsing of a touch notification.                                                                                                                         | N/A                |
| void      | ParseResponse        | uint8_t* payload Message** msg               | Calls the appropriate method depending on which type of response the payload contains.                                                                   | N/A                |
| void      | ClearBuffer          | uint8_t* buffer                              | Sets all values in the passed byte array to zero. Is called by "GetMessage" after parsing the data.                                                      | N/A                |
