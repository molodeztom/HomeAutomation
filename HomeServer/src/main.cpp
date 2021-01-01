/*
  Home Automation Project
  Home Server

  Hardware: 
  ESP Board mit exteternem OLED Display und div Sensoren

  Collects Information from Weather Station Base via Serial Connection
  Displays Values with OLED
  Send Values to Internet e.g. Thingiverse
  Maintain connection to Raspberry Server like RedNode 
  History:
  2020321   V0.1:   Copy from SerialTest VV0.21:  Serial Send Data in JSON Format
  20200321  V0.2:   Test Serial Communication using interupt
  20200321  V0.3:   Use Hardware Serial
  20200321  V0.4:   Add WLAN funcitonality to send values to the internet
  20200321  V0.5:   Cleanup  
  20200311  V0.6:   Introduced non blocking serial read 
  20200323  V0.7:   Fixed bug with too small array (byte instead of int index)
  20200325  V0.8:   json default to -127 to better recognize when serial read fails then do not send this value to thingspeak
  20200327  V0.9:   Send Information to Raspbi via MQTT
  20200417  V0.10:  Raspberry IP Adress not name, Debug connected corrected
  20200417  V0.11:  Restructured Sensor data in struct
  20200418  V0.12:  MQTT separate function, send error values in topic, use client ID
  20200418  V0.13:  MQTT separate topics for each sensor
  20200418  V0.14:  MQTT Error Topic for each sensor
  20200418  V0.15:  MQTT Only one generic error for each sensor station
  20200428  V0.16:  2. Temp B for Sensor 2
  20200429  V0.17:  4 Kanal 
  20200504  V0.18:  Introduce I2C instead of serial communication, Test
  20200513  V0.19:  I2C V0.18 reversed, unneeded LED Output deleted
  20200513  V0.20:  Converted to platformio project
  20200521  V0.21:  Adapted to Weather Station Changes, more values from Sens 1, Batt
  20200522  V0.22:  Send single sensor instead of all at once,  Some values and definitions outsourced to HomeAutomationCommon.h
  20200523  V0.23:  simple checksum over all sensor values, specific counter for timeout to send MQTT ERR after a while
  20200522  V1.00:  Stable error handling and messaging
  20200521  V1.1:   Some debug output changed
  20200601  V1.2:   LED disabled and set to HIGH (off)
  20200604  V1.3:   Serial read to end then give back control to main
  20200627  V1.4:   5. Sensor receive added
  20200628  V1.5:   DHT 11 from local sensor removed
  20201910  V1.6:   Include HomeAutomationCommon Test
      
*/
#include <Arduino.h>
#define DEBUG
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <PubSubClient.h> //MQTT
#include "Wire.h"

#include <HomeAutomationCommon.h>

const String sSoftware = "HomeServer V1.5";

/***************************
 * Thingspeak Settings
 **************************/

const unsigned long ThingspkUplInterv = 30 * 1000UL; //time in ms between uploads
unsigned long ThingspkUplTime = 0;                   // stores last time an upload to thingiverse was done

/* MQTT Settings
 **************************/
WiFiClient MQTT_client;
PubSubClient mqttClient(MQTT_client);

/***************************
 * Measurement Variables
 **************************/

int iSensorChannel = 1;

/***************************
 * Serial Rx variables
 **************************/

// SoftwareSerial mySerial (rxPin, txPin, inverse_logic);
// want to transmit the date to the main station over a different serial link than the one used by the monitor
//  SoftwareSerial swSer(D6, D7, false); // only first (RX) used
static boolean recvInProgress = false;
char cSerialRxIn[nMaxRxArray]; //Serial Rx Buffer
bool newData = false;          //Flag RxData received
bool bJsonOK = false;          //true if JSON could be decoded

int ledState = HIGH; // ledState used to set the LED

//forward declarations
void wifiConnectStation();
void recvSerialwStartEndMarkers();
void showSerialRead();
void readJSONMessage();
void sendMQTTMessage();

