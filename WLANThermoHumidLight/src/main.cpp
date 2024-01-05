/**************************************************************************
Home Automation Project
   RGB Light Sensor TCS34725

  Hardware:
  ESP Node MCU 0.1

  Test RGB Light sensor output values to serial debug

  History: master if not shown otherwise
  20231231  V0.1: Blink Test
  20240101  V0.2: Sensor auslesen und Serial Output
  20240101  V0.3: ESPNow Send aus WLANThermoHumidExt hinzu neu auch HomeAutomationCommon.h verwendet
  20240102  V0.4: send all light sensor values over espnow
  20240105  V1.0: cleanup
  20240105  V1.1: send sensor capabilities



*/
#include <Arduino.h>
// 1 means debug on 0 means off
#define DEBUG 1

// Bibliotheken für WiFi
#include <ESP8266WiFi.h>

// Bibliotheken für Sensor TCS34725 und DS18B20

#include "Spi.h"
#include <Wire.h>
#include "Adafruit_TCS34725.h"

#include <HomeAutomationCommon.h>

extern "C"
{
#include <espnow.h>
}

// DELETE THIS INCLUDE YOU DO NOT NEED THIS FILE WHEN USING THE PASSWORDS BELOW
// #include "D:\Arduino\HomeAutomationSecrets.h"
/* ADD YOUR OWN VALUES IF YOU WANT TO USE THIS PROJEKT****************************************************************************

const char *WIFI_SSID = ""
const char *WIFI_PWD = "";
//SSID provided by AP for some time. Sensor can contact and transfer MAC adress.
const char *APSSID = "";

*/

const String sSoftware = "ThermoHumiLightSens V1.0";
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

// Konstanten für WiFi
#define WIFI_CHANNEL 1
#define SEND_TIMEOUT 2450 // 245 Millisekunden timeout
#define CHAN 6            // sensor channel 1 = Developmentboard 2 = ESP gelötet
#define SLEEP_TIME 90E6   // Zeit in  uS z.B. 5 Minuten entspricht 300E6 normal 90


// Datenstruktur für das Rtc Memory mit Prüfsumme um die Gültigkeit
// zu überprüfen damit wird die MAC Adresse gespeichert
struct MEMORYDATA
{
  uint32_t crc32;
  uint8_t mac[6];
};

// Globale daten
volatile bool callbackCalled;

MEMORYDATA statinfo;

uint16_t nRed, nGreen, nBlue, nClear, nColorTemp, nLux; // sensor values

/* I2C Bus */
// if you use ESP8266-01 with not default SDA and SCL pins, define these 2 lines, else delete them
//  use Pin Numbers from GPIO e.g. GPIO4 = 4
//  For NodeMCU Lua Lolin V3: D1=GPIO5 = SCL D2=GPIO4 = SDA set to SDA_PIN 4, SCL_PIN 5, D7=13
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

uint32_t calculateCRC32(const uint8_t *data, size_t length);
void UpdateRtcMemory();
void ScanForSlave();
void MeasureLightValues();
void SendValuesESPNow();

void setup()
{
  // TODO: uncomment once includfiles are set Serial.begin(swBAUD_RATE);
  Serial.begin(56000);
#ifdef DEBUG

  delay(4000);
  Serial.println("START");
  Serial.println(sSoftware);
#endif

  pinMode(LED_BUILTIN, OUTPUT);
  // pinMode(LEDON, OUTPUT);

  if (tcs.begin())
  {
    Serial.println("Found sensor");
  }
  else
  {
    Serial.println("No TCS34725 found ... check your connections");
    while (1)
      ;
  }

  // digitalWrite(LEDON, HIGH); //switch on white LED

  //------------------------------------------------------------------------------------------WIFI begin
  // read target mac adress from rtc
  ESP.rtcUserMemoryRead(0, (uint32_t *)&statinfo, sizeof(statinfo));

  uint32_t crcOfData = calculateCRC32(((uint8_t *)&statinfo) + 4, sizeof(statinfo) - 4);
  WiFi.mode(WIFI_STA); // Station mode for esp-now sensor node

  if (statinfo.crc32 != crcOfData)
  { // mac adress not valid

    Serial.println("Scan vor Slave");
    ScanForSlave();

    Serial.printf("This mac: %s, ", WiFi.macAddress().c_str());
    Serial.printf("target mac: %02x%02x%02x%02x%02x%02x", statinfo.mac[0], statinfo.mac[1], statinfo.mac[2], statinfo.mac[3], statinfo.mac[4], statinfo.mac[5]);
    Serial.printf(", channel: %i\n", WIFI_CHANNEL);
  }
  if (esp_now_init() != 0)
  {

    Serial.println("*** ESP_Now init failed");
    ESP.restart();
  }
  // ESP Now Controller
  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
  // initialize peer data
  esp_now_add_peer(statinfo.mac, ESP_NOW_ROLE_SLAVE, WIFI_CHANNEL, NULL, 0);
  // register send callback
  esp_now_register_send_cb([](uint8_t *mac, uint8_t sendStatus)
                           {
#ifdef DEBUG
    Serial.print("send_cb, status = ");
    Serial.print(sendStatus);
    Serial.print(", to mac: ");
    char macString[50] = {0};
    sprintf(macString, "%02X:%02X:%02X:%02X:%02X:%02X", statinfo.mac[0], statinfo.mac[1], statinfo.mac[2], statinfo.mac[3], statinfo.mac[4], statinfo.mac[5]);
    Serial.println(macString);
#endif
    callbackCalled = true; });
  // remember callback already called
  callbackCalled = false;
  //------------------------------------------------------------------------------------------WIFI end
  
}

