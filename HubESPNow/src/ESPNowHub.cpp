/*

Hardware:
ESPNowHub V1.0 and CTRL Board V1 with external OLEDD display
and div sensors
Software:
One ESP with ESPNowHub and one with ESPWLANHub firmware needed. 
Functions:
Display sensor values on OLED 
Get internal sensor values from light sensor and BMP 180 air pressure
Receive external sensor values using ESP-Now protocol
Send sensor values as JSON over serial interface to ESPWLANHub
Control LED output and switches input.

20220918: V0.1:   New project derived from ESPHubHW test and WeatherStation main.cpp
20220918: V0.2:   add temp sensor DS18B20
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
20230115  V0.20:  +Debug macro using macros
20230115  V0.21:  +Disable AP on SW2 second press
20230128  V0.22:  +Display information when sensor connects first time
20230129  V0.23:  c clean up code check release compilation
20230129  V0.24:  b fixed 99999 display for temp, better switch reaction time
20340129  V0.25:  +serial communication to ESPWLAN

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
const String sSoftware = "HubESPNow V0.25";

SENSOR_DATA sSensor[nMaxSensors]; // SensValidMax in HomeAutomationCommon.h starts from 0 = local sensor and 1-max are the channels

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

/***************************
 * Serial Communication Settings
 **************************/
int serialCounter = 0;
long serialTransferTime = 0;                       // Timer for uploading data to main station
const unsigned long ulSendIntervall = 15 * 1000UL; // Upload to home server every 15x sec
/* SoftwareSerial mySerial (rxPin, txPin, inverse_logic);
   TX = GPIO01 + D10 PIN 22 RX = GPIO03 = D9 PIN21
   ESPNowHub HW Version 1 uses non standard PIN 7 GPIO13 = D7 for TXD to ESP WLAN and standard PIN 21 GPIO03 for receive from ESP WLAN
   HW Bridges have to be set
   want to transmit the date to the main station over a different serial link than the one used by the monitor */
SoftwareSerial swSer(D9, D7, false);

// Default display if any value becomes invalid. Invalid when for some time no value received
String textSensTemp[nMaxSensors];
String sNewSensorChannel;
String sNewSensorMAC;
bool bNewSensorFound = false;

int iLastReceivedChannel = 0;
// Timing seconds for various uses
long lSecondsTime = 0;
const unsigned long ulSecondsInterval = 1 * 1000UL; // time in sec TIMER

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
const unsigned long ulAPOpenInterval = 90 * 1000UL; // time in sec TIMER

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
const unsigned long ulSwitchReadInterval = 0.4 * 1000UL; // Time until switch is read next time in s TIMER

/***************************
 * Display Settings
 **************************/
const int I2C_DISPLAY_ADDRESS = 0x3c;
const int SDA_PIN = D2; // GPIO4
const int SCL_PIN = D1; // GPIO5
// const int SDA_PIN = D3; //GPIO0
// const int SCL_PIN = D4; //GPIO2

// Initialize the oled display for address 0x3c
SSD1306Wire display(0x3c, SDA_PIN, SCL_PIN);
long lsUpdateDisplay = 0;                                   // Timing
const unsigned long ulUpdateDisplayInterval = 0.3 * 1000UL; // Time until display is updated next time in s TIMER

// Forward declarations
void ledOn(uint8_t LedNr);
void ledOff(uint8_t LedNr);
void openWifiAP();
void updateDisplay();
void drawSens5Temp();
void formatTempExt();
void startOTA();
void formatNewSensorData();
void macAddrToString(byte *mac, char *str);
void sendDataToMainStation();

/***************************
 * Begin Atmosphere and iLightLocal Sensor Settings
 **************************/
// void readLight();
void readAtmosphere();
Adafruit_BMP085 bmp;
// const int Light_ADDR = 0b0100011;                      // address:0x23
// const int Atom_ADDR = 0b1110111;                       // address:0x77
long lreadTime = 0;
const unsigned long ulSensReadInterval = 60 * 1000UL; // Timing Time to evaluate received sens values for display in s TIMER

int ledNr = 0;

