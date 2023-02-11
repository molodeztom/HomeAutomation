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
20230130  V0.1: Initial version compiles not tested for function
20230131  V0.2: + OTA
20230131  V0.3: + use DEBUG macro
20230204  V0.4: c use arrays for sensor, LED Blink, use timers
20230311  V0.5: c receive values *100 as int to avoid float errors.
20230311  V0.6: c rename float to int variables
20230311  V0.7: c remove unneeded debug output
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

const String sSoftware = "ESPWLANHub V0.7";
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
const unsigned long ulMQTTTimer = 5 * 1000UL; // time in sec
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
    debugln("MQTT Client loop");
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
    if (iSensor < 0 || iSensor > 5)
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
    sSensor[iSensor].iTempA = jsonDocument["iTempA"] | InvalidMeasurement;
    sSensor[iSensor].iHumi = jsonDocument["iHumi"] | InvalidMeasurement;
    sSensor[iSensor].iVolt = jsonDocument["iVolt"] | InvalidMeasurement;
    sSensor[iSensor].iLight = jsonDocument["iLight"] | InvalidMeasurement;
    sSensor[iSensor].iAtmo = jsonDocument["iAtmo"] | InvalidMeasurement;
    sSensor[iSensor].iTempA = jsonDocument["iTempA"] | InvalidMeasurement;
    iCheckSumRec = jsonDocument["iCSum"] | InvalidMeasurement;
    iCheckSumLocal = sSensor[iSensor].iTempA + sSensor[iSensor].iHumi + sSensor[iSensor].iVolt + sSensor[iSensor].iLight + sSensor[iSensor].iAtmo;

    if ((iCheckSumRec == InvalidMeasurement) || (iCheckSumRec != iCheckSumLocal))
    {
      debug("Checksum Error: ");
      debug(iCheckSumRec);
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
  bool bError = false;
  debugln("F: sendMQTT message");
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
      Serial.println(mqttClient.state());
      bError = true;
    }
  }
  if (bError == false)
  {
    // TODO replace this by using an array and creating the MQTT channel names with a string operation
    // TODO uncomment mqtt send
    //  Sensor Loc
    debugln("check for valid sensor readings");
    if (sSensor[0].bSensorRec == true)
    {

      dtostrf(sSensor[0].iLight, 3, 0, valueStr);
      // mqttClient.publish("SensorLoc/iLight", valueStr);
      dtostrf(sSensor[0].iAtmo, 4, 2, valueStr);
      // mqttClient.publish("SensorLoc/iAtmo", valueStr);
      sSensor[0].bSensorRec = false;

      debugln("MQTT send SensorLoc");
    }
    else
    {
      if (sSensor[0].iTimeSinceLastRead > iSensorTimeout)
        //  mqttClient.publish("SensorLoc/Err", "1");
        debugln("Error Sens0");
      debugln(sSensor[0].iTimeSinceLastRead);
    }
    // Sensor 1
    if (sSensor[1].bSensorRec == true)
    {
      dtostrf((sSensor[1].iTempA / 100), 4, 2, valueStr);
      mqttClient.publish("Sensor1/iTempA", valueStr);
      dtostrf(sSensor[1].iVolt, 4, 2, valueStr);
      mqttClient.publish("Sensor1/iVolt", valueStr);
      dtostrf(sSensor[1].iHumi, 4, 2, valueStr);
      mqttClient.publish("Sensor1/iHumi", valueStr);
      sSensor[1].bSensorRec = false;
      debugln("MQTT send Sensor1");
    }
    else
    {
      if (sSensor[1].iTimeSinceLastRead > iSensorTimeout)

        // mqttClient.publish("Sensor1/Err", "1");
        debugln("Error Sens1");
      debugln(sSensor[1].iTimeSinceLastRead);
    }
    // Sensor 2
    if (sSensor[2].bSensorRec == true)
    {
      dtostrf((sSensor[2].iTempA) / 100, 4, 2, valueStr);
      mqttClient.publish("Sensor2/iTempA", valueStr);
      dtostrf(sSensor[2].iVolt, 4, 2, valueStr);
      mqttClient.publish("Sensor2/iVolt", valueStr);
      sSensor[2].bSensorRec = false;

      debugln("MQTT send Sensor2");
    }
    else
    {
      if (sSensor[2].iTimeSinceLastRead > iSensorTimeout)
      {
        //  mqttClient.publish("Sensor2/Err", "1");
        debugln("Error Sens2");
        debugln(sSensor[2].iTimeSinceLastRead);
      }
    }
    // Sensor 3
    if (sSensor[3].bSensorRec == true)
    {
      dtostrf(sSensor[3].iTempA, 4, 2, valueStr);
      mqttClient.publish("Sensor3/iTempA", valueStr);
      dtostrf(sSensor[3].iVolt, 4, 2, valueStr);
      mqttClient.publish("Sensor3/iVolt", valueStr);
      sSensor[3].bSensorRec = false;

      debugln("MQTT send Sensor3");
    }
    else
    {
      if (sSensor[3].iTimeSinceLastRead > iSensorTimeout)
        // mqttClient.publish("Sensor3/Err", "1");
        debugln("Error Sens3");
      debugln(sSensor[3].iTimeSinceLastRead);
    }
    // Sensor 4
    if (sSensor[4].bSensorRec == true)
    {
      dtostrf(sSensor[4].iTempA, 4, 2, valueStr);
      mqttClient.publish("Sensor4/iTempA", valueStr);
      dtostrf(sSensor[4].iTempB, 4, 2, valueStr);
      mqttClient.publish("Sensor4/iTempB", valueStr);
      dtostrf(sSensor[4].iVolt, 4, 2, valueStr);
      mqttClient.publish("Sensor4/iVolt", valueStr);
      sSensor[4].bSensorRec = false;

      debugln("MQTT send Sensor4");
    }
    else
    {
      if (sSensor[4].iTimeSinceLastRead > iSensorTimeout)
        //  mqttClient.publish("Sensor4/Err", "1");
        debugln("Error Sens4");
      debugln(sSensor[4].iTimeSinceLastRead);
    } // Sensor 5
    if (sSensor[5].bSensorRec == true)
    {
      dtostrf(float(sSensor[5].iTempA) / 100, 4, 3, valueStr);
      mqttClient.publish("Sensor5/fTempA", valueStr);

      dtostrf(float(sSensor[5].iTempB) / 100, 4, 3, valueStr);
      mqttClient.publish("Sensor5/fTempB", valueStr);

      dtostrf(float(sSensor[5].iVolt) / 100, 4, 3, valueStr);
      mqttClient.publish("Sensor5/fVolt", valueStr);

      dtostrf(float(sSensor[5].iHumi) / 100, 4, 3, valueStr);
      mqttClient.publish("Sensor5/fHumi", valueStr);

      sSensor[5].bSensorRec = false;

      debugln("MQTT send Sensor5");
    }
    else
    {
      if (sSensor[5].iTimeSinceLastRead > iSensorTimeout)
        mqttClient.publish("Sensor5/Err", "1");
      debugln("Error Sens5");
      debugln(sSensor[5].iTimeSinceLastRead);
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
