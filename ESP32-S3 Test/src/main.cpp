#include <Arduino.h>

// put function declarations here:
int myFunction(int, int);

void setup() {
    Serial.begin(56000);
    esp_sleep_enable_timer_wakeup(10e+6);
    Serial.println("Setup");
    delay(500);
    Serial.println("in setup routine");

  
}

void loop() {
Serial.println("Loop Start");

neopixelWrite(48,RGB_BRIGHTNESS,0,0); // Red
delay(1000);
neopixelWrite(48,0,200,0);
delay(1000);
neopixelWrite(48,0,0,255);
delay(1000);
neopixelWrite(48,0,0,0);
esp_deep_sleep_start();


 
}

