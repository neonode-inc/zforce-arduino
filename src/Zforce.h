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
#pragma once

#define MAX_PAYLOAD 127
#define ZFORCE_I2C_ADDRESS 0x50

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
	TOUCHFORMATTYPE = 11
};


typedef struct TouchData
{
	uint16_t x;
	uint16_t y;
	uint16_t sizeX;   //the estimated diameter of the touch object
	uint8_t id;
	TouchEvent event;
} TouchData;

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

	}
	TouchDescriptor *descriptor;

} TouchDescriptorMessage;


class Zforce 
{
    public:
		Zforce();
		void Start(int dr);
		int Read(uint8_t* payload);
		int Write(uint8_t* payload);
		bool Enable(bool isEnabled);
		bool TouchActiveArea(uint16_t minX, uint16_t minY, uint16_t maxX, uint16_t maxY);
		bool FlipXY(bool isFlipped);
		bool ReverseX(bool isReversed);
		bool ReverseY(bool isReversed);
		bool Frequency(uint16_t idleFrequency, uint16_t fingerFrequency);
		bool ReportedTouches(uint8_t touches);
		bool DetectionMode(bool mergeTouches, bool reflectiveEdgeFilter);	
		bool TouchFormat();	
		int GetDataReady();
		Message* GetMessage();
		void DestroyMessage(Message * msg);
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
		void ClearBuffer(uint8_t* buffer);
		uint8_t buffer[MAX_PAYLOAD];
		int dataReady;
		volatile MessageType lastSentMessage;
		TouchDescriptor *touchDescriptor = nullptr;
		const uint8_t maxBytesTouchFormat = 23;
};

extern Zforce zforce;