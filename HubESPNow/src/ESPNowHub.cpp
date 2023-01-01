/*

Hardware:
ESPNowHub V1.0 und CTRL Board V1 mit externem OLED Display und div Sensoren
Funktion:
Anzeige von div. Werten aus lokalen Sensoren auf OLED
Empfang von externen Temperaturwerten über ESP-Now
Versenden der Werte über WLAN an Thingspeak
Versenden der Werte in JSON Format an HomeServer über Serial

20220918: V0.1:   Neues Projekt aus ESPHubHW Test und WeatherStation main.cpp
20220918: V0.2:   Temperatursensor DS18B20 dazu
20220925  V0.3:   remove tempsensor it is connected to ESPWLAN instead
20220925  V0.4:   read BMP085 pressure sensor
20220925  V0.5:   read "Arbeitszimmer" sensor using ESPNow
20221121  V0.6:   remove display demo
20221218  V0.7:   remove blinking LEDS
20221218  V0.8:   read SW and switch on red led
20230101  V0.9:   Soft AP to separate function
20230101  V0.10:  display Soft AP on in OLED
20230101  V0.11:  display rest time for soft AP in OLED

 */
#include <Arduino.h>
#include <ArduinoOTA.h>
#define DEBUG
#include <ArduinoJson.h>
#include <SoftwareSerial.h>
#include "PCF8574.h"
#include "SSD1306Wire.h"
#include "images.h"

#include <ESP8266WiFi.h>

// BMP085 pressure sensor
#include <Adafruit_SPIDevice.h>
#include "Wire.h"
#include <Adafruit_BMP085.h>

// Bibliothek für ESP-Now
extern "C"
{
#include <espnow.h>
}

// common data e.g. sensor definitions
#include "D:\Projects\HomeAutomation\HomeAutomationCommon.h"

const String sSoftware = "HubESPNow V0.11";

// used to correct small deviances in sensor reading  0.xyz  x=Volt y=0.1Volt z=0.01V3
#define BATTCORR1 -0.099
#define BATTCORR2 -0.018
#define BATTCORR3 -0.018
#define BATTCORR4 -0.018
#define BATTCORR5 -0.21
#define SENSCORR1A -0.25

// struct for exchanging data over ESP NOW
struct DATEN_STRUKTUR
{
  int iSensorChannel = 99;  // default for none received
  float fESPNowTempA = -99; // Aussen A
  float fESPNowTempB = -99; // Aussen B
  float fESPNowHumi = -99;
  float fESPNowVolt = -99; // Batterie Sensor
};

// Timing seconds for various uses
long lSecondsTime = 0;
const unsigned long ulSecondsInterval = 1 * 1000UL; // time in sec

/***************************
 * ESP Now Settings
 **************************/
// SSID Open for some time. Sensors can contact AP to connect. After that time sensor use stored MAC adress
long lAPOpenTime = 0;       // Timing
int sSecondsUntilClose = 0; // counter to display how long still open
// TODO: set back to 240 sec
const unsigned long ulAPOpenInterval = 60 * 1000UL; // time in sec
bool APOpen = false;
// ESP Now
void on_receive_data(uint8_t *mac, uint8_t *r_data, uint8_t len);

/* I2C Bus */
// if you use ESP8266-01 with not default SDA and SCL pins, define these 2 lines, else delete them
//  use Pin Numbers from GPIO e.g. GPIO4 = 4
//  For NodeMCU Lua Lolin V3: D1=GPIO5 = SCL D2=GPIO4 = SDA set to SDA_PIN 4, SCL_PIN 5, D7=13
// #define SDA_PIN 4
// #define SCL_PIN 5

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
#define SW4 P0
#define SW3 P1
#define SW2 P2
#define SW1 P3

long lswitchReadTime = 0;                                // Timing
const unsigned long ulSwitchReadInterval = 0.3 * 1000UL; // Time until switch is read next time in s

/***************************
 * Display Settings
 **************************/

const int I2C_DISPLAY_ADDRESS = 0x3c;

const int SDA_PIN = D2;
const int SCL_PIN = D1;
// const int SDA_PIN = D3; //GPIO0
// const int SCL_PIN = D4; //GPIO2

// Initialize the oled display for address 0x3c
SSD1306Wire display(0x3c, SDA_PIN, SCL_PIN);
long lsUpdateDisplay = 0;                                   // Timing
const unsigned long ulUpdateDisplayInterval = 0.3 * 1000UL; // Time until display is updated next time in s

// Forward declarations
// flag changed in the ticker function every 10 minutes

void ledOn(uint8_t LedNr);
void ledOff(uint8_t LedNr);
void openWifiAP();
void updateDisplay();

/***************************
 * Begin Atmosphere and iLightLocal Sensor Settings
 **************************/
