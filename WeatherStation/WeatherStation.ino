/*
 * 
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




 */
#include <ESPWiFi.h>
#include <ESPHTTPClient.h>
//#include <JsonListener.h>
#include <ArduinoJson.h>

// time
//#include <coredecls.h>                  // settimeofday_cb()
tm timeInfo;

#include "SSD1306Wire.h"
#include "OLEDDisplayUi.h"
#include "Wire.h"
#include "DHT.h"

#include "WeatherStationFonts.h"
#include "WeatherStationImages.h"
#include <ESP8266WiFi.h>

//Bibliothek für ESP-Now
extern "C"{
#include <espnow.h>
}
const String sSoftware = "WeatherStation V0.30";
#include <SoftwareSerial.h>
#define swBAUD_RATE 57600 // speed for softserial, 115000 often fails
// SoftwareSerial mySerial (rxPin, txPin, inverse_logic);
// want to transmit the date to the main station over a different serial link than the one used by the monitor
SoftwareSerial swSer(D6, D7, false);
const unsigned long ulSendIntervall = 30*1000UL; //120 sek

#include <Adafruit_BMP085.h>

// Nach einem Reset ist die APSSID sichtbar damit ein entfernter Sensor sich 
// verbinden kann. Nach ca. 2 Minuten sollte die APSSID versteckt werden 
// Zähler zum Abschalten der sichtbaren APSSID  
int cntAPVisible = 2000;

#define BATTCORR2 -0.018
#define BATTCORR3 -0.018

// Prüfen ob überhaupt etwas empfangen wird. Zähler wird bei Empfang auf 0 gesetzt dann hochgezählt bei 2 = 2x refresh in ms wird Aussentemp zurückgesetzt.
#define REC_MAX 10
int rec_cnt1 = REC_MAX; 
int rec_cnt2 = REC_MAX; 

//Werte der Temperatursensoren -127 = der Wert, 
//den man erhält wenn kein Sensor angeschlossen ist 
/*
  float fTemperature1A = -127; // main station Temp 1 innen
  float fTemperature1B = -127;  //
  float fTemperature2A = -127;
  float fTemperature3A = -127;
  float fVolt2 = -99; //Batterie Sensor 2
  float fVolt3 = -99; //Batterie Sensor 3
  float fHumi2 = -1; //Luftfeuchte Sensor2
  float fHumiLocal = -1; //HumiLocal innen = Sensor 1
  float fTempLocal = -127; //TempLocal Innen
  int iLightLocal = 0;
  float fAtmoLoc = 0;
  */
  int iSensorChannel = 1;
  bool bExtDataValid = false; //Flag für externe Sensorwerte erhalten 
  const unsigned long ulSensReadIntervall = 20*1000UL; //Time to evaluate received sens values for display
  

//Datenstruktur für den Datenaustausch über ESP Now
struct DATEN_STRUKTUR {
    int iSensorChannel = 1;  //Kanal wenn nichts kommt Kanal 1 nehmen der neue Sensor ist Kanal 2
    float fESPNowTempA = 0;  //Aussen A
    float fESPNowTempB = 0;  //Aussen B
    float fESPNowHumi = -1;
    float fESPNowVolt = -1; //Batterie Sensor 2
    
};
//Data structure for a sensor station containing all possible values
struct SENSOR_DATA {
    int   iSensorChannel = 1;
    float fTempA = -127;
    float fTempB = -127;
    float fTempC = -127;
    float fHumi = -127;
    float fVolt = -127;
    float fAtmo = -127;
    int   iLight = 0;
};
SENSOR_DATA sSensorLoc, sSensor1, sSensor2, sSensor3, sSensor4;
//Sensor 1: Breadboard, Sensor 2 Blau/Grau, sSensor3: Blau/Rot

