/**************************************************************************/
/*
   Test mit OP aus 3,3 V digital 10 V Ausgang erzeugen
   Mit verschiedenen Werten takten

   20200509 V0.1  First test
*/
/**************************************************************************/

/**************************************************************************
Home Automation Project
   Valve Control for central heating 

  Hardware: 
  ESP Node MCU 0.1
  MCP4725A1 I2C to 0 - 5 V DC output
  LMxx to amplify 0-5 V DC to 0-10 V DC
  
  Connect to MQTT Server and subscribe valve closing rate
  control

  History:
  20200509  V0.1: Copy from Adafruit example for MCP4725A1
  20200510  V0.2: Output 3 fixed values to test valve behaviour
  20200510  V0.3: Get SetPoint as MCP digits from MQTT as int and set valve accordingly
  20201019  V0.4: Converted to visual studio code project
  (valveCurTest)----------------------
  20201205  V0.5: Include secrets.h software string output (valveCurTest)
  20201213  V0.6: + INA219 sensor tests sucessfull (valveCurTest)
  20201213  V0.7: + Temperature Sensor 1 added
  20201218  V0.8: c Current measurement in own timing slot
  20201218  V0.9: c publish current values via mqtt
  
  


**************************************************************************/

#include <Arduino.h>
#define DEBUG
#include <Wire.h>
#include <Adafruit_MCP4725.h>
#include <Adafruit_INA219.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h> //MQTT
#include "D:\Arduino\HomeAutomationSecrets.h"
//libs for DS18B20
#include "Spi.h"
#include <DallasTemperature.h>

const String sSoftware = "ValveControl V0.9";

/***************************
 * TempSensor Settings
 **************************/
//pin GPIO2 (NodeMCU) D4 =  GPIO4  (ESP12-F Modul) use GPIO Number, when using higher pins may need to setup INPUT_PULLUP
const byte bus = 12;
#define SENSCORR1A -0.3 //calibration
OneWire oneWire(bus);
DallasTemperature sensoren(&oneWire);
//Array to store temp sensor adresses
DeviceAddress adressen; //TODO rename
//Datenstruktur für den Datenaustausch
#define CHAN 5 // device channel 1 = Developmentboard 2 = ESP gelötet 5 = Valve Control
struct DATEN_STRUKTUR
{
  int chan = CHAN;
  float temp1 = -127; //Aussen A
  float temp2 = -127; //Aussen B
  float fHumidity = -1;
  float fVoltage = -1; //Batterie Sensor
};

/***************************
 * MQTT Settings
 **************************/
WiFiClient MQTT_client;
PubSubClient  mqttClient(MQTT_client);
//TODO one channel for ValveCntrl
//SubChannel ValvPos to receive positions from master
//SubChannel ValvMeasurement to send temperatures, volt, current ....

#define USERNAME "TestName/"
#define PREAMBLE "TestPreamble/"
#define T_CHANNEL "ValvPos"
#define T_COMMAND "command"

/***************************
 * Measurement Variables
 **************************/
DATEN_STRUKTUR data;
const int mqttRecBufSiz = 50;
char mqttBufValvPos[mqttRecBufSiz];
int iValvPosSetP; //Setpoint Valve Position
float shuntvoltage = 0;
float busvoltage = 0;
float current_mA = 0;
float loadvoltage = 0;
float power_mW = 0;
float current_mA_max = 0;

/***************************
 * Timing
 **************************/

const unsigned long ulMQTTInterval = 2 * 1000UL; //call MQTT server
long lMQTTTime = 0;
const unsigned long ulValveSetInterval = 10 * 1000UL; //set valve, measure temp, send mqtt
long lValveSetTime = 0;
const unsigned long ulCurMeasurementInt = 5  * 1000UL; //measure current and integrate
long lCurMeasurementTime = 0;

/***************************
 * otherSettings
 **************************/
#define swBAUD_RATE 115200

Adafruit_MCP4725 dac;
Adafruit_INA219 ina219;

int iMCP4725Adr = 0x60;
int iMCPMaxCode = 4096; //code for max output
float fVFact = 10.1;    // 4095 volts per digit
int iValveInitVolt = 5; // Hold for 5 minuts while AC is applied to init valve

