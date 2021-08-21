/**************************************************************************
Home Automation Project
   Lightning Detector

  Hardware: 
  ESP Node MCU 0.1
  GY-AS3935 Lightning Sensor
  
 Calibrate sensor antenna
 Output values to serial
 Detect lightnings and output to serial
 Output to LCD display
 Send results via MQTT to Home Automation Node Red

  History: master if not shown otherwise
  20210629  V0.1:  Copy from AE_AS3935Demo

  

**************************************************************************/
#include <Arduino.h>

#include <Wire.h>
//#include <FreqCounter.h>

#define AS3935_ADRS 0x03
#define LCD_ADRS 0x3E 

#define AFE_GB  0x1F  // ANALOG FRONT END GAIN BOOST = 12(Indoor) 0x00 to 0x1F
#define NF_LEV  0x02  // NOIS FLOOR LEVEL 0x00 to 0x07
#define WDTH    0x02  // WATCH DOG THRESHOLD 0x00 to 0x0F

String LCD_STRING;
byte reg0,reg1,reg2,reg3,reg4,reg5,reg6,reg7,reg8,reg3A,reg3B,reg3C,reg3D,regDUMMY;
unsigned long enargy;
unsigned int distance;
long int freq;
long int OLD_BUFFER = 10000;
long int NOW_BUFFER = 0;
int C;
long int F;
byte CAP_RESULT;

//Forward declarations
void INIT_AS3935();

void setup() {
  //pinMode(2,INPUT);
  

  Wire.begin();
  Serial.begin(9600);          // start serial communication at 9600bps
  Serial.println("READY");   // print the reading
  //INIT_LCD();
  //DISP_OPENNING();
  INIT_AS3935();

}

void loop() {
  // put your main code here, to run repeatedly:
}

void INIT_AS3935(void)
{
  ByteWrite(0x3C,0x96);
  ByteWrite(0x3D,0x96);
  ByteWrite(0x00,(AFE_GB << 1));  // SET ANALOG FRONT END GAIN BOOST
  ByteWrite(0x01,((NF_LEV << 4) | WDTH));
  ByteWrite(0x03,0x00);           // FRQ DIV RATIO = 1/16
 // CALIB_LCO();
}