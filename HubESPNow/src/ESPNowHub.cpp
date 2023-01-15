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
20230106  V0.12:  +OTA programming interface
20230107  V0.13:  OTA now works with SW2
20230107  V0.14:  Show OTA Info in display, End OTA if not already started
20230108  V0.15:  cError in readtime for sensor BMP
20230109  V0.16:  +Debug Macros
20230110  V0.16:  +Print MAC adress when sensor data received
20230111  V0.17:  +Draw temp sens5 on display
20230115  V0.18:  cSW1-4 align with PCB description, SW1 now OTA, SW2 New sensor
20230115  V0.19:  cUse sensor array instead of designated variables

 */
#include <Arduino.h>
#include <ArduinoOTA.h>
// 1 means debug on 0 means off
#define DEBUG 1
#include <ArduinoJson.h>
#include <SoftwareSerial.h>
#include "PCF8574.h"
#include "SSD1306Wire.h"
// Include the UI lib TODO try the ui LIB
// #include "OLEDDisplayUi.h"
#include "images.h"
#include <ArduinoOTA.h>

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
const String sSoftware = "HubESPNow V0.19";
// TODO here we start with a sensor array instead of designated sensor0-x variables
SENSOR_DATA sSensor[nMaxSensors]; // SensValidMax in HomeAutomationCommon.h starts from 0 = local sensor and 1-max are the channels

// debug macro
#if DEBUG == 1
#define debug(x) Serial.print(x)
#define debugln(x) Serial.println(x)
#else
#define debug(x)
#define debugln(x)
#endif

// used to correct small deviances in sensor reading sensor 0 is local sensor  0.xyz  x=Volt y=0.1Volt z=0.01V3,
const float fBattCorr[nMaxSensors] = {0, -0.099, -0.018, -0.018, -0.018, -0.21, 0, 0, 0, 0};
const float fTempCorr[nMaxSensors] = {0, -0.25, 0, 0, 0, 0, 0, 0, 0, 0};

// struct for exchanging data over ESP NOW
struct DATEN_STRUKTUR
{
  int iSensorChannel = 99;  // default for none received
  float fESPNowTempA = -99; // Aussen A
  float fESPNowTempB = -99; // Aussen B
  float fESPNowHumi = -99;
  float fESPNowVolt = -99; // Batterie Sensor
};

// Default display if any value becomes invalid. Invalid when for some time no value received
String textSensTemp[nMaxSensors];

// Timing seconds for various uses
long lSecondsTime = 0;
const unsigned long ulSecondsInterval = 1 * 1000UL; // time in sec

// program is running on one of these modes
// normal, AP Open to add a new sensor, OTA only active to do an OTA
enum eProgramModes
{
  normal,
  aPopen,
  oTAActive
};
eProgramModes ProgramMode = normal;
bool bOTARunning = false; // true if OTA already running to prevent stop

/***************************
 * ESP Now Settings
 **************************/
// SSID Open for some time. Sensors can contact AP to connect. After that time sensor use stored MAC adress
long lAPOpenTime = 0;       // Timing
int sSecondsUntilClose = 0; // counter to display how long still open
// TODO: set back to 240 sec
const unsigned long ulAPOpenInterval = 60 * 1000UL; // time in sec

// ESP Now
void on_receive_data(uint8_t *mac, uint8_t *r_data, uint8_t len);
/***************************
 * OTA
 **************************/
