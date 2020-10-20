/*
 Current Sensor Test
 Using an INA219 to measure current
 Try sample code for a different chip
 https://github.com/rkoptev/ACS712-arduino
 Find good library for INA219
 Tests:
 Try with DC
 measure and write to debug
 Try with AC
 measure and write to debug
 try to find maximum value and debut out
History:
  20201020  V0.1: Test NodeMCU with blink
 
*/

#include <Arduino.h>
#include "Adafruit_INA219.h"

void setup() {
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
    digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(4000);                       // wait for a second
  digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
  delay(1000);                       // wait for a second
}