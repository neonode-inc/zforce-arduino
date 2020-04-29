/*  Neonode zForce v7 interface library for Arduino

    Copyright (C) 2020 Neonode Inc.

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

/*  ------ Combined Keyboard and Relative Mouse Demo ------
    This demo converts the touch data from the sensor to make it act as a relative mouse along with a few keyboard buttons.
    The arduino is then recognized by the OS as a combined HID keyboard and relative mouse.
*/

#include <Keyboard.h>
#include <Mouse.h>
#include <Zforce.h>

//#define WAIT_SERIAL_PORT_CONNECTION   // uncomment to enforce waiting for serial port connection
                                        // (not only USB), requires active serial terminal/app to work
#define TOUCH_BUFFER_SIZE 1             // used by system to buffer touch, "1" means single touch buffer
const int holdTime = 80;                // sensitivity for mouse "left click", unit in milli-second
const int keyboardBoundary = 1100;      // separate mouse area and keyboard area on the x-axis
long globalMillis = millis();           // global timestamp

typedef struct Location
{
    uint16_t x;
    uint16_t y;
} Location;

typedef struct TouchPoint
{
    Location loc;
    uint8_t state;
} TouchPoint;

uint8_t nTouches = 0;
TouchPoint tp;
Location previousLoc;
Location initialLoc;
int previousButtonState = HIGH;         // for checking the state of a pushButton
volatile bool newTouchDataFlag = false; // new data flag works with data ready pin ISR

/*
 * Interrupt service callback for Data ready pin, set flag and get data in process()
 */
void dataReadyISR() { newTouchDataFlag = true; }

/*
 * Check if data is ready to fetch
 * @return true if there is data waiting to fetch
 */
bool isDataReady() { return digitalRead(PIN_NN_DR) == HIGH; }

/*
 * Sensor initialization and configuration
 * @return true if all steps are excuted successfully
 */
bool touchInit()
{
    bool ok = true;
    globalMillis = millis();
    zforce.Start(PIN_NN_DR);
    pinMode(PIN_NN_RST, OUTPUT);            // setup Reset pin
    digitalWrite(PIN_NN_RST, LOW);
    delay(10);
    digitalWrite(PIN_NN_RST, HIGH);         // Reset sensor
    
    uint16_t timeout = 500;
    Message *msg = NULL;

    do
    {
        msg = zforce.GetMessage();
        delay(1);
    } while (msg == NULL && --timeout >= 0); // Wait 500ms for the boot complete message
     
    if(timeout <= 0)
    {
        Serial.println("Timeout during getting boot complete");
        ok = false;
    }

    if (ok)
    {
        if (msg->type == MessageType::BOOTCOMPLETETYPE)
        {
            Serial.println(F("Sensor connected"));
        }
        else
        {
            Serial.print(F("BootComplete failed, unexpected sensor message. "));
            Serial.print("Expect ");
            Serial.print((uint8_t)MessageType::BOOTCOMPLETETYPE);
            Serial.print(", but get ");
            Serial.println((uint8_t)msg->type);
            ok = false;
        }
    }
    zforce.DestroyMessage(msg);

    if (ok)
    {
        zforce.Enable(true);                    // Send and get Enable message
        timeout = 500;                          // Timeout is set to 500ms

        do
        {
            msg = zforce.GetMessage();
            delay(1);
        } while (msg == NULL && --timeout >= 0); // Wait 500ms for the boot complete message

        if(timeout <= 0)
        {
            Serial.println("Timeout during enable");
            ok = false;
        }
        else if (msg->type == MessageType::ENABLETYPE)
        {
            Serial.println("Received enable response");
        }
        else
        {
            Serial.print("Received unknown response, expected: ");
            Serial.println((uint8_t)MessageType::ENABLETYPE);
            Serial.print("Got: ");
            Serial.println((uint8_t)msg->type);
            ok = false;
        }
        
        zforce.DestroyMessage(msg);
    }

    newTouchDataFlag = false;               // clear new data flag

    // enable interrupt
    attachInterrupt(digitalPinToInterrupt(PIN_NN_DR), dataReadyISR, RISING);

    Serial.print(F("Initialization took "));
    Serial.print(millis() - globalMillis);
    Serial.println("ms");

    return ok;
}