void setup()
{
  Serial.begin(115200);     // Hardware port for programming and debug
  swSer.begin(swBAUD_RATE); // SW port for serial communication
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
  if (esp_now_register_recv_cb(on_receive_data) != 0)
  {
    debugln("ESP-NOW register failed");
  }

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
                    display.display(); });

  // initialize Atmosphere sensor
  if (!bmp.begin())
  {
    debugln("Could not find BMP180 or BMP085 sensor at 0x77");
  }
  else
  {
    debugln("Found BMP180 or BMP085 sensor at 0x77");
  }
  // local sensor always read used for registration of new sensor
  sSensor[0].bSensorRegistered = true;

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
  delay(1000);
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
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
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
    bNewSensorFound = false;
    ProgramMode = normal;
    debugln("AP disconnect");
  }

  // Read local sensors pressure and light, update ext sensor display text every 60 seconds
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
      // only in normal mode e.g. do not activate while in OTA
      if (ProgramMode == normal)
      {
        ledOn(LEDGN);
        openWifiAP();
      }
      else if (ProgramMode == aPopen)
      {
        // SW 2 pressed again close AP
        WiFi.softAPdisconnect();
        ledOff(LEDGN);
        ProgramMode = normal;
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

  // Update display every x seconds and handle OTA
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
  // Upload Temperature Humidity to MainStation every xSeconds
  if ((millis() - serialTransferTime > ulSendIntervall))
  {
    debugln("Send to Main Station");
    sendDataToMainStation();
    serialTransferTime = millis();
  }
}

// Callback funktion for receiving data over ESPNOW
void on_receive_data(uint8_t *mac, uint8_t *r_data, uint8_t len)
{

  DATEN_STRUKTUR sESPReceive;
  int iChannelNr;
  int iLastSSinceLastRead;
  // copy received data to struct, to get access via variables
  memcpy(&sESPReceive, r_data, sizeof(sESPReceive));
  iChannelNr = sESPReceive.iSensorChannel;
  iLastReceivedChannel = iChannelNr; // remember last channel to detect new sensor
  // depending on channel fill values. 0 is local sensor and not used here
  if ((iChannelNr > 0) && (iChannelNr < nMaxSensors))
  {
    sSensor[iChannelNr].fTempA = roundf((sESPReceive.fESPNowTempA + fTempCorr[iChannelNr]) * 100) / 100;
    sSensor[iChannelNr].fHumi = roundf(sESPReceive.fESPNowHumi * 100) / 100;
    sSensor[iChannelNr].fVolt = roundf((sESPReceive.fESPNowVolt + fBattCorr[iChannelNr]) * 100) / 100;
    iLastSSinceLastRead = sSensor[iChannelNr].iSecSinceLastRead; // remember for display
    sSensor[iChannelNr].iSecSinceLastRead = 0;
    if (sSensor[iLastReceivedChannel].bSensorRegistered == false)
    {
      // do this only on very first packet of a sensor
      // save MAC Address and channel number in array
      char macAddr[18];
      // copy mac address
      macAddrToString(mac, macAddr);
      sSensor[iChannelNr].iSensorChannel = iChannelNr;
      sSensor[iChannelNr].sMacAddress = macAddr;
      formatNewSensorData();
    }

    debugln("Received Data");
    debugln("MAC Adress: ");
    debugln(sSensor[iChannelNr].sMacAddress);
    debug("Channel nr.:");
    debugln(iChannelNr);
    debug("Temperature A: ");
    debugln(sSensor[iChannelNr].fTempA);
    debug("Humidity: ");
    debugln(sSensor[iChannelNr].fHumi);
    debug("Batt V: ");
    debugln(sSensor[iChannelNr].fVolt);
    debug(iLastSSinceLastRead);
    debugln(" min since last read ");
    debugln();
  }
};

