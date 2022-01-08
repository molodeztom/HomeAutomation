/*
 WLAN Temperatursensor
 ESP Now mit dem Thermometer DS18B20 und Feuchtigkeitssensor DHT-22
 Wenn noch keine MAC Adresse ermittelt wurde
 sucht der Sensor ein WLAN mit SSID Thermometer
 Wenn er den AP gefunden hat, merkt er sich die MAC Adresse 
 solange die Stromversorgung nicht unterbrochen wurde
 Das ESP Now Protokoll ist sehr schnell sodass nur sehr kurze Zeit (us)
 höherer Strom fließt. Der Sensor sendet Temperaturdaten und geht dann für 5 Minuten in den 
 Tiefschlaf sodass nur ganz wenig Energie verbraucht wird und der
 Sensor daher mit Batterien betrieben werden kann. 

 20191228:  V0.1:   Neu aus WLANThermoExt und dann DHT-22 Code dazu
 20191228:  V0.2:   Temp und Humidity werden sauber übertragen
 20191228:  V0.3:   Sensor Auflösung wird auf 12 Bit gesetzt
 20191229   V0.4:   Data struct angepasst Kanal zuerst
 20200428   V0.5:   Show software version on startup 
 20200429   V0.6:   Add Humi Sensor change channel to 4 für PCB
 20200516   V0.7:   Channel 1 für Test des V1.3 PCB
 20200523   V0.8:   Converted to platformio, DEBUG with pre-compiler
 20200524   V0.9:   Removed some unneeded Debug prints
 20200628   V1.0:   Temp Sens correction 
 20201019   V1.1:   Include DHT_U problem with PIO 5
 20201019   V1.2:   Password sample TEST
 20220108   V1.3:   Rename TempSensAdressen
*/

#include <Arduino.h>
//#define DEBUG

//Bibliotheken für WiFi
#include <ESP8266WiFi.h>

//Bibliotheken für Temperatursensor DS18B20
#include "Spi.h"
//#include <OneWire.h>
#include "Wire.h"

//Bibliothek für ESP Now
#include <DallasTemperature.h>

//Bibliothek für DHT-22 Nachträglich DHT_U.h hinzugefügt 
//https://community.platformio.org/t/library-dependency-problem-related-to-core-5-0/15726/3
#include <DHT_U.h>
//#include "DHT.h"

extern "C"
{
#include <espnow.h>
}
// DELETE THIS INCLUDE YOU DO NOT NEED THIS FILE WHEN USING THE PASSWORDS BELOW
#include "D:\Arduino\HomeAutomationSecrets.h"
/* ADD YOUR OWN VALUES IF YOU WANT TO USE THIS PROJEKT****************************************************************************

const char *WIFI_SSID = ""
const char *WIFI_PWD = "";
//SSID provided by AP for some time. Sensor can contact and transfer MAC adress.
const char *APSSID = "";

*/

const String sSoftware = "WLANThermoHumiEXT V1.3";

//Konstanten für WiFi
#define WIFI_CHANNEL 1
#define SEND_TIMEOUT 2450 // 245 Millisekunden timeout
#define CHAN 5            // sensor channel 1 = Developmentboard 2 = ESP gelötet
#define SLEEP_TIME 90E6   //Zeit in  uS z.B. 5 Minuten entspricht 300E6 normal 90

//Pins für Temperatursensor
const byte bus = 4; //pin GPIO2 (NodeMCU) GPIO4 (ESP12-F Modul) verwenden
#define BAT_SENSE A0
float fVoltage;
#define BATCALVOLT 0.01076923073; //Pre Kalibrierung bei VBatt 4,19 V genauer wird es am Empfänger kalibriert
int cntSens = 0;                  //Anzahl Sensoren
#define SENSCORR1A -0.3

//Pins für DHT-22
#define DHTPIN 14     // Digital pin connected to the DHT sensor (3 and 4 won't work)
#define DHTTYPE DHT22 // DHT 22  (AM2302), AM2321
// Initialize DHT sensor.
DHT dht(DHTPIN, DHTTYPE);

//Datenstruktur für den Datenaustausch
struct DATEN_STRUKTUR
{
  int chan = CHAN;
  float temp1 = -127; //Aussen A
  float temp2 = -127; //Aussen B
  float fHumidity = -1;
  float fVoltage = -1; //Batterie Sensor
};

//Datenstruktur für das Rtc Memory mit Prüfsumme um die Gültigkeit
//zu überprüfen damit wird die MAC Adresse gespeichert
struct MEMORYDATA
{
  uint32_t crc32;
  uint8_t mac[6];
};

