/*
 
Hardware: 
ESPNowHub V1.0 und CTRL Board V1 mit externem OLED Display und div Sensoren 
Funktion:
Anzeige von div. Werten aus lokalen Sensoren auf OLED
Empfang von externen Temperaturwerten über ESP-Now 
Versenden der Werte über WLAN an Thingspeak
Versenden der Werte in JSON Format an HomeServer über Serial

202209189:  V0.1: Neues Projekt aus ESPHubHW Test und WeatherStation main.cpp



 */
#include <Arduino.h>
#define DEBUG
#include <SoftwareSerial.h>
#include "PCF8574.h"
#include "Wire.h"
#include "SSD1306Wire.h"
#include "images.h"

const String sSoftware = "HubESPNow V0.1";

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

/***************************
 * Display Settings
 **************************/

const int I2C_DISPLAY_ADDRESS = 0x3c;

const int SDA_PIN = D2;
const int SCL_PIN = D1;
//const int SDA_PIN = D3; //GPIO0
//const int SCL_PIN = D4; //GPIO2

// Initialize the oled display for address 0x3c
SSD1306Wire display(0x3c,SDA_PIN,SCL_PIN);

#define DEMO_DURATION 3000
typedef void (*Demo)(void);


// Forward declarations
void ledOn(uint8_t LedNr);
void ledOff(uint8_t LedNr);
void drawFontFaceDemo();
void drawTextFlowDemo();
void drawTextAlignmentDemo();
void drawRectDemo();
void drawCircleDemo();
void drawProgressBarDemo();
void drawImageDemo() ;

Demo demos[] = {drawFontFaceDemo, drawTextFlowDemo, drawTextAlignmentDemo, drawRectDemo, drawCircleDemo, drawProgressBarDemo, drawImageDemo};
int demoMode = 0;
int counter = 1;
int demoLength = (sizeof(demos) / sizeof(Demo));
long timeSinceLastModeSwitch = 0;



int ledNr = 0;


void setup()
{
  Serial.begin(115200);
  Serial.println();
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
  

   // initialize display
  display.init();
  //display.displayOn();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
 // display.setTextAlignment(TEXT_ALIGN_CENTER);
 // display.setContrast(255);
    display.drawString(2, 10, sSoftware);
    display.display();
    delay(2500);
  Serial.println("setup end");

}

void loop()
{
  // put your main code here, to run repeatedly:
// clear the display
 
  display.clear();
 // draw the current demo method
  demos[demoMode]();

  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  display.drawString(128, 54, String(millis()));
  // write the buffer to the display
  display.display();



  if (millis() - timeSinceLastModeSwitch > DEMO_DURATION) {
    demoMode = (demoMode + 1)  % demoLength;
    timeSinceLastModeSwitch = millis();
  }
  counter++;
  delay(10);

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

void drawTextFlowDemo() {
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawStringMaxWidth(0, 0, 128,
                             "Lorem ipsum\n dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt ut labore." );
}

void drawTextAlignmentDemo() {
  // Text alignment demo
  display.setFont(ArialMT_Plain_10);

  // The coordinates define the left starting point of the text
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, 10, "Left aligned (0,10)");

  // The coordinates define the center of the text
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 22, "Center aligned (64,22)");

  // The coordinates define the right end of the text
  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  display.drawString(128, 33, "Right aligned (128,33)");
}

void drawRectDemo() {
  // Draw a pixel at given position
  for (int i = 0; i < 10; i++) {
    display.setPixel(i, i);
    display.setPixel(10 - i, i);
  }
  display.drawRect(12, 12, 20, 20);

  // Fill the rectangle
  display.fillRect(14, 14, 17, 17);

  // Draw a line horizontally
  display.drawHorizontalLine(0, 40, 20);

  // Draw a line horizontally
  display.drawVerticalLine(40, 0, 20);
}

void drawCircleDemo() {
  for (int i = 1; i < 8; i++) {
    display.setColor(WHITE);
    display.drawCircle(32, 32, i * 3);
    if (i % 2 == 0) {
      display.setColor(BLACK);
    }
    display.fillCircle(96, 32, 32 - i * 3);
  }
}

void drawProgressBarDemo() {
  int progress = (counter / 5) % 100;
  // draw the progress bar
  display.drawProgressBar(0, 32, 120, 10, progress);

  // draw the percentage as String
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 15, String(progress) + "%");
}

void drawImageDemo() {
  // see http://blog.squix.org/2015/05/esp8266-nodemcu-how-to-create-xbm.html
  // on how to create xbm files
  display.drawXbm(34, 14, WiFi_Logo_width, WiFi_Logo_height, WiFi_Logo_bits);
}
