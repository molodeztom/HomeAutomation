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
History:
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
20230129  V0.25:  +serial communication to ESPWLAN
20230204  V0.26:  c timing, HW Led blink
20230205  V0.27:  c send values with less resolution /Branch Float Tests
20230209  V0.28:  c use int instead of float where possible to avoid loosing precision
20230211  V0.29:  c remove unneeded debug functions, use char[] instead of string
20230211  V0.30:  c rename float to int value names
20230212  V0.31:  c send sensor capabilities over serial
20230212  V0.32:  c send only valid values
20230212  V0.33:  d calculate correct checksum only used values
20230212  V1.00:  works with 2 different sensors now
20230218  V1.01:  + Menu 2 levels Test
20230219  V1.02:  + Menu 2 sensor details and details 2
20231015  V1.03:  c Taktile switch order due to upside down assembly in case
20231015  V1.04:  c Add Light Sensor again multiply by 100
20231015  V1.05:  c Multiply by 500 is a max around 162240 and a low around 10
20231016  V1.06:  c Multiply light by 100 is enough resolution. otherwise it is in steps of 5. Atmospheric pressure correction to NN
20240101  V1.07:  + sensor 6 for light, StaticJSONDocument moved from HomeAutomationCommon.h to here




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
#include <HomeAutomationCommon.h>

const String sSoftware = "HubESPNow V1.07";

const size_t capacity = JSON_OBJECT_SIZE(8) + 256;
StaticJsonDocument<capacity> jsonDocument;

// Now in HomeAutomationCommon.h SENSOR_DATA sSensor[nMaxSensors]; //  HomeAutomationCommon.h starts from 0 = local sensor and 1-max are the channels

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
// TODO move this into sensor code to add new sensors with more flexibilitie
const float fBattCorr[nMaxSensors] = {0, -0.099, -0.018, -0.018, -0.018, -0.21, 0, 0, 0, 0};
const float fTempCorrA[nMaxSensors] = {0, -0.25, 0, 0, 0, 0, 0, 0, 0, 0};
const float fTempCorrB[nMaxSensors] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
const int iAtmoCorr = 3400; //To correct local value to NN

// struct for exchanging data over ESP NOW

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

char textSensTemp[nMaxSensors][22];
char textSensHumi[nMaxSensors][22];
char textSensAtmo[nMaxSensors][22];
char textSensLight[nMaxSensors][22];

String sNewSensorChannel;
String sNewSensorMAC;
bool bNewSensorFound = false;
int iCurSensorDisplay = 0;

int iLastReceivedChannel = 0;
// Timing seconds for various uses
long lSecondsTime = 0;
const unsigned long ulSecondsInterval = 1 * 1000UL; // time in sec TIMER
const unsigned long ulOneMinuteTimer = 60 * 1000UL; // time in sec TIMER

const int iSensorTimeout = 240; // time in sec

// program is running on one of these modes
// normal, AP Open to add a new sensor, OTA only active to do an OTA
enum eProgramModes
{
  normal,
  aPopen,
  oTAActive
};
enum eMenuLevel1
{
  eSensorValue = 0,
  eSensorDetail
};
enum eMenuLevel2
{
  eSensorDetailA = 0,
  eSensorVolt,
  eBack,
  eEND
};

eProgramModes ProgramMode = normal;
eMenuLevel1 MenuLevel1 = eSensorValue;
eMenuLevel2 MenuLevel2 = eSensorDetailA;

bool bOTARunning = false; // true if OTA already running to prevent stop

/***************************
 * ESP Now Settings
 **************************/
// SSID Open for some time. Sensors can contact AP to connect. After that time sensor use stored MAC adress
long lAPOpenTime = 0;                                // Timing
int sSecondsUntilClose = 0;                          // counter to display how long still open
const unsigned long ulAPOpenInterval = 120 * 1000UL; // time in sec TIMER

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
/*
#define SW1 P3 //changed because ctrl pcb is mounted upside down
#define SW2 P2
#define SW3 P1
#define SW4 P0
*/
#define SW1 P0 //TODO: remove tmp until red switch or label changed
#define SW2 P1
#define SW3 P2
#define SW4 P3

