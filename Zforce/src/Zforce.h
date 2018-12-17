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

typedef struct Message
{
	MessageType type;
	Message* next;
} Message;

typedef struct TouchMessage : public Message
{
	uint16_t x;
	uint16_t y;
	uint8_t id;
	TouchEvent event;
} TouchMessage;

typedef struct EnableMessage : public Message
{
	bool enabled;
} EnableMessage;

typedef struct TouchActiveAreaMessage : public Message
{
	uint16_t minX;
	uint16_t minY;
	uint16_t maxX;
	uint16_t maxY;
} TouchActiveAreaMessage;

typedef struct FlipXYMessage : public Message
{
	bool flipXY;
} FlipXYMessage;

typedef struct ReverseXMessage : public Message
{
	bool reversed;
} ReverseXMessage;

typedef struct ReverseYMessage : public Message
{
	bool reversed;
} ReverseYMessage;

typedef struct ReportedTouchesMessage : public Message
{
	uint8_t reportedTouches;
} ReportedTouchesMessage;

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
		Message* GetMessage();
		void DestroyMessage(Message * msg);
		void DestroyEnableMessage(EnableMessage* msg);
		void DestroyTouchMessage(TouchMessage* msg);
    private:
		int GetDataReady();
		void VirtualParse(uint8_t* payload);
		void ParseTouchActiveArea(TouchActiveAreaMessage* msg, uint8_t* payload);
		void ParseEnable(EnableMessage* msg, uint8_t* payload);
		void ParseReportedTouches(ReportedTouchesMessage* msg, uint8_t* payload);
		void ParseReverseX(ReverseXMessage* msg, uint8_t* payload);
		void ParseReverseY(ReverseYMessage* msg, uint8_t* payload);
		void ParseFlipXY(FlipXYMessage* msg, uint8_t* payload);
		void ParseTouch(TouchMessage* msg, uint8_t* payload, uint8_t i);
		void Enqueue(Message* msg);
		Message* Dequeue();
		void ClearBuffer(uint8_t* buffer);
		uint8_t buffer[MAX_PAYLOAD];
		int dataReady;
		Message* headNode = nullptr;
		const int timeout = 1000;
		MessageIdentifier lastSentMessage;
};

extern Zforce zforce;