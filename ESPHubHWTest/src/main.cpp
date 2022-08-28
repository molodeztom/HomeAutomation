/*
  Home Automation Project
  Hardware Test

  Hardware:
  ESPNow HUB + CTRL Board

  Function:
  Test Hardware PCB and extention PCB

  History:
  20220822  V0.1:   Copy from Adruino Examples Blink Sketch
  20220827  V0.2:   Control PCB running light added 
  20220827  V0.3:   OLED display test added

*/
#include <Arduino.h>
#define DEBUG
#include <SoftwareSerial.h>
//#include "PCF8574.h"
#include "Wire.h"
#include "SSD1306Wire.h"
#include "images.h"

const String sSoftware = "ESPHubHWTest V0.3";

/* I2C Bus */
// if you use ESP8266-01 with not default SDA and SCL pins, define these 2 lines, else delete them
//  use Pin Numbers from GPIO e.g. GPIO4 = 4
//  For NodeMCU Lua Lolin V3: D1=GPIO5 = SCL D2=GPIO4 = SDA set to SDA_PIN 4, SCL_PIN 5, D7=13
//#define SDA_PIN 4
//#define SCL_PIN 5

/***************************
 * PCF857XC I2C to parallel
 **************************/
int iPCF857XAdr = 0x20;
bool LedOn = false;
bool bKeyPressed = false;
//PCF8574 pcf857X(iPCF857XAdr);

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

/***************************
 * Display Settings
 **************************/

const int I2C_DISPLAY_ADDRESS = 0x3c;

const int SDA_PIN = D2;
const int SCL_PIN = D1;
//const int SDA_PIN = D3; //GPIO0
//const int SCL_PIN = D4; //GPIO2

// Initialize the oled display for address 0x3c
SSD1306Wire display(0x3c,D2,D1);
/*
#define DEMO_DURATION 3000
typedef void (*Demo)(void);

int demoMode = 0;
int counter = 1;
*/


// Forward declarations
void ledOn(uint8_t LedNr);
void ledOff(uint8_t LedNr);
void drawFontFaceDemo();

int ledNr = 0;


void setup()
{
  Serial.begin(115200);
  Serial.println();
  pinMode(LED_BUILTIN, OUTPUT);
  /*
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
  */

   // initialize display
  display.init();
  //display.displayOn();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
 // display.setTextAlignment(TEXT_ALIGN_CENTER);
 // display.setContrast(255);
    display.drawString(2, 10, "Hello world");
    display.display();
    delay(1000);
  Serial.println("setup end");

}

void loop()
{
  // put your main code here, to run repeatedly:
// clear the display
 
   display.drawString(0, 0, "Hello world");


  digitalWrite(LED_BUILTIN, HIGH); // turn the LED on (HIGH is the voltage level)
  delay(500);                      // wait for a second
  digitalWrite(LED_BUILTIN, LOW);  // turn the LED off by making the voltage LOW
  delay(1000);                     // wait for a second
  /*
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
  */


}
/*
// function LED on off
void ledOn(uint8_t LedNr)
{
  pcf857X.digitalWrite(LedNr, LOW);
}
void ledOff(uint8_t LedNr)
{
  pcf857X.digitalWrite(LedNr, HIGH);
}
*/

void drawFontFaceDemo() {
  // Font Demo1
  // create more fonts at http://oleddisplay.squix.ch/
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 0, "Hello world");
  display.setFont(ArialMT_Plain_16);
  display.drawString(0, 10, "Hello world");
  display.setFont(ArialMT_Plain_24);
  display.drawString(0, 26, "Hello world");
}