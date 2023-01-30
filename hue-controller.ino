// Project: hue-controller
// Author: fussel132
// Date: 2022-11-19
// File: hue-controller.ino
// Version: 1.1.0
// Description: This is a simple ESP8266 based controller for Philips Hue Lights
//              It connects to the WiFi and sends HTTP/PUT requests to the Philips Hue Bridge
//              The lamps will be triggered if pin D6 will be set high by a relais or whatever
//              A blue LED indicates the connection state with the WiFi (blinking = connecting, steady = connected)
//              A red LED indicates the animation state (on = animation is off, off = animation is on)
//              A yellow LED indicates the motion state (steady = input (idle), blinking = input (controlling lamps), off = no input)
//              A button can be used to toggle the animation state
//              Debug output will be print with a baud rate of 115200

#include "ESP8266HTTPClient.h"
#include "ESP8266WiFi.h"
#include "ArduinoJson.h"

#define LED_LINK D2
#define LED_MOTION D3
#define LED_RED D5
#define BUTTON D1
#define RELAIS D6

const char *ssid = "SSID";         // Set before Upload!
const char *password = "Password"; // Set before Upload!
const char *hostname = "hue-controller";
const String bridgeIP = "hue.bridge.ip.address"; // Set before Upload!
const String authKey = "huebridge authkey";      // Set before Upload!
const int groupID = 1;                           // Set before Upload!
// Philips Hue Transitiontime is 1/10s while the Arduino Delay is 1/1000s. Here the Hue times are used
const int defaultTransitTime = 10; // Used for power off and if animation is turned off (otherwise using the below values)
const int firstSceneTransitTime = 2;
const int secondSceneTransitTime = 40;
const String startupScene1 = "sceneid1"; // Set before Upload!
const String startupScene2 = "sceneid2"; // Set before Upload!
/*
In theory, pull down resistors should prevent false signals.
But if installed for example in an electrical box, interference signals might be strong enough to trigger certain events.
To prevent false signals, adjust these delays (in ms).
*/
const int powerOffDelay = 1000; // Turn lamps off only if the input is still low after this delay
const int buttonDelay = 200;    // Ignore button presses if the button is pressed shorter than this delay
// Default behavior
bool animation = true; // Change this to false if you wish animations to be disabled upon boot

// Variables needed to run, do not change
unsigned long previousMillis = 0;
unsigned long beginButtonPress;
unsigned long lostMotionTime;
const long interval = 500;
bool motion = false;
bool buttonPressed = false;
bool ledOn = false;
bool linkLedOn = false;
bool modeChanged = false;
bool lostMotion = false;
int elapsed = 0;
String input = "";

WiFiClient client;
DynamicJsonDocument doc(1024);

void updateGroup(String body)
{
  httpPUT("http://" + bridgeIP + "/api/" + authKey + "/groups/" + String(groupID) + "/action", body);
}

void setGroupScene(int transitionTime, String sceneID)
{
  updateGroup("{\"on\":true, \"transitiontime\":" + String(transitionTime) + ", \"scene\":\"" + sceneID + "\"}");
}

void powerOff(int transitionTime)
{
  updateGroup("{\"on\":false, \"transitiontime\":" + String(transitionTime) + "}");
}

String httpPUT(String url, String body)
{
  HTTPClient http;
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");
  int httpCode = http.PUT(body);
  String payload = "{}";
  if (httpCode > 0)
  {
    payload = http.getString();
  }
  http.end();
  return payload;
}

void blinkLed(uint8_t led, long interval)
{
  unsigned long currentMillis = millis();
  if (led == LED_LINK && linkLedOn)
  {
    linkLedOn = false;
  }
  if (currentMillis - previousMillis >= interval)
  {
    previousMillis = currentMillis;
    if (ledOn)
    {
      ledOn = false;
      digitalWrite(led, LOW);
    }
    else
    {
      ledOn = true;
      digitalWrite(led, HIGH);
    }
  }
  delay(150); // Without this delay the ESP will crash after ~3s because who knows
}

void blinkLedFor(uint8_t led, long interval, long duration)
{
  long milli = millis();
  long elapsed = millis();
  while ((milli - elapsed) < duration)
  {
    blinkLed(led, interval);
    milli = millis();
  }
}

void setup()
{
  delay(1000);
  pinMode(LED_LINK, OUTPUT);
  pinMode(LED_MOTION, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(BUTTON, INPUT);
  pinMode(RELAIS, INPUT);
  WiFi.begin(ssid, password);
  WiFi.setHostname(hostname);
  while (WiFi.status() != WL_CONNECTED)
  {
    blinkLed(LED_LINK, interval);
  }
  digitalWrite(LED_LINK, HIGH);
  if (animation)
  {
    digitalWrite(LED_RED, LOW);
  }
  else
  {
    digitalWrite(LED_RED, HIGH);
  }
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
}

void loop()
{
  // If ESP is not connected to WiFi, blink the Link LED
  if (WiFi.status() != WL_CONNECTED)
  {
    blinkLed(LED_LINK, interval);
  }
  else
  {
    // Turn Link LED On if it is off
    if (!linkLedOn)
    {
      linkLedOn = true;
      digitalWrite(LED_LINK, HIGH);
    }

    // When the Relais is switched on, turn on the motion LED and Philips Hue Lights On / Off while respecting the animation flag
    if (digitalRead(RELAIS) == HIGH)
    {
      if (lostMotion)
      {
        unsigned long motionDetectedAgain = millis();
        if (lostMotionTime + powerOffDelay > motionDetectedAgain)
        {
          // False trigger, ignoring...
        }
        lostMotion = false;
      }
      if (!motion)
      {
        motion = true;
        // Switch on lights
        if (animation)
        {
          setGroupScene(firstSceneTransitTime, startupScene1);
          delay(100); // Just for safety :)
          setGroupScene(secondSceneTransitTime, startupScene2);
          blinkLedFor(LED_MOTION, interval, secondSceneTransitTime * 100);
        }
        else
        {
          setGroupScene(defaultTransitTime, startupScene2);
          blinkLedFor(LED_MOTION, interval, defaultTransitTime * 100);
        }
        digitalWrite(LED_MOTION, HIGH);
      }
    }
    else if (digitalRead(RELAIS) == LOW)
    {
      if (!lostMotion)
      {
        // Called once after motion stopped to save the time
        lostMotion = true;
        lostMotionTime = millis();
      }
      // Called every loop
      if (lostMotionTime + powerOffDelay < millis() && motion)
      {
        motion = false;
        // Switch off lights
        powerOff(defaultTransitTime);
        blinkLedFor(LED_MOTION, interval, defaultTransitTime * 100);
        digitalWrite(LED_MOTION, LOW);
      }
    }
  }

  if (digitalRead(BUTTON) == HIGH)
  {
    if (!buttonPressed)
    {
      // Called once after button press to save the time
      buttonPressed = true;
      beginButtonPress = millis();
    }
    // Called every loop
    if (beginButtonPress + buttonDelay < millis() && !modeChanged)
    {
      if (animation)
      {
        animation = false;
        digitalWrite(LED_RED, HIGH);
      }
      else
      {
        animation = true;
        digitalWrite(LED_RED, LOW);
      }
      modeChanged = true;
    }
  }
  else if (digitalRead(BUTTON) == LOW && buttonPressed)
  {
    // Called once after button release
    buttonPressed = false;
    if (!modeChanged)
    {
      // Button was pressed for less than buttonDelay (false trigger)
    }
    modeChanged = false;
  }
}