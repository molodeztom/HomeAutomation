/*
Hardware:
ESPNowHub V1.0 and CTRL Board V1 with external OLEDD display
and div sensors
Software:
One ESP with ESPNowHub and one with ESPWLANHub firmware needed.
Functions:
receive sensor values over serial interface in JSON format
send sensor values over WLAN in MQTT format
receieve local sensor value from DS18B20
History:
20230130  V0.1:   Initial version compiles not tested for function
20230131  V0.2:   + OTA
20230131  V0.3:   + use DEBUG macro
20230204  V0.4:   c use arrays for sensor, LED Blink, use timers
20230211  V0.5:   c receive values *100 as int to avoid float errors.
20230211  V0.6:   c rename float to int variables
20230211  V0.7:   c remove unneeded debug output
20230211  V0.8:   c all sensors float /100
20230212  V0.9:   c MQTT send using array
20230212  V0.10:  c recieve sensor capabilities over serial use loop for mqtt send
20230212  V0.11:  d calculate correct checksum only used values
20230212  V0.12:  d add iAtmo to checksum only once
20230212  V1.00:  works with 2 different sensors now
20231217  V1.01:  c unused sensors shall not send any mqtt message
20240105  V1.02:  + receive light sensor values and send by mqtt
*/

#include <Arduino.h>
#include <ArduinoOTA.h>
// 1 means debug on 0 means off
#define DEBUG 1
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <PubSubClient.h> //MQTT
#include "Wire.h"

#include <HomeAutomationCommon.h>

const String sSoftware = "ESPWLANHub V1.02";
const size_t capacity = JSON_OBJECT_SIZE(8) + 256;
StaticJsonDocument<capacity> jsonDocument;

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

/***************************
 * MQTT Settings
 ***************************/
WiFiClient MQTT_client;
PubSubClient mqttClient(MQTT_client);

/***************************
 * OTA
 **************************/
#define MYHOSTNAME "HubESPWLAN"

/***************************
 * Serial Rx variables
 **************************/

// SoftwareSerial mySerial (rxPin, txPin, inverse_logic);
// want to transmit the date to the main station over a different serial link than the one used by the monitor
//  SoftwareSerial swSer(D6, D7, false); // only first (RX) used
static boolean recvInProgress = false;
char cSerialRxIn[nMaxRxArray]; // Serial Rx Buffer
bool newData = false;          // Flag RxData received
bool bLedState = false;

// timing
long lMQTTLoop = 0;
const unsigned long ulMQTTTimer = 1 * 1000UL; // time in sec
long lOneMinuteTime = 0;
const unsigned long ulOneMinuteTimer = 60 * 1000UL; // time in sec
const int iSensorTimeout = 3;                       // minutes
long lSecondsTime = 0;
const unsigned long ulSecondsInterval = 1 * 1000UL; // time in sec TIMER

// forward declarations
void wifiConnectStation();
void recvSerialwStartEndMarkers();
void showSerialRead();
void readJSONMessage();
void sendMQTTMessage();

