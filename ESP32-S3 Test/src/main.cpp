#include <Arduino.h>

// put function declarations here:
int myFunction(int, int);
void print_wakeup_reason();

void setup() {
    Serial.begin(56000);
      
    Serial.println("Setup");
    delay(500);
    Serial.println("in setup routine");
    print_wakeup_reason();
    esp_sleep_enable_timer_wakeup(10e+6);
    //esp_deep_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);

  
}

void loop() {
Serial.println("Loop Start");

neopixelWrite(48,RGB_BRIGHTNESS,0,0); // Red
delay(5000);
//neopixelWrite(48,0,50,0);
//delay(1000);
neopixelWrite(50,50,0,50);
delay(5000);
//neopixelWrite(0,50,0,50);
Serial.flush();
esp_deep_sleep_start();


 
}

void print_wakeup_reason(){
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}