/*
 * Mouse event service to convert touch event to mouse event
 */
void loopMouse()
{
    if(tp.loc.x > keyboardBoundary)     // outside mouse area, do nothing
        return;

    if (tp.state == 0)                  // touch "down", buffer the location
    {
        previousLoc.x = initialLoc.x = tp.loc.x;
        previousLoc.y = initialLoc.y = tp.loc.y;
        globalMillis = millis();
    }
    else
    {
        if (tp.state == 1)              // touch "move"
        {
            if (tp.state == 1 && tp.loc.x <= keyboardBoundary)  // within mouse area
            {
                if((millis() - globalMillis) >= holdTime)   // not a "left click"
                    Mouse.move((tp.loc.x - previousLoc.x) / 1, (tp.loc.y - previousLoc.y) / 1);
                previousLoc.x = tp.loc.x;
                previousLoc.y = tp.loc.y;
            }
        }
        else if (tp.state == 2)         // touch "up"
        {
            if(millis() - globalMillis < holdTime)  // mouse "left click", sensitivity 
                                                    // can be tuned by changing "holdTime"
            {
                Mouse.click(MOUSE_LEFT);
            }
            tp.state = 0xFF;            // reset state machine
        }
    }
}

/*
 * Keyboard event service to convert touch event to keybard event
 */
void loopKeyboard()
{
    int buttonState = tp.state < 2;     // touch "down" or "move" represents keyboard state LOW
                                        // and touch "up" represents keyboard state HIGH

    if ((buttonState != previousButtonState) && (buttonState == HIGH))  // if the button state has changed, 
                                                                        // and it's currently pressed
    {   
        if (tp.loc.x > keyboardBoundary) // below conditions can be changed to fit your purpose
        {
            if (tp.loc.y < 250)
                Keyboard.print('A');
            else if (tp.loc.y < 500)
                Keyboard.print('B');
            else if (tp.loc.y < 750)
                Keyboard.print('C');
            else if (tp.loc.y < 1000)
                Keyboard.print('D');
            else
                Keyboard.print('E');
        }
        else
        {
            // May do something to catch the rest of the cases
        }
    }
    previousButtonState = buttonState;  // save the current button state for comparison next time
}

/*
 * Get touch events
 * @return 0 if no touch event is present, -1 if message is not touch message, otherwise the number of touches received
 */
uint8_t updateTouch()
{
    if (newTouchDataFlag == false)
        return 0;

    newTouchDataFlag = false;
    Message *touch = zforce.GetMessage();
    if (touch != NULL)
    {
        if (touch->type == MessageType::TOUCHTYPE)
        {
            auto size = ((TouchMessage *)touch)->touchCount;
            nTouches = size >= TOUCH_BUFFER_SIZE ? TOUCH_BUFFER_SIZE : size;
            for (uint8_t i = 0; i < nTouches; i++)
            {
                tp.loc.x = ((TouchMessage *)touch)->touchData[0].x;
                tp.loc.y = ((TouchMessage *)touch)->touchData[0].y;
                tp.state = ((TouchMessage *)touch)->touchData[0].event;
            }
            zforce.DestroyMessage(touch);
            return nTouches;
        }
        zforce.DestroyMessage(touch);
    }
    return -1;
}

void setup()
{
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    Serial.begin(115200);       // start serial port, this is optional and is 
                                // NOT required to run mouse and keybaord
                                // services, comment out to skip
    
    #ifdef WAIT_SERIAL_PORT_CONNECTION
    while (!Serial)             // 10Hz blink to indicate USB connection is waiting to establish
    {
        digitalWrite(LED_BUILTIN, LOW);
        delay(50);
        digitalWrite(LED_BUILTIN, HIGH);
        delay(50);
    }
    digitalWrite(LED_BUILTIN, HIGH);
    #endif
    
    Keyboard.begin();           // initialize keyboard
    Mouse.begin();              // initialize mouse
    if(!touchInit())
        Serial.println(F("Touch init failed"));
}

void loop()
{
    digitalWrite(LED_BUILTIN, LOW); // blink when new touch event is received
    updateTouch();
    digitalWrite(LED_BUILTIN, HIGH);
    loopKeyboard();
    loopMouse();
}
