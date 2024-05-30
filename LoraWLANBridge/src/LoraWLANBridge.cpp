/**************************************************************************
LoRa WLAN Bridge


  Hardware:
  ESP32-S3-DevKitC-1 mit Wroom N16R8
  LoRa E32-900T30D connected M0 M1 and Rx Tx

  Function:
  Receive data from RainSensor
  Debug out data

  History: master if not shown otherwise
  20240530  V0.1: Copy from RainSensor Project
  20240330  V0.2: Modify to read instead of send and debug out
  20240330  V0.3: Receive data structure



*/

#include <Arduino.h>
// 1 means debug on 0 means off
#define DEBUG 1
#include "LoRa_E32.h"

// Data structure for message
#include <HomeAutomationCommon.h>
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

const byte M0 = GPIO_NUM_10;  // LoRa M0
const byte M1 = GPIO_NUM_11;  // LoRa M1
const byte TxD = GPIO_NUM_12; // TX to LoRa Rx
const byte RxD = GPIO_NUM_13; // RX to LoRa Tx
const byte AUX = GPIO_NUM_14; // Auxiliary
const int ChannelNumber = 7;

// set LoRa to working mode 0  Transmitting
// LoRa_E32 e32ttl(RxD, TxD,AUX, M0, M1, UART_BPS_9600);
// use hardware serial #1
LoRa_E32 e32ttl(&Serial1, AUX, M0, M1); // RX, TX

const String sSoftware = "LoraWLANBridge V0.3";

// put function declarations here:

void printParameters(struct Configuration configuration);
void receiveValuesLoRa();
void printReceivedData();

void setup()
{
  Serial.begin(56000);
  delay(2000);
  Serial.println();
  Serial.println(sSoftware);
  Serial1.begin(9600, SERIAL_8N1, RxD, TxD);
 
  delay(500);
  Serial.println("in setup routine");
 // Serial.println("Boot Nr.: " + String(bootCount));
  // esp_sleep_enable_timer_wakeup(90e+6);
  //  Startup all pins and UART
  e32ttl.begin();
  delay(500);
  /*   ResponseStructContainer c;
  c = e32ttl.getConfiguration();
  // It's important get configuration pointer before all other operation
  Configuration configuration = *(Configuration*) c.data;
  Serial.println(c.status.getResponseDescription());
  Serial.println(c.status.code);

  printParameters(configuration) */
  ;

  // Send message
  // ResponseStatus rs = e32ttl.sendMessage("Hello, world? "  + String(bootCount));
}

void loop()
{
  Serial.println("Loop Start");
  neopixelWrite(RGB_BUILTIN, 0, 60, 60); // Green
 

  delay(100);

  neopixelWrite(RGB_BUILTIN, 0, 0, 0); // Red
  Serial.println("Wait for receiving a message");
  delay(2000);
  receiveValuesLoRa();

  /*   if (Serial.available())
    {
      String input = Serial1.readString();
      e32ttl.sendMessage(input);
    }
    ResponseStatus rs = e32ttl.sendMessage("Hello, world? ");
    String result = rs.getResponseDescription(); */
  // Serial.println(result);
  Serial.println("Loop End");

  // esp_deep_sleep_start();
}

void receiveValuesLoRa()
{
  ESPNOW_DATA_STRUCTURE sLoRaReceiveData;
  int iChannelNr = ChannelNumber;
  // If something available
  if (e32ttl.available() > 1)
  {
    // read the String message
    ResponseStructContainer rc = e32ttl.receiveMessage(sizeof(sLoRaReceiveData));
    memcpy(&sLoRaReceiveData, rc.data, sizeof(sLoRaReceiveData));
    // e32ttl.receiveMessage(sizeof(data));
    String result = rc.status.getResponseDescription();
    Serial.println(result);

    Serial.println("Received something");

    // Is something goes wrong print error
    if (rc.status.code != 1)
    {
      Serial.println(rc.status.getResponseDescription());
    }
    else
    {
      // Print the data received
      Serial.println(rc.status.getResponseDescription());
      debugln("Received Data");
           debug("Channel nr.:");
      debugln(iChannelNr);
      sSensor[iChannelNr].iTempB = roundf((sLoRaReceiveData.fESPNowTempB ) * 100);
      debugln("TempB: ");
      debugln(sSensor[iChannelNr].iTempB);
      sSensor[iChannelNr].iHumi = roundf(sLoRaReceiveData.fESPNowHumi * 100);
      debug("Humidity: ");
      debugln(sSensor[iChannelNr].iHumi);
    }
  }
}
void printReceivedData()
{

}

