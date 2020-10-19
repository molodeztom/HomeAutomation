/*
 
Hardware: 
ESP Board mit exteternem OLED Display und div Sensoren
Funktion:
Anzeige von div. Werten aus lokalen Sensoren auf OLED
Empfang von externen Temperaturwerten über ESP-Now 
Versenden der Werte über WLAN an Thingspeak
Versenden der Werte in JSON Format an HomeServer über Serial

201909:   V0.1:   Weather Station Demo modifiziert mit mehr sensoren Die Sensoren werden im LAN Ausgegeben
20191005: V0.2:   Externes Wetter entfernen und dafür andere Anzeige, Lokale Temperatur statt vom Server verwenden
20191005: V0.3:   Keine Uhr mehr dafür je ein Frame innen und Aussentemperatur
20191005: V0.4:   Wetter Server entfernt
20191005: V0.5:   ESP NOW Protokoll dazu Thingspeak und WLAN vorläufig auskommentiert
20191006  V0.6:   Aussensensor 2 Temperaturen holen und anzeigen
20191006  V0.7:   Feuchte, Helligkeit und Luftdruck anzeigen
20191208  V0.8:   WLAN code nur AP ESP_NOW aktiv aufgeräumt
20191216  V0.9:   WLAN Code für Senden wieder hinzu
20191220  V0.10:  NTP Time holen (WebTime)
20191221  V0.11:  NTP Time wieder entfernt geht nicht wegen Reset  LABel V2.0
20191222  V0.12:  Kanal auswerten um 2. Sensorboard zu empfangen
20191228  V0.13:  Temperatur des BMP auch auslesen 
20191228  V0.14:  Empfange Humidity 
20191228  V0.15:  Temp Aussen A statt B übertragen da B abgeschaltet wurde.
20191228  V0.16:  Fehler behoben: cntAPVisible Thermometer zu früh abgeschaltet
20200107  V0.17:  Batterispannung statt Temp B auf Display
20200111  V0.18:  Batteriespannung hier in der Station korrigieren ist einfacher
20200304  V0.19:  Batteriespannung Korrektur geändert, echte und korrigerte Spannung ausgeben. Wifi.persistent(false) aktiviert
20200304  V0.20:  /SerialTest: Test "Hello World" über Serielle Schnittstelle
20200321  V0.21:  Serial Send Data in JSON Format
20200321  V0.22:  Cleanup Variable names
20200321  V0.23:  Write all measurements via serial
20200322  V0.24:  Improved extSensor receive detection  
20200322  V0.25:  Use DHT 10 with lib
20200322  V0.25:  remove old humid temp function Bugfix: wrong variable for measurement timer lreadtime
20200417  V0.26:  3rd Sensor added
20200417  V0.27:  (ReStruct) Restruct sensor data into struct 
20200428  V0.28:  2. Temperatur für Sensor 2
20200429  V0.29:  Channel 4 
20200513  V0.30:  Reverse to older version without I2C communication between ESPs 
20200514  V0.31:  move to platformio
20200516  V0.32   TODO: temporarily removed waiting for sensor 2 send
20200518  V0.33   Change serial communication only one sensor at a time
20200521  V0.34   Get Battery voltage from sensor 1
20200521  V0.35   More stable error detection and timing
20200522  V0.36   Some values and definitions outsourced to HomeAutomationCommon.h
20200522  V1.0   Stable error handling and messaging
20200522  V1.1   Github statt Bitbucket Test
20200522  V1.2   Wieder Bitbucket 
20200607  V1.3   10 ms delay while sending sensor data over serial to make it more stable
                 Debug print to show all stages of loop, works well so far with Home Server in NoDebug mode
20200627  V1.4   Add Sensor #5
20020627  V1.5    Remove Local Temp and Humi is incorrect anyway
20200628  V1.6    Remove old dht code

 */

#include <Arduino.h>
#define DEBUG
#include <ArduinoJson.h>

// time
//#include <coredecls.h>                  // settimeofday_cb()
tm timeInfo;
#include "Spi.h"
#include "SSD1306Wire.h"
#include "OLEDDisplayUi.h"
#include "Wire.h"

#include "WeatherStationFonts.h"
#include "WeatherStationImages.h"
#include <ESP8266WiFi.h>

#include "D:\Arduino\HomeAutomation\HomeAutomationCommon.h"

//Bibliothek für ESP-Now
extern "C"
{
#include <espnow.h>
}
const String sSoftware = "WeatherStation V1.4";
#include <SoftwareSerial.h>
#include <Adafruit_BMP085.h>

