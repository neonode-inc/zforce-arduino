#pragma once

#define MAX_PAYLOAD 255
#define ZFORCE_I2C_ADDRESS 0x50

class Zforce 
{
    public:
		Zforce();
		void ReadMessage();
		int WriteMessage(uint8_t* data);
		int GetLength();
		int GetDataReady();
		int Start(int dr);
		uint8_t * GetPayload();

    private:
		uint8_t payload[MAX_PAYLOAD];
		int length;
		int dataReady;
};

extern Zforce zforce;