#define MYHOSTNAME "HubESPNow"

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
#define SW1 P0
#define SW2 P1
#define SW3 P2
#define SW4 P3

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
void drawSens5Temp();
void formatTempExt();
void startOTA();

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
  // TODO remove or make better
  int iEspNowErr = 0;
  Serial.begin(115200);
  Serial.println(sSoftware);
  pinMode(LED_BUILTIN, OUTPUT);
  ProgramMode = normal;
  // WIFI ESPNOW
  WiFi.persistent(false); // https://www.arduinoforum.de/arduino-Thread-Achtung-ESP8266-schreibt-bei-jedem-wifi-begin-in-Flash
  // Reset WiFI
  WiFi.disconnect();
  WiFi.mode(WIFI_AP);
  WiFi.begin();
  delay(1000);

  if (esp_now_init() != 0)
  {
    debugln("Init ESP-Now failed restart");
    ESP.restart();
  }

  // ESP Role  1=Master, 2 = Slave 3 = Master + Slave
  debugln("Set espnow Role 2");
  if (esp_now_set_self_role(2) != 0)
  {
    debugln("ESP-NOw set role failed");
  };

  // callback for received ESP NOW messages
  iEspNowErr = esp_now_register_recv_cb(on_receive_data);
  debugln(iEspNowErr);

  // OTA handler
  ArduinoOTA.onStart([]()
                     {
                       debugln("OTA Start");
                       bOTARunning = true;
                       display.clear();
                       display.drawString(0, 10, "OTA Wait for Upload");
                       // write the buffer to the display
                       display.display();
                       ledOn(LEDBL); });

  ArduinoOTA.onEnd([]()
                   {
                    display.clear();
                    display.drawString(0, 10, "OTA Successful");
                    // write the buffer to the display
                    display.display();
                    ledOff(LEDBL);
                    delay(2000);
                    bOTARunning = false;
                  
                    debugln("OTA End"); });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                        { 
                          
                          display.clear();
                    display.drawString(0, 10, "OTA Update");
                    display.drawProgressBar(0,40,100,20,(progress / (total / 100)));

                        //   snprintf(sTemp,20,"Progress: %u%%\r", (progress / (total / 100)));
                          // display.drawString(0, 40,sTemp);
                    // write the buffer to the display
                    display.display();
                          Serial.printf("Progress: %u%%\r", (progress / (total / 100))); });

  // initialize Atmosphere sensor
  if (!bmp.begin())
  {
    debugln("Could not find BMP180 or BMP085 sensor at 0x77");
  }
  else
  {
    debugln("Found BMP180 or BMP085 sensor at 0x77");
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

  // initialize display text to no value
  for (int n = 0; n < nMaxSensors; n++)
  {
    textSensTemp[n] = "----------";
  }

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

  debugln("setup end");
}

void loop()
{

  // 1 sec timer for blinking and time update
  if ((millis() - lSecondsTime > ulSecondsInterval))
  {
    // counter how long AP still open
    if (ProgramMode == aPopen)
    {
      sSecondsUntilClose--;
    }
    lSecondsTime = millis();
  }

  // Close Wifi AP after some time
  if ((millis() - lAPOpenTime > ulAPOpenInterval) && ProgramMode == aPopen)
  {
    WiFi.softAPdisconnect();
    ledOff(LEDGN);
    ProgramMode = normal;

    debugln("AP disconnect");
  }

  // Read local sensors pressure and light, update ext sensor display text every x seconds
  if (millis() - lreadTime > ulSensReadInterval)
  {
    debugln("Readlocal");
    // readLight();
    readAtmosphere();
    formatTempExt();

    lreadTime = millis();
  }

  // Read input switches every x seconds
  if (millis() - lswitchReadTime > ulSwitchReadInterval)
  {
    if (pcf857X.digitalRead(SW2) == HIGH)
    {
      // SW2 pressed starts AP to add more sensors
      //  TODO make better
      // only in normal mode e.g. do not activate while in OTA
      if (ProgramMode == normal)
      {

        ledOn(LEDGN);
        openWifiAP();
      }
    }
    if (pcf857X.digitalRead(SW1) == HIGH)
    {
      // SW1 pressed starts OTA to update firmware
      if (ProgramMode != oTAActive)
      {
        // not yet running
        ledOn(LEDRT);
        startOTA();
        ledOff(LEDGN);
      }
      else if (bOTARunning == false)
      {
        // SW 1 pressed again stop update and reboot

        debugln("OTA stopped");

        ledOff(LEDGN);
        ESP.restart();
      }
    }

    lswitchReadTime = millis();
  }

  // Update display every x seconds
  if (millis() - lsUpdateDisplay > ulUpdateDisplayInterval)
  {
    if (ProgramMode == oTAActive)
    {
      ArduinoOTA.handle();
    }

    updateDisplay();
    lsUpdateDisplay = millis();
  }

  // Every second check sensor readings, if no reading count up and then do not display
  if ((millis() - lSensorValidTime > ulSensorValidIntervall))
  {
    //
    for (int n = 0; n < nMaxSensors; n++)
    {
      sSensor[n].iSecSinceLastRead++;
    }
    lSensorValidTime = millis();
  }
}