// Default Anzeige wenn Sensor nicht empfangen wird
String textAusA  = "----------";
String textAusB  = "----------";
String textAusC  = "----------";
String textBatVoltB = "----------";
/***************************
 * WIFI Settings
 **************************/
const char* WIFI_SSID = "xxxxxxxxxxxxxxxxxxxx";
const char* WIFI_PWD = "xxxxxxxxxxxxxxxxxxx";

/***************************
/***************************
 * ESP Now Settings
 **************************/
//SSID die der AP eine Zeitlang aufmacht. Damit kann der Sensor mit dem AP Kontakt aufnehmen und seine MAC bekanntgeben.  
char* APSSID = "Thermometer";



/***************************
 * Thingspeak Settings
 **************************/

WiFiClient thingspeak_client;
const char *host = "api.thingspeak.com";                  //IP address of the thingspeak server
const char *api_key ="IA4E5S3IM4QRB4LJ";                  //Your own thingspeak api_key
const int httpPort = 80;

/***************************
 * Temperature Settings
 **************************/
#define pin 14       // ESP8266-12E  D5 OLD TODO remove when read temp humidity is removed
#define DHTPIN 14       // ESP8266-12E  D5 read emperature and Humidity data for DHT11
#define DHTTYPE DHT11   // DHT 11 for lib
DHT dht(DHTPIN, DHTTYPE);


void readTemperatureHumidity();

long lreadTime = 0; 
long uploadTime = 0; 
long serialTransferTime = 0; //Timer for uploading data to main station

/***************************
 * Begin Atmosphere and iLightLocal Sensor Settings
 **************************/
void readLight();
void readAtmosphere();
Adafruit_BMP085 bmp;
const int Light_ADDR = 0b0100011;   // address:0x23
const int Atom_ADDR = 0b1110111;  // address:0x77


/***************************
 * Serial Communication Settings
 **************************/
 int serialCounter = 0;

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

const int SDA_PIN = D3;  //GPIO0
const int SDC_PIN = D4;	 //GPIO2
#else
//const int SDA_PIN = GPIO5;
//const int SDC_PIN = GPIO4 

const int SDA_PIN = GPIO0;
const int SDC_PIN = GPIO2 
#endif

const boolean IS_METRIC = true;
 // Initialize the oled display for address 0x3c
 SSD1306Wire     display(I2C_DISPLAY_ADDRESS, SDA_PIN, SDC_PIN);
 OLEDDisplayUi   ui( &display );

// flag changed in the ticker function every 10 minutes