// void readLight();
void readAtmosphere();
Adafruit_BMP085 bmp;
// const int Light_ADDR = 0b0100011;                      // address:0x23
// const int Atom_ADDR = 0b1110111;                       // address:0x77
long lreadTime = 0;
const unsigned long ulSensReadInterval = 70 * 1000UL; // Timing Time to evaluate received sens values for display

int ledNr = 0;

void setup()
{
  Serial.begin(115200);
  Serial.println(sSoftware);
  pinMode(LED_BUILTIN, OUTPUT);
  // WIFI ESPNOW
  WiFi.persistent(false); // https://www.arduinoforum.de/arduino-Thread-Achtung-ESP8266-schreibt-bei-jedem-wifi-begin-in-Flash
  // Reset WiFI
  WiFi.disconnect();
  WiFi.mode(WIFI_AP);
  WiFi.begin();
  // AP for sensors to ask for station MAC adress
  // TODO activate again openWifiAP();
  delay(1000);

  if (esp_now_init() != 0)
  {
    Serial.println("Init ESP-Now failed restart");
    ESP.restart();
  }
  // ESP Role  1=Master, 2 = Slave 3 = Master + Slave
  Serial.println("Set espnow Role 2");
  esp_now_set_self_role(2);
  // callback for received ESP NOW messages
  esp_now_register_recv_cb(on_receive_data);

  // initialize Atmosphere sensor
  if (!bmp.begin())
  {
    Serial.println("Could not find BMP180 or BMP085 sensor at 0x77");
  }
  else
  {
    Serial.println("Found BMP180 or BMP085 sensor at 0x77");
  }

  // initialize pcf8574
  pcf857X.begin();
  pcf857X.pinMode(SW4, INPUT);
  pcf857X.pinMode(SW3, INPUT);
  pcf857X.pinMode(SW1, INPUT);
  pcf857X.pinMode(SW2, INPUT);
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
  // display.displayOn();
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

  // 1 sec timer for blinking and time update
  if ((millis() - lSecondsTime > ulSecondsInterval))
  {
    // counter how long AP still open
    if (APOpen == true)
    {
      sSecondsUntilClose--;
    }
    lSecondsTime = millis();
  }

  // Close Wifi AP after some time
  if ((millis() - lAPOpenTime > ulAPOpenInterval) && APOpen == true)
  {
    WiFi.softAPdisconnect();
    ledOff(LEDRT);
    APOpen = false;
#ifdef DEBUG
    Serial.println("AP disconnect");
#endif
  }

  // Read local sensors pressure and light every x seconds
  if (millis() - lreadTime > ulSensReadInterval)
  {
    Serial.println("Readlocal");
    // readLight();
    readAtmosphere();
    // formatTempExt();

    lreadTime = millis();
  }

  // Read input switches every x seconds
  if (millis() - lswitchReadTime > ulSwitchReadInterval)
  {
    if (pcf857X.digitalRead(SW4) == HIGH)
    {
      ledOn(LEDRT);
      openWifiAP();
    }

    lreadTime = millis();
  }

  // Update display every x seconds
  if (millis() - lsUpdateDisplay > ulUpdateDisplayInterval)
  {
    updateDisplay();
    lsUpdateDisplay = millis();
  }

  // Every x seconds check sensor readings, if no reading count up and then do not display
  if ((millis() - lSensorValidTime > ulSensorValidIntervall))
  {
    Serial.println("Sensor count up");
    sSensor1.iSensorCnt++;
    sSensor2.iSensorCnt++;
    sSensor3.iSensorCnt++;
    // sSensor4.iSensorCnt++;
    sSensor5.iSensorCnt++;
    lSensorValidTime = millis();
  }
}