// the setup function runs once when you press reset or power the board
void setup()
{
  Serial.begin(swBAUD_RATE);
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);

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
  //Set timeout counter to maximum to provoke error on startup
  sSensor0.iSensorCnt = SensValidMax;
  sSensor1.iSensorCnt = SensValidMax;
  sSensor2.iSensorCnt = SensValidMax;
  sSensor3.iSensorCnt = SensValidMax;
  sSensor4.iSensorCnt = SensValidMax;
  sSensor5.iSensorCnt = SensValidMax;
  Serial.println(sSoftware);
  digitalWrite(LED_BUILTIN, ledState);
  Serial.println("Setup done");
}

void loop()
{
  //LED Blink TODO remove
  //static unsigned long previousMillis = 0; //for blink
  //const long interval = 500;
  //Receive serial message from weather station
  recvSerialwStartEndMarkers();
  showSerialRead();
  mqttClient.loop(); //MQTT keep alive
  //At least one sensor reading is available
  if (newData)
  {
    newData = false;
    readJSONMessage();
    sendMQTTMessage();
  }
  //Every x seconds reading count up to detect timeout and then do not display
  //counter goes up to SensValidMax if not reset during a sensor reading
  //also used to send MQTT error every interval
  if ((millis() - lSensorValidTime > ulSensorValidIntervall))
  {
    sSensor0.iSensorCnt++;
    sSensor1.iSensorCnt++;
    sSensor2.iSensorCnt++;
    sSensor3.iSensorCnt++;
    //sSensor4.iSensorCnt++;
    sSensor5.iSensorCnt++;
    //will be true if after a while none of the sensors gets data. Then we need to send ERR MQTT Message
    if (sSensor0.iSensorCnt > SensValidMax)
    {
      sendMQTTMessage();
#ifdef DEBUG
      Serial.println("Sensor reading timeout");
#endif
    }
    lSensorValidTime = millis();
  }

  /*
    //Upload Temperature Humidity to ThingSpeak
  if(bJsonOK == true && (millis() - uploadTime > uploadInterval)){
    uploadMeasurements();
    uploadTime = millis();
    Serial.print("Upload Time: ");
    Serial.println(uploadTime);
  
    bJsonOK = false;
    }
    */

  //--------------------------blink part

  //unsigned long currentMillis = millis();
  /*
  if (currentMillis - previousMillis >= interval)
  {
    previousMillis = currentMillis;
    if (ledState == LOW)
    {
      ledState = HIGH;
    }
    else
    {
      ledState = LOW;
    }
  }
  digitalWrite(LED_BUILTIN, ledState);
  */
}
//Read JSON values from serial recieved message
void readJSONMessage()
{
  int iSensor;   //Sensor number
  int iCheckSum; //temp variable to check values very simple method
  DeserializationError err = deserializeJson(jsonDocument, cSerialRxIn);
  if (err)
  {
    Serial.print(F("deserializeJson failed with code "));
    Serial.println(err.c_str());
  }
  else
  {
    //get single values from JSON to local values if not found set default value to recognize the problem
    //common values for all sensors
    iSensor = jsonDocument["sensor"] | InvalidMeasurement;
    if (iSensor < 0 || iSensor > 5)
      iSensor = 99; // do not go into any switch case
#ifdef DEBUG
    int serialCount; //displays a counter received from BaseStation
    serialCount = jsonDocument["time"];
    Serial.print("Deserialize Sensor nr: ");
    Serial.println(iSensor);
    Serial.println(serialCount);
#endif
    switch (iSensor)
    {
    case 0:
           sSensor0.iLight = jsonDocument["iLightLoc"] | InvalidMeasurement;
      sSensor0.fAtmo = jsonDocument["fAtmoLoc"] | InvalidMeasurement;
      iCheckSum = sSensor0.iLight + sSensor0.fAtmo;
      if (iCheckSum < InvalidMeasurement)
      {
        sSensor0.bSensorRec = true;
        sSensor0.iSensorCnt = 0;
      }
      break;
    case 1:
      sSensor1.fTempA = jsonDocument["fTemp1A"] | InvalidMeasurement;
      sSensor1.fHumi = jsonDocument["fHumi1"] | InvalidMeasurement;
      sSensor1.fVolt = jsonDocument["fVolt1"] | InvalidMeasurement;
      iCheckSum = sSensor1.fTempA + sSensor1.fHumi + sSensor1.fVolt;
      if (iCheckSum < InvalidMeasurement)
      {
        sSensor1.bSensorRec = true;
        sSensor1.iSensorCnt = 0;
      }
      break;
    case 2:
      sSensor2.fTempA = jsonDocument["fTemp2A"] | InvalidMeasurement;
      sSensor2.fVolt = jsonDocument["fVolt2"] | InvalidMeasurement;
      iCheckSum = sSensor2.fTempA + sSensor2.fVolt;
      if (iCheckSum < InvalidMeasurement)
      {
        sSensor2.bSensorRec = true;
        sSensor2.iSensorCnt = 0;
      }
      break;
    case 3:
      sSensor3.fTempA = jsonDocument["fTemp3A"] | InvalidMeasurement;
      sSensor3.fVolt = jsonDocument["fVolt3"] | InvalidMeasurement;
      iCheckSum = sSensor3.fTempA + sSensor3.fVolt;
      if (iCheckSum < InvalidMeasurement)
      {
        sSensor3.bSensorRec = true;
        sSensor3.iSensorCnt = 0;
      }

      break;
    case 4:
      sSensor4.fTempA = jsonDocument["fTemp4A"] | InvalidMeasurement;
      sSensor4.fHumi = jsonDocument["fHumi4"] | InvalidMeasurement;
      sSensor4.fVolt = jsonDocument["fVolt4"] | InvalidMeasurement;
      iCheckSum = sSensor4.fTempA + sSensor4.fHumi + sSensor4.fVolt;
      if (iCheckSum < InvalidMeasurement)
      {
        sSensor4.bSensorRec = true;
        sSensor4.iSensorCnt = 0;
      }
      break;
    case 5:
      sSensor5.fTempA = jsonDocument["fTemp5A"] | InvalidMeasurement;
      sSensor5.fHumi = jsonDocument["fHumi5"] | InvalidMeasurement;
      sSensor5.fVolt = jsonDocument["fVolt5"] | InvalidMeasurement;
      iCheckSum = sSensor5.fTempA + sSensor5.fHumi + sSensor5.fVolt;
      if (iCheckSum < InvalidMeasurement)
      {
        sSensor5.bSensorRec = true;
        sSensor5.iSensorCnt = 0;
      }
      break;

    default:
      Serial.println("Error: Sensor Nr recieved out of range");
      break;
    }
    bJsonOK = false;
    /*
#ifdef DEBUG
    Serial.println("deserialized json document:");
    Serial.print("serialCount: ");
    Serial.println(serialCount);
    Serial.print("TempLocal: ");
    Serial.println(sSensor0.fTempA);
    Serial.print("HumiLocal ");
    Serial.println(sSensor0.fHumi);
    Serial.print("LightLocal: ");
    Serial.println(sSensor0.iLight);
    Serial.print("AtmoLoc: ");
    Serial.println(sSensor0.fAtmo);
    Serial.print("Humi 1: ");
    Serial.println(sSensor1.fHumi);
    Serial.print("Temp 1A: ");
    Serial.println(sSensor1.fTempA);
    Serial.print("Batt 1: ");
    Serial.println(sSensor1.fVolt);
    Serial.print("Temp 2A: ");
    Serial.println(sSensor2.fTempA);
    Serial.print("Batt 2: ");
    Serial.println(sSensor2.fVolt);
    Serial.print("Temp 3: ");
    Serial.println(sSensor3.fTempA);
    Serial.print("Batt3: ");
    Serial.println(sSensor3.fVolt);
   #endif
   
    Serial.print("Temp 4A: "); 
    Serial.println(sSensor4.fTempA);
    Serial.print("Temp 4B: ");
    Serial.println(sSensor4.fTempB);
    Serial.print("Batt 4: ");
    Serial.println(sSensor4.fVolt);
    Serial.print("Humi 4: ");
    Serial.println(sSensor4.fHumi);
    */
  }
}