//declaring prototypes for display
void drawProgress(OLEDDisplay *display, int percentage, String label);
void drawTempIn(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawTempExtA(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawTempExtB(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawTempExtC(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawHumi(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawAtmo(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawLight(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawBattVoltB(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState* state);

// Add frames
// this array keeps function pointers to all frames
// frames are the single views that slide from right to left
FrameCallback frames[] = {drawTempIn, drawTempExtA, drawBattVoltB,drawTempExtC, drawHumi, drawAtmo, drawLight};
int numberOfFrames = 6;
OverlayCallback overlays[] = { drawHeaderOverlay };
int numberOfOverlays = 1;

/***************************
 * END Display Settings
 **************************/


// Als subfunktion damit es nicht so oft aufgerufen wird
// In String umwandeln damit die Ausgabe schneller geht
// Der cnt zählt hoch solange nichts empfangen wurde. 
void formatTempExt(){
    Serial.print("No recieve 1 for: ");
    Serial.println(rec_cnt1);
    Serial.print("No recieve 2 for: ");
    Serial.println(rec_cnt2);
    //Empfang Sensor 1 Breadboard
 if (rec_cnt1 >= REC_MAX) {
  //kein Sensor gefunden
//   Serial.println("kein Sensor empfangen");
   textAusA = "----------";
   textAusB = "----------";

 }
  else {
    if (rec_cnt1 <1){
    //nur wenn kürzlich gelesen wurde
    //Wir wandeln die Temperatur nach integer mit einer Dezimalstelle
//    Serial.print("formatTemp1A: ");
  //  Serial.println(sSensor1.fTempA);
    //Ausgabetext generieren
    textAusA = String(sSensor1.fTempA) + " °C";
//    Serial.println(textTemp1B);
    textAusB = String(sSensor1.fTempB) + " °C";

    }
  }
  //Empfang Sensor 2 blau/braun
    if (rec_cnt2 >= REC_MAX) {
  //kein Sensor gefunden
//   Serial.println("kein Sensor empfangen");
   textAusC = "----------";
   textBatVoltB = "----------";
 }
  else {
    if (rec_cnt2 <1){
    //nur wenn kürzlich gelesen wurde
    //Wir wandeln die Temperatur nach integer mit einer Dezimalstelle
//    Serial.print("formatTempExt Aussen: ");
  //  Serial.println(sSensor2.fTempA);
    //Ausgabetext generieren
     textAusC = String(sSensor2.fTempA) + " °C";
 //   Serial.println(textSens2Volt);
    textBatVoltB = String(sSensor2.fVolt) + " V";
    }
   }
   rec_cnt1++; //recieve counter erhöhen
   rec_cnt2++; //recieve counter erhöhen
}

//Callback funktion wenn Daten über ESP-Now empfangen wurden 
void on_receive_data(uint8_t *mac, uint8_t *r_data, uint8_t len) {
 
    DATEN_STRUKTUR sESPReceive;
    //wir kopieren die empfangenen Daten auf die Datenstruktur

    memcpy(&sESPReceive, r_data, sizeof(sESPReceive));
    //um über die Datenstruktur zugreifen zu können
    iSensorChannel = sESPReceive.iSensorChannel;
    Serial.print("Channel: ");
    Serial.println (iSensorChannel);

    //depending on channel
    switch(iSensorChannel){
      case 1: 
        sSensor1.fTempA = sESPReceive.fESPNowTempA;
        sSensor1.fTempB = sESPReceive.fESPNowTempB;
        sSensor1.fHumi = sESPReceive.fESPNowHumi;
        rec_cnt1 = 0; //Receive counter zurücksetzen
        Serial.println("Recieved Data 1");
        Serial.print("Temperatur 1A: ");
        Serial.println(sSensor1.fTempA);
        Serial.print("Temperatur 1B: ");
        Serial.println(sSensor1.fTempB);
        Serial.print("extHumidity: ");
        Serial.println(sSensor1.fHumi);
      break;
      case 2:
        sSensor2.fTempA = sESPReceive.fESPNowTempA;
        sSensor2.fTempB = sESPReceive.fESPNowTempB;
        sSensor2.fVolt = sESPReceive.fESPNowVolt + BATTCORR2;
        rec_cnt2 = 0; //Receive counter zurücksetzen
        Serial.println("Recieved Data 2");
        Serial.print("Temperatur 2A: ");
        Serial.println(sSensor2.fTempA);
        Serial.print("Temperatur 2B: ");
        Serial.println(sSensor2.fTempB);
        Serial.print("Batt2 V: ");
        Serial.println(sSensor2.fVolt);
        break;
      case 3:
      //TODO new variables for Sens 3
        sSensor3.fTempA = sESPReceive.fESPNowTempA;
        sSensor3.fVolt = sESPReceive.fESPNowVolt + BATTCORR3;
        //rec_cnt2 = 0; //Receive counter zurücksetzen TODO cnt für 3 
        Serial.println("Recieved Data 3");
        Serial.print("Temperatur 3: ");
        Serial.println(sSensor3.fTempA);
        Serial.print("Batt3 V: ");
        Serial.println(sSensor3.fVolt);
      break;
       case 4:
      //TODO new variables for Sens 3
        sSensor4.fTempA = sESPReceive.fESPNowTempA;
        sSensor4.fVolt = sESPReceive.fESPNowVolt + BATTCORR3;
        sSensor4.fHumi = sESPReceive.fESPNowHumi;
        //rec_cnt2 = 0; //Receive counter zurücksetzen TODO cnt für 3 
        Serial.println("Recieved Data 4");
        Serial.print("Temperatur 4: ");
        Serial.println(sSensor4.fTempA);
        Serial.print("Batt4 V: ");
        Serial.println(sSensor4.fVolt);
        Serial.print("extHumidity: ");
        Serial.println(sSensor4.fHumi);

      break;
      default:
        Serial.println("Default Channel do nothing");
       break;

    }
  
};


void setup() {
  Serial.begin(swBAUD_RATE);
  swSer.begin(swBAUD_RATE);
  Serial.println (sSoftware);
  WiFi.persistent(false); //https://www.arduinoforum.de/arduino-Thread-Achtung-ESP8266-schreibt-bei-jedem-wifi-begin-in-Flash
  dht.begin(); //DHT 11 Sensor   
  // Wire Sensoren initialisieren
  Wire.begin(0,2);
  Wire.beginTransmission(Atom_ADDR);
  //initialize Atmosphere sensor
  if (!bmp.begin()) {
    Serial.println("Could not find BMP180 or BMP085 sensor at 0x77");
  }else{
    Serial.println("Find BMP180 or BMP085 sensor at 0x77");
  }
  Wire.endTransmission();

  //initialize light sensor
  Wire.beginTransmission(Light_ADDR);
  Wire.write(0b00000001);
  Wire.endTransmission();

  // initialize displayy
  display.init();
  display.clear();
  display.display();

  //display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setContrast(255);

  
  //Station starten für AP nicht nötig
 WiFi.disconnect();
 WiFi.mode(WIFI_AP);
 WiFi.begin(); 


  //AP starten um für den Sensor die MAC Adresse zur verfügung zu stellen
  WiFi.softAP(APSSID); 
  Serial.println("AP starten");
  Serial.println(APSSID);
  delay(1000);
  //ESP-Now initialisieren  

  if (esp_now_init()!=0) {
	Serial.println("Init ESP-Now failed restart");  
    ESP.restart();
  }
  // ESP Rolle festlegen 1=Master, 2 = Slave 3 = Master + Slave
  Serial.println("Set espnow Role 2");
  esp_now_set_self_role(2); 
  //und callback funktion registrieren
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

void loop() {  
  //Read Temperature Humidity every x seconds
  if(millis() - lreadTime > ulSensReadIntervall){
    /*Serial.print("Read local Values: ");
    Serial.println(lreadTime);
    Serial.print("Sens Read Intervall: ");
    Serial.println(ulSensReadIntervall); */
    readTempHumDHT11();
    readLight();
    readAtmosphere();
    formatTempExt();
    lreadTime = millis();
	if (cntAPVisible >0) cntAPVisible--; //Zähler für AP offen wenn 0 AP unsichtbar machen
	if (cntAPVisible <= 0) {
	  WiFi.softAP(APSSID,NULL,1,1);
    Serial.println("AP visible");
      }
  }

  //Upload Temperature Humidity to MainStation every xSeconds only if data was received recently
  if((millis() - serialTransferTime > ulSendIntervall) && (rec_cnt1 <1)&& (rec_cnt2 <1)){
    sendDataToMainStation();
    serialTransferTime = millis();
  }
  
   //Upload Temperature Humidity every x Minutes

   
  int remainingTimeBudget = ui.update();

  if (remainingTimeBudget > 0) {
    // You can do some work here
    // Don't do stuff if you are below your
    // time budget.

    delay(remainingTimeBudget);
  }
}

void sendDataToMainStation(){
  //serialize Sensor Data to json and send using sw serial connection
  //Allocate JsonBuffer
   const int capacity = JSON_OBJECT_SIZE(14) +204;
  StaticJsonDocument<capacity> jsonDocument;
jsonDocument["sensor"] = "AllSensors";
jsonDocument["time"] = serialCounter++;
jsonDocument["fTempLoc"] = sSensorLoc.fTempA;
jsonDocument["fHumiLoc"] = sSensorLoc.fHumi;
jsonDocument["iLightLoc"] = sSensorLoc.iLight;
jsonDocument["fAtmoLoc"] = sSensorLoc.fAtmo;
jsonDocument["fTemp1A"] = sSensor1.fTempA;
jsonDocument["fTemp1B"] = sSensor1.fTempB;
jsonDocument["fHumi1"] = sSensor1.fHumi;
jsonDocument["fTemp2A"] = sSensor2.fTempA;
jsonDocument["fTemp2B"] = sSensor2.fTempB;
jsonDocument["fVolt2"] = sSensor2.fVolt;
jsonDocument["fTemp3A"] = sSensor3.fTempA;
jsonDocument["fVolt3"] = sSensor3.fVolt;
/*
jsonDocument["fTemp4A"] = sSensor4.fTempA;
jsonDocument["fTemp4B"] = sSensor4.fTempB;
jsonDocument["fHumi4"] = sSensor4.fHumi;
jsonDocument["fVolt4"] = sSensor4.fVolt;
*/

swSer.print("%"); // $$ Start Code
serializeJson(jsonDocument, swSer);
swSer.print("$"); // $$ End Code
//debug
char output[336];
serializeJson(jsonDocument, output);
Serial.println(output);

  
}

void drawProgress(OLEDDisplay *display, int percentage, String label) {
  display->clear();
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(64, 10, label);
  display->drawProgressBar(2, 28, 124, 10, percentage);
  display->display();
}

void updateData(OLEDDisplay *display) {
  drawProgress(display, 10, "Updating time...");
 
}

// Zeichnet einen Frame diese Funktionen werden von der OLEDDisplayUI Lib aufgerufen. x und y werden in der Lib gesetzt um den Frame zu scrollen etc. 
// Bei der Ausgabe also immer die Position mit übernehmen. Position abgleichen mit eigenen Werten, diese beziehen sich auf x, y 0 wenn der Frame fest angezeigt wird


/*
 * Gebe Innen Temperatur  aus
 */
void drawTempIn(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  //Beschreibung als Text
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(64 + x, 38 + y, "Innen");
  //Eigene Temperatur
  display->setFont(ArialMT_Plain_24);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  String temp = String(sSensorLoc.fTempA) + (IS_METRIC ? "°C" : "°F"); //Test Temperatur fest auf 90
  display->drawString(40 + x, 5 + y, temp);
 }

/*
 * Gebe Außentemperatur aus 
 */
 void drawTempExtA(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  //Beschreibung als Text
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(61 + x, 38 + y, "Außen A");
  //Eigene Temperatur
  display->setFont(ArialMT_Plain_24);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(25 + x, 5 + y, textAusA);
 }

/*
 * Gebe Außentemperatur 2 aus 
 */
 void drawTempExtB(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  //Beschreibung als Text
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(61 + x, 38 + y, "Außen B");
  //Eigene Temperatur
  display->setFont(ArialMT_Plain_24);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(25 + x, 5 + y, textAusB);
 }

 /*
 * Gebe Außentemperatur C aus 
 */
 void drawTempExtC(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  //Beschreibung als Text
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(61 + x, 38 + y, "Außen C");
  //Eigene Temperatur
  display->setFont(ArialMT_Plain_24);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(25 + x, 5 + y, textAusC );
 }

 /*
 * Gebe Batteriespannung 2 aus 
 */
 void drawBattVoltB(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  //Beschreibung als Text
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(61 + x, 38 + y, "Batterie B");
  //Eigene Spannung
  display->setFont(ArialMT_Plain_24);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(25 + x, 5 + y, textBatVoltB);
 }

void drawHumi(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  //Beschreibung als Text
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(64 + x, 38 + y, "Luftfeuchte");
  //Eigene Temperatur
  display->setFont(ArialMT_Plain_24);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  String temp = String(sSensorLoc.fHumi) + " %"; 
  display->drawString(40 + x, 5 + y, temp);
 }

 void drawAtmo(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  //Beschreibung als Text
  display->setFont(ArialMT_Plain_10 );
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(64 + x, 38 + y, "Luftdruck");
  display->setFont(ArialMT_Plain_16);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  String temp = String(sSensorLoc.fAtmo) + " hPA"; 
  display->drawString(18 + x, 5 + y, temp);
 }

 void drawLight(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  //Beschreibung als Text
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(64 + x, 38 + y, "Helligkeit");
  //Eigene Temperatur
  display->setFont(ArialMT_Plain_24);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  String temp = String(sSensorLoc.iLight) + " lm"; 
  display->drawString(30 + x, 5 + y, temp);
 }

//Die gelbe Zeile unten Uhrzeit und Frame Pointer erst mal keine Temperatur mehr
void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState* state) {

timeInfo.tm_hour = 1;


  char buff[14];
  // sprintf_P(buff, PSTR("%02d:%02d"), timeInfo->tm_hour, timeInfo->tm_min);
 sprintf_P(buff, PSTR("%02d:%02d"), timeInfo.tm_hour, timeInfo.tm_min);
  display->setColor(WHITE);
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(0, 54, String(buff));
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  //String temp = String(currentWeather.temp, 1) + (IS_METRIC ? "°C" : "°F");
  //display->drawString(128, 54, temp);
  display->drawHorizontalLine(0, 52, 128);
}

void readTempHumDHT11()
{
  //Copied from DHTtester
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  sSensorLoc.fHumi = dht.readHumidity();
  // Read temperature as Celsius (the default)
  sSensorLoc.fTempA = dht.readTemperature();
  // Check if any reads failed and exit early (to try again).
  if (isnan(sSensorLoc.fHumi) || isnan(sSensorLoc.fTempA) ) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }


  // Compute heat index in Celsius (isFahreheit = false)
  float hic = dht.computeHeatIndex(sSensorLoc.fTempA, sSensorLoc.fHumi, false);

  Serial.print("IntHumidity: ");
  Serial.print(sSensorLoc.fHumi);
  Serial.println("% ");
  Serial.print("Temperature: ");
  Serial.print(sSensorLoc.fTempA);
  Serial.println("°C ");
  Serial.print("Heat index: ");
  Serial.print(hic);
  Serial.println("°C ");
 

  
}


void readLight(){
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
  for (sSensorLoc.iLight = 0; Wire.available() >= 1; ) {
    char c = Wire.read();
    sSensorLoc.iLight = (sSensorLoc.iLight << 8) + (c & 0xFF);
  }
  sSensorLoc.iLight = sSensorLoc.iLight / 1.2;
  Serial.print("light: ");
  Serial.println(sSensorLoc.iLight);
}


void readAtmosphere(){
  int bmpTemp;
  sSensorLoc.fAtmo = bmp.readPressure();
  sSensorLoc.fAtmo = sSensorLoc.fAtmo / 100;
  //TODO bmpTemp remove or output?
  bmpTemp = bmp.readTemperature();
  Serial.print("Pressure = ");
  Serial.print(sSensorLoc.fAtmo);
  Serial.println(" Pascal");
  Serial.print("BMP Temp = ");
  Serial.println(bmpTemp);
}
/*
 * In WIFI Station Mode schalten um Verbindung mit Internet herzustellen
 */
void wifiConnectStation(){
  WiFi.mode(WIFI_AP_STA);
  Serial.print("WifiStat connecting to "); Serial.print(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PWD);
  while (WiFi.status() != WL_CONNECTED){
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
