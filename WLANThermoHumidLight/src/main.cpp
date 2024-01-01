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

const String sSoftware = "ThermoHumiLightSens V0.3";
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

// Datenstruktur für den Datenaustausch
// TODO: change to ESPNOW_DATA_STRUCTURE from HomeAutomationCommon.h
/* struct DATEN_STRUKTUR
{
  int chan = CHAN;
  float temp1 = -127; //Aussen A
  float temp2 = -127; //Aussen B
  float fHumidity = -1;
  float fVoltage = -1; //Batterie Sensor
}; */

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
// Forward declarations
uint32_t calculateCRC32(const uint8_t *data, size_t length);
void UpdateRtcMemory();
void ScanForSlave();

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
  // Wir lesen aus dem RTC Memory
  ESP.rtcUserMemoryRead(0, (uint32_t *)&statinfo, sizeof(statinfo));

  uint32_t crcOfData = calculateCRC32(((uint8_t *)&statinfo) + 4, sizeof(statinfo) - 4);
  WiFi.mode(WIFI_STA); // Station mode for esp-now sensor node

  if (statinfo.crc32 != crcOfData)
  { // wir haben keine gültige MAC Adresse

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
  // Peer Daten initialisieren
  esp_now_add_peer(statinfo.mac, ESP_NOW_ROLE_SLAVE, WIFI_CHANNEL, NULL, 0);
  // wir registrieren die Funktion, die nach dem Senden aufgerufen werden soll
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
  // Flag auf false setzen
  callbackCalled = false;
  //------------------------------------------------------------------------------------------WIFI end
  ESPNOW_DATA_STRUCTURE data;
  data.fESPNowTempA = 99;

  // Batterie messen
  // data.fVoltage = fVoltage;

  uint8_t bs[sizeof(data)];
  // Datenstruktur in den Sendebuffer kopieren
  memcpy(bs, &data, sizeof(data));
  // Daten an Thermometer senden
  esp_now_send(NULL, bs, sizeof(data)); // NULL means send to all peers
}

void loop()
{

  digitalWrite(LED_BUILTIN, HIGH); // turn the LED on (HIGH is the voltage level)

  delay(100);                     // wait for a second
  digitalWrite(LED_BUILTIN, LOW); // turn the LED off by making the voltage LOW
  uint16_t r, g, b, c, colorTemp, lux;

  tcs.getRawData(&r, &g, &b, &c);
  colorTemp = tcs.calculateColorTemperature(r, g, b);
  colorTemp = tcs.calculateColorTemperature_dn40(r, g, b, c);
  lux = tcs.calculateLux(r, g, b);

  Serial.print("Color Temp: ");
  Serial.print(colorTemp, DEC);
  Serial.print(" K - ");
  Serial.print("Lux: ");
  Serial.print(lux, DEC);
  Serial.print(" - ");
  Serial.print("R: ");
  Serial.print(r, DEC);
  Serial.print(" ");
  Serial.print("G: ");
  Serial.print(g, DEC);
  Serial.print(" ");
  Serial.print("B: ");
  Serial.print(b, DEC);
  Serial.print(" ");
  Serial.print("C: ");
  Serial.print(c, DEC);
  Serial.print(" ");
  Serial.println(" ");

  delay(500); // wait for a second
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
