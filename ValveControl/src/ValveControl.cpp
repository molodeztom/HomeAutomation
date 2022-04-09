/**************************************************************************
Home Automation Project
   Valve Control for central heating 

  Hardware: 
  ESP Node MCU 0.1
  MCP4725A1 I2C to 0 - 5 V DC output
  LMxx to amplify 0-5 V DC to 0-10 V DC
  
  Connect to MQTT Server and subscribe valve closing rate
  control. Measure IN and OUT Temp. Display values on LCD. Send Temp Values with MQTT. 

  History: master if not shown otherwise
  20200509  V0.1:  Copy from Adafruit example for MCP4725A1
  20200510  V0.2:  Output 3 fixed values to test valve behaviour
  20200510  V0.3:  Get SetPoint as MCP digits from MQTT as int and set valve accordingly
  20201019  V0.4:  Converted to visual studio code project
  20201230  V0.5:  Include secrets.h software string output (valveCurTest)
                   + INA219 sensor tests sucessfull (valveCurTest)
                   + Temperature Sensor 1 added
                   c publish current values via mqtt
  20201230  V0.6:  +LCD display with hello world
  20210101  V0.7:  c Values for valve 0-5 instead of 4095
  20210101  V0.8:  + Watchdog for MQTT and error msg display send errors with MQTT to server 
  20210101  V0.9:  b ActValue leading blanks removed
  20210102  V0.10: + Light sensor GY-302 BH1750, switch off LCD backlight when dark (sic)
  20210102  V0.11: + tactile switch for manual valve select
  20210102  V0.12: + on valve pos 1-2 use pos 3 but open and close in timed intervalls 
  20210821  V0.13: c PCB Hardware introduced testing 
  20210821  V0.14: + OTA
  20210826  V0.15: + PC875 parallel to I2C interface
  20210828  V0.16: + https://github.com/xreef/PCF8575_library
  20210829  V0.17: c use PC8575 lib for breadboard tests
  20210829  V0.18: c Input with tactile switch over PC857x now working, debut codes removed
  20210829  V0.19: c Lightsensor back to code
  20210830  V0.20: c Timing back to original values (due to mqtt connect errors), Valve ON display bug corrected
  20210830  V0.21: + ErrorNumber to reset errors specifically
  20211127  V0.22: c Adapt to Ctrl PCB disable Switch and test LEDs with running light
  20211127  V0.23: c Switch LED 1 according to switch 1
  20211230  V0.24: c LED with defines, LED with function
  20211230  V0.25: c LED according to function
  20211230  V0.26: c Switches according to function
  20211231  V0.27: c Light sensor less 
  20211231  V1.00: i Released and working fixed on wall, MQTT Man Auto Mode published, start with Auto Mode
  20220101  V1.01: c On interval mode open to pos 3 instead of 5, mode1 off time greater than on time
  20220102  V1.02: b Mode1 changed interval handling, off time double on time, fixed boolean operations bug
  20220102  V1.03: b Interval2 counter was missing always 1 minute, Clean Code remove ToDos
  20220107  V1.04: c On error activate red led, show error permanently on display until quit by red switch
  20220107  V1.05: c Do not reset bError automatically, send text on Err channel on startup
   

**************************************************************************/

#include <Arduino.h>
#include <ArduinoOTA.h>
//#define DEBUG
#include <Wire.h>
#include <Adafruit_MCP4725.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <PubSubClient.h> //MQTT
#include <LiquidCrystal_I2C.h>
#include <BH1750.h>

#include "D:\Arduino\HomeAutomationSecrets.h"
//libs for DS18B20
#include "Spi.h"
#include <DallasTemperature.h>
#include "PCF8574.h"

const String sSoftware = "ValveCtrl V1.05";

/***************************
 * LCD Settings
 **************************/
LiquidCrystal_I2C lcd(0x27, 20, 4); // set the LCD address to 0x27 for a 16 chars and 2 line display
/* I2C Bus */
//if you use ESP8266-01 with not default SDA and SCL pins, define these 2 lines, else delete them
// use Pin Numbers from GPIO e.g. GPIO4 = 4
// For NodeMCU Lua Lolin V3: D1=GPIO5 = SCL D2=GPIO4 = SDA set to SDA_PIN 4, SCL_PIN 5, D7=13
#define SDA_PIN 4
#define SCL_PIN 5
#define ONE_WIRE_PIN 12

/***************************
 * TempSensor Settings
 **************************/
