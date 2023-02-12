/*
Include File for common parameters.
Used in WeatherStation and HomeServer


20200522: V0.1  Test if it works at all
20200522: V0.2  Works with WeatherStation and HomeServer
20200522  V1.0  Stable error handling and messaging
20200627  V1.1  Sensor #5 added
20230115  V1.2  nMaxSensors
20230211  V1.3  rename float to int variable names
20230212  V1.4  sensor capabilities

*/
// DELETE THIS INCLUDE YOU DO NOT NEED THIS FILE WHEN USING THE PASSWORDS BELOW
#include "D:\Arduino\HomeAutomationSecrets.h"

#define swBAUD_RATE 57600 // speed for softserial, 115000 often fails

/* ADD YOUR OWN VALUES IF YOU WANT TO USE THIS PROJEKT****************************************************************************

const char *WIFI_SSID = ""
const char *WIFI_PWD = "";

***************************
 * ESP Now Settings
 **************************
//SSID die der AP eine Zeitlang aufmacht. Damit kann der Sensor mit dem AP Kontakt aufnehmen und seine MAC bekanntgeben.
const char *APSSID = "";

***************************
 * Thingspeak Settings
 **************************
WiFiClient thingspeak_client;
const char *host = "api.thingspeak.com";  //IP address of the thingspeak server
const char *api_key = ""; //Your own thingspeak api_key
const int httpPort = 80;
***************************
 MQTT Settings
 **************************
#define SERVER ""
#define SERVERPORT 1883
#define MQTT_USERNAME ""
#define MQTT_KEY ""
#define MQTT_CLIENTD ""

***********************************************************************************************************/

/***************************
 * Measurement Variables
 **************************/
const float InvalidMeasurement = 9999; // value set when a value was not correctly received via serial and JSON
const int nMaxSensors = 10;            // max number of sensors allowed in system it is an array so starts from 0 to < nMaxSensors
// used for ESP Now Data protocol same for sensors and ESPNowHub
//TODO add interface version add sensor capabilities do it when programming a new sensor
struct ESPNOW_DATA_STRUCTURE
{
  int iSensorChannel = 99;  // default for none received
  float fESPNowTempA = -99; // Aussen A
  float fESPNowTempB = -99; // Aussen B
  float fESPNowHumi = -99;
  float fESPNowVolt = -99; // Batterie Sensor
};

// sensor capabilities

const uint16_t TEMPA_ON = 1 << 0; // 1 (0x1)
const uint16_t TEMPB_ON = 1 << 1; // 2 (0x2)
const uint16_t VOLT_ON = 1 << 2;  // 4 (0x4);
const uint16_t HUMI_ON = 1 << 3;  // 8 (0x8)
const uint16_t LIGHT_ON = 1 << 4; // 16 (0x10)
const uint16_t ATMO_ON = 1 << 5;  // 32 (0x20)
const uint16_t OPT1_ON = 1 << 6;  // 64 (0x40)
const uint16_t OPT2_ON = 1 << 7;  // 128 (0x80)
const uint16_t OPT3_ON = 1 << 8;  // 256 (0x100)
// used for local storage in ESPNowHub
struct SENSOR_DATA
{
  int iSensorChannel = nMaxSensors + 1;
  uint16_t sSensorCapabilities = 0;
  int iTempA = InvalidMeasurement;
  int iTempB = InvalidMeasurement;
  int iTempC = InvalidMeasurement;
  int iHumi = InvalidMeasurement;
  int iVolt = InvalidMeasurement;
  int iAtmo = InvalidMeasurement;
  String sMacAddress = "0000000000000000000";
  int iLight = 0;
  bool bSensorRec = false; // true when valid sensor data received in time frame
  // int iSensorCnt = 6;      //Counter for timeout no values after some time renamed
  bool bSensorRegistered = false; // true when sensor is first time received
  int iTimeSinceLastRead;         // Minutes count up between sensor readings 0 if a new reading comes in, used for sensor data recieved over ESPNow
};
// TODO remove single variables in favor of array
// SENSOR_DATA sSensor0, sSensor1, sSensor2, sSensor3, sSensor4, sSensor5;
SENSOR_DATA sSensor[nMaxSensors]; //  HomeAutomationCommon.h starts from 0 = local sensor and 1-max are the channels
// Sensor 1: Breadboard, Sensor 2 Blau/Grau, sSensor3: Blau/Rot
//  Sensor reading timeout
long lSensorValidTime = 0;
// const unsigned long ulOneSecondTimer = 6 * 1000UL; //time in sec
// const int iSensorTimeout = 1;              //  times interval will make reading invalid 60*x 120 sec. Used for recieving data over serial

/***************************
 * Serial Rx variables
 **************************/
const char startMarker = '%';
const char endMarker = '$';
const int nMaxRxArray = 256;

/***************************
 * JSON definitions
 **************************/
const size_t capacity = JSON_OBJECT_SIZE(8) + 256;
StaticJsonDocument<capacity> jsonDocument;
