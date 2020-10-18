
/*
 WLAN Temperatursensor
 ESP Now mit dem Thermometer
 Wenn noch keine MAC Adresse ermittelt wurde
 sucht der Sensor ein WLAN mit SSID Thermometer
 Wenn er den AP gefunden hat, merkt er sich die MAC Adresse 
 solange die Stromversorgung nicht unterbrochen wurde
 Das ESP Now Protokoll ist sehr schnell sodass nur sehr kurze Zeit (us)
 höherer Strom fließt. Der Sensor sendet Temperaturdaten und geht dann für 5 Minuten in den 
 Tiefschlaf sodass nur ganz wenig Energie verbraucht wird und der
 Sensor daher mit Batterien betrieben werden kann. 
 201909: V0.1:  Kopiert aus https://www.az-delivery.de/blogs/azdelivery-blog-fur-arduino-und-raspberry-pi/entfernter-temperatursensor-fur-thermometer-mit-esp8266?ls=de
                    Aber nur die Teile die man für den Sensor auslesen benötigt
 20190922:  V0.2:   WLAN Code dazu für Kommunikation mit Basis
 20190922:  V1.0: Funktionierende Übertragung der ext Temperatur nach OLEd Thermometer v1.0
 20190924:  V1.1: Mehr Debug infos 
 20190924:  V1.2: 2. Sensor noch falsch adressen ist MAC 8 Array nicht adressen x Sensor Array
 20190924:  senIndex V1.3: Sensoren über Index statt Adresse auslesen siehe hier https://lastminuteengineers.com/multiple-ds18b20-arduino-tutorial/
 20191229   V1.4  Batteriespannung an ADC mit Spannungsteiler 100k / 10k messen und ausgeben. 100k direkt an Batterie verlötet um Kurzschlussgefahr zu vermeiden. Batt- nicht verdrahten! Wird über GND über Schutzschaltung gemessen. 
 20200107   V1.5  Batteriespannung Faktor angepasst, Sleep Time als Define 
 20200417   V1.6  Angepasst für Sensor 3 ChanNr und BattCalibration
 20200428   V1.7  Test mit weiterem Sensor 4 um den Sensor Eingang für PCB V1.3 zu prüfen
 20200428   V1.8  2. Sensor daneben und aktiviert für vergleich
 20200522   V1.9  Converted to platformio, DEBUG with pre-compiler settings
 
 
 */

#include <Arduino.h>
#define DEBUG

//Bibliotheken für WiFi
#include <ESP8266WiFi.h>

//Bibliotheken für Temperatursensor DS18B20
//#include <OneWire.h>


//Bibliothek für ESP Now
#include <DallasTemperature.h>

extern "C"
{
#include <espnow.h>
}
// DELETE THIS INCLUDE YOU DO NOT NEED THIS FILE WHEN USING THE PASSWORDS BELOW
#include "D:\Arduino\HomeAutomationSecrets.h"

const String sSoftware = "WLANThermoEXT V1.9";

//Konstanten für WiFi
#define WIFI_CHANNEL 1
#define SEND_TIMEOUT 2450 // 245 Millisekunden timeout
#define CHAN 2            // sensor channel 1 = Developmentboard 2 = ESP gelötet 3 = Blau/Rot 4 = ESP Test für Humidity
#define SLEEP_TIME 85E6   //Zeit in  uS z.B. 5 Minuten entspricht 300E6

//Pins für Temperatursensor
const byte bus = 4; //pin GPIO2 (NodeMCU) GPIO4 (ESP12-F Modul) verwenden
#define BAT_SENSE A0
float fVoltage;

#define BATCALVOLT 0.01076923073; //Pre Kalibrierung bei VBatt 4,19 V genauer wird es am Empfänger kalibriert

//Datenstruktur für den Datenaustausch
struct DATEN_STRUKTUR
{
  int chan = CHAN;
  float temp1 = -127;   //Aussen A
  float temp2 = -127;   //Aussen B nicht vorhanden
  float fHumidity = -1; //hier kein Sensor
  float fVoltage = -1;  //Batterie Sensor 2
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
DeviceAddress adressen;
//Array um Sensoradressen zu speichern TODO Anzahl Sensoren automatisch ermitteln
//DeviceAddress adressenarray[2];

void setup()
{
#ifdef DEBUG
  Serial.begin(115200);
  Serial.println("START");
  #endif

  Serial.println(sSoftware);
  pinMode(bus, INPUT); //we have an external 10k resistor attached
  //Wir ermitteln die Anzahl der Sensoren am Eindraht-Bus
  sensoren.begin();
#ifdef DEBUG
  Serial.print(sensoren.getDeviceCount(), DEC);
  Serial.println(" Sensoren gefunden.");
#endif

  //Nun prüfen wir ob einer der Sensoren am Bus ein Temperatur Sensor bis zu 2 werden gelesen
  if (!sensoren.getAddress(adressen, 0))
  {
    Serial.println("Kein Temperatursensor vorhanden!");
  }
//adressen anzeigen
#ifdef DEBUG

  Serial.print("Adresse1: ");
  printAddress(adressen);
  Serial.println();
#endif
  //2. Sensor
  if (!sensoren.getAddress(adressen, 1))
  {
    Serial.println("Kein Temperatursensor vorhanden!");
  }
//adressen anzeigen
#ifdef DEBUG
  Serial.print("Adresse2: ");
  printAddress(adressen);
  Serial.println();
#endif
  //Nun setzen wir noch die gewünschte Auflösung (9, 10, 11 oder 12 bit) TODO das ist nur für den ersten Sensor oder?
  sensoren.setResolution(adressen, 12);
//Zur Kontrolle lesen wir den Wert wieder aus
#ifdef DEBUG
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
  data.temp1 = sensoren.getTempCByIndex(0);
  data.temp2 = sensoren.getTempCByIndex(1);
  data.fVoltage = fVoltage;
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
#endif
}

void loop()
{
  //warten bis Daten gesendet wurden
  if (callbackCalled || (millis() > SEND_TIMEOUT))
  {

    delay(100);
    //Für Sleep Time Sekunden in den Tiefschlafmodus
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

#ifdef DEBUG
  Serial.println("Scan done");
  if (scanResults == 0)
  {
    Serial.println("No WiFi devices in AP Mode found");
  }
  else
  {

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
      // Check if the current device starts with `Thermometer`
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
//----------------------------------------------------------------------------------------WIFI END

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
