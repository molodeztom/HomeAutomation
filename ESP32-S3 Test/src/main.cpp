/**************************************************************************
ESP32 Test Project


  Hardware:
  ESP32-S3-DevKitC-1 mit Wroom N16R8

  Try different methods for deepsleep and wakeup

  History: master if not shown otherwise
  20240317  V0.1: Wakeup with timer and count boots in RTC memory




*/
#include <Arduino.h>

#define BUTTON_PIN_BITMASK 0x8000

const String sSoftware = "ESP32 DeepSleep Tests V0.2";

RTC_DATA_ATTR int bootCount = 0;

// put function declarations here:
int myFunction(int, int);
void print_wakeup_reason();

void setup()
{
  Serial.begin(56000);
  Serial.flush();
  delay(2000);
  Serial.println();
  Serial.println(sSoftware);
  ++bootCount;
  delay(500);
  Serial.println("in setup routine");
  Serial.println("Boot Nr.: " + String(bootCount));
  print_wakeup_reason();
  esp_sleep_enable_timer_wakeup(10e+5);
  /*
    First we configure the wake up source
    We set our ESP32 to wake up for an external trigger.
    There are two types for ESP32, ext0 and ext1 .
    ext0 uses RTC_IO to wakeup thus requires RTC peripherals
    to be on while ext1 uses RTC Controller so doesnt need
    peripherals to be powered on.
    Note that using internal pullups/pulldowns also requires
    RTC peripherals to be turned on.
  */
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_15, 1); // 1 = High, 0 = Low
  /*   esp_sleep_enable_ext1_wakeup(BUTTON_PIN_BITMASK, ESP_EXT1_WAKEUP_ANY_HIGH);
    gpio_pullup_dis(GPIO_NUM_15);
    gpio_pulldown_dis(GPIO_NUM_15);
   */
}

void loop()
{
  Serial.println("Loop Start");
  neopixelWrite(RGB_BUILTIN, 0, 60, 0); // Green

  delay(2000);

  neopixelWrite(RGB_BUILTIN, 0, 0, 0); // Red
  delay(1000);

  Serial.println("Loop End");
  Serial.flush();
  esp_deep_sleep_start();
}

void print_wakeup_reason()
{
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch (wakeup_reason)
  {
  case ESP_SLEEP_WAKEUP_EXT0:
    Serial.println("Wakeup caused by external signal using RTC_IO");
    break;
  case ESP_SLEEP_WAKEUP_EXT1:
    Serial.println("Wakeup caused by external signal using RTC_CNTL");
    break;
  case ESP_SLEEP_WAKEUP_TIMER:
    Serial.println("Wakeup caused by timer");
    break;
  case ESP_SLEEP_WAKEUP_TOUCHPAD:
    Serial.println("Wakeup caused by touchpad");
    break;
  case ESP_SLEEP_WAKEUP_ULP:
    Serial.println("Wakeup caused by ULP program");
    break;
  default:
    Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason);
    break;
  }
}
