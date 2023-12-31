/**************************************************************************
Home Automation Project
   Valve Control for central heating 

  Hardware: 
  ESP Node MCU 0.1
 
  LMxx to amplify 0-5 V DC to 0-10 V DC
  
  Test RGB Light sensor output values to serial debug

  History: master if not shown otherwise
  20231231  V0.1:  Blink Test

  
*/
#include <Arduino.h>
#define DEBUG

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



void setup() {
  //run once:
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LEDON, OUTPUT);
  digitalWrite(LEDON, HIGH); //switch on white LED
}


void loop() {
  digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)

  delay(100);                       // wait for a second
  digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW

  delay(500);                       // wait for a second
}

