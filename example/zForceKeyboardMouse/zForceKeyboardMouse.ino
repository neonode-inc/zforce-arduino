/*  Neonode zForce v7 interface library for Arduino

    This example code is distributed freely.
    This is an exception from the rest of the library that is released
    under GNU Lesser General Public License.

    The purpose of this example code is to demonstrate parts of the 
    library's functionality and capabilities. It is free to use, copy
    and edit without restrictions.

*/

#include <Zforce.h>
#include <Keyboard.h>
#include <Mouse.h>

// IMPORTANT: change "1" to assigned GPIO digital pin for dataReady signal in your setup:
#define DATA_READY 1

long globalMillis = millis();         // global timestamp
const int keyboardBoundary = 750;     // set boundary between mouse and keyboard areas on the x-axis to 75 mm.
const int holdTime = 150;             // sensitivity for mouse "left-click", unit in milli-second

TouchData previousTouch;

void setup()
{
  Keyboard.begin();           // initialize keyboard
  Mouse.begin();              // initialize mouse
  Serial.begin(115200);
  Serial.println("zforce start");
  zforce.Start(DATA_READY);

  Message* msg = nullptr;
  
  zforce.Enable(true);

  msg = zforce.GetMessage();

  do
  {
    msg = zforce.GetMessage();
  } while (msg == nullptr);

  if (msg->type == MessageType::ENABLETYPE)
  {
    Serial.print("Message type is: ");
    Serial.println((int)msg->type);
    Serial.println("Sensor is now enabled and will report touches.");
  }

  zforce.DestroyMessage(msg);
}

void loop()
{
  Message* touch = zforce.GetMessage();

  if (touch != nullptr)
  {
    if (touch->type == MessageType::TOUCHTYPE)
    {
      //Sends reported touch coordinates [x,y] and event data to mouse and keyboard function.
      loopMouse(((TouchMessage*)touch)->touchData[0].x, ((TouchMessage*)touch)->touchData[0].y, ((TouchMessage*)touch)->touchData[0].event);
      loopKeyboard(((TouchMessage*)touch)->touchData[0].x, ((TouchMessage*)touch)->touchData[0].y, ((TouchMessage*)touch)->touchData[0].event);
    }
    zforce.DestroyMessage(touch);
  }
}

void loopMouse(int16_t x , int16_t y, int8_t event)
{
  if (x <= keyboardBoundary) //return if the touch object is outside mouse area
  {
    return;
  }

  switch (event)
  {
    case 0:  // DOWN event
      previousTouch.x =  x;
      previousTouch.y =  y;
      globalMillis = millis();
      Serial.println("Mouse Input - DOWN");
      break;

    case 1: // MOVE event
      if ((millis() - globalMillis) >= holdTime)
      {
        Mouse.move((x - previousTouch.x), (y - previousTouch.y));
        Serial.println("Mouse Input - Moving cursor");
      }
      previousTouch.x = x;
      previousTouch.y = y;
      break;

    case 2: // UP event
      Serial.print("Mouse Input - UP");
      if (millis() - globalMillis < holdTime)  // mouse "left click", sensitivity
      { // can be tuned by changing "holdTime"
        Mouse.click(MOUSE_LEFT);
        Serial.print("(Left-Click)");
      }
      Serial.println("");
      break;

    default:
      break;
  }
}

void loopKeyboard(int16_t x , int16_t y, int8_t event)
{
  if (x > keyboardBoundary) //return if the touch object is outside keyboard area
  {
    return; 
  }

  // Only act on event == DOWN, i.e. when an object has entered the touch area
  if (event == 0)
  {
    //assign Key to the given interval
    if (y < 250)
    {
    Keyboard.print('A'); //Print Key "A"
    Serial.println("Keyboard Input - Button Press 'A'");
    }
    else if (y < 500)
    {
    Keyboard.print('B'); //Print Key "B"
    Serial.println("Keyboard Input - Button Press 'B'");
    }
    else if (y < 750)
    {
    Keyboard.print('C'); //Print Key "C"
    Serial.println("Keyboard Input - Button Press 'C'");
    }
    else if (y < 1000)
    {
    Keyboard.print('D'); //Print Key "D"
    Serial.println("Keyboard Input - Button Press 'D'");
    }
    else
    {
    Keyboard.print('E'); //Print Key "E"
    Serial.println("Keyboard Input - Button Press 'E'");
    }
  }
}