//pin GPIO2 (NodeMCU) D4 =  GPIO4  (ESP12-F Modul) use GPIO Number, when using higher pins may need to setup INPUT_PULLUP
//const byte bus = 12;
#define SENSCORR1A -0.3 //calibration
OneWire oneWire(ONE_WIRE_PIN);
DallasTemperature sensoren(&oneWire);
//Array to store temp sensor adress
DeviceAddress TempSensAdress;
//Datenstruktur f�r den Datenaustausch
#define CHAN 5 // device channel 1 = Developmentboard 2 = ESP gel�tet 5 = Valve Control

/***************************
 * Ctrl PCB Settings
 **************************/
#define LEDGB P4
#define LEDGN P5
#define LEDBL P6
#define LEDRT P7
#define SWGB P0
#define SWGN P1
#define SWBL P2
#define SWRT P3

byte gradeChar[8] = {
    0b00111,
    0b00101,
    0b00111,
    0b00000,
    0b00000,
    0b00000,
    0b00000,
    0b00000};

struct DATEN_STRUKTUR
{
  int chan = CHAN;
  float temp1 = -127; //Aussen A
  float temp2 = -127; //Aussen B
  float fHumidity = -1;
  float fVoltage = -1; //Batterie Sensor
};

/***************************
 * WiFI and MQTT Settings
 **************************/
WiFiClient MQTT_client;
PubSubClient mqttClient(MQTT_client);
//SubChannel ValvPos to receive positions from master
//SubChannel ValvMeasurement to send temperatures, volt, current ....

#define USERNAME "TestName/"
#define T_CHANNEL "ValveControl"
#define T_COMMAND "command"
#define MYHOSTNAME "ValveControl"

/***************************
 * Measurement Variables
 **************************/
DATEN_STRUKTUR data;
const int mqttRecBufSiz = 50;
char mqttBufValvPos[mqttRecBufSiz];
int iValvPosSetP;          //Setpoint Valve Position
int iManMode = 6;          //6 = auto, 0-5 manual valve setting
int iAutoValue = 5;        //value for valve from MQTT keeps always last updated value
int iValveIntervalCnt = 0; //Counter valve interval in seconds
bool bValveOn = true;      //Valve on/off used for interval
int iActValvPos = 5;       //Valve position actually set

/***************************
 * Timing
 **************************/

const unsigned long ulMQTTInterval = 3 * 1000UL; //call MQTT server
long lMQTTTime = 0;
const unsigned long ulValveSetInterval = 60 * 1000UL; //Normal 60 set valve, measure temp, send mqtt  60 sec
long lValveSetTime = 0;
const unsigned long ulUpdateLCDInterval = 0.3 * 1000UL; //normal 1 Write text on LCD, get button, count time for valve interval, do not change
long lUpdateLCDTime = 0;
int iValvInterval2 = 4;                  //interval valve value in minutes (depending on ulValveSetInterval set to 60) 5 minutes on, can be changed per mqtt remotely
int iValvInterval1 = iValvInterval2 * 2; //interval valve value 1

/***************************
 * Digital Analog Converter
 **************************/

Adafruit_MCP4725 dac;

int iMCP4725Adr = 0x60;
int iMCPMaxCode = 4096; //code for max output

/***************************
 * Light Sensor GY-302 BH1750
 **************************/
int iGY302Adr = 0x23;
BH1750 lightSensor;
float fLux = -127;
#define MIN_BACKLIGHT_LUX 1

/***************************
 * PCF857XC I2C to parallel 
 **************************/
int iPCF857XAdr = 0x20;
bool LedOn = false;
bool bKeyPressed = false;
PCF8574 pcf857X(iPCF857XAdr);

/***************************
 * otherSettings
 **************************/
#define swBAUD_RATE 115200
bool bError = false; //Error
String sErrorText = "";
String sLastErrorText = ""; //store last Error to send to Server
bool bErrorDisp = false;
int iCount = 0;     //Counter for display to show its running
int iErrNumber = 0; //helper to reset an error, is set to error if condition of that error not true bError is reset

//Forward declarations
void printAddress(DeviceAddress adressen);
void wifiConnectStation();
void mQTTConnect();
void updateLCDandLED();
void ledOn(uint8_t LedNr);
void ledOff(uint8_t LedNr);