// SoftwareSerial mySerial (rxPin, txPin, inverse_logic);
// want to transmit the date to the main station over a different serial link than the one used by the monitor
SoftwareSerial swSer(D6, D7, false);

//used to correct small deviances in sensor reading  0.xyz  x=Volt y=0.1Volt z=0.01V3

#define BATTCORR1 -0.099
#define BATTCORR2 -0.018
#define BATTCORR3 -0.018
#define BATTCORR4 -0.018
#define BATTCORR5 -0.21
#define SENSCORR1A -0.25

//struct for exchanging data over ESP NOW
struct DATEN_STRUKTUR
{
  int iSensorChannel = 99;  //default for none received
  float fESPNowTempA = -99; //Aussen A
  float fESPNowTempB = -99; //Aussen B
  float fESPNowHumi = -99;
  float fESPNowVolt = -99; //Batterie Sensor
};

// Default display if any value becomes invalid. Invalid when for some time no value received
String textSens1Temp = "----------";
String textSens2Temp = "----------";
String textSens3Temp = "----------";

/***************************
 * ESP Now Settings
 **************************/
//SSID Open for some time. Sensors can contact AP to connect. After that time sensor use stored MAC adress
long lAPOpenTime = 0;
const unsigned long ulAPOpenIntervall = 240 * 1000UL; //time in sec
bool APOpen = false;

/***************************
 * Temperature Settings
 **************************/


/***************************
 * Forward declarations
 **************************/
long lreadTime = 0;
long uploadTime = 0;
void sendDataToMainStation();
void formatTempExt();
void on_receive_data(uint8_t *mac, uint8_t *r_data, uint8_t len);

/***************************
 * Begin Atmosphere and iLightLocal Sensor Settings
 **************************/
void readLight();
void readAtmosphere();
Adafruit_BMP085 bmp;
const int Light_ADDR = 0b0100011;                      // address:0x23
const int Atom_ADDR = 0b1110111;                       // address:0x77
const unsigned long ulSensReadIntervall = 70 * 1000UL; //Time to evaluate received sens values for display

/***************************
 * Serial Communication Settings
 **************************/
int serialCounter = 0;
long serialTransferTime = 0;                       //Timer for uploading data to main station
const unsigned long ulSendIntervall = 15 * 1000UL; //Upload to home server every x sec

/***************************
 * Begin Settings
 **************************/
/*
#define TZ              2       // (utc+) TZ in hours
#define DST_MN          60      // use 60mn for summer time in some countries
#define TZ_MN           ((TZ)*60)
#define TZ_SEC          ((TZ)*3600)
#define DST_SEC         ((DST_MN)*60)
*/

/***************************
 * Display Settings
 **************************/

const int I2C_DISPLAY_ADDRESS = 0x3c;
#if defined(ESP8266)
//const int SDA_PIN = D1;
//const int SDC_PIN = D2;

const int SDA_PIN = D3; //GPIO0
const int SDC_PIN = D4; //GPIO2
#else
//const int SDA_PIN = GPIO5;
//const int SDC_PIN = GPIO4
//TODO remove this
//const int SDA_PIN = GPIO0;
//const int SDC_PIN = GPIO2
#endif

const boolean IS_METRIC = true;
// Initialize the oled display for address 0x3c
SSD1306Wire display(I2C_DISPLAY_ADDRESS, SDA_PIN, SDC_PIN);
OLEDDisplayUi ui(&display);

// flag changed in the ticker function every 10 minutes
//declaring prototypes for display
void updateData(OLEDDisplay *display);
void drawProgress(OLEDDisplay *display, int percentage, String label);
void drawSensLocTemp(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);
void drawsSens1Temp(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);
void drawSens2Temp(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);
void drawsSens3Temp(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);
void drawHumi(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);
void drawAtmo(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);
void drawLight(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);
void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState *state);

// Add frames this array keeps function pointers to all frames
// frames are the single views that slide from right to left
FrameCallback frames[] = {drawSensLocTemp, drawsSens1Temp, drawSens2Temp, drawsSens3Temp, drawHumi, drawAtmo, drawLight};
int numberOfFrames = 7;
OverlayCallback overlays[] = {drawHeaderOverlay};
int numberOfOverlays = 1;

