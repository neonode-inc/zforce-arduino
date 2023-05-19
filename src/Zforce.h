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
#pragma once

// Largest transaction size, excluding i2c header.
#define MAX_PAYLOAD 255
// The buffer must be able to contain both the i2c header, and MAX_PAYLOAD size.
#define BUFFER_SIZE (MAX_PAYLOAD+2)
#define ZFORCE_DEFAULT_I2C_ADDRESS 0x50

enum TouchEvent
{
	DOWN = 0,
	MOVE = 1,
	UP = 2,
	INVALID = 3,
	GHOST = 4
};

enum class MessageType
{
	NONE = 0,
	ENABLETYPE = 1,
	TOUCHACTIVEAREATYPE = 2,
	REVERSEXTYPE = 3,
	REVERSEYTYPE = 4,
	FLIPXYTYPE = 5,
	REPORTEDTOUCHESTYPE = 6,
	TOUCHTYPE = 7,
	BOOTCOMPLETETYPE = 8,
	FREQUENCYTYPE = 9,
	DETECTIONMODETYPE = 10,
	TOUCHFORMATTYPE = 11,
	TOUCHMODETYPE = 12,
	FLOATINGPROTECTIONTYPE = 13,
	PLATFORMINFORMATIONTYPE = 14
};

typedef struct TouchData
{
	uint32_t x;
	uint32_t y;
	uint32_t sizeX;   //the estimated diameter of the touch object
	uint8_t id;
	TouchEvent event;
} TouchData;

enum class TouchModes
{
	NORMAL,
	CLICKONTOUCH,
	UNSUPPORTED
};

typedef struct Message
{
	virtual ~Message()
	{
		
	}
	MessageType type;
} Message;

typedef struct TouchMessage : public Message
{
	virtual ~TouchMessage()
	{
		delete[] touchData;
		touchData = nullptr;
	}
	uint32_t timestamp;
	uint8_t touchCount;
	TouchData* touchData;
} TouchMessage;

typedef struct EnableMessage : public Message
{
	virtual ~EnableMessage()
	{

	}
	bool enabled;
} EnableMessage;

typedef struct TouchActiveAreaMessage : public Message
{
	virtual ~TouchActiveAreaMessage()
	{
		
	}
	uint16_t minX;
	uint16_t minY;
	uint16_t maxX;
	uint16_t maxY;
} TouchActiveAreaMessage;

typedef struct FrequencyMessage : public Message
{
	virtual ~FrequencyMessage()
	{

	}
	uint16_t idleFrequency;
	uint16_t fingerFrequency;
} FrequencyMessage;


typedef struct FlipXYMessage : public Message
{
	virtual ~FlipXYMessage()
	{
		
	}
	bool flipXY;
} FlipXYMessage;

typedef struct ReverseXMessage : public Message
{
	virtual ~ReverseXMessage()
	{
		
	}
	bool reversed;
} ReverseXMessage;

typedef struct ReverseYMessage : public Message
{
	virtual ~ReverseYMessage()
	{
		
	}
	bool reversed;
} ReverseYMessage;

typedef struct ReportedTouchesMessage : public Message
{
	virtual ~ReportedTouchesMessage()
	{
		
	}
	uint8_t reportedTouches;
} ReportedTouchesMessage;

typedef struct DetectionModeMessage : public Message
{
	virtual ~DetectionModeMessage()
	{
		
	}
	bool mergeTouches;
	bool reflectiveEdgeFilter;
} DetectionModeMessage;

typedef struct TouchModeMessage : public Message
{
	virtual ~TouchModeMessage()
	{

	}
	TouchModes mode;
	int clickOnTouchRadius;
	int clickOnTouchTime;
} TouchModeMessage;

enum class TouchDescriptor : uint8_t
{
		Id = 0,
        Event = 1,
        LocXByte1 = 2,
        LocXByte2 = 3,
        LocXByte3 = 4,
        LocYByte1 = 5,
        LocYByte2 = 6,
        LocYByte3 = 7,
        LocZByte1 = 8 ,
        LocZByte2 = 9,
        LocZByte3 = 10,
        SizeXByte1 = 11,
        SizeXByte2 = 12,
        SizeXByte3 = 13,
        SizeYByte1 = 14,
        SizeYByte2 = 15,
        SizeYByte3 = 16,
        SizeZByte1 = 17,
        SizeZByte2 = 18,
        SizeZByte3 = 19,
        Orientation = 20,
        Confidence = 21,
        Pressure = 22,
		MaxValue = 23 // Maximum value of enum
};