void setup()
{
  Serial.begin(swBAUD_RATE);
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.println(sSoftware);

  // WIFI Setup
  WiFi.persistent(false); // https://www.arduinoforum.de/arduino-Thread-Achtung-ESP8266-schreibt-bei-jedem-wifi-begin-in-Flash
  wifiConnectStation();
  if (WiFi.status() == WL_CONNECTED)
  {
    uint8_t macAddr[6];
    WiFi.macAddress(macAddr);
    Serial.printf("Connected, mac address: %02x:%02x:%02x:%02x:%02x:%02x\n", macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
  }
  mqttClient.setServer(SERVER, SERVERPORT);
  mqttClient.setKeepAlive(10);
  // Set timeout counter to maximum to provoke error on startup
  for (int n = 0; n < nMaxSensors; n++)
  {
    sSensor[n].iTimeSinceLastRead = iSensorTimeout;
  }

  // digitalWrite(LED_BUILTIN, ledState);
  ArduinoOTA.setHostname(MYHOSTNAME);
  ArduinoOTA.setPassword(OTA_PWD);
  ArduinoOTA.begin();
  debugln("Setup done");
}

void loop()
{

  // Receive serial message from weather station
  recvSerialwStartEndMarkers();
  ArduinoOTA.handle();
#if DEBUG == 1
  showSerialRead();
#endif
  // mqttClient.loop(); // MQTT keep alive
  //  At least one sensor reading is available
  if (newData)
  {
    newData = false;
    debugln("New data arrived");
    readJSONMessage();
    sendMQTTMessage();
  }

  // also used to send MQTT error every interval
  if ((millis() - lSecondsTime > ulSecondsInterval))
  {

    if (bLedState == LOW)
    {
      bLedState = HIGH;
    }
    else
    {
      bLedState = LOW;
    }

    digitalWrite(LED_BUILTIN, bLedState);
    // debugln("1 Second");
    //  TODO Sensor 4 was not incremented in original code
    //   will be true if after a while none of the sensors gets data. Then we need to send ERR MQTT Message

    lSecondsTime = millis();
  }

  if ((millis() - lOneMinuteTime > ulOneMinuteTimer))
  {

    debugln("1 Minute");
    // Every minute reading count up to detect timeout and then do not display
    // counter goes up to iSensSerialTimeout if not reset during a sensor reading
    for (int n = 0; n < nMaxSensors; n++)
    {
      sSensor[n].iTimeSinceLastRead++;
    }
    lOneMinuteTime = millis();
  }

  if ((millis() - lMQTTLoop > ulMQTTTimer))
  {
    mqttClient.loop(); // MQTT keep alive
    lMQTTLoop = millis();
  }
}
// Read JSON values bz name from serial recieved message
void readJSONMessage()
{
  int iSensor;         // Sensor number
  int iCheckSumLocal;  // Checksum computed local
  int iCheckSumRec;    // Checksum received serial
  bool bError = false; // error handling

  DeserializationError err = deserializeJson(jsonDocument, cSerialRxIn);
  if (err)
  {
    debug(F("deserializeJson failed with code "));
    debugln(err.c_str());
    bError = true;
  };
  if (bError == false)
  {
    // get single values from JSON to local values if not found set default value to recognize the problem
    // common values for all sensors even if a measurement is not supported
    iSensor = jsonDocument["sensor"] | InvalidMeasurement;
    if (iSensor < 0 || iSensor > 6)
    {
      debugln("sensor number out of range");
      bError = true; // abort
    }
  }
  if (bError == false)
  {

#ifdef DEBUG
    int serialCount; // displays a counter received from BaseStation
    serialCount = jsonDocument["time"];
    debug("Deserialize Sensor nr: ");
    debugln(iSensor);
    debugln(serialCount);
#endif

    sSensor[iSensor].sSensorCapabilities = jsonDocument["SensCap"];
    sSensor[iSensor].iTempA = jsonDocument["iTempA"] | InvalidMeasurement;
    sSensor[iSensor].iTempB = jsonDocument["iTempB"] | InvalidMeasurement;
    sSensor[iSensor].iHumi = jsonDocument["iHumi"] | InvalidMeasurement;
    sSensor[iSensor].iVolt = jsonDocument["iVolt"] | InvalidMeasurement;
    sSensor[iSensor].iLight = jsonDocument["iLight"] | InvalidMeasurement;
    sSensor[iSensor].iAtmo = jsonDocument["iAtmo"] | InvalidMeasurement;
    sSensor[iSensor].nLux = jsonDocument["nLux"] | InvalidMeasurement;
    sSensor[iSensor].nRed = jsonDocument["nRed"] | InvalidMeasurement;
    sSensor[iSensor].nGreen = jsonDocument["nGreen"] | InvalidMeasurement;
    sSensor[iSensor].nBlue = jsonDocument["nBlue"] | InvalidMeasurement;
    sSensor[iSensor].nClear = jsonDocument["nClear"] | InvalidMeasurement;
    sSensor[iSensor].nColorTemp = jsonDocument["nColor"] | InvalidMeasurement;
    iCheckSumRec = jsonDocument["iCSum"] | InvalidMeasurement;
    iCheckSumLocal = sSensor[iSensor].sSensorCapabilities;
    // use only values we really received for checksum calculation
    if (sSensor[iSensor].iTempA != InvalidMeasurement)
      iCheckSumLocal += sSensor[iSensor].iTempA;
    if (sSensor[iSensor].iTempB != InvalidMeasurement)
      iCheckSumLocal += sSensor[iSensor].iTempB;
    if (sSensor[iSensor].iHumi != InvalidMeasurement)
      iCheckSumLocal += sSensor[iSensor].iHumi;
    if (sSensor[iSensor].iVolt != InvalidMeasurement)
      iCheckSumLocal += sSensor[iSensor].iVolt;
    if (sSensor[iSensor].iLight != InvalidMeasurement)
      iCheckSumLocal += sSensor[iSensor].iLight;
    if (sSensor[iSensor].iAtmo != InvalidMeasurement)
      iCheckSumLocal += sSensor[iSensor].iAtmo;
    if (sSensor[iSensor].nLux != InvalidMeasurement)
      iCheckSumLocal += sSensor[iSensor].nLux;
    if (sSensor[iSensor].nRed != InvalidMeasurement)
      iCheckSumLocal += sSensor[iSensor].nRed;
    if (sSensor[iSensor].nGreen != InvalidMeasurement)
      iCheckSumLocal += sSensor[iSensor].nGreen;
    if (sSensor[iSensor].nBlue != InvalidMeasurement)
      iCheckSumLocal += sSensor[iSensor].nBlue;
    if (sSensor[iSensor].nClear != InvalidMeasurement)
      iCheckSumLocal += sSensor[iSensor].nClear;
    if (sSensor[iSensor].nColorTemp != InvalidMeasurement)
      iCheckSumLocal += sSensor[iSensor].nColorTemp;

    if ((iCheckSumRec == InvalidMeasurement) || (iCheckSumRec != iCheckSumLocal))
    {
      debug("Checksum Error recieved / calculated: ");
      debugln(iCheckSumRec);
      debugln(iCheckSumLocal);

      bError = true;
    }
    else
    {
      // on successful reading set true and begin time from 0
      sSensor[iSensor].bSensorRec = true;
      sSensor[iSensor].iTimeSinceLastRead = 0;
    }
  }
}

// Send all sensor values via MQTT to home server raspberry
// send errors per sensor value in a separate channel, sensor data is stored in an array with float values
void sendMQTTMessage()
{
  char valueStr[20]; // helper to convert string to MQTT char
  char cChannelName[50];
  char cErrorText[10];
  bool bError = false;
  debugln("sendMQTT message");
  if (!mqttClient.connected())
  {
    debug("MQTT not connected, rc=");
    Serial.println(mqttClient.state());
    debugln("reconnect MQTT");
    // Attempt to connect
    if (mqttClient.connect("MQTT_CLIENTD", MQTT_USERNAME, MQTT_KEY))
    {
      debugln("MQTT connected");
    }
    else
    {
      debug("MQTT failed, rc=");
      debugln(mqttClient.state());
      bError = true;
    }
  }
  if (bError == false)
  {
    // check all sensors if valid data available, send MQTT message according to sensor capabilities, sensor 0 = sensor loc
    for (int i = 0; i < nMaxSensors; i++)
    {
      if (sSensor[i].bSensorRec == true)
      {
        uint16_t sSensorCapabilities = sSensor[i].sSensorCapabilities;

        debug("Sensor Capabilities: ");
        debugln(sSensorCapabilities);
        debugln("channels sent: ");

        if (sSensorCapabilities & TEMPA_ON)
        {
          sprintf(cChannelName, "Sensor%i/fTempA", i);
          dtostrf(float(sSensor[i].iTempA) / 100, 4, 3, valueStr);
          mqttClient.publish(cChannelName, valueStr);
          debug(cChannelName);
          debug(" ");
          debugln(valueStr);
        }
        if (sSensorCapabilities & TEMPB_ON)
        {
          sprintf(cChannelName, "Sensor%i/fTempB", i);
          dtostrf(float(sSensor[i].iTempB) / 100, 4, 3, valueStr);
          mqttClient.publish(cChannelName, valueStr);
          debug(cChannelName);
          debug(" ");
          debugln(valueStr);
        }

        if (sSensorCapabilities & VOLT_ON)
        {
          sprintf(cChannelName, "Sensor%i/fVolt", i);
          dtostrf(float(sSensor[i].iVolt) / 100, 4, 3, valueStr);
          mqttClient.publish(cChannelName, valueStr);
          debug(cChannelName);
          debug(" ");
          debugln(valueStr);
        }

        if (sSensorCapabilities & HUMI_ON)
        {
          sprintf(cChannelName, "Sensor%i/fHumi", i);
          dtostrf(float(sSensor[i].iHumi) / 100, 4, 3, valueStr);
          mqttClient.publish(cChannelName, valueStr);
          debug(cChannelName);
          debug(" ");
          debugln(valueStr);
        }

        if (sSensorCapabilities & LIGHT_ON)
        {
          sprintf(cChannelName, "Sensor%i/iLight", i);
          dtostrf(float(sSensor[i].iLight) / 100, 4, 3, valueStr);
          mqttClient.publish(cChannelName, valueStr);
          debug(cChannelName);
          debug(" ");
          debugln(valueStr);
          // TODO use different capabilities for light and RGB light
          sprintf(cChannelName, "Sensor%i/nLux", i);
          dtostrf(float(sSensor[i].nLux), 4, 0, valueStr);
          mqttClient.publish(cChannelName, valueStr);
          debug(cChannelName);
          debug(" ");
          debugln(valueStr);

          sprintf(cChannelName, "Sensor%i/nRed", i);
          dtostrf(float(sSensor[i].nRed), 4, 0, valueStr);
          mqttClient.publish(cChannelName, valueStr);

          debug(cChannelName);
          debug(" ");
          debugln(valueStr);
          sprintf(cChannelName, "Sensor%i/nGreen", i);
          dtostrf(float(sSensor[i].nGreen), 4, 0, valueStr);
          mqttClient.publish(cChannelName, valueStr);
          debug(cChannelName);
          debug(" ");
          debugln(valueStr);
          sprintf(cChannelName, "Sensor%i/nBlue", i);
          dtostrf(float(sSensor[i].nBlue), 4, 0, valueStr);
          mqttClient.publish(cChannelName, valueStr);
          debug(cChannelName);
          debug(" ");
          debugln(valueStr);
          sprintf(cChannelName, "Sensor%i/nClear", i);
          dtostrf(float(sSensor[i].nClear), 4, 0, valueStr);
          mqttClient.publish(cChannelName, valueStr);
          debug(cChannelName);
          debug(" ");
          debugln(valueStr);
          sprintf(cChannelName, "Sensor%i/nColor", i);
          dtostrf(float(sSensor[i].nColorTemp), 4, 0, valueStr);
          mqttClient.publish(cChannelName, valueStr);
          debug(cChannelName);
          debug(" ");
          debugln(valueStr);
        }
        if (sSensorCapabilities & ATMO_ON)
        {
          sprintf(cChannelName, "Sensor%i/iAtmo", i);
          dtostrf(float(sSensor[i].iAtmo) / 100, 4, 3, valueStr);
          mqttClient.publish(cChannelName, valueStr);
          debug(cChannelName);
          debug(" ");
          debugln(valueStr);
        }
        if (sSensorCapabilities & OPT1_ON)
        {
          debugln("OPT1_ON");
        }

        if (sSensorCapabilities & OPT2_ON)
        {
          debugln("OPT2_ON");
        }

        if (sSensorCapabilities & OPT3_ON)
        {
          debugln("OPT3_ON");
        }

        sSensor[i].bSensorRec = false;
      }

      else
      {
        // if sensor was not read for a long time send mqtt error do not send anything if sensor is unused (recognized by SensorCapabilities not set)
        if ((sSensor[i].iTimeSinceLastRead > iSensorTimeout) && (sSensor[i].sSensorCapabilities != 0))
        {
          sprintf(cChannelName, "Sensor%i/Err", i);
          mqttClient.publish(cChannelName, "1");
          sprintf(valueStr, "Error Sensor%i ", i);
          debug(valueStr);
          debugln(sSensor[i].iTimeSinceLastRead);
        }
      }
    }
  }
}

// recieve payload between start and end marker (exluded) from serial
void recvSerialwStartEndMarkers()
{
  static char cRx;     // one byte read
  static byte nRx = 0; // received chars counter

  while (Serial.available() > 0 && newData == false)
  {
    cRx = Serial.read();
    if (recvInProgress == true)
    {
      if (cRx != endMarker)
      {
        // read payload
        cSerialRxIn[nRx] = cRx;
        nRx++;
        if (nRx >= nMaxRxArray)
        {
          debugln("Array too small. Size:");
          nRx = 0; // too much stop reading
          recvInProgress = false;
          newData = false;
        }
      }
      else
      {
        // end Marker found
        recvInProgress = false;
        nRx = 0;
        newData = true;
      }
    }
    else if (cRx == startMarker)
    {

      recvInProgress = true;
    }
  } // end while serial available
} // end recvSerial..

void showSerialRead()
{

  if (newData == true)
  {
    debug("input read: ");
    debugln(cSerialRxIn);
  }
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
}