void sendDataToMainStation()
{
// serialize Sensor Data to json and send using sw serial connection
// only send sensor data when recently a sensor sent something
// Allocate JsonBuffer TODO calculate new JsonBuffer size
#ifdef DEBUG
  char output[256];
#endif
  StaticJsonDocument<capacity> jsonDocument;
  // change to array
  for (int n = 0; n < nMaxSensors; n++)
  {
    if (sSensor[n].bSensorRec == true)
    {
      // Sensor n recieved something
      // TODO this is Sensor 5 as an example add all other values
      jsonDocument.clear();
      jsonDocument["sensor"] = n;
      jsonDocument["time"] = serialCounter++;
      jsonDocument["fTempA"] = sSensor[n].fTempA;
      jsonDocument["fHumi"] = sSensor[n].fHumi;
      jsonDocument["fVolt"] = sSensor[n].fVolt;
      jsonDocument["iLight"] = sSensor[n].iLight;
      jsonDocument["fAtmo"] = sSensor[n].fAtmo;
      sSensor[n].bSensorRec = false;
      swSer.print(startMarker); // $$ Start Code
      serializeJson(jsonDocument, swSer);
      swSer.print(endMarker); // $$ End Code
      delay(10);
#ifdef DEBUG
      serializeJson(jsonDocument, output);
      Serial.println(output);
      memset(&output, 0, 256);
#endif
    }
  }
}

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
#if DEBUG == 1
  int bmpTemp;
  bmpTemp = bmp.readTemperature();
#endif
  debug("Pressure = ");
  debug(sSensor[0].fAtmo);
  debugln(" Pascal");
  debug("BMP Temp = ");
  debugln(bmpTemp);
}

// Open WIFI AP for external sensor registration (ask for station MAC adress)
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

// Connect to local WIFI to enable OTA
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

/* not called in recv data to avoid delay
called every x seconds to update values for all nMaxSensors
create string for display output
Local sensor has no recieve timeout */
void formatTempExt()
{
  // if sensor not updated within time SensValidMax is reached and no value displayed
  // and sensor has a valid first reading
  debugln("formatTempExt");

  for (int n = 0; n < nMaxSensors; n++)
  {
    if ((sSensor[n].iSecSinceLastRead > SensValidMax) || (sSensor[n].bSensorRegistered == false))
    {
      // kein Sensor gefunden
      debug("Sensor Nr.: ");
      debugln(n);
      debug("channel: ");
      debugln(sSensor[n].iSensorChannel);
      debugln(" no sensor data received");
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

void formatNewSensorData()
{
  // When a sensor connects for the first time in AP mode output data
  // this is detected by bSensorRegistered which is initially false and set true on second read
  // channel 0 is local sensor thus not displayed
  // output MAC adress and channel
  debug("formatNewSensorData: ");
  debugln(sSensor[iLastReceivedChannel].sMacAddress);
  debug("Channel Nr.:");
  debugln(sSensor[iLastReceivedChannel].iSensorChannel);
  debugln();
  sNewSensorChannel = String(sSensor[iLastReceivedChannel].iSensorChannel);
  sNewSensorMAC = (sSensor[iLastReceivedChannel].sMacAddress);
  sSensor[iLastReceivedChannel].bSensorRegistered = true; // set true to not display multiple times
  bNewSensorFound = true;
}

/* update the display independently of other functions
all display output is done here */
void updateDisplay()
{
  // clear the display
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  // test output millisec
  display.drawString(128, 54, String(millis()));
  // display flag for open AP
  if (ProgramMode == aPopen)
  {
    display.drawString(0, 0, "AP remaining s: ");
    display.drawString(80, 0, String(sSecondsUntilClose));
    if (bNewSensorFound == true)
    {
      // display data of new sensor received
      display.setFont(ArialMT_Plain_10);
      display.drawString(0, 10, "last sensor added");
      display.drawString(0, 20, "Channel:");
      display.drawString(50, 20, sNewSensorChannel);
      display.drawString(0, 30, "MAC:");
      display.drawString(30, 30, sNewSensorMAC);
    }
  }
  if (ProgramMode == oTAActive)
  {
    display.setFont(ArialMT_Plain_16);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.drawString(0, 0, "Waiting for OTA");
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
  // Disconnect AP and connect to local WLAN
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
  }
  debug("connected to WLAN");
  display.drawString(0, 15, "OTA Init");
  ArduinoOTA.setHostname(MYHOSTNAME);
  ArduinoOTA.setPassword(OTA_PWD);
  ArduinoOTA.begin();
}

void macAddrToString(byte *mac, char *str)
{
  for (int i = 0; i < 6; i++)
  {
    byte digit;
    digit = (*mac >> 8) & 0xF;
    *str++ = (digit < 10 ? '0' : 'A' - 10) + digit;
    digit = (*mac) & 0xF;
    *str++ = (digit < 10 ? '0' : 'A' - 10) + digit;
    *str++ = ':';
    mac++;
  }
  // replace the final colon with a nul terminator
  str[-1] = '\0';
}