long lswitchReadTime = 0;                                // Timing
const unsigned long ulSwitchReadInterval = 0.1 * 1000UL; // Time until switch is read next time in s TIMER
// timing debounce switches, set when any switch pressed next switch event blocked
long lDebounceTime = 0;
const unsigned long ulDebounceInterval = 1 * 1000UL;
bool bSwitchBlocked = false; // does not read key when true for some time time is DebounceInterval

/***************************
 * Display Settings
 **************************/
const int I2C_DISPLAY_ADDRESS = 0x3c;
const int SDA_PIN = D2; // GPIO4
const int SCL_PIN = D1; // GPIO5
// const int SDA_PIN = D3; //GPIO0
// const int SCL_PIN = D4; //GPIO2

bool bLedState = false;

// Initialize the oled display for address 0x3c
SSD1306Wire display(0x3c, SDA_PIN, SCL_PIN);
long lsUpdateDisplay = 0;                                   // Timing
const unsigned long ulUpdateDisplayInterval = 0.3 * 1000UL; // Time until display is updated next time in s TIMER

// Forward declarations
void ledOn(uint8_t LedNr);
void ledOff(uint8_t LedNr);
void openWifiAP();
void updateDisplay();
void drawSens0();
void drawSens1();
void drawSens2();
void drawSens5();
void formatSensData();
void startOTA();
void formatNewSensorData();
void macAddrToString(byte *mac, char *str);
void sendDataToMainStation();
void handleSwitches();
void handleSW1();
void handleSW2();
void handleSW3();
void handleSW4();
void drawSensDetail();
void drawSensDetailBack();
void drawSensDetail2();

/***************************
 * Begin Atmosphere and iLightLocal Sensor Settings
 **************************/