// Send all sensor values via MQTT to home server raspberry
// send errors per sensor value in a separate channel
void sendMQTTMessage()
{
  char valueStr[20]; //helper to convert string to MQTT char
  if (!mqttClient.connected())
  {
#ifdef DEBUG
    Serial.print("MQTT not connected, rc=");
    Serial.print(mqttClient.state());
    Serial.println("reconnect MQTT");
#endif
    // Attempt to connect
    if (mqttClient.connect("MQTT_CLIENTD", MQTT_USERNAME, MQTT_KEY))
    {
#ifdef DEBUG
      Serial.println("MQTT connected");
#endif
    }
    else
    {
      Serial.print("MQTT failed, rc=");
      Serial.print(mqttClient.state());
    }
  }

  //Sensor Loc
  if (sSensor0.bSensorRec == true)
  {
    
    dtostrf(sSensor0.iLight, 3, 0, valueStr);
    mqttClient.publish("SensorLoc/iLight", valueStr);
    dtostrf(sSensor0.fAtmo, 4, 2, valueStr);
    mqttClient.publish("SensorLoc/fAtmo", valueStr);
    sSensor0.bSensorRec = false;
#ifdef DEBUG
    Serial.println("MQTT send SensorLoc");
#endif
  }
  else
  {
    if (sSensor0.iSensorCnt > SensValidMax)
      mqttClient.publish("SensorLoc/Err", "1");
  }
  //Sensor 1
  if (sSensor1.bSensorRec == true)
  {
    dtostrf(sSensor1.fTempA, 4, 2, valueStr);
    mqttClient.publish("Sensor1/fTempA", valueStr);
    dtostrf(sSensor1.fVolt, 4, 2, valueStr);
    mqttClient.publish("Sensor1/fVolt", valueStr);
    dtostrf(sSensor1.fHumi, 4, 2, valueStr);
    mqttClient.publish("Sensor1/fHumi", valueStr);
    sSensor1.bSensorRec = false;
#ifdef DEBUG
    Serial.println("MQTT send Sensor1");
#endif
  }
  else
  {
    if (sSensor1.iSensorCnt > SensValidMax)
  
      mqttClient.publish("Sensor1/Err", "1");
  }
  //Sensor 2
  if (sSensor2.bSensorRec == true)
  {
    dtostrf(sSensor2.fTempA, 4, 2, valueStr);
    mqttClient.publish("Sensor2/fTempA", valueStr);
    dtostrf(sSensor2.fVolt, 4, 2, valueStr);
    mqttClient.publish("Sensor2/fVolt", valueStr);
    sSensor2.bSensorRec = false;
#ifdef DEBUG
    Serial.println("MQTT send Sensor2");
#endif
  }
  else
  {
    if (sSensor2.iSensorCnt > SensValidMax)
    {
      mqttClient.publish("Sensor2/Err", "1");
      Serial.println("Error Sens2");
      Serial.println(sSensor2.iSensorCnt);
    }
  }
  //Sensor 3
  if (sSensor3.bSensorRec == true)
  {
    dtostrf(sSensor3.fTempA, 4, 2, valueStr);
    mqttClient.publish("Sensor3/fTempA", valueStr);
    dtostrf(sSensor3.fVolt, 4, 2, valueStr);
    mqttClient.publish("Sensor3/fVolt", valueStr);
    sSensor3.bSensorRec = false;
#ifdef DEBUG
    Serial.println("MQTT send Sensor3");
#endif
  }
  else
  {
    if (sSensor3.iSensorCnt > SensValidMax)
      mqttClient.publish("Sensor3/Err", "1");
  }
  //Sensor 4
  if (sSensor4.bSensorRec == true)
  {
    dtostrf(sSensor4.fTempA, 4, 2, valueStr);
    mqttClient.publish("Sensor4/fTempA", valueStr);
    dtostrf(sSensor4.fTempB, 4, 2, valueStr);
    mqttClient.publish("Sensor4/fTempB", valueStr);
    dtostrf(sSensor4.fVolt, 4, 2, valueStr);
    mqttClient.publish("Sensor4/fVolt", valueStr);
    sSensor4.bSensorRec = false;
#ifdef DEBUG
    Serial.println("MQTT send Sensor4");
#endif
  }
  else
  {
    if (sSensor4.iSensorCnt > SensValidMax)
      mqttClient.publish("Sensor4/Err", "1");
  } //Sensor 5
  if (sSensor5.bSensorRec == true)
  {
    dtostrf(sSensor5.fTempA, 4, 2, valueStr);
    mqttClient.publish("Sensor5/fTempA", valueStr);
    dtostrf(sSensor5.fTempB, 4, 2, valueStr);
    mqttClient.publish("Sensor5/fTempB", valueStr);
    dtostrf(sSensor5.fVolt, 4, 2, valueStr);
    mqttClient.publish("Sensor5/fVolt", valueStr);
    dtostrf(sSensor5.fHumi, 4, 2, valueStr);
    mqttClient.publish("Sensor5/fHumi", valueStr);
    sSensor5.bSensorRec = false;
#ifdef DEBUG
    Serial.println("MQTT send Sensor5");
#endif
  }
  else
  {
    if (sSensor5.iSensorCnt > SensValidMax)
      mqttClient.publish("Sensor5/Err", "1");
  }
}

