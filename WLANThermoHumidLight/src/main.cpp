/**************************************************************************
Home Automation Project
   Valve Control for central heating 

  Hardware: 
  ESP Node MCU 0.1
 
  LMxx to amplify 0-5 V DC to 0-10 V DC
  
  Test RGB Light sensor output values to serial debug

  History: master if not shown otherwise
  20231231  V0.1: Blink Test
  20240101  V0.2: Sensor auslesen und Serial Output

  
*/
#include <Arduino.h>
#include "Spi.h"
#include <Wire.h>
#include "Adafruit_TCS34725.h"

//TODO fix errors#include <HomeAutomationCommon.h>


// 1 means debug on 0 means off
#define DEBUG 1
const String sSoftware = "ThermoHumiLightSens V0.1";
// debug macro
#if DEBUG == 1
#define debug(x) Serial.print(x)
#define debugln(x) Serial.println(x)
#define debugv(s, v)    \
  {                     \
    Serial.print(F(s)); \
    Serial.println(v);  \
  }

#else
#define debug(x)
#define debugln(x)
#define debugv(format, ...)
#define debugarg(...)
#endif


/* I2C Bus */
//if you use ESP8266-01 with not default SDA and SCL pins, define these 2 lines, else delete them
// use Pin Numbers from GPIO e.g. GPIO4 = 4
// For NodeMCU Lua Lolin V3: D1=GPIO5 = SCL D2=GPIO4 = SDA set to SDA_PIN 4, SCL_PIN 5, D7=13
#define SDA_PIN 4
#define SCL_PIN 5

/***************************
 * Pin Settings
 **************************/
#define LEDON D7

/***************************
 * TCS34725
 **************************/
/* Initialise with specific int time and gain values */
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_614MS, TCS34725_GAIN_1X);

// forward declarations

void setup() {
  //TODO: uncomment once includfiles are set Serial.begin(swBAUD_RATE);
  Serial.begin(56000 );

  Serial.println(sSoftware);

  
  pinMode(LED_BUILTIN, OUTPUT);
 // pinMode(LEDON, OUTPUT);

    if (tcs.begin()) {
    Serial.println("Found sensor");
  } else {
    Serial.println("No TCS34725 found ... check your connections");
    while (1);
  }



 // digitalWrite(LEDON, HIGH); //switch on white LED
}


void loop() {
  digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)

  delay(100);                       // wait for a second
  digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
  uint16_t r, g, b, c, colorTemp, lux;

 tcs.getRawData(&r, &g, &b, &c);
 colorTemp = tcs.calculateColorTemperature(r, g, b);
 colorTemp = tcs.calculateColorTemperature_dn40(r, g, b, c);
 lux = tcs.calculateLux(r, g, b);

  Serial.print("Color Temp: "); Serial.print(colorTemp, DEC); Serial.print(" K - ");
  Serial.print("Lux: "); Serial.print(lux, DEC); Serial.print(" - ");
  Serial.print("R: "); Serial.print(r, DEC); Serial.print(" ");
  Serial.print("G: "); Serial.print(g, DEC); Serial.print(" ");
  Serial.print("B: "); Serial.print(b, DEC); Serial.print(" ");
  Serial.print("C: "); Serial.print(c, DEC); Serial.print(" ");
  Serial.println(" ");

  delay(500);                       // wait for a second
}