typedef struct TouchDescriptorMessage : public Message
{
	virtual ~TouchDescriptorMessage()
	{
		delete descriptor;
		descriptor = nullptr;
	}
	TouchDescriptor *descriptor;

} TouchDescriptorMessage;

typedef struct PlatformInformationMessage : public Message
{
	virtual ~PlatformInformationMessage()
	{
		delete[] mcuUniqueIdentifier;
		mcuUniqueIdentifier = nullptr;
	}
	uint8_t firmwareVersionMajor;
	uint8_t firmwareVersionMinor;
	char* mcuUniqueIdentifier;
	uint8_t mcuUniqueIdentifierLength;
} PlatformInformationMessage;

typedef struct FloatingProtectionMessage : public Message
{
	virtual ~FloatingProtectionMessage()
	{

	}
	bool enabled;
	uint16_t time;
} FloatingProtectionMessage;

typedef struct TouchMetaInformation
{
	TouchDescriptor *touchDescriptor;
	uint8_t touchByteCount = 0;
} TouchMetaInformation;

class Zforce 
{
    public:
		Zforce();
		void Start(int dr);
		void Start(int dr, int i2cAddress);
		int Read(uint8_t* payload);
		int Write(uint8_t* payload);
		bool SendRawMessage(uint8_t* payload, uint8_t payloadLength);
		uint8_t* ReceiveRawMessage(uint8_t* receivedLength, uint16_t *remainingLength);
		bool Enable(bool isEnabled);
		bool GetEnable();
		bool TouchActiveArea(uint16_t minX, uint16_t minY, uint16_t maxX, uint16_t maxY);
		bool FlipXY(bool isFlipped);
		bool ReverseX(bool isReversed);
		bool ReverseY(bool isReversed);
		bool Frequency(uint16_t idleFrequency, uint16_t fingerFrequency);
		bool ReportedTouches(uint8_t touches);
		bool DetectionMode(bool mergeTouches, bool reflectiveEdgeFilter);	
		bool TouchFormat();	
		bool TouchMode(uint8_t mode, int16_t clickOnTouchRadius, int16_t clickOnTouchTime);
		bool FloatingProtection(bool enabled, uint16_t time);
		int GetDataReady();
		Message* GetMessage();
		void DestroyMessage(Message * msg);
		bool GetPlatformInformation();
		uint8_t FirmwareVersionMajor;
		uint8_t FirmwareVersionMinor;
		char* MCUUniqueIdentifier;
    private:
		Message* VirtualParse(uint8_t* payload);
		void ParseTouchActiveArea(TouchActiveAreaMessage* msg, uint8_t* payload);
		void ParseEnable(EnableMessage* msg, uint8_t* payload);
		void ParseFrequency(FrequencyMessage* msg, uint8_t* payload);
		void ParseReportedTouches(ReportedTouchesMessage* msg, uint8_t* payload);
		void ParseReverseX(ReverseXMessage* msg, uint8_t* payload);
		void ParseReverseY(ReverseYMessage* msg, uint8_t* payload);
		void ParseFlipXY(FlipXYMessage* msg, uint8_t* payload);
		void ParseTouch(TouchMessage* msg, uint8_t* payload);
		void ParseDetectionMode(DetectionModeMessage* msg, uint8_t* payload);
		void ParseResponse(uint8_t* payload, Message** msg);
		void ParseTouchDescriptor(TouchDescriptorMessage* msg, uint8_t* payload);
		void ParseTouchMode(TouchModeMessage* msg, uint8_t* payload);
		void ParseFloatingProtection(FloatingProtectionMessage* msg, uint8_t* payload);
		void ParsePlatformInformation(PlatformInformationMessage* msg, uint8_t* rawData, uint32_t length);
		void ClearBuffer(uint8_t* buffer);
		uint8_t SerializeInt(int32_t value, uint8_t* serialized);
		void DecodeOctetString(uint8_t* rawData, uint32_t* position, uint32_t* destinationLength, uint8_t** destination);
		uint16_t GetLength(uint8_t* rawData);
		uint8_t GetNumLengthBytes(uint8_t* rawData);
		uint8_t buffer[BUFFER_SIZE];
		int dataReady;
		int i2cAddress;
		uint16_t remainingRawLength;
		volatile MessageType lastSentMessage;
		TouchMetaInformation touchMetaInformation;
		bool touchDescriptorInitialized;
};

extern Zforce zforce;