// recieve payload between start and end marker (exluded) from serial
void recvSerialwStartEndMarkers()
{
  static char cRx;     //one byte read
  static byte nRx = 0; //received chars counter

  while (Serial.available() > 0 && newData == false)
  {
    cRx = Serial.read();
    if (recvInProgress == true)
    {
      if (cRx != endMarker)
      {
        //read payload
        cSerialRxIn[nRx] = cRx;
        nRx++;
        if (nRx >= nMaxRxArray)
        {
          Serial.println("Array too small. Size:");
          nRx = 0; //too much stop reading
          recvInProgress = false;
          newData = false;
        }
      }
      else
      {
        //end Marker found
        recvInProgress = false;
        nRx = 0;
        newData = true;
      }
    }
    else if (cRx == startMarker)
    {

      recvInProgress = true;
    }
  } //end while serial available
} //end recvSerial..

void showSerialRead()
{
#ifdef DEBUG
  if (newData == true)
  {
    Serial.print("input read: ");
    Serial.println(cSerialRxIn);
  }
#endif
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
}

//upload temperature humidity data to thinkspeak.com
void uploadMeasurements()
{
  // Serial.println("IN upload Temperature Humidity");
  if (!thingspeak_client.connect(host, httpPort))
  {
    Serial.println("connection failed");
    return;
  }
  // Three values(field1 field2 field3 field4) have been set in thingspeak.com
  //Add values only if valid
  String thispkSnd;
  thispkSnd = String("GET ") + "/update?api_key=" + api_key;
  if (sSensor0.fTempA != InvalidMeasurement)
    thispkSnd = thispkSnd + "&field1=" + sSensor0.fTempA;
  if (sSensor0.fHumi != InvalidMeasurement)
    thispkSnd = thispkSnd + "&field2=" + sSensor0.fHumi;
  if (sSensor0.iLight != InvalidMeasurement)
    thispkSnd = thispkSnd + "&field3=" + sSensor0.iLight;
  if (sSensor0.fAtmo != InvalidMeasurement)
    thispkSnd = thispkSnd + "&field4=" + (sSensor0.fAtmo);
  if (sSensor1.fTempA != InvalidMeasurement)
    thispkSnd = thispkSnd + "&field5=" + sSensor1.fTempA;
  if (sSensor1.fHumi != InvalidMeasurement)
    thispkSnd = thispkSnd + "&field8=" + sSensor1.fHumi;
  if (sSensor2.fTempA != InvalidMeasurement)
    thispkSnd = thispkSnd + "&field6=" + sSensor2.fTempA;
  if (sSensor2.fVolt != InvalidMeasurement)
    thispkSnd = thispkSnd + "&field7=" + sSensor2.fVolt;

  thispkSnd = thispkSnd + " HTTP/1.1\r\n" + "Host: " + host + "\r\n" + "Connection: close\r\n\r\n";
  //Serial.println("thingspeak String");
  //(Serial.println(thispkSnd);
  thingspeak_client.print(thispkSnd);
  thispkSnd = "";
  //thingspeak_client.print(String("GET ") + "/update?api_key="+api_key+"&field1="+sSensorLoc.fTempA+"&field2="+sSensorLoc.fHumi +"&field3="+sSensorLoc.iLight +"&field4="+(sSensorLoc.fAtmo/100) +"&field5="+sSensor1.fTempA +"&field6="+sSensor1.fTempB +"&field7="+sSensor2.fVolt +"&field8="+sSensor1.fHumi + " HTTP/1.1\r\n" +"Host: " + host + "\r\n" + "Connection: close\r\n\r\n");
  while (thingspeak_client.available())
  {
    String line = thingspeak_client.readStringUntil('\r');
    //Serial.print(line);
  }
}