void setup(void)
{
  bool bDACStatus;
  Serial.begin(swBAUD_RATE);
  Serial.println("");
  Serial.println(sSoftware);
  pinMode(ONE_WIRE_PIN, INPUT_PULLUP);

  Wire.begin(SDA_PIN, SCL_PIN);

  /* LCD */
  lcd.init(); // initialize the lcd
  lcd.backlight();
  //lcd.noBacklight();
  lcd.setCursor(0, 0);
  lcd.print("Init");
  lcd.setCursor(0, 1);
  lcd.print(sSoftware);
  delay(10000);

  bDACStatus = dac.begin(iMCP4725Adr); //init dac with I2C adress
  if (bDACStatus == false)
  {
    bError = true;
    iErrNumber = 1;
    sErrorText = "1 DACnotFound ";
    Serial.println("1 DAC not found   ");
  }

  iValvPosSetP = 5; //Initial value for valve should be 5 V. The valve will automatically recognize 0-10 V regulation mode.
  iAutoValue = 5;
  iValveIntervalCnt = 0;
  bValveOn = true;
  //Light Sensor use high res mode and default adress
  lightSensor.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, iGY302Adr);
  lightSensor.setMTreg(220); //more resolution

  //WIFI Setup
  WiFi.persistent(false); //https://www.arduinoforum.de/arduino-Thread-Achtung-ESP8266-schreibt-bei-jedem-wifi-begin-in-Flash
  wifiConnectStation();
  if (WiFi.status() == WL_CONNECTED)
  {
    uint8_t macAddr[6];
    WiFi.macAddress(macAddr);
    Serial.printf("Connected, mac address: %02x:%02x:%02x:%02x:%02x:%02x\n", macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
  }
  //OTA Setup PWD comes from HomeAutomationSecrets.h outside repository
  ArduinoOTA.setHostname(MYHOSTNAME);
  ArduinoOTA.setPassword(OTA_PWD);
  ArduinoOTA.begin();

  //Connect mQTT
  mqttClient.setServer(SERVER, SERVERPORT);

  if (!mqttClient.connected())
  {
    mQTTConnect();
  }

        mqttClient.publish(T_CHANNEL "/Err", "startup");


  //initialize pcf8574
  pcf857X.begin();
  pcf857X.pinMode(SWGB, INPUT);
  pcf857X.pinMode(SWGN, INPUT);
  pcf857X.pinMode(SWRT, INPUT);
  pcf857X.pinMode(SWBL, INPUT);
  pcf857X.pinMode(LEDRT, OUTPUT);
  pcf857X.pinMode(LEDGN, OUTPUT);
  pcf857X.pinMode(LEDGB, OUTPUT);
  pcf857X.pinMode(LEDBL, OUTPUT);

  //LED Test
  ledOn(LEDRT);
  ledOn(LEDGN);
  ledOn(LEDGB);
  ledOn(LEDBL);
  delay(1000);
  ledOff(LEDRT);
  ledOff(LEDGN);
  ledOff(LEDGB);
  ledOff(LEDBL);

  //initialize temp sensors
  sensoren.begin();

  //Nun prüfen wir ob einer der Sensoren am Bus ein Temperatur Sensor bis zu 2 werden gelesen
  if (!sensoren.getAddress(TempSensAdress, 0))
  {
    Serial.println("2: Kein Temperatursensor vorhanden!");
    bError = true;
    iErrNumber = 2;
    sErrorText = "2 No TempSens1   ";
  }
//adressen anzeigen
#ifdef DEBUG
  Serial.print("Adresse1: ");
  printAddress(adressen);
  Serial.println();
#endif

  //2. Sensor
  if (!sensoren.getAddress(TempSensAdress, 1))
  {
    Serial.println("3: Kein Temperatursensor vorhanden!");
    bError = true;
    iErrNumber = 3;
    sErrorText = "3 No TempSens2  ";
  }
#ifdef DEBUG
  //adressen anzeigen

  Serial.print("Adresse2: ");
  printAddress(adressen);
  Serial.println();
#endif

#ifdef DEBUG
  //Nun setzen wir noch die gewünschte Auflösung (9, 10, 11 oder 12 bit)
  sensoren.setResolution(adressen, 12);
  Serial.print("Aufl�sung = ");
  Serial.print(sensoren.getResolution(adressen), DEC);
  Serial.println();
#endif

  //Temperaturmessung starten
  sensoren.requestTemperatures();

  delay(750); //750 ms warten bis die Messung fertig ist

  //Temperaturwert holen und in Datenstruktur zum Senden speichern
  data.temp1 = sensoren.getTempCByIndex(0) + SENSCORR1A;
  data.temp2 = sensoren.getTempCByIndex(1);

  Serial.println("Setup done");
  delay(1000);
  lValveSetTime = ulValveSetInterval - 100; //trigger first event sooner
}