void loop()
{

  digitalWrite(LED_BUILTIN, HIGH); // turn the LED on (HIGH is the voltage level)

  delay(100);                     // wait for a second
  digitalWrite(LED_BUILTIN, LOW); // turn the LED off by making the voltage LOW
  MeasureLightValues();
  SendValuesESPNow();

  delay(20000); // wait for a second
}

// read sensor values and calculate
void MeasureLightValues()
{
  tcs.getRawData(&nRed, &nGreen, &nBlue, &nClear);
  // nColorTemp = tcs.calculateColorTemperature(nRed, nGreen, nBlue);
  nColorTemp = tcs.calculateColorTemperature_dn40(nRed, nGreen, nBlue, nClear);
  nLux = tcs.calculateLux(nRed, nGreen, nBlue);

  Serial.print("Color Temp: ");
  Serial.print(nColorTemp, DEC);
  Serial.print(" K - ");
  Serial.print("Lux: ");
  Serial.print(nLux, DEC);
  Serial.print(" - ");
  Serial.print("R: ");
  Serial.print(nRed, DEC);
  Serial.print(" ");
  Serial.print("G: ");
  Serial.print(nGreen, DEC);
  Serial.print(" ");
  Serial.print("B: ");
  Serial.print(nBlue, DEC);
  Serial.print(" ");
  Serial.print("C: ");
  Serial.print(nClear, DEC);
  Serial.print(" ");
  Serial.println(" ");
}

void SendValuesESPNow()
{
  ESPNOW_DATA_STRUCTURE data;

  data.nVersion = 1; // V1: including Light
  data.iSensorChannel = 6;
  data.sSensorCapabilities = 0;
  data.sSensorCapabilities = RGB_ON;
  // check battery voltage TODO activate on real pcb
  // data.fVoltage = fVoltage;
  data.nColorTemp = nColorTemp;
  data.nLux = nLux;
  data.nRed = nRed;
  data.nGreen = nGreen;
  data.nBlue = nBlue;
  data.nClear = nClear;
  

  uint8_t bs[sizeof(data)];
  // Datenstruktur in den Sendebuffer kopieren
  memcpy(bs, &data, sizeof(data));
  // Daten an Thermometer senden
  esp_now_send(NULL, bs, sizeof(data)); // NULL means send to all peers

}

// Unterprogramm zum Berechnen der Prüfsumme
uint32_t calculateCRC32(const uint8_t *data, size_t length)
{
  uint32_t crc = 0xffffffff;
  while (length--)
  {
    uint8_t c = *data++;
    for (uint32_t i = 0x80; i > 0; i >>= 1)
    {
      bool bit = crc & 0x80000000;
      if (c & i)
      {
        bit = !bit;
      }
      crc <<= 1;
      if (bit)
      {
        crc ^= 0x04c11db7;
      }
    }
  }
  return crc;
}

// Schreibt die Datenstruktur statinfo mit korrekter Prüfsumme in das RTC Memory
void UpdateRtcMemory()
{
  uint32_t crcOfData = calculateCRC32(((uint8_t *)&statinfo) + 4, sizeof(statinfo) - 4);
  statinfo.crc32 = crcOfData;
  ESP.rtcUserMemoryWrite(0, (uint32_t *)&statinfo, sizeof(statinfo));
}

// sucht nach einem geeigneten AccessPoint
void ScanForSlave()
{

  int8_t scanResults = WiFi.scanNetworks();
  // reset on each scan

  if (scanResults == 0)
  {
    Serial.println("No WiFi devices in AP Mode found");
  }
  else
  {
#ifdef DEBUG
    Serial.print("Found ");
    Serial.print(scanResults);
    Serial.println(" devices ");
#endif
    for (int i = 0; i < scanResults; ++i)
    {
      // Print SSID and RSSI for each device found
      String SSID = WiFi.SSID(i);
      int32_t RSSI = WiFi.RSSI(i);
      String BSSIDstr = WiFi.BSSIDstr(i);
#ifdef DEBUG
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(SSID);
      Serial.print(" (");
      Serial.print(RSSI);
      Serial.print(")");
      Serial.println("");
#endif
      delay(10);
      // Check if the current device starts with
      if (SSID.indexOf(APSSID) == 0)
      {
        // SSID of interest
#ifdef DEBUG

        Serial.println("Found a Slave.");
        Serial.print(i + 1);
        Serial.print(": ");
        Serial.print(SSID);
        Serial.print(" [");
        Serial.print(BSSIDstr);
        Serial.print("]");
        Serial.print(" (");
        Serial.print(RSSI);
        Serial.print(")");
        Serial.println("");
#endif
        int mac[6];
        // wir ermitteln die MAC Adresse und speichern sie im RTC Memory
        if (6 == sscanf(BSSIDstr.c_str(), "%x:%x:%x:%x:%x:%x%c", &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]))
        {
          for (int ii = 0; ii < 6; ++ii)
          {
            statinfo.mac[ii] = (uint8_t)mac[ii];
          }
          UpdateRtcMemory();
        }

        // Nachdem der AP gefunden wurde können wir abbrechen
        break;
      }
    }
  }
  // RAM freigeben
  WiFi.scanDelete();
}
