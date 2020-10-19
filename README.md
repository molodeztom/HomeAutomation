# HomeAutomation
This project is still in prototype status
Specifications:
several remote sensors with ESP8266 type a with temperature type b with humidity and temperature
remote sensors shall save energy thus using sleep mode and esp-now protocol
remote sensors report battery voltage
home server as receiver for esp-now and in combination with serial connection to weather station acts as bridge between esp-now and wifi
weather station as receiver, displays values on OLED and provides MQTT messages with measurement values
MQTT is received on a raspberry
raspberry with docker images uses node-red to write mqtt measurements to influx db. Displays values from influx db in grafana.
valve control is esp8266 hardware controlling a valve for central heating goal is to reduce warm water input depending on room temperatures (not yet implemented)
remote display is a simple mqtt receiver with OLED to display outdoor temp and humidity