void readLight();
void readAtmosphere();
Adafruit_BMP085 bmp;
const int Light_ADDR = 0b0100011; // address:0x23
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

  digitalWrite(LED_BUILTIN, LOW);
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
                       ledOn(LEDRT); });

  ArduinoOTA.onEnd([]()
                   {
                    display.clear();
                    display.drawString(0, 10, "OTA Successful");
                    // write the buffer to the display
                    display.display();
                    ledOff(LEDRT);
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
  // initialize sensor capabilities TODO implement in in sensor to send its capabilities
  // TODO uncomment if set productive
  sSensor[0].sSensorCapabilities = 0;
  sSensor[0].sSensorCapabilities = LIGHT_ON | ATMO_ON;
  sSensor[1].sSensorCapabilities = 0;
  sSensor[1].sSensorCapabilities = TEMPA_ON | VOLT_ON | HUMI_ON;
  sSensor[2].sSensorCapabilities = 0;
  sSensor[2].sSensorCapabilities = TEMPA_ON | VOLT_ON;
  sSensor[3].sSensorCapabilities = 0;
  sSensor[3].sSensorCapabilities = TEMPA_ON | VOLT_ON;
  sSensor[4].sSensorCapabilities = 0;
  sSensor[4].sSensorCapabilities = TEMPA_ON | TEMPB_ON;
  sSensor[5].sSensorCapabilities = 0;
  sSensor[5].sSensorCapabilities = TEMPA_ON | VOLT_ON | HUMI_ON;
  sSensor[6].sSensorCapabilities = 0;
  sSensor[6].sSensorCapabilities = LIGHT_ON;
  // sSensor[0].iLight = 250; // TODO remove when we connect a real light sensor

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
    strcpy(textSensTemp[n], "----------");
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
    for (int n = 0; n < nMaxSensors; n++)
    {
      sSensor[n].iTimeSinceLastRead++;
    }
    // digitalWrite(LED_BUILTIN, HIGH);
    lSecondsTime = millis();
  }

  // Close Wifi AP after some time
  if ((millis() - lAPOpenTime > ulAPOpenInterval) && ProgramMode == aPopen)
  {
    WiFi.softAPdisconnect();
    ledOff(LEDGB);
    bNewSensorFound = false;
    ProgramMode = normal;
    debugln("AP disconnect");
  }

  // Read local sensors pressure and light, update ext sensor display text every 60 seconds
  if (millis() - lreadTime > ulSensReadInterval)
  {
    debugln("Readlocal");
    readLight();
    readAtmosphere();
    formatSensData();
    lreadTime = millis();
  }
  if ((millis() - lDebounceTime > ulDebounceInterval))
  {
    bSwitchBlocked = false;
    lDebounceTime = millis();
  }
  // Read input switches every x seconds
  if (millis() - lswitchReadTime > ulSwitchReadInterval)
  {
    handleSwitches();
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
  /*  if ((millis() - lSensorValidTime > ulOneMinuteTimer))
   {
     //

     lSensorValidTime = millis();
   } */
  // Upload Temperature Humidity to MainStation every xSeconds
  if ((millis() - serialTransferTime > ulSendIntervall))
  {
    sendDataToMainStation();
    serialTransferTime = millis();
  }
}
void handleSW1()
{
  // SW1 pressed starts OTA to update firmware
  if (ProgramMode != oTAActive)
  {
    // not yet running
    ledOn(LEDBL);
    startOTA();
    ledOff(LEDBL);
  }
  else if (bOTARunning == false)
  {
    // SW 1 pressed again stop update and reboot
    debugln("OTA stopped");
    ledOff(LEDBL);
    ESP.restart();
  }
}
void handleSW2()
{
  // SW2 pressed starts AP to add more sensors
  // only in normal mode e.g. do not activate while in OTA
  bSwitchBlocked = true;
  if (ProgramMode == normal)
  {
    ledOn(LEDGB);
    openWifiAP();
  }
  else if (ProgramMode == aPopen)
  {
    // SW 2 pressed again close AP
    WiFi.softAPdisconnect();
    ledOff(LEDGB);
    ProgramMode = normal;
  }
}

void handleSW3()
{
  // change menu level 2
  bSwitchBlocked = true;
  if (MenuLevel1 == eSensorValue)
  {
    MenuLevel1 = eSensorDetail;
    MenuLevel2 = eSensorDetailA;
  }
  if (MenuLevel1 == eSensorDetail && MenuLevel2 == eBack)
  {
    MenuLevel1 = eSensorValue;
    MenuLevel2 = eSensorDetailA;
  }
}
void handleSW4()
{
  // change menu level 1
  bSwitchBlocked = true;
  if (MenuLevel1 == eSensorValue)
  {
    iCurSensorDisplay++;
    // skip unregistered sensors
    while (sSensor[iCurSensorDisplay].bSensorRegistered == false && (iCurSensorDisplay <= nMaxSensors))
    {
      iCurSensorDisplay++;
    }
    if (iCurSensorDisplay > nMaxSensors || sSensor[iCurSensorDisplay].bSensorRegistered == false)
      iCurSensorDisplay = 0;

    debug("Display Sensor Nr: ");
    debugln(iCurSensorDisplay);
  }
  if (MenuLevel1 == eSensorDetail)
  {
    debugln(MenuLevel2);
    MenuLevel2 = (eMenuLevel2)(((int)MenuLevel2 + 1) % (eEND));
    debugln(MenuLevel2);
  }
}
void handleSwitches()
{
  if ((pcf857X.digitalRead(SW1) == HIGH) && (bSwitchBlocked == false))
    handleSW1();
  if ((pcf857X.digitalRead(SW2) == HIGH) && (bSwitchBlocked == false))
    handleSW2();
  if ((pcf857X.digitalRead(SW3) == HIGH) && (bSwitchBlocked == false))
    handleSW3();
  if ((pcf857X.digitalRead(SW4) == HIGH) && (bSwitchBlocked == false))
    handleSW4();
}

// Callback funktion for receiving data over ESPNOW
void on_receive_data(uint8_t *mac, uint8_t *r_data, uint8_t len)
{

  ESPNOW_DATA_STRUCTURE sESPReceive;
  int iChannelNr;
  int iLastSSinceLastRead;
  // copy received data to struct, to get access via variables
  digitalWrite(LED_BUILTIN, LOW);
  memcpy(&sESPReceive, r_data, sizeof(sESPReceive));
  iChannelNr = sESPReceive.iSensorChannel;
  iLastReceivedChannel = iChannelNr; // remember last channel to detect new sensor
                                     // depending on channel fill values. 0 is local sensor and not used here
  debugln("Received Data");
  debugln("MAC Adress: ");
  debugln(sSensor[iChannelNr].sMacAddress);
  debug("Channel nr.:");
  debugln(iChannelNr);

  if ((iChannelNr > 0) && (iChannelNr < nMaxSensors))
  {

    // recieve sensor values instantly convert to int

    if (sSensor[iChannelNr].sSensorCapabilities & TEMPA_ON)
    {
      sSensor[iChannelNr].iTempA = roundf((sESPReceive.fESPNowTempA + fTempCorrA[iChannelNr]) * 100);
      debug("Temperature A: ");
      debugln(sSensor[iChannelNr].iTempA);
    }
    if (sSensor[iChannelNr].sSensorCapabilities & TEMPB_ON)
    {
      sSensor[iChannelNr].iTempB = roundf((sESPReceive.fESPNowTempB + fTempCorrB[iChannelNr]) * 100);
      debugln("capabilities TempBOn");
    }
    if (sSensor[iChannelNr].sSensorCapabilities & HUMI_ON)
    {
      sSensor[iChannelNr].iHumi = roundf(sESPReceive.fESPNowHumi * 100);
      debug("Humidity: ");
      debugln(sSensor[iChannelNr].iHumi);
    }
    if (sSensor[iChannelNr].sSensorCapabilities & VOLT_ON)
    {
      sSensor[iChannelNr].iVolt = roundf((sESPReceive.fESPNowVolt + fBattCorr[iChannelNr]) * 100);
      debug("Batt V: ");
      debugln(sSensor[iChannelNr].iVolt);
    }

    iLastSSinceLastRead = sSensor[iChannelNr].iTimeSinceLastRead; // remember for display
    debug("min since last read: ");
    debugln(iLastSSinceLastRead);

    sSensor[iChannelNr].iTimeSinceLastRead = 0;
    sSensor[iChannelNr].bSensorRec = true;
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
  }
};

void sendDataToMainStation()
{
// serialize Sensor Data to json and send using sw serial connection
// only send sensor data when recently a sensor sent something
// For all sensor we send all data even if sensor does not support something
// Allocate JsonBuffer TODO calculate new JsonBuffer size
#ifdef DEBUG
  char output[256];
#endif
  debugln("Send to Main Station");
  int iCheckSum = 0;
  StaticJsonDocument<capacity> jsonDocument;

  // change to array
  for (int n = 0; n < nMaxSensors; n++)
  {
    if (sSensor[n].bSensorRec == true)
    {
      // Sensor n recieved something
      // checksum is computed at reciever as well
      debug("Send Sensor Nr.: ");
      debugln(n);
      // Checksum with 100 times values
      iCheckSum = sSensor[n].sSensorCapabilities;

      // JSON with values * 100 in int format
      // send only values the sensor supports capabilities not evaluated since there will be invalid values anyway
      jsonDocument.clear();
      jsonDocument["sensor"] = n;
      jsonDocument["time"] = serialCounter++;
      jsonDocument["SensCap"] = sSensor[n].sSensorCapabilities;

      // check value plausibility
      if ((sSensor[n].iTempA < 4000) && (sSensor[n].iTempA > -4000))
      {
        jsonDocument["iTempA"] = sSensor[n].iTempA;
        iCheckSum += sSensor[n].iTempA;
      }
      if ((sSensor[n].iHumi < 10500) && (sSensor[n].iHumi > 300))
      {
        jsonDocument["iHumi"] = sSensor[n].iHumi;
        iCheckSum += sSensor[n].iHumi;
      }
      if ((sSensor[n].iVolt < 500) && (sSensor[n].iVolt > 100))
      {
        jsonDocument["iVolt"] = sSensor[n].iVolt;
        iCheckSum += sSensor[n].iVolt;
      }
      if ((sSensor[n].iLight < 800000) && (sSensor[n].iLight >= 0)) // TODO: Remove coment when max value in sunshine is determined
      // if ((sSensor[n].iLight >= 0))
      {
        jsonDocument["iLight"] = sSensor[n].iLight;
        iCheckSum += sSensor[n].iLight;
      }
      if ((sSensor[n].iAtmo < 120000) && (sSensor[n].iAtmo > 80000))
      {
        jsonDocument["iAtmo"] = sSensor[n].iAtmo;
        iCheckSum += sSensor[n].iAtmo;
      }
      jsonDocument["iCSum"] = iCheckSum;

      sSensor[n].bSensorRec = false;
      swSer.print(startMarker); // $$ Start Code
      serializeJson(jsonDocument, output);

      swSer.print(output);
      swSer.print(endMarker); // $$ End Code
      delay(10);
      debug("JSON: ");
      debugln(output);
      memset(&output, 0, 256);
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
  sSensor[0].iAtmo = bmp.readPressure() + iAtmoCorr;
  // sSensor[0].iAtmo = sSensor[0].iAtmo; //TODO: remove seems unnecessarry
  sSensor[0].bSensorRec = true;
  sSensor[0].iTimeSinceLastRead = 0;
#if DEBUG == 1
  int bmpTemp;
  bmpTemp = bmp.readTemperature();
#endif
  debug("Pressure = ");
  debug(sSensor[0].iAtmo);
  debugln(" Pascal");
  debug("BMP Temp = ");
  debugln(bmpTemp);
}

void readLight()
{
  // reset
  Wire.beginTransmission(Light_ADDR);
  Wire.write(0b00000111);
  Wire.endTransmission();

  Wire.beginTransmission(Light_ADDR);
  Wire.write(0b00100000);
  Wire.endTransmission();
  // typical read delay 120ms
  delay(120);
  Wire.requestFrom(Light_ADDR, 2); // 2byte every time
  for (sSensor[0].iLight = 0; Wire.available() >= 1;)
  {
    char c = Wire.read();
    sSensor[0].iLight = (sSensor[0].iLight << 8) + (c & 0xFF);
  }
  sSensor[0].iLight = sSensor[0].iLight * 100; // * 100 skalierung
  sSensor[0].bSensorRec = true;
#ifdef DEBUG
  Serial.print("light: ");
  Serial.println(sSensor[0].iLight);
#endif
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
void formatSensData()
{
  // if sensor not updated within time iSensTimeout is reached and no value displayed
  // and sensor has a valid first reading
  debugln("formatSensData");

  for (int n = 0; n < nMaxSensors; n++)
  {
    if ((sSensor[n].iTimeSinceLastRead > iSensorTimeout) || (sSensor[n].bSensorRegistered == false))
    {
      // no sensor found
      debug("Sensor Nr.: ");
      debugln(n);
      debug("channel: ");
      debugln(sSensor[n].iSensorChannel);
      debugln(" no sensor data received since ");
      debugln(sSensor[n].iTimeSinceLastRead);
      strcpy(textSensTemp[n], "----------");
    }
    else
    {
      // sensor found write a formatted string into display array
      // precision is controlled by the division 100 means 2 decimals 10 means 1 decimal

      sprintf(textSensTemp[n], "%2.i.%i °C", sSensor[n].iTempA / 100, abs(sSensor[n].iTempA) % 100);
      sprintf(textSensHumi[n], "%i.%i  %%", (sSensor[n].iHumi / 10) / 10, abs(sSensor[n].iHumi / 10) % 10);
      sprintf(textSensAtmo[n], "%i.%1i hPA", (sSensor[n].iAtmo / 10) / 10, abs(sSensor[n].iAtmo / 10) % 10);

      debug("TempA: ");
      debugln(textSensTemp[n]);
      debug("Humi: ");
      debugln(textSensHumi[n]);
      debug("Atmo: ");
      debugln(textSensAtmo[n]);
    }
  }
}
// TODO make this generic according to sensor capabilities and sensor name from array

void drawSens0()
{

  display.setFont(ArialMT_Plain_16);
  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  display.drawString(110, 10, textSensAtmo[0]);
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(61, 54, "Lokal");
}
void drawSens1()
{
  display.setFont(ArialMT_Plain_24);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(25, 2, textSensTemp[1]);
  display.drawString(25, 28, textSensHumi[1]);
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(61, 54, "Aussen");
}

void drawSens2()
{
  display.setFont(ArialMT_Plain_24);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(25, 5, textSensTemp[2]);
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(61, 54, "Schlafzimmer");
}

void drawSens5()
{

  display.setFont(ArialMT_Plain_24);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(25, 2, textSensTemp[5]);
  display.drawString(25, 28, textSensHumi[5]);
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(61, 54, "Arbeitszimmer");
}

void drawSensDetail()
{
  String sSensorMAC;
  String sSensorChannel;
  char textLastRead[20];

  sSensorChannel = String(sSensor[iCurSensorDisplay].iSensorChannel);
  sSensorMAC = (sSensor[iCurSensorDisplay].sMacAddress);
  // format last time read into min:sec
  // TODO clean
  // int sec = (sSensor[iCurSensorDisplay].iTimeSinceLastRead) % 60;
  // sprintf(textLastRead, "%i.%i ", (((sSensor[iCurSensorDisplay].iTimeSinceLastRead) / 60), (int)((sSensor[iCurSensorDisplay].iTimeSinceLastRead) % 60)));
  // sprintf(textLastRead, " %i:%.2u", ((sSensor[iCurSensorDisplay].iTimeSinceLastRead) / 60), sec);
  sprintf(textLastRead, " %i:%.2u", ((sSensor[iCurSensorDisplay].iTimeSinceLastRead) / 60), (sSensor[iCurSensorDisplay].iTimeSinceLastRead) % 60);

  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(5, 2, "Kanal: ");
  display.drawString(40, 2, sSensorChannel);
  if (iCurSensorDisplay == 0)
  {
    display.drawString(5, 12, "MAC: Lokal");
  }
  else
  {
    display.drawString(5, 12, "MAC: ");
    display.drawString(35, 12, sSensorMAC);
  }

  display.drawString(5, 22, "Letzter Empfang: ");
  display.drawString(85, 22, textLastRead);

  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(54, 54, "Sensor Details 1");
}

void drawSensDetail2()
{
  char cTextSensorVolt[20];
  char textCapabilities[50];
  char textCapabilities1[50];
  uint16_t iSensCap = sSensor[iCurSensorDisplay].sSensorCapabilities;
  // convert bit mask into text
  sprintf(textCapabilities, "TempA: %s Humi: %s Volt: %s ", (iSensCap & TEMPA_ON) ? "1" : "0", (iSensCap & HUMI_ON) ? "1" : "0", (iSensCap & VOLT_ON) ? "1" : "0");
  sprintf(textCapabilities1, "Atmo: %s TempB: %s Light: %s ", (iSensCap & ATMO_ON) ? "1" : "0", (iSensCap & TEMPB_ON) ? "1" : "0", (iSensCap & LIGHT_ON) ? "1" : "0");
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);

  if (iCurSensorDisplay == 0)
  {
    display.drawString(5, 2, "USB");
  }
  else
  {
    sprintf(cTextSensorVolt, "%i.%i V", sSensor[iCurSensorDisplay].iVolt / 100, abs(sSensor[iCurSensorDisplay].iVolt) % 100);
    display.drawString(5, 2, "Batt: ");
    display.drawString(30, 2, cTextSensorVolt);
  }

  display.drawString(5, 12, "Sensor Werte: ");
  display.drawString(5, 23, textCapabilities);
  display.drawString(5, 33, textCapabilities1);

  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(54, 54, "Sensor Details 2");
}

void drawSensDetailBack()
{
  display.setFont(ArialMT_Plain_24);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(45, 10, "Zurück");
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
  // display.drawString(128, 54, String(millis()));
  // display flag for open AP
  if (ProgramMode == aPopen)
  {
    display.drawString(0, 0, "AP remaining s: ");
    display.drawString(80, 0, String(sSecondsUntilClose));
    if (bNewSensorFound == true)
    {

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
    if (MenuLevel1 == eSensorValue)
    {
      switch (iCurSensorDisplay)
      {
      case 0:
        drawSens0();
        break;
      case 1:
        drawSens1();
        break;
      case 2:
        drawSens2();
        break;
      case 5:
        drawSens5();
        break;
      default:
        drawSens5();
        break;
      }
    }
    if (MenuLevel1 == eSensorDetail)
    {
      switch (MenuLevel2)
      {
      case eSensorDetailA:
        drawSensDetail();
        break;
      case eSensorVolt:
        drawSensDetail2();
        break;
      case eBack:
        drawSensDetailBack();
        break;
      default:
        drawSens5();
        break;
      }
    }
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
  ledOff(LEDGB);
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
