/**************************************************************************/
/*
  I2C Scanner find which device(s) and adresses are on the I2C bus

Copyright (c) 2018 Luis Llamas
(www.luisllamas.es)
Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions and limitations under the License
 
*/
/**************************************************************************/

/**************************************************************************
Home Automation Project
   Valve Control for central heating 
   I2C Scanner used to find the current sensor

  Hardware: 
  ESP Node MCU 0.1
  MCP4725A1 I2C to 0 - 5 V DC output
  LMxx to amplify 0-5 V DC to 0-10 V DC
  
  History:
  20201102 V0.1  First test from library I2C Scanner sample

  

**************************************************************************/

#include <Arduino.h>
#include "I2CScanner.h"
#include <Wire.h>

I2CScanner scanner;

//if you use ESP8266-01 with not default SDA and SCL pins, define these 2 lines, else delete them	
// use Pin Numbers from GPIO e.g. GPIO4 = 4
// For NodeMCU Lua Lolin V3: D1=GPIO5 = SCL D2=GPIO4 = SDA set to SDA_PIN 4, SCL_PIN 5
#define SDA_PIN 4
#define SCL_PIN 5

void setup() 
{	
	//uncomment the next line if you use custom sda and scl pins for example with ESP8266-01 (sda = 4, scl = 5)
	Wire.begin(SDA_PIN, SCL_PIN);
	
	Serial.begin(9600);
	while (!Serial) {};

	scanner.Init();
}

void loop() 
{
	scanner.Scan();
	delay(5000);
}