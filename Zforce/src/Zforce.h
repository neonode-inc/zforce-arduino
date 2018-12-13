#pragma once

#define MAX_PAYLOAD 255
#define ZFORCE_I2C_ADDRESS 0x50

enum TouchEvent
{
	DOWN = 0,
	MOVE = 1,
	UP = 2,
	INVALID = 3,
	GHOST = 4
};

enum MessageType
{
	ENABLE,
	TOUCHACTIVEAREA,
	REVERSEX,
	REVERSEY,
	FLIPXY,
	REPORTEDTOUCHES,
	TOUCH
};

enum MessageIdentifier
{
	ENABLE,
	TOUCHACTIVEAREA,
	REVERSEX,
	REVERSEY,
	FLIPXY,
	REPORTEDTOUCHES
};

struct Message
{
	MessageType type;
};

struct Touch : Message
{
	uint16_t x;
	uint16_t y;
	uint8_t id;
	TouchEvent event;
};

struct Enable : Message
{
	bool enabled;
};

struct TouchActiveArea : Message
{
	uint16_t minX;
	uint16_t minY;
	uint16_t maxX;
	uint16_t maxY;
};

struct FlipXY : Message
{
	bool flipXY;
};

struct ReverseX : Message
{
	bool reverseX;
};

struct ReverseY : Message
{
	bool reverseY;
};

struct ReportedTouches : Message
{
	uint8_t reportedTouches;
};

class Zforce 
{
    public:
		Zforce();
		int Read(uint8_t* payload);
		int Write(uint8_t* payload);
		bool Enable(bool isEnabled);
		bool TouchActiveArea(int minX, int minY, int maxX, int maxY); // Missing
		bool FlipXY(bool isFlipped); // Missing
		bool ReverseX(bool isRev); // Missing
		bool ReverseY(bool isRev); // Missing
		bool ReportedTouches(uint8_t reportedTouches); // Missing
		void Start(int dr);
		void Stop(); // Missing
		Message GetMessage();
    private:
		int GetLength();
		int GetDataReady();
		void VirtualParse(uint8_t* payload);
		void ParseTouchActiveArea(Message* msg, uint8_t* payload);
		void Enqueue(Message msg);
		void Dequeue(Message* msg);
		uint8_t payload[MAX_PAYLOAD];
		int dataReady;
		const int timeout = 1000;
		MessageIdentifier lastSentMessage;
};

extern Zforce zforce;