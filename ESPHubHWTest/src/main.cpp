/*
  Home Automation Project
  Hardware Test

  Hardware:
  ESPNow HUB + CTRL Board

  Function:
  Test Hardware PCB and extention PCB

  History:
  20220822  V0.1:   Copy from Adruino Examples Blink Sketch

*/
#include <Arduino.h>
#define DEBUG
#include <SoftwareSerial.h>
#include "PCF8574.h"

/* I2C Bus */
// if you use ESP8266-01 with not default SDA and SCL pins, define these 2 lines, else delete them
//  use Pin Numbers from GPIO e.g. GPIO4 = 4
//  For NodeMCU Lua Lolin V3: D1=GPIO5 = SCL D2=GPIO4 = SDA set to SDA_PIN 4, SCL_PIN 5, D7=13
#define SDA_PIN 4
#define SCL_PIN 5

/***************************
 * PCF857XC I2C to parallel
 **************************/
int iPCF857XAdr = 0x20;
bool LedOn = false;
bool bKeyPressed = false;
PCF8574 pcf857X(iPCF857XAdr);

/***************************
 * Ctrl PCB Settings
 **************************/
#define LEDGB P6
#define LEDGN P5
#define LEDBL P7
#define LEDRT P4
#define SWGB P0
#define SWGN P1
#define SWBL P2
#define SWRT P3

// Forward declarations
void ledOn(uint8_t LedNr);
void ledOff(uint8_t LedNr);
int ledNr = 0;

void setup()
{
  // put your setup code here, to run once:
  // run once:
  pinMode(LED_BUILTIN, OUTPUT);
  // initialize pcf8574
  pcf857X.begin();
  pcf857X.pinMode(SWGB, INPUT);
  pcf857X.pinMode(SWGN, INPUT);
  pcf857X.pinMode(SWRT, INPUT);
  pcf857X.pinMode(SWBL, INPUT);
  pcf857X.pinMode(LEDRT, OUTPUT);
  pcf857X.pinMode(LEDGN, OUTPUT);
  pcf857X.pinMode(LEDGB, OUTPUT);
  pcf857X.pinMode(LEDBL, OUTPUT);

  // LED Test
  ledOn(LEDRT);
  ledOn(LEDGN);
  ledOn(LEDGB);
  ledOn(LEDBL);
  delay(2000);
  ledOff(LEDRT);
  ledOff(LEDGN);
  ledOff(LEDGB);
  ledOff(LEDBL);
}

void loop()
{
  // put your main code here, to run repeatedly:
  digitalWrite(LED_BUILTIN, HIGH); // turn the LED on (HIGH is the voltage level)
  delay(500);                      // wait for a second
  digitalWrite(LED_BUILTIN, LOW);  // turn the LED off by making the voltage LOW
  delay(1000);                     // wait for a second
  switch (ledNr)
  {
  case 0:
    ledOn(LEDBL);
    ledOff(LEDRT);
    break;
  case 1:
    ledOn(LEDGB);
    ledOff(LEDBL);
    break;
  case 2:
    ledOn(LEDGN);
    ledOff(LEDGB);
    break;
  case 3:
    ledOn(LEDRT);
    ledOff(LEDGN);
    break;

  default:
    break;
  }
  ledNr++;
  if (ledNr > 3)
    ledNr = 0;
  delay(500);
}

// function LED on off
void ledOn(uint8_t LedNr)
{
  pcf857X.digitalWrite(LedNr, LOW);
}
void ledOff(uint8_t LedNr)
{
  pcf857X.digitalWrite(LedNr, HIGH);
}