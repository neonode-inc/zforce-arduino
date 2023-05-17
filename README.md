# Neonode Touch Sensor Module Interface library for Arduino

An Arduino library for communicating with the _Neonode Touch Sensor Module_ optical touch sensor. Handles the fundamental BER encoded ASN.1 messages communicated over I2C.  
For more information, visit [Neonode homepage](https://neonode.com/).  


# Introduction
The library offers an easy way to communicate with the sensor as well as some primitive parsing of the ASN.1 serialized response messages. This makes it easy to get x and y coordinates from touch notifications or set different settings in the sensor. The library does not have support for all messages available in the ASN.1 protocol, however methods for sending and receiving raw ASN.1 message are available and can be used to access the full functionality.  


# Support and Additional information
Support information, user's guides, sensor product specification and much more can be found in our [Support Center](https://support.neonode.com/).  

- [Neonode Touch Sensor Module User's Guide](https://support.neonode.com/docs/pages/viewpage.action?pageId=101351508)  
- [zForce Message Specification](https://support.neonode.com/docs/display/AIRTSUsersGuide/Protocol+version+1.12)  
- [Configuration Parameters Overview](https://support.neonode.com/docs/display/AIRTSUsersGuide/Parameter+Overview)  
- [zForce Programmer](https://support.neonode.com/docs/display/ZFPUG/)

# Supported Platforms and Hardware

## Platforms

The library version 1.8 has been tested on and officially supports these platforms:  
- Arduino Uno Rev3
- Raspberry Pi Pico  

Other platforms might work as well.  

## Touch Sensor Modules
All commercially available _Neonode Touch Sensor Modules_ are supported (this excludes customer specific firmwares).  
Neonode generally recommends using the latest firmware compatible with your sensor, visit [Neonode Support Center](https://support.neonode.com/) to download latest firmware.  

# How to Use the library

## Main Loop
The library is built around using `zforce.GetMessage()` as the main method for reading messages from the sensor. The `GetMessage()` method checks if the data ready pin is high and, if it is, reads the awaiting message from the sensor. The received message is then parsed and a pointer to a `Message` is returned.  
A successful `GetMessage()` call will allocate memory for the new `Message` dynamically. It is up to the end user to destroy the message by calling `zforce.DestroyMessage()` when the message information is no longer needed.  
Please check the supplied example code for usage examples.

## Send and Read Messages
The library has support for setting some basic configuration parameters in the sensor, for example `zforce.SetTouchActiveArea()`. When writing any message to the sensor, the end user has to make sure that data ready signal is `LOW` before writing (i.e. there must be no messages awaiting to be read from the sensor). If data ready signal is `HIGH`, `GetMessage()` method needs to be called until `nullptr` is received as response, indicating there are no more messages awaiting in the sensor.  

When a message has been sent, the sensor always creates a response that has to be read by the host. It could take some time for the sensor to create the response and put it in the I2C buffer, which is why it is recommended to call the `GetMessage()` method in a do-while loop after sending a request.  

### Configuration of the Sensor
#### 1.xx firmware
Firmwares of versions 1.xx _do not_ support persistent storage of configuration parameters. The user must make sure the sensor is configured for the user's need after every startup and after every reset. A received message of type `BOOTCOMPLETE` indicates that the sensor has started up and is ready to be configured. See code example below.  
#### 2.xx firmware
Firmware of versions 2.xx _do_ support both persistent storage of configuration parameters and configuration at runtime. Configuration parameters set at runtime will _not_ be stored persistently in the sensor. To alter the persistent configuration a separate tool is needed, see [zForce Programmer](https://support.neonode.com/docs/display/ZFPUG/) at [Neonode Support Center](https://support.neonode.com/).  
### Configuration Parameters
An overview of the configuration parameters can be found here: [Configuration Parameters Overview](https://support.neonode.com/docs/display/AIRTSUsersGuide/Parameter+Overview)  

**NOTE:** All configuration of the sensor should be done before sensor is initialized using `Start()` method.  

**Code Example:**  
```C++
// Make sure that there is nothing in the I2C buffer before writing to the sensor
Message* msg = zforce.GetMessage();
if(msg != nullptr)
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
} while (msg == nullptr);
 
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

# Methods Overview


## Public Methods



| Return Type   | Method | Parameters |Description | Return |
| --- | --- | --- | --- | --- |
| Constructor | `Zforce` | None | Not used. | None |
| `void` | `Start` | `int dataReady` | Initialize communication with the sensor including starting the I2C connection and configure the dataReady pin according to given parameter. Default sensor I2C address 0x50 will be used. | None |
| `void` | `Start` | `int dataReady`, `int i2cAddress` | Initialize communication with the sensor including starting the I2C connection and configure the dataReady pin and I2C address according to provided parameters. | None |
| `int` | `Read` | `uint8_t* payload` | Initiates an I2C read sequence. Response is copied to `payload` array. No parsing of the received message is done and no `Message` is created. <BR> **CAUTION:** The user must ensure that sufficient space is available in `payload` to hold the complete I2C message. <BR> *Recommendation:* For reading raw ASN.1 messages it is advised to use the `ReceiveRawMessage` method instead. | Error code according to the Atmel data sheet if an Atmel platform is used. 0 for success. Non-Atmel platforms will always return 0. |
| `int` | `Write` | `uint8_t* payload` | Initiates an I2C write sequence. Data from `payload` array is sent. <BR> *IMPORTANT:* For a successful write to the sensor, it is expected that `payload[0]` = `0xEE` and `payload[1]` = length of the subsequent ASN.1 message to send. <BR> *Recommendation:* For sending raw ASN.1 messages, it is advised to use `SendRawMessage()` instead. | Error code according to the Atmel data sheet if an Atmel platform is used. 0 for success.  Non-Atmel platforms will always return 0. |
| `bool` | `SendRawMessage` | `uint8_t* payload`, `uint8_t payloadLength` | Sends a custom formatted raw ASN.1 message to the sensor. `payload` is a pointer to the buffer containing the ASN.1 message to send. `payloadLength` is the length of the message to send. `SendRawMessage` is the preferred method for writing custom ASN.1 serialized messages to the sensor. | `true` if successful, otherwise `false` *. |
| `bool` | `ReceiveRawMessage` | `uint8_t* receivedLength`, `uint16_t *remainingLength` | Receive a raw ASN.1 message. No parsing of the message is done and no `Message` is created. The only validation done is decoding the initial ASN.1 payload length to see if more data should follow. `receivedLength` is a pointer to where the size of the returned data should be placed. `remainingLength` is a pointer to where the size of the remaining data should be placed, if any. `ReceiveRawMessage` is the preferred method for reading raw ASN.1 serialized messages directly from the sensor. <BR> **CAUTION:** Any subsequent read or write operations, even reading notifications, touches, etc will _overwrite_ the message receive buffer, so make sure to copy any data you want to save. | If successful, a pointer to the ASN.1 payload of the received data is returned, otherwise `nullptr` is returned. The length of the message received is stored in `receivedLength` and length of remaining data the sensor has to send is stored in `remainingLength`. |
| `uint8_t` | `Enable` | `bool isEnabled` | Enables the sensor for sending touch notifications. Operation mode is set to normal detection mode and sensor is enabled. | `true` if successful, otherwise `false` *.|
| `bool` | `GetEnable` | `None` | Gets the current enable status of the sensor. Useful to make sure if there is a sensor connected or just a check to see if it is currently disabled or enabled. | `true` if successful, otherwise `false` *.  |
| `bool` | `TouchActiveArea` | `uint16_t minX`, `uint16_t minY`, `uint16_t maxX`, `uint16_t maxY` | Writes a touch active area configuration message to the sensor with the passed parameters. | `true` if the write succeeded, otherwise `false` *. |
| `bool` | `FlipXY` | `bool isFlipped` | Writes a flip-xy configuration message to the sensor with the passed parameters. | `true` if the write succeeded, otherwise `false` *. |
| `bool` | `ReverseX` | `bool isReversed` | Writes a reverse x configuration message to the sensor with the passed parameters. | `true` if the write succeeded, otherwise `false` *. |
| `bool` | `ReverseY` | `bool isReversed` | Writes a reverse y configuration message to the sensor with the passed parameters. | `true` if the write succeeded, otherwise `false` *. |
| `bool` | `Frequency` | `uint16_t idleFrequency`, `uint16_t fingerFrequency` | Writes a frequency configuration message to the sensor with the passed parameters. | `true` if the write succeeded, otherwise `false` *. |
| `bool` | `ReportedTouches` | `uint8_t touches` | Writes a reported touches configuration message to the sensor with the passed parameters. | `true` if the write succeeded, otherwise `false` *. |
| `bool` | `DetectionMode` | `bool mergeTouches`, `bool reflectiveEdgeFilter` | Writes a detection mode configuration message to the sensor with the passed parameters.  <BR> *NOTE:* Firmware versions 2.xx does _not_ support mergeTouches. | `true` if the write succeeded, otherwise `false` *. |
| `bool` | `TouchMode` | `uint8_t mode`, `int16_t clickOnTouchRadius`, `int16_t clickOnTouchTime` | Writes a touchMode configuration message to the sensor with the passed parameters. Valid modes: 0 = normal, 1 =  clickOnTouch.  <BR> *NOTE:* Some sensor firmware will not return clickOnTouchRadius or clickOnTouchTime in response message if mode is set = normal. In this case, these values will be set to -1 in the parsed response Message received using `GetMessage()` method.| `true` if the write succeeded, otherwise `false` *. |
| `bool` | `FloatingProtection` | `bool enabled`, `uint16_t time` | Writes a floating protection configuration message to the sensor with the passed parameters. | `true` if the write succeeded, otherwise `false` *. |
| `int` | `GetDataReady` | None | Performs a digital read on the data ready pin. | The current status of the data ready pin (`HIGH`/ `LOW`). |
| `Message*` | `GetMessage` | None | Reads and parses a message from the sensor if data ready signal is `HIGH`. |  A pointer to a `Message` with parsed content if a read was successful, otherwise `nullptr`. |
| `void` | `DestroyMessage` | `Message* msg` | Deletes the pointed message null. | None |
| `bool` | `GetPlatformInformation` | None | Requests firmware version and MCU ID from sensor. This method is automatically called as part of `Start()` method and stores values in class members `FirmwareVersionMajor`, `FirmwareVersionMinor`, `MCUUniqueIdentifier`. | `true` if write succeeded, otherwise `false` *. | 

*) On non-Atmel platforms, there will be no error signalled if low level I2C communication fails. This is due to shortcomings in underlying I2C library.  

## Public Members

| Type   | Name | Description |
| --- | --- | --- |
| `uint8_t` | `FirmwareVersionMajor` | Sensor firmware version, major. Set when sensor is initialized by calling the `Start()` method. |
| `uint8_t` | `FirmwareVersionMinor` | Sensor firmware version, minor. Set when sensor is initialized by calling the `Start()` method. |
| `char*` | `MCUUniqueIdentifier` |  Pointer to string holding the sensor's MCU ID. Set when sensor is initialized by calling the `Start()` method. |


# Release notes


## zForce Arduino Library 1.8.0


### Contents of Release
- Neonode zForce Library version 1.8.0 in source form at Neonode's GitHub public repository.

### Main Features

- Added configurable I2C address to support touch sensors configured with non-default I2C address.
- New functionality for sending and receiving raw ASN.1 messages:
  - method: SendRawMessage
  - method: ReceiveRawMessage
- Added functionality for reading sensor ID:
- method: GetPlatformInformation
  - member: FirmwareVersionMajor
  - member: FirmwareVersionMinor
  - member: MCUUniqueIdentifier
- Support for ASN.1 messages up to 255 bytes (was 127 bytes in earlier versions).
- Enable method now also sets the sensor in digitizer mode before enabling.

### Bugfixes

- Touch notification timestamp fixed.
- Corrections to Enable method.
- Corrections to Frequency method.
- Corrections to TouchMode method.
- Corrections to FloatingProtection method.
- Corrections to TouchFormat method.

### Known Issues
- None.
