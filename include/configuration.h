/*
* ----------------------------------------------------------------------------
* The MIT License (MIT)
*
* Copyright (c) 2016 - Matthias Jentsch
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
* ----------------------------------------------------------------------------
*/

#ifndef __configuration_H__
#define __configuration_H__

// initialize the configuration module; return true if successfully initialized; false if no configuration was found
unsigned char ICACHE_FLASH_ATTR configuration_init();
// called after the single shot ultrasonic measurement is finished
void ICACHE_FLASH_ATTR configuration_sendSingleShotMeasurement();
// start the configuration mode
void ICACHE_FLASH_ATTR configuration_start();
// WiFi network name (SSID)
char* ICACHE_FLASH_ATTR configuration_getWifiSsid();
// WiFi password
char* ICACHE_FLASH_ATTR configuration_getWifiPassword();
// the ESP8266 set this hostname after a connection to the access point is established
char* ICACHE_FLASH_ATTR configuration_getHostname();
// the deep sleep period in seconds
unsigned short ICACHE_FLASH_ATTR configuration_getDeepSleepPeriod();
// if the difference between the last measurement and the current measurement is greater than this value in mm the data should be posted to the internet immediately
unsigned short ICACHE_FLASH_ATTR configuration_getMinDifferenceToPost();
// after this max count of deep sleep cycles also an unchanged measurement will be postet
unsigned short ICACHE_FLASH_ATTR configuration_getMaxDataAgeToPost();
// if TRUE the data should be posted to a Thingspeak server
unsigned char ICACHE_FLASH_ATTR configuration_shouldPostToThingspeak();
// Thingspeak server URL
char* ICACHE_FLASH_ATTR configuration_getThingspeakServerUrl();
// API key for ThingSpeak
char* ICACHE_FLASH_ATTR configuration_getThingspeakApiKey();
// if TRUE the data should be posted to a MQTT server
unsigned char ICACHE_FLASH_ATTR configuration_shouldPostToMqtt();
// MQTT server name or IP address
char* ICACHE_FLASH_ATTR configuration_getMqttServer();
// MQTT server port
unsigned short ICACHE_FLASH_ATTR configuration_getMqttPort();
// MQTT username
char* ICACHE_FLASH_ATTR configuration_getMqttUsername();
// MQTT password
char* ICACHE_FLASH_ATTR configuration_getMqttPassword();
// MQTT client name
char* ICACHE_FLASH_ATTR configuration_getMqttClientName();
// MQTT topic
char* ICACHE_FLASH_ATTR configuration_getMqttTopic();
// returns the cistern parameters in the parameters
void ICACHE_FLASH_ATTR configuration_getCisternParameters(unsigned char *cisternType, unsigned int *cisternRadius,
	unsigned int *cisternLength, unsigned int *distanceEmpty, unsigned int *litersFull);
// gets the distance in millimeters water to ultrasonic sensor if the cistern is empty
unsigned int ICACHE_FLASH_ATTR configuration_getDistanceEmpty();
// return the log type: 0 = logging disabled; 1 = logging will be sent using insecure TCP connection; 2 = logging will be sent using secure TCP connection
unsigned char ICACHE_FLASH_ATTR configuration_getLogType();
// host name or IPv4addres: if we have a wifi connection we send the log to this host
char* ICACHE_FLASH_ATTR configuration_getLogHost();
// if we have a wifi connection we send the log to this port
unsigned short ICACHE_FLASH_ATTR configuration_getLogPort();

#endif // __configuration_H__
