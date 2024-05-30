/**************************************************************************
RainSensor


  Hardware:
  ESP32-S3-DevKitC-1 mit Wroom N16R8
  LoRa E32-900T30D connected M0 M1 and Rx Tx
  Hall Sensor with Adapter 

  Try sending a message to remote LoRa
  Sleep for a certain time
  Put LoRa to sleep mode
  Setup LoRa to defined parameters
  Measure Temperature
  Send Temperature and Time Stamp

  History: master if not shown otherwise
  20240525  V0.1: Wakeup with timer and count boots in RTC memory (Copy from ESP32-S3 Test V0.2)
  20240528  V0.2: Send Hello world periodically



*/

#include <Arduino.h>
// #include <SoftwareSerial.h>
#include "LoRa_E32.h"

#define BUTTON_PIN_BITMASK 0x8000
/*PIN definitions
LoRa
M0 GPI16
M1 GPI17
TX GPI18
RX GPI19
SDA GPIO 21
SCL GPIO 22
*/

const byte M0 = GPIO_NUM_10;  // LoRa M0
const byte M1 = GPIO_NUM_11;  // LoRa M1
const byte TxD = GPIO_NUM_12; // TX to LoRa Rx
const byte RxD = GPIO_NUM_13; // RX to LoRa Tx
const byte AUX = GPIO_NUM_14; // Auxiliary

// set LoRa to working mode 0  Transmitting
// LoRa_E32 e32ttl(RxD, TxD,AUX, M0, M1, UART_BPS_9600);

LoRa_E32 e32ttl(&Serial1, AUX, M0, M1); // RX, TX

const String sSoftware = "ESP32 LoRa Test V0.3";

RTC_DATA_ATTR int bootCount = 0;

// put function declarations here:

void printParameters(struct Configuration configuration);

void setup()
{
  Serial.begin(56000);
  Serial.flush();
  delay(2000);
  Serial.println();
  Serial.println(sSoftware);
  Serial1.begin(9600, SERIAL_8N1, RxD, TxD);
  ++bootCount;
  delay(500);
  Serial.println("in setup routine");
  Serial.println("Boot Nr.: " + String(bootCount));
  //esp_sleep_enable_timer_wakeup(90e+6);
   // Startup all pins and UART
  e32ttl.begin();
  delay(500);
    /*   ResponseStructContainer c;
    c = e32ttl.getConfiguration();
    // It's important get configuration pointer before all other operation
    Configuration configuration = *(Configuration*) c.data;
    Serial.println(c.status.getResponseDescription());
    Serial.println(c.status.code);
 
    printParameters(configuration) */;

  Serial.println("Hi, I'm going to send message!");
 
  // Send message
  ResponseStatus rs = e32ttl.sendMessage("Hello, world? "  + String(bootCount));
   
}

void loop()
{
  Serial.println("Loop Start");
  neopixelWrite(RGB_BUILTIN, 0, 60, 60); // Green
   ++bootCount;

  delay(100);

  neopixelWrite(RGB_BUILTIN, 0, 0, 0); // Red
    Serial.println("Hi, I'm going to send message!");
 
  // Send message
  ResponseStatus rs = e32ttl.sendMessage("Hello, world? "  + String(bootCount));
  delay(1000);

 /*  // If something available
  if (e32ttl.available() > 1)
  {
    // read the String message
    ResponseContainer rc = e32ttl.receiveMessage();
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
      Serial.println(rc.data);
    }
  }
  if (Serial.available())
  {
    String input = Serial1.readString();
    e32ttl.sendMessage(input);
  }
  ResponseStatus rs = e32ttl.sendMessage("Hello, world? ");
  String result = rs.getResponseDescription();
  Serial.println(result); */
  Serial.println("Loop End");
  Serial.flush();
  //esp_deep_sleep_start();
}

void printParameters(struct Configuration configuration) {
    Serial.println("----------------------------------------");
 
    Serial.print(F("HEAD BIN: "));  Serial.print(configuration.HEAD, BIN);Serial.print(" ");Serial.print(configuration.HEAD, DEC);Serial.print(" ");Serial.println(configuration.HEAD, HEX);
    Serial.println(F(" "));
    Serial.print(F("AddH BIN: "));  Serial.println(configuration.ADDH, BIN);
    Serial.print(F("AddL BIN: "));  Serial.println(configuration.ADDL, BIN);
    Serial.print(F("Chan BIN: "));  Serial.print(configuration.CHAN, DEC); Serial.print(" -> "); Serial.println(configuration.getChannelDescription());
    Serial.println(F(" "));
    Serial.print(F("SpeedParityBit BIN    : "));  Serial.print(configuration.SPED.uartParity, BIN);Serial.print(" -> "); Serial.println(configuration.SPED.getUARTParityDescription());
    Serial.print(F("SpeedUARTDataRate BIN : "));  Serial.print(configuration.SPED.uartBaudRate, BIN);Serial.print(" -> "); Serial.println(configuration.SPED.getUARTBaudRate());
    Serial.print(F("SpeedAirDataRate BIN  : "));  Serial.print(configuration.SPED.airDataRate, BIN);Serial.print(" -> "); Serial.println(configuration.SPED.getAirDataRate());
 
    Serial.print(F("OptionTrans BIN       : "));  Serial.print(configuration.OPTION.fixedTransmission, BIN);Serial.print(" -> "); Serial.println(configuration.OPTION.getFixedTransmissionDescription());
    Serial.print(F("OptionPullup BIN      : "));  Serial.print(configuration.OPTION.ioDriveMode, BIN);Serial.print(" -> "); Serial.println(configuration.OPTION.getIODroveModeDescription());
    Serial.print(F("OptionWakeup BIN      : "));  Serial.print(configuration.OPTION.wirelessWakeupTime, BIN);Serial.print(" -> "); Serial.println(configuration.OPTION.getWirelessWakeUPTimeDescription());
    Serial.print(F("OptionFEC BIN         : "));  Serial.print(configuration.OPTION.fec, BIN);Serial.print(" -> "); Serial.println(configuration.OPTION.getFECDescription());
    Serial.print(F("OptionPower BIN       : "));  Serial.print(configuration.OPTION.transmissionPower, BIN);Serial.print(" -> "); Serial.println(configuration.OPTION.getTransmissionPowerDescription());
 
    Serial.println("----------------------------------------");
 
}
void printModuleInformation(struct ModuleInformation moduleInformation) {
    Serial.println("----------------------------------------");
    Serial.print(F("HEAD BIN: "));  Serial.print(moduleInformation.HEAD, BIN);Serial.print(" ");Serial.print(moduleInformation.HEAD, DEC);Serial.print(" ");Serial.println(moduleInformation.HEAD, HEX);
 
    Serial.print(F("Freq.: "));  Serial.println(moduleInformation.frequency, HEX);
    Serial.print(F("Version  : "));  Serial.println(moduleInformation.version, HEX);
    Serial.print(F("Features : "));  Serial.println(moduleInformation.features, HEX);
    Serial.println("----------------------------------------");
 
}