void setup()
{
  Serial.begin(swBAUD_RATE);
  swSer.begin(swBAUD_RATE);
  Serial.println(sSoftware);
  WiFi.persistent(false); //https://www.arduinoforum.de/arduino-Thread-Achtung-ESP8266-schreibt-bei-jedem-wifi-begin-in-Flash
  //dht.begin(); //DHT 11 Sensor
  // init wire Sensors
  Wire.begin(0, 2);
  Wire.beginTransmission(Atom_ADDR);
  //initialize Atmosphere sensor
  if (!bmp.begin())
  {
    Serial.println("Could not find BMP180 or BMP085 sensor at 0x77");
  }
  else
  {
    Serial.println("Find BMP180 or BMP085 sensor at 0x77");
  }
  Wire.endTransmission();

  //initialize light sensor
  Wire.beginTransmission(Light_ADDR);
  Wire.write(0b00000001);
  Wire.endTransmission();

  // initialize display
  display.init();
  display.clear();
  display.display();

  //display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setContrast(255);

  //Reset WiFI
  WiFi.disconnect();
  WiFi.mode(WIFI_AP);
  WiFi.begin();
  //AP for sensors to ask for station MAC adress
  WiFi.softAP(APSSID);
  Serial.println("Start AP");
  Serial.println(APSSID);
  APOpen = true;
  lAPOpenTime = millis();
  delay(1000);

  if (esp_now_init() != 0)
  {
    Serial.println("Init ESP-Now failed restart");
    ESP.restart();
  }
  // ESP Role  1=Master, 2 = Slave 3 = Master + Slave
  Serial.println("Set espnow Role 2");
  esp_now_set_self_role(2);
  //callback for received ESP NOW messages
  esp_now_register_recv_cb(on_receive_data);

  //Display settings
  ui.setTargetFPS(30);
  ui.setTimePerFrame(1000);
  ui.setTimePerTransition(1000);
  ui.setActiveSymbol(activeSymbole);
  ui.setInactiveSymbol(inactiveSymbole);
  // You can change this to
  // TOP, LEFT, BOTTOM, RIGHT
  ui.setIndicatorPosition(BOTTOM);
  // Defines where the first frame is located in the bar.
  ui.setIndicatorDirection(LEFT_RIGHT);
  // You can change the transition that is used
  // SLIDE_LEFT, SLIDE_RIGHT, SLIDE_TOP, SLIDE_DOWN
  ui.setFrameAnimation(SLIDE_LEFT);
  ui.setFrames(frames, numberOfFrames);
  ui.setOverlays(overlays, numberOfOverlays);
  // Inital UI takes care of initalising the display too.
  ui.init();
  Serial.println("setup end");
  updateData(&display);
}

void loop()
{
  //Close Wifi AP after some time

  if ((millis() - lAPOpenTime > ulAPOpenIntervall) && APOpen == true)
  {
    WiFi.softAPdisconnect();
 
    APOpen = false;
#ifdef DEBUG
    Serial.println("AP disconnect");
#endif
  }

  //Read local temperature humidity every x seconds
  if (millis() - lreadTime > ulSensReadIntervall)
  {
    Serial.println("Readlocal");
    //readTempHumDHT11();
    readLight();
    readAtmosphere();
    formatTempExt();

    lreadTime = millis();
  }

  //Every x seconds check sensor readings, if no reading count up and then do not display
  if ((millis() - lSensorValidTime > ulSensorValidIntervall))
  {
    Serial.println("Sensor count up");
    sSensor1.iSensorCnt++;
    sSensor2.iSensorCnt++;
    sSensor3.iSensorCnt++;
    //sSensor4.iSensorCnt++;
    sSensor5.iSensorCnt++;
    lSensorValidTime = millis();
  }

  //Upload Temperature Humidity to MainStation every xSeconds
  if ((millis() - serialTransferTime > ulSendIntervall))
  {
    Serial.println("Send to Main Station");
    sendDataToMainStation();
    serialTransferTime = millis();
  }

  int remainingTimeBudget = ui.update();

  if (remainingTimeBudget > 0)
  {
    //eat remaining time for consistency
    delay(remainingTimeBudget);
  }
}

