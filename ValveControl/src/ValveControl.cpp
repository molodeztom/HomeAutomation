/**************************************************************************/
/*!
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
  
  


**************************************************************************/

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MCP4725.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h> //MQTT

/***************************
 * WIFI Settings
 **************************/
const char* WIFI_SSID = "xxxxxxxxxxxxxx";
const char* WIFI_PWD = "xxxxxxxxxxxxxxxxxxxx";

/***************************
 * MQTT Settings
 **************************/
WiFiClient MQTT_client;
PubSubClient client(MQTT_client);
#define SERVER      "raspberrypi"
#define SERVERPORT  1883
#define MQTT_USERNAME ""
#define MQTT_KEY  ""

#define USERNAME  "TestName/"
#define PREAMBLE  "TestPreamble/"
#define T_CHANNEL "ValvPos"
#define T_COMMAND "command"

/***************************
 * Measurement Variables
 **************************/
  const int mqttRecBufSiz = 50;
  char mqttBufValvPos[mqttRecBufSiz];
  int iValvPosSetP; //Setpoint Valve Position

/***************************
 * Timing
 **************************/

const unsigned long ulMQTTInterval = 2*1000UL; //call MQTT server
long lMQTTTime =0; 
const unsigned long ulValveSetInterval = 5*1000UL; //call MQTT server
long lValveSetTime =0; 


  

/***************************
 * otherSettings
 **************************/
#define swBAUD_RATE 115200

Adafruit_MCP4725 dac;

int iMCP4725Adr = 0x60;
int iMCPMaxCode = 4096; //code for max output
float fVFact = 10.1; // 4095 volts per digit
int iValveInitVolt = 5; // Hold for 5 minuts while AC is applied to init valve


void wifiConnectStation();
void mQTTConnect();


void setup(void) {
  Serial.begin(swBAUD_RATE);
  Serial.println("");
  Serial.println("ValveControl V0.2");

  dac.begin(iMCP4725Adr);  //init dac with I2C adress
  
//WIFI Setup
  WiFi.persistent(false); //https://www.arduinoforum.de/arduino-Thread-Achtung-ESP8266-schreibt-bei-jedem-wifi-begin-in-Flash
  wifiConnectStation();
  if (WiFi.status() == WL_CONNECTED)
  {
    uint8_t macAddr[6];
    WiFi.macAddress(macAddr);
    Serial.printf("Connected, mac address: %02x:%02x:%02x:%02x:%02x:%02x\n", macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
  }
  client.setServer(SERVER, SERVERPORT);
  //Connect mQTT 
  if (!client.connected()) {
    mQTTConnect();     
  }
  Serial.println("Setup done");
  delay(1000);


}

void loop(void) {
  /*
    uint16_t i;
    uint16_t iVolt = 0;
              i=4095;
        Serial.println("Maximalwert");
        Serial.println(i);
        dac.setVoltage(i, false);
          delay(20000);*/

  // call MQTT loop within defined timeframe reconnect if connection lost
  if((unsigned long)(millis() - lMQTTTime) > ulMQTTInterval)
  { 
     bool alive = client.loop();
    if (alive == false){
      Serial.println("client loop failure");
    }
    //--------------------------check MQTT connection 
    if (!client.connected()) {
        mQTTConnect();     
    }
    lMQTTTime = millis();  
  }
  // set valve position every 60 seconds to current set position
   if((unsigned long)(millis() - lValveSetTime) > ulValveSetInterval)
   {
      Serial.println("Set Valve to: ");
      Serial.println(iValvPosSetP);
      dac.setVoltage(iValvPosSetP, false);
      lValveSetTime = millis();  
      
   }
 
}

void callback(char* topic, byte* data, unsigned int dataLength) {
  // handle message arrived 
  iValvPosSetP = 0;
  Serial.println(iValvPosSetP);
  Serial.print(topic);
  Serial.println(": ");

  
  // check topic and handle assuming there is only one INT in buffer
  // we receive number as a string in MQTT, so read string and convert to int
  if(strcmp(topic, "ValvPos") == 0){
     memset(&mqttBufValvPos[0],0,mqttRecBufSiz);
     strncpy(mqttBufValvPos, (char*)data, dataLength);
     mqttBufValvPos[dataLength+1] = '\0';
    Serial.println(mqttBufValvPos);
    iValvPosSetP = atoi(mqttBufValvPos);
     Serial.println(iValvPosSetP);

  }
  
 }

//Connect to local WIFI 
void wifiConnectStation(){
  WiFi.mode(WIFI_STA);
  Serial.println("WifiStat connecting to "); Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PWD);
  while (WiFi.status() != WL_CONNECTED){
    delay(250);
    Serial.print(".");
  }
  Serial.println();
}

//Connect to MQTT Server 
void mQTTConnect(){
 if (!client.connected()) {
        Serial.println("Attempting MQTT connection...");
        // Attempt to connect
        if (client.connect("", MQTT_USERNAME, MQTT_KEY)) {
          Serial.println("connected");
          //client.publish("Test", "Test from Valve Control");
         bool subscribeOK = false;
          subscribeOK = client.subscribe(T_CHANNEL);
          if (subscribeOK){
            Serial.println("subscribed ValvPos");
          }
           
        client.setCallback(callback);
        
      } else {
        Serial.print("failed, rc=");
        Serial.print(client.state());
      }
  }
}