//Globale daten
volatile bool callbackCalled;

MEMORYDATA statinfo;
OneWire oneWire(bus);

DallasTemperature sensoren(&oneWire);
//Forward declarations
uint32_t calculateCRC32(const uint8_t *data, size_t length);
void UpdateRtcMemory();
void ScanForSlave();
void printAddress(DeviceAddress adressen);

//Array um Sensoradressen zu speichern (DeviceAdress ist bereits ein Array mit 8 Adressen)NEIN stimmt nicht
DeviceAddress TempSensAdress;
//Array um Sensoradressen zu speichern TODO Anzahl Sensoren automatisch ermitteln
//DeviceAddress adressenarray[2];

void setup()
{
    Serial.begin(115200);
#ifdef DEBUG
 
  delay(4000);
  Serial.println("START");
  Serial.println("START");
  Serial.println(sSoftware);
#endif

  // Initialize DHT sensor.
  dht.begin();
  //Init WireBus
  pinMode(bus, INPUT); //we have an external 10k resistor attached
  //Wir ermitteln die Anzahl der Sensoren am Eindraht-Bus
  sensoren.begin();

#ifdef DEBUG
  Serial.print(sensoren.getDeviceCount(), DEC);
  Serial.println(" Sensoren gefunden.");
#endif

  //Nun prüfen wir ob einer der Sensoren am Bus ein Temperatur Sensor bis zu 2 werden gelesen
  if (!sensoren.getAddress(TempSensAdress, 0))
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

  //------------------------------------------------------------------------------------------WIFI begin
  //Wir lesen aus dem RTC Memory
  ESP.rtcUserMemoryRead(0, (uint32_t *)&statinfo, sizeof(statinfo));

  uint32_t crcOfData = calculateCRC32(((uint8_t *)&statinfo) + 4, sizeof(statinfo) - 4);
  WiFi.mode(WIFI_STA); // Station mode for esp-now sensor node

  if (statinfo.crc32 != crcOfData)
  { //wir haben keine gültige MAC Adresse

    Serial.println("Scan vor Slave");
    ScanForSlave();

    Serial.printf("This mac: %s, ", WiFi.macAddress().c_str());
    Serial.printf("target mac: %02x%02x%02x%02x%02x%02x", statinfo.mac[0], statinfo.mac[1], statinfo.mac[2], statinfo.mac[3], statinfo.mac[4], statinfo.mac[5]);
    Serial.printf(", channel: %i\n", WIFI_CHANNEL);
  }
  if (esp_now_init() != 0)
  {

    Serial.println("*** ESP_Now init failed");
    ESP.restart();
  }
  //ESP Now Controller
  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
  //Peer Daten initialisieren
  esp_now_add_peer(statinfo.mac, ESP_NOW_ROLE_SLAVE, WIFI_CHANNEL, NULL, 0);
  //wir registrieren die Funktion, die nach dem Senden aufgerufen werden soll
  esp_now_register_send_cb([](uint8_t *mac, uint8_t sendStatus) {
#ifdef DEBUG
    Serial.print("send_cb, status = ");
    Serial.print(sendStatus);
    Serial.print(", to mac: ");
    char macString[50] = {0};
    sprintf(macString, "%02X:%02X:%02X:%02X:%02X:%02X", statinfo.mac[0], statinfo.mac[1], statinfo.mac[2], statinfo.mac[3], statinfo.mac[4], statinfo.mac[5]);
    Serial.println(macString);
#endif
    callbackCalled = true;
  });
  //Flag auf false setzen
  callbackCalled = false;
  //------------------------------------------------------------------------------------------WIFI end

  //Batterie prüfen
  int nVoltageRaw;
  nVoltageRaw = analogRead(BAT_SENSE);
  fVoltage = (float)nVoltageRaw * BATCALVOLT;
#ifdef DEBUG
  Serial.print("nVoltage Raw: ");
  Serial.println(nVoltageRaw);
  Serial.print("fVoltage: ");
  Serial.println(fVoltage);
#endif

  //Temperaturmessung starten
  sensoren.requestTemperatures();

  delay(750); //750 ms warten bis die Messung fertig ist

  //Temperaturwert holen und in Datenstruktur zum Senden speichern

  DATEN_STRUKTUR data;
  data.temp1 = sensoren.getTempCByIndex(0) + SENSCORR1A;
  data.temp2 = sensoren.getTempCByIndex(1);
  //Batterie messen
  data.fVoltage = fVoltage;
  //Humidity auslesen
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float fHumidity = dht.readHumidity();
 // Serial.print("Humidity: ");
 // Serial.println(fHumidity);

  // Check if any reads failed and exit early (to try again).
  if (isnan(fHumidity))
  {
    Serial.println(F("Failed to read from DHT sensor!"));
    fHumidity = -1;
  }
  data.fHumidity = fHumidity;

  uint8_t bs[sizeof(data)];
  //Datenstruktur in den Sendebuffer kopieren
  memcpy(bs, &data, sizeof(data));
  //Daten an Thermometer senden
  esp_now_send(NULL, bs, sizeof(data)); // NULL means send to all peers
#ifdef DEBUG
  Serial.print("Temperatur1: ");
  Serial.print(data.temp1);
  Serial.println("°C");
  Serial.print("Temperatur2: ");
  Serial.print(data.temp2);
  Serial.println("°C");
  Serial.print("fVoltage: ");
  Serial.print(data.fVoltage);
  Serial.println("V");
  Serial.print("fHumidity: ");
  Serial.print(data.fHumidity);
  Serial.println(" %rel");
#endif
}