//Callback funktion wenn Daten über ESP-Now empfangen wurden
void on_receive_data(uint8_t *mac, uint8_t *r_data, uint8_t len)
{

  DATEN_STRUKTUR sESPReceive;
   //copy received data to struct, to get access via variables
  memcpy(&sESPReceive, r_data, sizeof(sESPReceive));
  //TODO check values and send further only if correct
  //iSensorChannel = sESPReceive.iSensorChannel;
#ifdef DEBUG
  Serial.print("Channel: ");
  Serial.println(sESPReceive.iSensorChannel);
#endif

  //depending on channel
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

void sendDataToMainStation()
{
//serialize Sensor Data to json and send using sw serial connection
//only send sensor data when recently a sensor sent something
//Allocate JsonBuffer TODO calculate new JsonBuffer size
#ifdef DEBUG
  char output[256];
#endif
  StaticJsonDocument<capacity> jsonDocument;
  if (sSensor0.bSensorRec == true)
  {
    //SensorLoc recieved something
    jsonDocument.clear();
    jsonDocument["sensor"] = 0;
    jsonDocument["time"] = serialCounter++;
    jsonDocument["iLightLoc"] = sSensor0.iLight;
    jsonDocument["fAtmoLoc"] = sSensor0.fAtmo;
    sSensor0.bSensorRec = false;
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
  if (sSensor1.bSensorRec == true)
  {
    //Sensor 1 recieved something
    jsonDocument.clear();
    jsonDocument["sensor"] = 1;
    jsonDocument["time"] = serialCounter++;
    jsonDocument["fTemp1A"] = sSensor1.fTempA;
    jsonDocument["fHumi1"] = sSensor1.fHumi;
    jsonDocument["fVolt1"] = sSensor1.fVolt;
    sSensor1.bSensorRec = false;
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
  if (sSensor2.bSensorRec == true)
  {
    //Sensor 2 recieved something
    jsonDocument.clear();
    jsonDocument["sensor"] = 2;
    jsonDocument["time"] = serialCounter++;
    jsonDocument["fTemp2A"] = sSensor2.fTempA;
    jsonDocument["fVolt2"] = sSensor2.fVolt;
    sSensor2.bSensorRec = false;
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
  if (sSensor3.bSensorRec == true)
  {
    //Sensor 3 recieved something
    jsonDocument.clear();
    jsonDocument["sensor"] = 3;
    jsonDocument["time"] = serialCounter++;
    jsonDocument["fTemp3A"] = sSensor3.fTempA;
    jsonDocument["fVolt3"] = sSensor3.fVolt;
    sSensor3.bSensorRec = false;
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
  if (sSensor4.bSensorRec == true)
  {
    //Sensor 4 recieved something
    jsonDocument.clear();
    jsonDocument["sensor"] = 4;
    jsonDocument["time"] = serialCounter++;
    jsonDocument["fTemp4A"] = sSensor4.fTempA;
    jsonDocument["fHumi4]"] = sSensor4.fHumi;
    jsonDocument["fVolt4"] = sSensor4.fVolt;
    sSensor4.bSensorRec = false;
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
  if (sSensor5.bSensorRec == true)
  {
    //Sensor 5 recieved something
    jsonDocument.clear();
    jsonDocument["sensor"] = 5;
    jsonDocument["time"] = serialCounter++;
    jsonDocument["fTemp5A"] = sSensor5.fTempA;
    jsonDocument["fHumi5"] = sSensor5.fHumi;
    jsonDocument["fVolt5"] = sSensor5.fVolt;
    sSensor5.bSensorRec = false;
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

void drawProgress(OLEDDisplay *display, int percentage, String label)
{
  display->clear();
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(64, 10, label);
  display->drawProgressBar(2, 28, 124, 10, percentage);
  display->display();
}

// Als subfunktion damit es nicht so oft aufgerufen wird
// In String umwandeln damit die Ausgabe schneller geht
// Loc sensor has no recieve timeout of course
void formatTempExt()
{
  //Empfang Sensor 1
  if (sSensor1.iSensorCnt > SensValidMax)
  {
    //kein Sensor gefunden
    //   Serial.println("kein Sensor empfangen");
    textSens1Temp = "----------";
  }
  else
  {
    textSens1Temp = String(sSensor1.fTempA) + " °C";
  }
  //Empfang Sensor 2 blau/braun
  if (sSensor2.iSensorCnt > SensValidMax)
  {
    //kein Sensor gefunden
    //   Serial.println("kein Sensor empfangen");
    textSens2Temp = "----------";
  }
  else
  {
    textSens2Temp = String(sSensor2.fTempA) + " °C";
  }
  //Empfang Sensor 3
  if (sSensor3.iSensorCnt > SensValidMax)
  {
    //kein Sensor gefunden
    //   Serial.println("kein Sensor empfangen");
    textSens3Temp = "----------";
  }
  else
  {
    textSens3Temp = String(sSensor3.fTempA) + " °C";
  }
}

void updateData(OLEDDisplay *display)
{
  drawProgress(display, 10, "Updating time...");
}

// Zeichnet einen Frame diese Funktionen werden von der OLEDDisplayUI Lib aufgerufen. x und y werden in der Lib gesetzt um den Frame zu scrollen etc.
// Bei der Ausgabe also immer die Position mit übernehmen. Position abgleichen mit eigenen Werten, diese beziehen sich auf x, y 0 wenn der Frame fest angezeigt wird

/*
 * Gebe Innen Temperatur  aus
 */
void drawSensLocTemp(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
  //Beschreibung als Text
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(64 + x, 38 + y, "Innen");
  //Eigene Temperatur
  display->setFont(ArialMT_Plain_24);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  String temp = String(sSensor5.fTempA) + "°C";
  display->drawString(40 + x, 5 + y, temp);
}

void drawsSens1Temp(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
  //Beschreibung als Text Sensor 1 Temp A (Balcony)
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(61 + x, 38 + y, "Bal kon");
  //Eigene Temperatur
  display->setFont(ArialMT_Plain_24);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(25 + x, 5 + y, textSens1Temp);
}

void drawSens2Temp(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{

  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(61 + x, 38 + y, "Schlafzimmer");
  display->setFont(ArialMT_Plain_24);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(25 + x, 5 + y, textSens2Temp);
}

void drawsSens3Temp(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(61 + x, 38 + y, "Wohnzimmer");
  display->setFont(ArialMT_Plain_24);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(25 + x, 5 + y, textSens3Temp);
}

void drawHumi(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
  //Beschreibung als Text
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(64 + x, 38 + y, "Luftfeuchte");

  display->setFont(ArialMT_Plain_24);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  String temp = String(sSensor5.fHumi) + " %";
  display->drawString(40 + x, 5 + y, temp);
}

void drawAtmo(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
  //Beschreibung als Text
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(64 + x, 38 + y, "Luftdruck");
  display->setFont(ArialMT_Plain_16);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  String temp = String(sSensor0.fAtmo) + " hPA";
  display->drawString(18 + x, 5 + y, temp);
}

void drawLight(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
  //Beschreibung als Text
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(64 + x, 38 + y, "Helligkeit");
  //Eigene Temperatur
  display->setFont(ArialMT_Plain_24);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  String temp = String(sSensor0.iLight) + " lm";
  display->drawString(30 + x, 5 + y, temp);
}

//Die gelbe Zeile unten Uhrzeit und Frame Pointer erst mal keine Temperatur mehr
void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState *state)
{

  timeInfo.tm_hour = 1;

  char buff[14];
  // sprintf_P(buff, PSTR("%02d:%02d"), timeInfo->tm_hour, timeInfo->tm_min);
  sprintf_P(buff, PSTR("%02d:%02d"), timeInfo.tm_hour, timeInfo.tm_min);
  display->setColor(WHITE);
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(0, 54, String(buff));
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  display->drawHorizontalLine(0, 52, 128);
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
  for (sSensor0.iLight = 0; Wire.available() >= 1;)
  {
    char c = Wire.read();
    sSensor0.iLight = (sSensor0.iLight << 8) + (c & 0xFF);
  }
  sSensor0.iLight = sSensor0.iLight / 1.2;
  sSensor0.bSensorRec = true;
#ifdef DEBUG
  Serial.print("light: ");
  Serial.println(sSensor0.iLight);
#endif
}

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
/*
 * In WIFI Station Mode schalten um Verbindung mit Internet herzustellen
 */
void wifiConnectStation()
{
  WiFi.mode(WIFI_AP_STA);
  Serial.print("WifiStat connecting to ");
  Serial.print(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PWD);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(250);
    Serial.print(".");
  }
}

/*
//upload temperature humidity data to thinkspak.com
void uploadTemperatureHumidity(){
   Serial.println("IN upload Temperature Humidity");
   if(!thingspeak_client.connect(host, httpPort)){
    Serial.println("connection failed");
    return;
  }
  // Three values(field1 field2 field3 field4) have been set in thingspeak.com 
  thingspeak_client.print(String("GET ") + "/update?api_key="+api_key+"&field1="+fTempLocal+"&field2="+fHumiLocal +"&field3="+iLightLocal +"&field4="+(fAtmoLoc/100) +"&field5="+fTemper2B +"&field6="+fTemper1 +"&field7="+fVolt +"&field8="+fHumidity + " HTTP/1.1\r\n" +"Host: " + host + "\r\n" + "Connection: close\r\n\r\n");
  while(thingspeak_client.available()){
    String line = thingspeak_client.readStringUntil('\r');
    Serial.print(line);
  }

}
*/

/*
  
  while (!time(nullptr)) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("");
  time_t now = time(nullptr);

  
}
  */