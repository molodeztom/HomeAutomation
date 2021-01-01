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
  20200509  V0.1: Copy from Adafruit example for MCP4725A1
  20200510  V0.2: Output 3 fixed values to test valve behaviour
  20200510  V0.3: Get SetPoint as MCP digits from MQTT as int and set valve accordingly
  20201019  V0.4: Converted to visual studio code project
  20201230  V0.5: Include secrets.h software string output (valveCurTest)
                  + INA219 sensor tests sucessfull (valveCurTest)
                  + Temperature Sensor 1 added
                  c publish current values via mqtt
  20201230  V0.6: +LCD display with hello world
  20210101  V0.7: c Values for valve 0-5 instead of 4095
  
  


**************************************************************************/

#include <Arduino.h>
#define DEBUG
#include <Wire.h>
#include <Adafruit_MCP4725.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <PubSubClient.h> //MQTT
#include <LiquidCrystal_I2C.h>

#include "D:\Arduino\HomeAutomationSecrets.h"
//libs for DS18B20
#include "Spi.h"
#include <DallasTemperature.h>

const String sSoftware = "ValveCtrl V0.7";

/***************************
 * LCD Settings
 **************************/
LiquidCrystal_I2C lcd(0x27, 20, 4); // set the LCD address to 0x27 for a 16 chars and 2 line display
/* I2C Bus */
//if you use ESP8266-01 with not default SDA and SCL pins, define these 2 lines, else delete them
// use Pin Numbers from GPIO e.g. GPIO4 = 4
// For NodeMCU Lua Lolin V3: D1=GPIO5 = SCL D2=GPIO4 = SDA set to SDA_PIN 4, SCL_PIN 5
#define SDA_PIN 4
#define SCL_PIN 5
byte gradeChar[8] = {
    0b00111,
    0b00101,
    0b00111,
    0b00000,
    0b00000,
    0b00000,
    0b00000,
    0b00000};

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
//Datenstruktur f�r den Datenaustausch
#define CHAN 5 // device channel 1 = Developmentboard 2 = ESP gel�tet 5 = Valve Control
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
PubSubClient mqttClient(MQTT_client);
//TODO one channel for ValveCntrl
//SubChannel ValvPos to receive positions from master
//SubChannel ValvMeasurement to send temperatures, volt, current ....

#define USERNAME "TestName/"
#define T_CHANNEL "ValveControl"
#define T_COMMAND "command"


/***************************
 * Measurement Variables
 **************************/
DATEN_STRUKTUR data;
const int mqttRecBufSiz = 50;
char mqttBufValvPos[mqttRecBufSiz];
int iValvPosSetP; //Setpoint Valve Position

/***************************
 * Timing
 **************************/

const unsigned long ulMQTTInterval = 2 * 1000UL; //call MQTT server
long lMQTTTime = 0;
const unsigned long ulValveSetInterval = 5 * 1000UL; //set valve, measure temp, send mqtt
long lValveSetTime = 0;

/***************************
 * otherSettings
 **************************/
#define swBAUD_RATE 115200

Adafruit_MCP4725 dac;

int iMCP4725Adr = 0x60;
int iMCPMaxCode = 4096; //code for max output
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
  //TODO Remove if not needed or external PUll UP for Temp Sensor Bus
  pinMode(12, INPUT_PULLUP);

  Wire.begin(SDA_PIN, SCL_PIN);

  dac.begin(iMCP4725Adr); //init dac with I2C adress

  /* LCD */
  lcd.init(); // initialize the lcd
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Init");
  lcd.setCursor(0, 1);
  lcd.print(sSoftware);
  delay(1000);

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
  if (!mqttClient.connected())
  {
    mQTTConnect();
  }
  Serial.println("Setup done");
  delay(1000);

  //initialize temp sensors
  sensoren.begin();

  //Nun pr�fen wir ob einer der Sensoren am Bus ein Temperatur Sensor bis zu 2 werden gelesen
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
  //Nun setzen wir noch die gew�nschte Aufl�sung (9, 10, 11 oder 12 bit) TODO das ist nur f�r den ersten Sensor oder?
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
}

void loop(void)
{

  // call MQTT loop within defined timeframe reconnect if connection lost
  if ((unsigned long)(millis() - lMQTTTime) > ulMQTTInterval)
  {
    bool alive = mqttClient.loop();
    if (alive == false)
    {
      Serial.println(" mqttClient loop failure");
    }
    //--------------------------check MQTT connection
    if (!mqttClient.connected())
    {
      mQTTConnect();
    }
    lMQTTTime = millis();
  }
  // set valve position every 60 seconds to current set position
  // measure temp and display
  if ((unsigned long)(millis() - lValveSetTime) > ulValveSetInterval)
  {
    int iValvVolt;
    //Convert position 0-5 to voltage 0-4095
    iValvVolt = map(iValvPosSetP,0,5,0,4095);
    Serial.println("Set Valve to: ");
    Serial.println(iValvPosSetP);
    Serial.println(iValvVolt);
    dac.setVoltage(iValvVolt, false);

    data.temp1 = 0;
    data.temp2 = 0;
    //Test get temperatures
    //Temperaturmessung starten
    sensoren.requestTemperatures();

    delay(750);                               //750 ms warten bis die Messung fertig ist
    data.temp1 = sensoren.getTempCByIndex(0); //+ SENSCORR1A;
    data.temp2 = sensoren.getTempCByIndex(1);

    // Write display we have 2 columns with each 16 characters
    char valueStr[20]; //helper for conversion
    lcd.clear();
    lcd.createChar(0, gradeChar);
    lcd.setCursor(0, 0);
    lcd.print("I:");
    dtostrf(data.temp1, 3, 1, valueStr);
    lcd.print(valueStr);
    lcd.print(" O:");
    dtostrf(data.temp2, 3, 1, valueStr);
    lcd.print(valueStr);
    lcd.print(" ");
    lcd.write(0);
    lcd.print("C");
    lcd.setCursor(0, 1);
    lcd.print("Ventil:");
    lcd.print(iValvPosSetP);

#ifdef DEBUG
    Serial.print("Temperatur1: ");
    Serial.print(data.temp1);
    Serial.println("�C");
    Serial.print("Temperatur2: ");
    Serial.print(data.temp2);
    Serial.println("�C");
#endif

    dtostrf(iValvPosSetP, 3, 0, valueStr);
    mqttClient.publish(T_CHANNEL"/ActPos", valueStr);
    dtostrf(data.temp1, 3, 2, valueStr);
    mqttClient.publish(T_CHANNEL"/TempIn", valueStr);
    dtostrf(data.temp2, 3, 2, valueStr);
    mqttClient.publish(T_CHANNEL"/TempOut", valueStr);
    Serial.println("publish");
    lValveSetTime = millis();
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
  if (strcmp(topic, T_CHANNEL"/SetPos") == 0)
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
    if (mqttClient.connect("", MQTT_USERNAME, MQTT_KEY))
    {
      Serial.println("connected");
      // mqttClient.publish("Test", "Test from Valve Control");
      bool subscribeOK = false;
      subscribeOK = mqttClient.subscribe(T_CHANNEL"/SetPos");
      if (subscribeOK)
      {
        Serial.println("subscribed ValvPos");
      }

      mqttClient.setCallback(callback);
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
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
