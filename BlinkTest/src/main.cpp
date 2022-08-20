/*
  Home Automation Project
  Hardware Test Blink

  Hardware: 
  ESP Board, all TFR PCBs with ESP

  Function:
  Simple Blink ESP internal LED

  History:
  202220820   V0.1:   Copy from Adruino Examples Blink Sketch
  
*/
#include <Arduino.h>
#define DEBUG
#include <SoftwareSerial.h>



void setup() {
  //run once:
  pinMode(LED_BUILTIN, OUTPUT);
}


void loop() {
  digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(500);                       // wait for a second
  digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
  delay(1000);                       // wait for a second
}