void loop(void)
{

  // call MQTT loop within defined timeframe reconnect if connection lost
  if ((unsigned long)(millis() - lMQTTTime) > ulMQTTInterval)
  {
    bool bMQTTalive = mqttClient.loop();
    //OTA Functions
    ArduinoOTA.handle();

    if (WiFi.status() not_eq WL_CONNECTED)
    {
      sErrorText = "5 No WLAN         ";
      iErrNumber = 5;
      bError = true;
    }
    else
    {
      //WLAN ok check MQTT
      if (bMQTTalive == false)
      {
        Serial.println(" mqttClient loop failure");
        bError = true;
        iErrNumber = 4;
        sErrorText = "4 MQTTFailure   ";
      }
      /* TODO remove testing error quit button
      else
      {
                //Mqtt works reset anz WLAN or mqtt error
        if ((iErrNumber == 4) | (iErrNumber == 5))
        {
          //reset previous MQTT or WLAN error and try again
          bError = false;
          iErrNumber = 0;
        }
      }
      */

      //--------------------------check MQTT connection and retry
      if (!mqttClient.connected())
      {
        mQTTConnect();
      }
    }
    lMQTTTime = millis();
  }
  // slow actions: set valve position every x seconds to current set position
  // measure temp
  if ((unsigned long)(millis() - lValveSetTime) > ulValveSetInterval)
  {
    int iValvVolt;
    bool bDACStatus = false;
    //OTA Functions
    ArduinoOTA.handle();

    //on Error open valve to full TODO seems not good try to not use and see how it works with red LED alarm instead
    if (bError == true)
    {
      //iAutoValue = 5;
      ledOn(LEDRT);
    }

    if (iValvPosSetP == 1)
    {
      //on position 1 we use interval on/off interval 1 is longer then interval 2
      ledOn(LEDGB);
      if ((bValveOn == true) & (iValveIntervalCnt >= iValvInterval2))
      {
        //On time uses shorter interval
        bValveOn = false; //toggle valve
        iActValvPos = 0;  //valve off
        //Serial.println("Int1 OFF");
        //Serial.println("Pos 1 Int2 triggered");
        iValveIntervalCnt = 0;
      }

      else if ((bValveOn == false) & (iValveIntervalCnt >= iValvInterval1))
      {
        //off time uses longer interval
        bValveOn = true; //toggle valve
        iActValvPos = 3; //valve little bit open
        //Serial.println("Int1 ON");
        iValveIntervalCnt = 0;
      }
    }
    //OTA Functions
    ArduinoOTA.handle();

    if (iValvPosSetP == 2)
    {
      //on position 2 we use interval on/off
      ledOn(LEDGB);
      if (iValveIntervalCnt >= iValvInterval2)
      {
        // Serial.println("Int2 triggered");
        iValveIntervalCnt = 0;
        if (bValveOn == true)
        {
          bValveOn = false; //toggle valve
          iActValvPos = 0;  //valve off
                            // Serial.println("Int2 OFF");
        }
        else
        {
          bValveOn = true; //toggle valve
          iActValvPos = 3; //valve little bit open
                           // Serial.println("Int2 ON");
        }
      }
    }
    //OTA Functions
    ArduinoOTA.handle();
    if (iValvPosSetP > 2 or iValvPosSetP == 0)
    {
      ledOff(LEDGB);
      iActValvPos = iValvPosSetP; //valve position actually set, when 3-5 use value directly
      if (iValvPosSetP == 0)
        bValveOn = false;
      else
        bValveOn = true; //on higher levels valve always on according to set value
    }

    //Convert position 0-5 to voltage 0-4095 0 means fully closed 5 means fully open
    iValvVolt = map(iActValvPos, 0, 5, 0, 4095);
#ifdef DEBUG
    Serial.println("Set Valve to: ");
    Serial.println(iActValvPos);
    Serial.println(iValvVolt);
#endif
    bDACStatus = dac.setVoltage(iValvVolt, false);
    if (bDACStatus == false)
    {
      bError = true;
      iErrNumber = 6;
      sErrorText = "6 DACnotFound";
      Serial.println("6 DAC not found");
    }
    else
    {
      if (iErrNumber == 6)
      {
        //reset error
        bError = false;
        iErrNumber = 0;
      }
    }
    //OTA Functions
    ArduinoOTA.handle();

    data.temp1 = 0;
    data.temp2 = 0;
    //Test get temperatures
    //Temperaturmessung starten
    sensoren.requestTemperatures();

    delay(750);                               //750 ms warten bis die Messung fertig ist
    data.temp1 = sensoren.getTempCByIndex(0); //+ SENSCORR1A;
    data.temp2 = sensoren.getTempCByIndex(1);

#ifdef DEBUG
    Serial.print("Temperatur1: ");
    Serial.print(data.temp1);
    Serial.println(" C");
    Serial.print("Temperatur2: ");
    Serial.print(data.temp2);
    Serial.println(" C");
    Serial.print("Light: ");
    Serial.println(fLux);
#endif
    //OTA Functions
    ArduinoOTA.handle();

    //Publish values over MQTT
    char valueStr[20]; //helper for conversion
    dtostrf(iValvVolt, 1, 0, valueStr);
    mqttClient.publish(T_CHANNEL "/ActPos", valueStr);
    dtostrf(iValvPosSetP, 1, 0, valueStr);
    mqttClient.publish(T_CHANNEL "/Mode", valueStr);
    dtostrf(data.temp1, 3, 2, valueStr);
    mqttClient.publish(T_CHANNEL "/TempIn", valueStr);
    dtostrf(data.temp2, 3, 2, valueStr);
    mqttClient.publish(T_CHANNEL "/TempOut", valueStr);
    dtostrf(fLux, 1, 0, valueStr);
    mqttClient.publish(T_CHANNEL "/Light", valueStr);
    //display man / auto mode
    if (iManMode < 6)
      mqttClient.publish(T_CHANNEL "/ManAuto", "MAN");
    else
      mqttClient.publish(T_CHANNEL "/ManAuto", "AUTO");
    //display on/off

    if (bError == true)
    {
      mqttClient.publish(T_CHANNEL "/Err", sLastErrorText.c_str());
      sLastErrorText = sErrorText; // if more than one error send both
    }

    //counter to show alive on LCD
    iCount++;
    if (iCount > 9)
    {
      iCount = 0;
    }
    iValveIntervalCnt++; //count interval seconds
    lValveSetTime = millis();
  }

  //fast actions LCD update, manual switch input; every 1 seconds fixed time
  if ((unsigned long)(millis() - lUpdateLCDTime) > ulUpdateLCDInterval)
  {
    //OTA Functions
    ArduinoOTA.handle();

    //read manual switch each press sets a number from 0 to 6. 6 = auto, 0-5 man valve position

    if ((pcf857X.digitalRead(SWGB)) == HIGH)
    {
      //set to Auto
      iManMode = 6;
    }
    if ((pcf857X.digitalRead(SWBL)) == HIGH)
    {
      //manual value up 6 is max
      iManMode++;
      if (iManMode > 5)
        iManMode = 5;
    }
    if ((pcf857X.digitalRead(SWGN)) == HIGH)
    {
      //manual value up 0 is min
      if (iManMode > 0)
        iManMode--;
    }
    if ((pcf857X.digitalRead(SWRT)) == HIGH)
    {
      //quit error
      bError = false;
    }

    //manual pos override
    if (iManMode < 6)
    {
      //manual get last set value
      iValvPosSetP = iManMode;
    }
    else
    {
      //auto use last value from MQTT

      iValvPosSetP = iAutoValue;
    }

    updateLCDandLED();
    //OTA Functions
    ArduinoOTA.handle();

    //Measure light and switch on backlight if bright

    fLux = lightSensor.readLightLevel();
    if (fLux < MIN_BACKLIGHT_LUX)
    {
      lcd.noBacklight();
    }
    else
      lcd.backlight();

    //lcd.backlight();

    lUpdateLCDTime = millis();
  }
}