void printParameters(struct Configuration configuration)
{
  Serial.println("----------------------------------------");

  Serial.print(F("HEAD BIN: "));
  Serial.print(configuration.HEAD, BIN);
  Serial.print(" ");
  Serial.print(configuration.HEAD, DEC);
  Serial.print(" ");
  Serial.println(configuration.HEAD, HEX);
  Serial.println(F(" "));
  Serial.print(F("AddH BIN: "));
  Serial.println(configuration.ADDH, BIN);
  Serial.print(F("AddL BIN: "));
  Serial.println(configuration.ADDL, BIN);
  Serial.print(F("Chan BIN: "));
  Serial.print(configuration.CHAN, DEC);
  Serial.print(" -> ");
  Serial.println(configuration.getChannelDescription());
  Serial.println(F(" "));
  Serial.print(F("SpeedParityBit BIN    : "));
  Serial.print(configuration.SPED.uartParity, BIN);
  Serial.print(" -> ");
  Serial.println(configuration.SPED.getUARTParityDescription());
  Serial.print(F("SpeedUARTDataRate BIN : "));
  Serial.print(configuration.SPED.uartBaudRate, BIN);
  Serial.print(" -> ");
  Serial.println(configuration.SPED.getUARTBaudRate());
  Serial.print(F("SpeedAirDataRate BIN  : "));
  Serial.print(configuration.SPED.airDataRate, BIN);
  Serial.print(" -> ");
  Serial.println(configuration.SPED.getAirDataRate());

  Serial.print(F("OptionTrans BIN       : "));
  Serial.print(configuration.OPTION.fixedTransmission, BIN);
  Serial.print(" -> ");
  Serial.println(configuration.OPTION.getFixedTransmissionDescription());
  Serial.print(F("OptionPullup BIN      : "));
  Serial.print(configuration.OPTION.ioDriveMode, BIN);
  Serial.print(" -> ");
  Serial.println(configuration.OPTION.getIODroveModeDescription());
  Serial.print(F("OptionWakeup BIN      : "));
  Serial.print(configuration.OPTION.wirelessWakeupTime, BIN);
  Serial.print(" -> ");
  Serial.println(configuration.OPTION.getWirelessWakeUPTimeDescription());
  Serial.print(F("OptionFEC BIN         : "));
  Serial.print(configuration.OPTION.fec, BIN);
  Serial.print(" -> ");
  Serial.println(configuration.OPTION.getFECDescription());
  Serial.print(F("OptionPower BIN       : "));
  Serial.print(configuration.OPTION.transmissionPower, BIN);
  Serial.print(" -> ");
  Serial.println(configuration.OPTION.getTransmissionPowerDescription());

  Serial.println("----------------------------------------");
}
void printModuleInformation(struct ModuleInformation moduleInformation)
{
  Serial.println("----------------------------------------");
  Serial.print(F("HEAD BIN: "));
  Serial.print(moduleInformation.HEAD, BIN);
  Serial.print(" ");
  Serial.print(moduleInformation.HEAD, DEC);
  Serial.print(" ");
  Serial.println(moduleInformation.HEAD, HEX);

  Serial.print(F("Freq.: "));
  Serial.println(moduleInformation.frequency, HEX);
  Serial.print(F("Version  : "));
  Serial.println(moduleInformation.version, HEX);
  Serial.print(F("Features : "));
  Serial.println(moduleInformation.features, HEX);
  Serial.println("----------------------------------------");
}