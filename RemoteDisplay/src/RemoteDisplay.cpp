/*

Home Automation Project
   Remote Display

  Hardware: 
  ESP Board mit eingebautem OLED display 
  AZDelivery NodeMCU ESP8266-12E mit OLED Display CP2102 WLAN WIFI Development Board
  https://smile.amazon.de/dp/B079H29YZM?ref_=pe_3044161_248799201_302_E_DDE_dt_1&th=1
  
  Connect to MQTT Server and subscribe Temperature Values
  Displays Values with OLED

  History:
  20200327  V0.1: Copy from u8g2 Hello World 
  20200327  V0.2: Display test string   
  20200331  V0.3: Use display code from weather station 
  20200104  V0.4: Clean up code, fix mqtt buffer char problems
  20200104  V0.5: Add ext Humidity display
  20200304  V0.6: Add LED Blink to check if mqtt client loop still active
  20200404  V0.7: Add reconnect function in case the MQTT connect is lost
  20200404  V0.8: Add get ntp time 
  20200405  V0.9: NTP Time optimized 
  20200406  V0.10: Update Time display more often
*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Time.h>                       // time() ctime()
//#include <sys/time.h>
#include "Spi.h"
#include "SSD1306Wire.h"
#include "OLEDDisplayUi.h"
#include "Wire.h"

#include "WeatherStationFonts.h"
#include "WeatherStationImages.h"
#include <PubSubClient.h> //MQTT

//#include "D:\Arduino\HomeAutomation\HomeAutomationCommon.h"


/***************************
 * WIFI Settings
 **************************/
const char* WIFI_SSID = "xxxxxxxxxxx";
const char* WIFI_PWD = "xxxxxxxxxxxxxxxxx";

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
#define T_CHANNEL "Thermo"
#define T_COMMAND "command"

#define swBAUD_RATE 115200

int ledState = LOW;             // ledState used to set the LED


/***************************
 * Measurement Variables
 **************************/
  const int mqttRecBufSiz = 50;
  char mqttBufTemp[mqttRecBufSiz];
  char mqttBufHumi[mqttRecBufSiz];

 /***************************
 * NTP Settings
 **************************/
 
const char* NTP_SERVER = "de.pool.ntp.org";
const char* TZ_INFO    = "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00";  // enter your time zone (https://remotemonitoringsystems.ca/time-zone-abbreviations.php)

  struct tm timeInfo;
  time_t now;

/***************************
 * Timing
 **************************/

const unsigned long ulMQTTInterval = 2*1000UL; //call MQTT server
long lMQTTTime =0; 
const unsigned long ulRecLEDInterval = 120*1000UL; //reset Rec LED when call did not come for x sec
long lLEDTime =0; 
const unsigned long ulRetrieveNTP = 60*1000UL; //get time from NTP every 4 hours
long lRetrieveNTPTime =0; 
const unsigned long ulUpdTimeDispInterv = 50 *1000UL; //update Time display every 50 secs
int LastUpdTimDisp =0; 

/***************************
 * Display Settings
 **************************/

const int I2C_DISPLAY_ADDRESS = 0x3c;
#if defined(ESP8266)
const int SDA_PIN = D3;
const int SDC_PIN = D4;
#else
const int SDA_PIN = GPIO0;
const int SDC_PIN = GPIO2 
#endif
 // Initialize the oled display for address 0x3c
 SSD1306Wire     display(I2C_DISPLAY_ADDRESS, SDA_PIN, SDC_PIN);
 OLEDDisplayUi   ui( &display );

