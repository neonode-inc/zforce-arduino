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
	ENABLETYPE,
	TOUCHACTIVEAREATYPE,
	REVERSEXTYPE,
	REVERSEYTYPE,
	FLIPXYTYPE,
	REPORTEDTOUCHESTYPE,
	TOUCHTYPE
};

enum MessageIdentifier
{
	ENABLE,
	TOUCHACTIVEAREA,
	REVERSEX,
	REVERSEY,
	FLIPXY,
	REPORTEDTOUCHES,
	UNKNOWN
};

struct Message
{
	MessageType type;
	Message* next;
};

struct TouchData : Message
{
	uint16_t x;
	uint16_t y;
	uint8_t id;
	TouchEvent event;
};

struct EnableData : Message
{
	bool enabled;
};

struct TouchActiveAreaData : Message
{
	uint16_t minX;
	uint16_t minY;
	uint16_t maxX;
	uint16_t maxY;
};

struct FlipXYData : Message
{
	bool flipXY;
};

struct ReverseXData : Message
{
	bool reversed;
};

struct ReverseYData : Message
{
	bool reversed;
};

struct ReportedTouchesData : Message
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
//		bool TouchActiveArea(int minX, int minY, int maxX, int maxY); // Missing
//		bool FlipXY(bool isFlipped); // Missing
//		bool ReverseX(bool isRev); // Missing
//		bool ReverseY(bool isRev); // Missing
//		bool ReportedTouches(uint8_t reportedTouches); // Missing
		void Start(int dr);
//		void Stop(); // Missing
		Message GetMessage();
    private:
		int GetLength();
		int GetDataReady();
		void VirtualParse(uint8_t* payload);
		void ParseTouchActiveArea(TouchActiveAreaData* msg, uint8_t* payload);
		void ParseEnable(EnableData* msg, uint8_t* payload);
		void ParseReportedTouches(ReportedTouchesData* msg, uint8_t* payload);
		void ParseReverseX(ReverseXData* msg, uint8_t* payload);
		void ParseReverseY(ReverseYData* msg, uint8_t* payload);
		void ParseFlipXY(FlipXYData* msg, uint8_t* payload);
		void ParseTouch(TouchData* msg, uint8_t* payload);
		void Enqueue(Message* msg);
		Message Dequeue();
		void ClearBuffer(uint8_t* buffer);
		uint8_t buffer[MAX_PAYLOAD];
		int dataReady;
		Message* headNode = nullptr;
		const int timeout = 1000;
		MessageIdentifier lastSentMessage;
};

extern Zforce zforce;