// Callback funktion wenn Daten über ESP-Now empfangen wurden
void on_receive_data(uint8_t *mac, uint8_t *r_data, uint8_t len)
{

  DATEN_STRUKTUR sESPReceive;
  // copy received data to struct, to get access via variables
  memcpy(&sESPReceive, r_data, sizeof(sESPReceive));
  // TODO check values and send further only if correct
  // iSensorChannel = sESPReceive.iSensorChannel;
#ifdef DEBUG
  Serial.print("Channel: ");
  Serial.println(sESPReceive.iSensorChannel);
#endif

  // depending on channel
  switch (sESPReceive.iSensorChannel)
  {
  case 1:
    sSensor1.fTempA = roundf((sESPReceive.fESPNowTempA + SENSCORR1A) * 100) / 100;
    sSensor1.fHumi = roundf(sESPReceive.fESPNowHumi * 100) / 100;
    sSensor1.fVolt = roundf((sESPReceive.fESPNowVolt + BATTCORR1) * 100) / 100;
    sSensor1.bSensorRec = true;
    sSensor1.iSensorCnt = 0;

#ifdef DEBUG
    Serial.print("Temperatur 1A: ");
    Serial.println(sSensor1.fTempA);
    Serial.print("extHumidity: ");
    Serial.println(sSensor1.fHumi);
    Serial.print("Batt1 V: ");
    Serial.println(sSensor1.fVolt);
#endif
    break;
  case 2:
    sSensor2.fTempA = roundf(sESPReceive.fESPNowTempA * 100) / 100;
    sSensor2.fVolt = roundf((sESPReceive.fESPNowVolt + BATTCORR2) * 100) / 100;
    sSensor2.bSensorRec = true;
    sSensor2.iSensorCnt = 0;
#ifdef DEBUG
    Serial.println("Recieved Data 2");
    Serial.print("Temperatur 2A: ");
    Serial.println(sSensor2.fTempA);
    Serial.print("Batt2 V: ");
    Serial.println(sSensor2.fVolt);
#endif
    break;
  case 3:
    sSensor3.fTempA = roundf(sESPReceive.fESPNowTempA * 100) / 100;
    sSensor3.fVolt = roundf((sESPReceive.fESPNowVolt + BATTCORR3) * 100) / 100;
    sSensor3.bSensorRec = true;
    sSensor3.iSensorCnt = 0;
#ifdef DEBUG
    Serial.println("Recieved Data 3");
    Serial.print("Temperatur 3: ");
    Serial.println(sSensor3.fTempA);
    Serial.print("Batt3 V: ");
    Serial.println(sSensor3.fVolt);
#endif
    break;
  case 4:
    sSensor4.fTempA = roundf(sESPReceive.fESPNowTempA * 100) / 100;
    sSensor4.fVolt = roundf((sESPReceive.fESPNowVolt + BATTCORR4) * 100) / 100;
    sSensor4.fHumi = roundf(sESPReceive.fESPNowHumi * 100) / 100;
    sSensor4.bSensorRec = true;
    sSensor4.iSensorCnt = 0;
#ifdef DEBUG
    Serial.println("Recieved Data 4");
    Serial.print("Temperatur 4: ");
    Serial.println(sSensor4.fTempA);
    Serial.print("Batt4 V: ");
    Serial.println(sSensor4.fVolt);
    Serial.print("extHumidity: ");
    Serial.println(sSensor4.fHumi);
#endif

    break;
  case 5:
    sSensor5.fTempA = roundf(sESPReceive.fESPNowTempA * 100) / 100;
    sSensor5.fVolt = roundf((sESPReceive.fESPNowVolt + BATTCORR5) * 100) / 100;
    sSensor5.fHumi = roundf(sESPReceive.fESPNowHumi * 100) / 100;
    sSensor5.bSensorRec = true;
    sSensor5.iSensorCnt = 0;
#ifdef DEBUG
    Serial.println("Recieved Data 5");
    Serial.print("Temperatur 5: ");
    Serial.println(sSensor5.fTempA);
    Serial.print("Batt5 V: ");
    Serial.println(sSensor5.fVolt);
    Serial.print("extHumidity: ");
    Serial.println(sSensor5.fHumi);
#endif

    break;
  default:
    Serial.println("Default Channel do nothing");
    break;
  }
};

// function LED on off
void ledOn(uint8_t LedNr)
{
  pcf857X.digitalWrite(LedNr, LOW);
}
void ledOff(uint8_t LedNr)
{
  pcf857X.digitalWrite(LedNr, HIGH);
}

// BMP180 pressure sensor

void readAtmosphere()
{

  sSensor0.fAtmo = bmp.readPressure();
  sSensor0.fAtmo = sSensor0.fAtmo / 100;

#ifdef DEBUG
  int bmpTemp;
  bmpTemp = bmp.readTemperature();
  Serial.print("Pressure = ");
  Serial.print(sSensor0.fAtmo);
  Serial.println(" Pascal");
  Serial.print("BMP Temp = ");
  Serial.println(bmpTemp);
#endif
}

// Open WIFI AP for sensors to ask for station MAC adress
void openWifiAP()
{
  WiFi.mode(WIFI_AP);
  WiFi.begin();
  WiFi.softAP(APSSID);
  Serial.println("Start AP");
  Serial.println(APSSID);
  APOpen = true;
  sSecondsUntilClose = ulAPOpenInterval / 1000UL;
  lAPOpenTime = millis();
}

void updateDisplay()
{
  // clear the display
  display.clear(); // TODO do we need this
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  // test output millisec
  display.drawString(128, 54, String(millis()));
  // display flag for open AP
  if (APOpen == true)
  {
    display.drawString(115, 0, "AP:");
    display.drawString(128, 0, String(sSecondsUntilClose));
  }
  // write the buffer to the display
  display.display();
}