//declaring prototypes 
void drawProgress(OLEDDisplay *display, int percentage, String label);
void drawTempExtA(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawHumi(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState* state);
void wifiConnectStation();
void mQTTConnect();
bool getNTPtime(int sec);
void showTime(tm localTime);

// Add frames
// this array keeps function pointers to all frames
// frames are the single views that slide from right to left
FrameCallback frames[] = {drawTempExtA, drawHumi};
int numberOfFrames = 2;
OverlayCallback overlays[] = { drawHeaderOverlay };
int numberOfOverlays = 1;



   
void updateData(OLEDDisplay *display) {
  drawProgress(display, 10, "Updating time...");
 }


void setup(void) {
  Serial.begin(swBAUD_RATE);
 
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

   // initialize display
  display.init();
  display.clear();
  display.display();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setContrast(255);
 // ui.disableAutoTransition();


  //Display settings
  ui.setTargetFPS(30);
  ui.setTimePerFrame(3000);   //How long one frame stands still
  ui.setTimePerTransition(2000); //How fast is it moving
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
  //updateData(&display);
  //TODO remove after debug LED Blink
    //pinMode(LED_BUILTIN, OUTPUT);
    pinMode(D7, OUTPUT);
    pinMode(D6, OUTPUT);

//Set Time and NTP
  configTime(0, 0, NTP_SERVER);
  setenv("TZ", TZ_INFO, 1);
  delay(200); //need delay to make setenv work 
  if (getNTPtime(10)) {  // wait up to 10sec to sync
    lRetrieveNTPTime = 0;
  } else {
    Serial.println("Time not set");
  }
      showTime(timeInfo);

  Serial.println("Setup done");
}

void loop(void) {
   //TODO Remove after debug LED Blink
   //LED is set on when client.loop called
   //every 500 ms LED is set off id loop never called will stay dark
   static unsigned long previousMillis = 0; //for blink
   const unsigned long ledInterval = 500; 
   ui.update();
  // Get Time from NTP
  if((unsigned long)(millis() - lRetrieveNTPTime) > ulRetrieveNTP)
  {
    Serial.println("get NTP time in loop");
    getNTPtime(10);
    showTime(timeInfo);
    lRetrieveNTPTime = millis();
    
  }
 

// Update time in display


  if(LastUpdTimDisp >= (int) 58) {
         Serial.print("Local tim sec: "); 
   Serial.println(timeInfo.tm_sec);
       getNTPtime(10);
    showTime(timeInfo);
    /*
      time(&now);
      localtime_r(&now, &timeInfo);
      Serial.println("Update time for disp: ");
      */
      LastUpdTimDisp = timeInfo.tm_sec;
  }
  
 
  // call MQTT loop within defined timeframe reconnect if connection lost
  // TODO debug remove: Blink Red LED everytime the loop is called
  if((unsigned long)(millis() - lMQTTTime) > ulMQTTInterval)
  { 
    //TODO remove debug set LED ON when function is called
    if(ledState == LOW) {
     ledState = HIGH;
    } else{
     ledState = LOW;
    }
    bool alive = client.loop();
    if (alive == false){
      Serial.println("client loop failure");
      ledState = LOW; //LED off as long as no new mqtt connection established
    }
    lMQTTTime = millis();  
    //--------------------------check MQTT connection 
    if (!client.connected()) {
      ledState = LOW; //LED off as long as no new mqtt connection established
      mQTTConnect();     
    }
    digitalWrite(D7,ledState);
  }
  // Recieve Control LED Green reset time and LEd 
  // Will be set and Green ON if callback comes within defined timeframe 
  if((unsigned long)(millis() - lLEDTime) > ulRecLEDInterval)
  {     
     lLEDTime = millis();  
     digitalWrite(D6,LOW);
     Serial.println("Set LED LOW");
  }
   
    yield(); //not sure we need this
} //end loop 

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

void callback(char* topic, byte* data, unsigned int dataLength) {
  // handle message arrived 
  Serial.print(topic);
  Serial.println(": ");
  // check topic and handle 
  if(strcmp(topic, "Thermo") == 0){
     memset(&mqttBufTemp[0],0,mqttRecBufSiz);
    strncpy(mqttBufTemp, (char*)data, dataLength);
    mqttBufTemp[dataLength+1] = '\0';
    strcat (mqttBufTemp, " °C");
    Serial.println(mqttBufTemp);
  }
  else if(strcmp(topic, "Humi") == 0){
    memset(&mqttBufHumi[0],0,mqttRecBufSiz);
    strncpy(mqttBufHumi, (char*)data, dataLength);
    mqttBufHumi[dataLength+1] = '\0';
    strcat (mqttBufHumi, " %");
    Serial.println(mqttBufHumi);
    
  }
   //TODO: remove: LED Green ON when callback comes within timeframe
   lLEDTime = millis();      
   digitalWrite(D6,HIGH);
 }

 bool getNTPtime(int sec) {

  {
    Serial.println("in getNTP time");
    uint32_t start = millis();
    // Get time from network time service
    do {
      time(&now);
      localtime_r(&now, &timeInfo);
      Serial.print(".");
      delay(10);
    } while (((millis() - start) <= (1000 * sec)) && (timeInfo.tm_year < (2016 - 1900)));
    if (timeInfo.tm_year <= (2016 - 1900)) {
      Serial.println("NTP get Call not successful");
      return false;  // the NTP call was not successful
    }
    Serial.println();
    /*

  Serial.print("now ");  Serial.println(now);
   localtime(&now); //TODO Test if this makes better format in first place
 
   
   char time_output[30];
    strftime(time_output, 30, "%a  %d-%m-%y %T", localtime(&now));
    Serial.println(time_output);
    Serial.println();
    Serial.println("show Time getNTPTime after strftime");
       showTime(timeInfo);
     */
 
   
  }
  return true;
}
void showTime(tm localTime) {
   char time_output[30];
    strftime(time_output, 30, "%a  %d-%m-%y %T", localtime(&now));
    Serial.println(time_output);
    Serial.println();
  /*  
  Serial.print(localTime.tm_mday);
  Serial.print('/');
  Serial.print(localTime.tm_mon + 1);
  Serial.print('/');
  Serial.print(localTime.tm_year - 100);
  Serial.print('-');
  Serial.print(localTime.tm_hour);
  Serial.print(':');
  Serial.print(localTime.tm_min);
  Serial.print(':');
  Serial.print(localTime.tm_sec);
  Serial.print(" Day of Week ");
  if (localTime.tm_wday == 0)   Serial.println(7);
  else Serial.println(localTime.tm_wday);
  */

}


void drawProgress(OLEDDisplay *display, int percentage, String label) {
  display->clear();
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(64, 10, label);
  display->drawProgressBar(2, 28, 124, 10, percentage);
  display->display();
}



// Zeichnet einen Frame diese Funktionen werden von der OLEDDisplayUI Lib aufgerufen. x und y werden in der Lib gesetzt um den Frame zu scrollen etc. 
// Bei der Ausgabe also immer die Position mit übernehmen. Position abgleichen mit eigenen Werten, diese beziehen sich auf x, y 0 wenn der Frame fest angezeigt wird

/*
 * Gebe Außentemperatur aus 
 */
 void drawTempExtA(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  //Beschreibung als Text
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(61 + x, 38 + y, "Temp Bal kon");
  //Eigene Temperatur
  display->setFont(ArialMT_Plain_24);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(25 + x, 5 + y, mqttBufTemp);
 }
 

 void drawHumi(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  //Beschreibung als Text
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(64 + x, 38 + y, "Humi Bal kon");
  //Eigene Luftfeuchte
  display->setFont(ArialMT_Plain_24);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(65 + x, 5 + y, mqttBufHumi);
 }


//Die gelbe Zeile unten Uhrzeit und Frame Pointer erst mal keine Temperatur mehr
void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState* state) {
  // time currently not updated TODO: Get time from server
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

//Connect to MQTT Server 
void mQTTConnect(){
 if (!client.connected()) {
        Serial.println("Attempting MQTT connection...");
        // Attempt to connect
        if (client.connect("", MQTT_USERNAME, MQTT_KEY)) {
          Serial.println("connected");
          //client.publish("Test", "Test from Remote Display");
         bool subscribeOK = false;
          subscribeOK = client.subscribe(T_CHANNEL);
          if (subscribeOK){
            Serial.println("subscribed Thermo");
          }
            subscribeOK = client.subscribe("Humi");
          if (subscribeOK){
            Serial.println("subscribed Humi");
          }
        client.setCallback(callback);
        
      } else {
        Serial.print("failed, rc=");
        Serial.print(client.state());
      }
  }
}
