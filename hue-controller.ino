// Project: hue-controller
// Author: fussel132
// Date: 2022-11-19
// File: hue-controller.ino
// Version: 1.0.2
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

const char *ssid = "SSID";                          // Set before Upload!
const char *password = "Password";                  // Set before Upload!
const char *hostname = "hue-controller";
const String bridgeIP = "hue.bridge.ip.address";    // Set before Upload!
const String authKey = "huebridge authkey";         // Set before Upload!
const int groupID = 1;                              // Set before Upload!
const int defaultTransitTime = 15; // Used if animation is turned off, otherwise for the first scene
const int animatedTransitTime = 30;
const String startupScene1 = "sceneid1";            // Set before Upload!
const String startupScene2 = "sceneid2";            // Set before Upload!

unsigned long previousMillis = 0;
const long interval = 500;
bool motion = false;
bool animation = true;                              // Change this to false if you wish animations to be disabled upon boot
bool buttonPressed = false;
bool ledOn = false;
bool linkLedOn = false;
int elapsed = 0;

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
  Serial.println("Response code: " + String(httpCode));
  if (httpCode > 0)
  {
    payload = http.getString();
  }
  http.end();
  Serial.println("Response: " + payload);
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

void setup()
{
  delay(1000);
  pinMode(LED_LINK, OUTPUT);
  pinMode(LED_MOTION, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(BUTTON, INPUT);
  pinMode(RELAIS, INPUT);
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  WiFi.setHostname(hostname);
  Serial.println("Connecting to " + String(ssid));
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
  Serial.print("Connected to " + String(ssid) + ", IP address: ");
  Serial.print(WiFi.localIP());
  Serial.print(", Hostname: ");
  Serial.println(WiFi.getHostname());
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
    if (digitalRead(RELAIS) == HIGH && !motion)
    {
      motion = true;
      Serial.print("Motion detected. Turning on lights with");
      Serial.println(animation ? " animation." : "out animation.");
      // Switch on lights
      // From where will the hue system transit when turning on? From black to scene1 or from the last "on" setting (bright flash??)
      if (animation)
      {
        setGroupScene(defaultTransitTime, startupScene1);
        // Delay? Nope
        setGroupScene(animatedTransitTime, startupScene2);
      }
      else
      {
        setGroupScene(defaultTransitTime, startupScene2);
      }
      // Blink LED for 5 seconds
      long milli = millis();
      long elapsed = millis();
      while ((milli - elapsed) < (animation ? 5000 : 2000))
      {
        blinkLed(LED_MOTION, interval);
        milli = millis();
      }
      digitalWrite(LED_MOTION, HIGH);
    }
    else if (digitalRead(RELAIS) == LOW && motion)
    {
      motion = false;
      Serial.println("Motion stopped. Turning off lights.");
      // Switch off lights
      powerOff(defaultTransitTime);
      // Blink LED for 2 seconds
      long milli = millis();
      long elapsed = millis();
      while ((milli - elapsed) < 2000)
      {
        blinkLed(LED_MOTION, interval);
        milli = millis();
      }
      digitalWrite(LED_MOTION, LOW);
    }
  }

  // Check if button is pressed and detect whether animation should be played or not
  if (digitalRead(BUTTON) == HIGH && !buttonPressed)
  {
    buttonPressed = true;
    if (animation)
    {
      animation = false;
      digitalWrite(LED_RED, HIGH);
      Serial.println("Animation disabled.");
    }
    else
    {
      animation = true;
      digitalWrite(LED_RED, LOW);
      Serial.println("Animation enabled.");
    }
  }
  else if (digitalRead(BUTTON) == LOW && buttonPressed)
  {
    buttonPressed = false;
  }
}