void loop()
{
  //warten bis Daten gesendet wurden
  if (callbackCalled || (millis() > SEND_TIMEOUT))
  {

    delay(100);
    //Für 30 Sekunden in den Tiefschlafmodus
    //dann erfolgt ein Reset und der ESP8266 wird neu gestartet
    //Daten im RTC Memory werden beim Reset nicht gelöscht.
    ESP.deepSleep(SLEEP_TIME);
  }
}

//Unterprogramm zum Berechnen der Prüfsumme
uint32_t calculateCRC32(const uint8_t *data, size_t length)
{
  uint32_t crc = 0xffffffff;
  while (length--)
  {
    uint8_t c = *data++;
    for (uint32_t i = 0x80; i > 0; i >>= 1)
    {
      bool bit = crc & 0x80000000;
      if (c & i)
      {
        bit = !bit;
      }
      crc <<= 1;
      if (bit)
      {
        crc ^= 0x04c11db7;
      }
    }
  }
  return crc;
}

//Schreibt die Datenstruktur statinfo mit korrekter Prüfsumme in das RTC Memory
void UpdateRtcMemory()
{
  uint32_t crcOfData = calculateCRC32(((uint8_t *)&statinfo) + 4, sizeof(statinfo) - 4);
  statinfo.crc32 = crcOfData;
  ESP.rtcUserMemoryWrite(0, (uint32_t *)&statinfo, sizeof(statinfo));
}

//sucht nach einem geeigneten AccessPoint
void ScanForSlave()
{

  int8_t scanResults = WiFi.scanNetworks();
  // reset on each scan

  if (scanResults == 0)
  {
    Serial.println("No WiFi devices in AP Mode found");
  }
  else
  {
#ifdef DEBUG
    Serial.print("Found ");
    Serial.print(scanResults);
    Serial.println(" devices ");
#endif
    for (int i = 0; i < scanResults; ++i)
    {
      // Print SSID and RSSI for each device found
      String SSID = WiFi.SSID(i);
      int32_t RSSI = WiFi.RSSI(i);
      String BSSIDstr = WiFi.BSSIDstr(i);
#ifdef DEBUG
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(SSID);
      Serial.print(" (");
      Serial.print(RSSI);
      Serial.print(")");
      Serial.println("");
#endif
      delay(10);
      // Check if the current device starts with
      if (SSID.indexOf(APSSID) == 0)
      {
        // SSID of interest
#ifdef DEBUG

        Serial.println("Found a Slave.");
        Serial.print(i + 1);
        Serial.print(": ");
        Serial.print(SSID);
        Serial.print(" [");
        Serial.print(BSSIDstr);
        Serial.print("]");
        Serial.print(" (");
        Serial.print(RSSI);
        Serial.print(")");
        Serial.println("");
#endif
        int mac[6];
        // wir ermitteln die MAC Adresse und speichern sie im RTC Memory
        if (6 == sscanf(BSSIDstr.c_str(), "%x:%x:%x:%x:%x:%x%c", &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]))
        {
          for (int ii = 0; ii < 6; ++ii)
          {
            statinfo.mac[ii] = (uint8_t)mac[ii];
          }
          UpdateRtcMemory();
        }

        //Nachdem der AP gefunden wurde können wir abbrechen
        break;
      }
    }
  }
  // RAM freigeben
  WiFi.scanDelete();
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