void callback(char *topic, byte *data, unsigned int dataLength)
{
  // handle message arrived

#ifdef DEBUG
  Serial.print("Remote valve value: ");
  Serial.println(iAutoValue);
  Serial.println("Topic Recvd: ");
  Serial.print(topic);

#endif

  // check topic and handle assuming there is only one INT in buffer
  // we receive number as a string in MQTT, so read string and convert to int
  if (strcmp(topic, T_CHANNEL "/SetPos") == 0)
  {
    // valve mode value
    memset(&mqttBufValvPos[0], 0, mqttRecBufSiz);
    strncpy(mqttBufValvPos, (char *)data, dataLength);
    mqttBufValvPos[dataLength + 1] = '\0';
    iAutoValue = atoi(mqttBufValvPos);
    //if we recieve MQTT messages everything should be ok again. Reset error.
    //Todo check how it works without reset reset will be done with red button instead
    /*
    if ((iErrNumber == 4) | (iErrNumber == 5))
    {
      //reset previous MQTT or WLAN error and try again
      bError = false;
      iErrNumber = 0;
    }
  }
  else 
  */
  }
    if (strcmp(topic, T_CHANNEL "/SetInt") == 0)
    {
      // interval lenght for mode 1-2
      memset(&mqttBufValvPos[0], 0, mqttRecBufSiz);
      strncpy(mqttBufValvPos, (char *)data, dataLength);
      mqttBufValvPos[dataLength + 1] = '\0';
      iValvInterval2 = atoi(mqttBufValvPos);
      iValvInterval1 = iValvInterval2 * 2;
#ifdef DEBUG
      Serial.print("Interval Set to: ");
      Serial.println(iValvInterval2);
#endif
    }
  }

  void updateLCDandLED()
  {
    // Write display we have 2 columns with each 16 characters
    char valueStr[20]; //helper for conversion
    //lcd.clear();
    lcd.createChar(0, gradeChar);
    lcd.setCursor(0, 0);
    lcd.print("I:");
    dtostrf(data.temp1, 3, 1, valueStr);
    lcd.print(valueStr);
    lcd.setCursor(6, 0);
    lcd.print(" O:");
    dtostrf(data.temp2, 3, 1, valueStr);
    lcd.print(valueStr);
    lcd.print(" ");
    lcd.setCursor(14, 0);
    lcd.write(0);
    lcd.print("C");

    if (bError == true && bErrorDisp == true)
    {
      lcd.setCursor(0, 1);
      lcd.print(sErrorText);
      ledOn(LEDRT);
      bErrorDisp = false; //display error alternately
    }
    else
    {
      lcd.setCursor(0, 1);
      lcd.print("Vent:");
      lcd.print(iValvPosSetP);
      ledOff(LEDRT);
      //display man / auto mode
      if (iManMode < 6)
        lcd.print(" Man");
      else
        lcd.print(" Aut");
      //display on/off
      if (bValveOn == true)
      {
        lcd.print(" Auf");
        ledOn(LEDGN);
      }

      else
      {
        lcd.print(" Zu ");
        ledOff(LEDGN);
      }

      bErrorDisp = true; //display error alternately
    }
    lcd.setCursor(15, 1);
    lcd.print(iCount);
  }

  //Connect to local WIFI
  void wifiConnectStation()
  {
    WiFi.mode(WIFI_STA);
    WiFi.hostname(MYHOSTNAME);
#ifdef DEBUG
    Serial.println("WifiStat connecting to ");
    Serial.println(WIFI_SSID);
#endif
    WiFi.begin(WIFI_SSID, WIFI_PWD);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Connect to WLAN");
    lcd.setCursor(0, 1);
    int iCount = 0;
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(250);
      Serial.print(".");
      lcd.print(".");
      iCount++;
      if (iCount > 16)
      {
        lcd.setCursor(0, 1);
        lcd.print("                ");
        lcd.setCursor(0, 1);
        iCount = 0;
      }
    }
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Waiting");
    Serial.println();
  }

  //Connect to MQTT Server
  void mQTTConnect()
  {
    if (!mqttClient.connected())
    {
#ifdef DEBUG
      Serial.println("Attempting MQTT connection...");
      Serial.println(MQTT_USERNAME);
      Serial.print(" ");
      Serial.print(MQTT_KEY);

#endif
      // Attempt to connect
      if (mqttClient.connect("", MQTT_USERNAME, MQTT_KEY))
      {
        Serial.println("connected");

        mqttClient.subscribe(T_CHANNEL "/SetPos");

        mqttClient.subscribe(T_CHANNEL "/SetInt");

        mqttClient.setCallback(callback);
        //if mqtt error reset on next connect
        if(iErrNumber == 4) bError = false;
      }
      else
      {
        Serial.print("failed, rc=");
        Serial.print(mqttClient.state());
        bError = true;
        iErrNumber = 4;
        sErrorText = "4 MQTTFailure   ";
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

  // function um eine Sensoradresse zu drucken
  void printAddress(DeviceAddress adressen)
  {
    for (uint8_t i = 0; i < 8; i++)
    {
      if (adressen[i] < 16)
        Serial.print("0");
      Serial.print(adressen[i], HEX);
      Serial.print(":");
    }
  }