//Forward declarations
void printAddress(DeviceAddress adressen);
void wifiConnectStation();
void mQTTConnect();

void setup(void)
{
  Serial.begin(swBAUD_RATE);
  Serial.println("");
  Serial.println(sSoftware);
  //TODO Remove if not needed or external PUll UP
  pinMode(12, INPUT_PULLUP);

  dac.begin(iMCP4725Adr); //init dac with I2C adress

  //WIFI Setup
  WiFi.persistent(false); //https://www.arduinoforum.de/arduino-Thread-Achtung-ESP8266-schreibt-bei-jedem-wifi-begin-in-Flash
  wifiConnectStation();
  if (WiFi.status() == WL_CONNECTED)
  {
    uint8_t macAddr[6];
    WiFi.macAddress(macAddr);
    Serial.printf("Connected, mac address: %02x:%02x:%02x:%02x:%02x:%02x\n", macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
  }
   mqttClient.setServer(SERVER, SERVERPORT);
  //Connect mQTT
  if (! mqttClient.connected())
  {
    mQTTConnect();
  }

  //initialize temp sensors
  sensoren.begin();

  //Nun prüfen wir ob einer der Sensoren am Bus ein Temperatur Sensor bis zu 2 werden gelesen
  if (!sensoren.getAddress(adressen, 0))
  {
    Serial.println("0: Kein Temperatursensor vorhanden!");
  }
//adressen anzeigen
#ifdef DEBUG
  Serial.print("Adresse1: ");
  printAddress(adressen);
  Serial.println();

  //2. Sensor
  if (!sensoren.getAddress(adressen, 1))
  {
    Serial.println("1: Kein Temperatursensor vorhanden!");
  }
  //adressen anzeigen

  Serial.print("Adresse2: ");
  printAddress(adressen);
  Serial.println();
#endif

#ifdef DEBUG
  //Nun setzen wir noch die gewünschte Auflösung (9, 10, 11 oder 12 bit) TODO das ist nur für den ersten Sensor oder?
  sensoren.setResolution(adressen, 12);
  Serial.print("Auflösung = ");
  Serial.print(sensoren.getResolution(adressen), DEC);
  Serial.println();
#endif

  // Initialize the INA219.
  // By default the initialization will use the largest range (32V, 2A).  However
  // you can call a setCalibration function to change this range (see comments).

  if (!ina219.begin())
  {
    Serial.println("Failed to find INA219 chip");
    //  while (1) { delay(10); }
  }

  // To use a slightly lower 32V, 1A range (higher precision on amps):
  //ina219.setCalibration_32V_1A();
  // Or to use a lower 16V, 400mA range (higher precision on volts and amps):
  //ina219.setCalibration_16V_400mA();

  Serial.println("Measuring voltage and current with INA219 ...");

  //Temperaturmessung starten
  sensoren.requestTemperatures();

  delay(750); //750 ms warten bis die Messung fertig ist

  //Temperaturwert holen und in Datenstruktur zum Senden speichern
  data.temp1 = sensoren.getTempCByIndex(0) + SENSCORR1A;
  data.temp2 = sensoren.getTempCByIndex(1);

  Serial.println("Setup done");
  delay(1000);
}

void loop(void)
{
  /*
    uint16_t i;
    uint16_t iVolt = 0;
              i=4095;
        Serial.println("Maximalwert");
        Serial.println(i);
        dac.setVoltage(i, false);
          delay(200);
          */

  // call MQTT loop within defined timeframe reconnect if connection lost
  if ((unsigned long)(millis() - lMQTTTime) > ulMQTTInterval)
  {
    bool alive =  mqttClient.loop();
    if (alive == false)
    {
      Serial.println(" mqttClient loop failure");
    }
    //--------------------------check MQTT connection
    if (! mqttClient.connected())
    {
      mQTTConnect();
    }
    lMQTTTime = millis();
  }
  // set valve position every 60 seconds to current set position
  if ((unsigned long)(millis() - lValveSetTime) > ulValveSetInterval)
  {
    Serial.println("Set Valve to: ");
    Serial.println(iValvPosSetP);
    dac.setVoltage(iValvPosSetP, false);
    lValveSetTime = millis();

    data.temp1 = 0;
    data.temp2 = 0;
    //Test get temperatures
    //Temperaturmessung starten
    sensoren.requestTemperatures();

    delay(750);                               //750 ms warten bis die Messung fertig ist
    data.temp1 = sensoren.getTempCByIndex(0); //+ SENSCORR1A;
    data.temp2 = sensoren.getTempCByIndex(1);
     char valueStr[20]; //helper to convert string to MQTT char
      dtostrf(current_mA, 3, 2, valueStr);
      Serial.print("publish: ");
      Serial.println(valueStr);

     mqttClient.publish("ValveControl/Current", valueStr);


#ifdef DEBUG
    Serial.print("Temperatur1: ");
    Serial.print(data.temp1);
    Serial.println("°C");
    Serial.print("Temperatur2: ");
    Serial.print(data.temp2);
    Serial.println("°C");
#endif
  }
  // measure valve current within defined timeframe
  if ((unsigned long)(millis() - lCurMeasurementTime) > ulCurMeasurementInt)
  {
    shuntvoltage = 0;
    busvoltage = 0;
    current_mA = 0;
    loadvoltage = 0;
    power_mW = 0;
    //Test get current measurements
    shuntvoltage = ina219.getShuntVoltage_mV();
    busvoltage = ina219.getBusVoltage_V();
    current_mA = ina219.getCurrent_mA();
    power_mW = ina219.getPower_mW();
    loadvoltage = busvoltage + (shuntvoltage / 1000);
    Serial.print("Bus Voltage:   ");
    Serial.print(busvoltage);
    Serial.println(" V");
    Serial.print("Shunt Voltage: ");
    Serial.print(shuntvoltage);
    Serial.println(" mV");
    Serial.print("Load Voltage:  ");
    Serial.print(loadvoltage);
    Serial.println(" V");
    Serial.print("Current:       ");
    Serial.print(current_mA);
    Serial.println(" mA");
    Serial.print("Power:         ");
    Serial.print(power_mW);
    Serial.println(" mW");
    Serial.println("");
   
   current_mA_max = current_mA; 
    lCurMeasurementTime = millis();
  }
}

void callback(char *topic, byte *data, unsigned int dataLength)
{
  // handle message arrived
  iValvPosSetP = 0;
  Serial.println(iValvPosSetP);
  Serial.print(topic);
  Serial.println(": ");

  // check topic and handle assuming there is only one INT in buffer
  // we receive number as a string in MQTT, so read string and convert to int
  if (strcmp(topic, "ValvPos") == 0)
  {
    memset(&mqttBufValvPos[0], 0, mqttRecBufSiz);
    strncpy(mqttBufValvPos, (char *)data, dataLength);
    mqttBufValvPos[dataLength + 1] = '\0';
    Serial.println(mqttBufValvPos);
    iValvPosSetP = atoi(mqttBufValvPos);
    Serial.println(iValvPosSetP);
  }
}

//Connect to local WIFI
void wifiConnectStation()
{
  WiFi.mode(WIFI_STA);
  Serial.println("WifiStat connecting to ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PWD);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(250);
    Serial.print(".");
  }
  Serial.println();
}

//Connect to MQTT Server
void mQTTConnect()
{
  if (!mqttClient.connected())
  {
    Serial.println("Attempting MQTT connection...");
    Serial.println(MQTT_USERNAME);
    Serial.print(" ");
    Serial.print(MQTT_KEY);
    // Attempt to connect
    if ( mqttClient.connect("", MQTT_USERNAME, MQTT_KEY))
    {
      Serial.println("connected");
      // mqttClient.publish("Test", "Test from Valve Control");
      bool subscribeOK = false;
      subscribeOK =  mqttClient.subscribe(T_CHANNEL);
      if (subscribeOK)
      {
        Serial.println("subscribed ValvPos");
      }

       mqttClient.setCallback(callback);
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print( mqttClient.state());
    }
  }
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