// Callback funktion wenn Daten über ESP-Now empfangen wurden
void on_receive_data(uint8_t *mac, uint8_t *r_data, uint8_t len)
{

  DATEN_STRUKTUR sESPReceive;
  int iChannelNr;
  int iLastSSinceLastRead;
  // copy received data to struct, to get access via variables
  memcpy(&sESPReceive, r_data, sizeof(sESPReceive));
 
 
  iChannelNr = sESPReceive.iSensorChannel;

  if (ProgramMode == aPopen)
  {
    // TODO display which new sensor connected with channel and MAC
    // needs to remember which channels are already connecte
    // may do this when changed to dynamic sensor array
  }

  // depending on channel fill values 0 is local sensor and not used here
  // new use array
  if ((iChannelNr > 0) && (iChannelNr < nMaxSensors))
  {
    sSensor[iChannelNr].fTempA = roundf((sESPReceive.fESPNowTempA + fTempCorr[iChannelNr]) * 100) / 100;
    sSensor[iChannelNr].fHumi = roundf(sESPReceive.fESPNowHumi * 100) / 100;
    sSensor[iChannelNr].fVolt = roundf((sESPReceive.fESPNowVolt + fBattCorr[iChannelNr]) * 100) / 100;
    sSensor[iChannelNr].bSensorRec = true;
    iLastSSinceLastRead = sSensor[iChannelNr].iSecSinceLastRead; //remember for display
    sSensor[iChannelNr].iSecSinceLastRead = 0;
    debugln("Received Data");
    // TODO check values and send further only if correct
    Serial.printf("Sensor mac: %02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    debugln("");
    debug("Channel Nr.:");
    debugln(iChannelNr);
    debug("Temperature A: ");
    debugln(sSensor[iChannelNr].fTempA);
    debug("Humidity: ");
    debugln(sSensor[iChannelNr].fHumi);
    debug("Batt V: ");
    debugln(sSensor[iChannelNr].fVolt);
    debug(iLastSSinceLastRead);
    debugln(" min since last read ");
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

  sSensor[0].fAtmo = bmp.readPressure();
  sSensor[0].fAtmo = sSensor[0].fAtmo / 100;

  int bmpTemp;
  bmpTemp = bmp.readTemperature();
  debug("Pressure = ");
  debug(sSensor[0].fAtmo);
  debugln(" Pascal");
  debug("BMP Temp = ");
  debugln(bmpTemp);
}

// Open WIFI AP for sensors to ask for station MAC adress
void openWifiAP()
{
  WiFi.mode(WIFI_AP);
  WiFi.begin();
  WiFi.softAP(APSSID);
  debug("Start AP: ");
  debugln(APSSID);
  ProgramMode = aPopen;
  sSecondsUntilClose = ulAPOpenInterval / 1000UL;
  lAPOpenTime = millis();
}

// Connect to local WIFI
void wifiConnectStation()
{
  WiFi.mode(WIFI_STA);
  WiFi.hostname(MYHOSTNAME);

  debugln("WifiStat connecting to ");
  debugln(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PWD);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(250);
    debug(".");
  }

  debugln();
}

// Als subfunktion damit es nicht so oft aufgerufen wird
// In String umwandeln damit die Ausgabe schneller geht
// Loc sensor has no recieve timeout of course
void formatTempExt()
{
  // if sensor not updated within time SensValidMax is reached and no value displayed
  for (int n = 0; n < nMaxSensors; n++)
  {
    if (sSensor[n].iSecSinceLastRead > SensValidMax)
    {
      // kein Sensor gefunden
      //    Serial.println("kein Sensor empfangen");
      textSensTemp[n] = "----------";
    }
    else
    {
      textSensTemp[n] = String(sSensor[n].fTempA) + " °C";
    }
  }
 
}

void drawSens5Temp()
{
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(61, 38, "Arbeitszimmer");
  display.setFont(ArialMT_Plain_24);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(25, 5, textSensTemp[5]);
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
  if (ProgramMode == aPopen)
  {
    display.drawString(115, 0, "AP:");
    display.drawString(128, 0, String(sSecondsUntilClose));
  }
  if (ProgramMode == oTAActive)
  {
    display.setFont(ArialMT_Plain_16);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.drawString(0, 0, "Wait for OTA");
  }
  if (ProgramMode == normal)
  {
    drawSens5Temp();
  }
  // write the buffer to the display
  display.display();
}

void startOTA()
{
  // Disconnect Wifi and connect to local WLAN
  // OTA Setup PWD comes from HomeAutomationSecrets.h outside repository

  debugln("StartOTA");
  debugln("AP disconnect");

  WiFi.softAPdisconnect();
  ledOff(LEDGN);
  ProgramMode = oTAActive;
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.hostname(MYHOSTNAME);

  debugln("WifiStat connecting to ");
  debugln(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PWD);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(250);
    debug(".");
    /*lcd.print(".");
    iCount++;
    if (iCount > 16)
    {
      lcd.setCursor(0, 1);
      lcd.print("                ");
      lcd.setCursor(0, 1);
      iCount = 0;
    }
    */
  }
  debug("connected to WLAN");
  display.drawString(0, 15, "OTA Init");
  ArduinoOTA.setHostname(MYHOSTNAME);
  ArduinoOTA.setPassword(OTA_PWD);
  ArduinoOTA.begin();
}
