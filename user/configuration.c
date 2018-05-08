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

#include "user_interface.h"
#include "osapi.h"
#include "gpio.h"
#include "espconn.h"
#include "mem.h"
#include "json.h"
#include "jsonparse.h"
#include <espmissingincludes.h>
#include <io.h>
#include <powermanagement.h>
#include <ultrasonicmeter.h>
#include <cJSON.h>
#include <configuration.h>

// configuration data that will be stored into flash memory (4KB max)
typedef struct
{
	unsigned char version; // if not CONFIGURATION_DATA_VERSION then the data in the struct is not valid
	char wifiSsid[32]; // WiFi network name (SSID)
	char wifiPassword[64]; // WiFi password
	unsigned char cisternType;	// cistern type; 1 = horizontal cylinder; 2 = vertical cylinder
	unsigned int cisternRadius; // cistern radius in millimeters
	unsigned int cisternLength; // cistern length in millimeters only for the type 1 cistern needed
	unsigned int distanceEmpty; // Distance in millimeters water to ultrasonic sensor if the cistern is empty
	unsigned int litersFull; // Liters if the cistern is full and flooding
	char hostname[256];	// the ESP8266 set this hostname after a connection to the access point is established
	unsigned short deepSleepPeriod;	// the deep sleep period in seconds
	unsigned short minDifferenceToPost; // if the difference between the last measurement and the current measurement is greater than this value in mm the data should be posted to the internet immediately
	unsigned short maxDataAgeToPost; // after this max count of deep sleep cycles also an unchanged measurement will be postet
	unsigned char shouldPostToThingspeak; // if TRUE the data should be posted to a Thingspeak server
	char thingspeakServerUrl[256]; // Thingspeak server URL
	char thingspeakApiKey[32]; // API key for Thingspeak
	unsigned char shouldPostToMqtt;	// if TRUE the data should be posted to a MQTT server
	char mqttServer[256];	// MQTT server name or IP address
	unsigned short mqttPort; // MQTT server port
	char mqttUsername[256];	// MQTT username
	char mqttPassword[256];	// MQTT password
	char mqttClientName[256];	// MQTT client name
	char mqttTopic[256];	// MQTT topic
	unsigned char logType; // 0 = logging disabled; 1 = logging will be sent using insecure TCP connection; 2 = logging will be sent using secure TCP connection
	char logHost[256]; // host name or IPv4addres: if we have a wifi connection we send the log to this host
	unsigned short logPort; // if we have a wifi connection we send the log to this port
	unsigned char alignment; // aligned to 4-byte boundary
} ConfigurationData;

// the instance of the data
static ConfigurationData configuration_data;

// the socket for receiving the configuration data
static struct espconn configuration_socketConnection;
// the tcp connection for receiving the configuration data
static esp_tcp configuration_tcpConnection;

// will be called after data was received via the tcp server connection
static bool ICACHE_FLASH_ATTR configuration_parseData(cJSON *pConfigurationData)
{
	os_printf("Data received ...\n");
	char *ssid = cJSON_GetObjectItem(pConfigurationData, "SSID")->valuestring;
	char *password = cJSON_GetObjectItem(pConfigurationData, "Password")->valuestring;
	unsigned char cisternType = (unsigned char)cJSON_GetObjectItem(pConfigurationData, "CisternType")->valueint;
	unsigned int cisternRadius = (unsigned int)cJSON_GetObjectItem(pConfigurationData, "CisternRadius")->valueint;
	unsigned int cisternLength = (unsigned int)cJSON_GetObjectItem(pConfigurationData, "CisternLength")->valueint;
	unsigned int distanceEmpty = (unsigned int)cJSON_GetObjectItem(pConfigurationData, "DistanceEmpty")->valueint;
	unsigned int litersFull = (unsigned int)cJSON_GetObjectItem(pConfigurationData, "LitersFull")->valueint;
	char *hostname = cJSON_GetObjectItem(pConfigurationData, "HostName")->valuestring;
	int deepSleepPeriod = cJSON_GetObjectItem(pConfigurationData, "DeepSleepPeriod")->valueint;
	int minDifferenceToPost = cJSON_GetObjectItem(pConfigurationData, "MinDifferenceToPost")->valueint;
	int maxDataAgeToPost = cJSON_GetObjectItem(pConfigurationData, "MaxDataAgeToPost")->valueint;
	unsigned char shouldPostToThingspeak = (unsigned char)cJSON_GetObjectItem(pConfigurationData, "ShouldPostToThingspeak")->valueint;
	char *thingspeakServerUrl = cJSON_GetObjectItem(pConfigurationData, "ThingspeakServerUrl")->valuestring;
	char *thingspeakApiKey = cJSON_GetObjectItem(pConfigurationData, "ThingspeakApiKey")->valuestring;
	unsigned char shouldPostToMqtt = (unsigned char)cJSON_GetObjectItem(pConfigurationData, "ShouldPostToMqtt")->valueint;
	char *mqttServer = cJSON_GetObjectItem(pConfigurationData, "MqttServer")->valuestring;
	unsigned short mqttPort = (unsigned short)cJSON_GetObjectItem(pConfigurationData, "MqttPort")->valueint;
	char *mqttUsername = cJSON_GetObjectItem(pConfigurationData, "MqttUsername")->valuestring;
	char *mqttPassword = cJSON_GetObjectItem(pConfigurationData, "MqttPassword")->valuestring;
	char *mqttClientName = cJSON_GetObjectItem(pConfigurationData, "MqttClientName")->valuestring;
	char *mqttTopic = cJSON_GetObjectItem(pConfigurationData, "MqttTopic")->valuestring;
	unsigned char logType = (unsigned char)cJSON_GetObjectItem(pConfigurationData, "LogType")->valueint;
	char *logHost = cJSON_GetObjectItem(pConfigurationData, "LogHost")->valuestring;
	unsigned short logPort = (unsigned short)cJSON_GetObjectItem(pConfigurationData, "LogPort")->valueint;

	// all data found in the received json data?
	if (strlen(ssid) > 0 && strlen(password) > 0 &&
		((cisternType == 1 && cisternLength > 0) || cisternType == 2) &&
		cisternRadius > 0 && distanceEmpty > 0 && litersFull > 0 &&
		strlen(hostname) > 0 && deepSleepPeriod > 0 &&
		minDifferenceToPost > 0 && maxDataAgeToPost > 0 &&
		((shouldPostToThingspeak == 1 && strlen(thingspeakServerUrl) > 0 && strlen(thingspeakApiKey) > 0) ||
		(shouldPostToMqtt == 1 && strlen(mqttServer) > 0 && mqttPort != 0 && strlen(mqttClientName) > 0 && strlen(mqttTopic) > 0)))
	{
		os_printf("Found valid configuration!\n");

		// store the configuration data in structure
		os_memset(&configuration_data, 0, sizeof(configuration_data));
		configuration_data.version = CONFIGURATION_DATA_VERSION;
		os_strcpy(configuration_data.wifiSsid, ssid);
		os_strcpy(configuration_data.wifiPassword, password);
		configuration_data.cisternType = cisternType;
		configuration_data.cisternRadius = cisternRadius;
		configuration_data.cisternLength = cisternLength;
		configuration_data.distanceEmpty = distanceEmpty;
		configuration_data.litersFull = litersFull;
		os_strcpy(configuration_data.hostname, hostname);
		configuration_data.deepSleepPeriod = deepSleepPeriod;
		configuration_data.minDifferenceToPost = minDifferenceToPost;
		configuration_data.maxDataAgeToPost = maxDataAgeToPost;
		configuration_data.shouldPostToThingspeak = shouldPostToThingspeak;
		os_strcpy(configuration_data.thingspeakServerUrl, thingspeakServerUrl);
		os_strcpy(configuration_data.thingspeakApiKey, thingspeakApiKey);
		configuration_data.shouldPostToMqtt = shouldPostToMqtt;
		os_strcpy(configuration_data.mqttServer, mqttServer);
		configuration_data.mqttPort = mqttPort;
		os_strcpy(configuration_data.mqttUsername, mqttUsername);
		os_strcpy(configuration_data.mqttPassword, mqttPassword);
		os_strcpy(configuration_data.mqttClientName, mqttClientName);
		os_strcpy(configuration_data.mqttTopic, mqttTopic);
		configuration_data.logType = logType;
		os_strcpy(configuration_data.logHost, logHost);
		configuration_data.logPort = logPort;
		return TRUE;
	}
	else
	{
		os_printf("Data incomplete!\n");
		return FALSE;
	}
}

// will be called after data was received via the tcp server connection
static void ICACHE_FLASH_ATTR configuration_receiveCallback(void *arg, char *pdata, unsigned short len)
{
	os_printf("Command received ...\n");
	cJSON *root = cJSON_Parse(pdata);
	cJSON *response = cJSON_CreateObject();
	cJSON *data = cJSON_CreateObject();
	cJSON_AddItemToObject(response, "ResponseData", data);
	char *rendered;
	unsigned char writeResponse = FALSE;

	switch (cJSON_GetObjectItem(root, "CommandCode")->valueint)
	{
		// DoMeasurement
	case 1:
		ultrasonicMeter_startMeasurement(configuration_sendSingleShotMeasurement, TRUE);
		break;

		// ReadConfiguration
	case 2:
		if (configuration_data.version == CONFIGURATION_DATA_VERSION)
		{
			cJSON_AddNumberToObject(response, "ResponseCode", 2);
			cJSON_AddStringToObject(data, "SSID", configuration_data.wifiSsid);
			cJSON_AddStringToObject(data, "Password", configuration_data.wifiPassword);
			cJSON_AddNumberToObject(data, "CisternType", configuration_data.cisternType);
			cJSON_AddNumberToObject(data, "CisternRadius", configuration_data.cisternRadius);
			cJSON_AddNumberToObject(data, "CisternLength", configuration_data.cisternLength);
			cJSON_AddNumberToObject(data, "DistanceEmpty", configuration_data.distanceEmpty);
			cJSON_AddNumberToObject(data, "LitersFull", configuration_data.litersFull);
			cJSON_AddStringToObject(data, "HostName", configuration_data.hostname);
			cJSON_AddNumberToObject(data, "DeepSleepPeriod", configuration_data.deepSleepPeriod);
			cJSON_AddNumberToObject(data, "MinDifferenceToPost", configuration_data.minDifferenceToPost);
			cJSON_AddNumberToObject(data, "MaxDataAgeToPost", configuration_data.maxDataAgeToPost);
			cJSON_AddNumberToObject(data, "ShouldPostToThingspeak", configuration_data.shouldPostToThingspeak);
			cJSON_AddStringToObject(data, "ThingspeakServerUrl", configuration_data.thingspeakServerUrl);
			cJSON_AddStringToObject(data, "ThingspeakApiKey", configuration_data.thingspeakApiKey);
			cJSON_AddNumberToObject(data, "ShouldPostToMqtt", configuration_data.shouldPostToMqtt);
			cJSON_AddStringToObject(data, "MqttServer", configuration_data.mqttServer);
			cJSON_AddNumberToObject(data, "MqttPort", configuration_data.mqttPort);
			cJSON_AddStringToObject(data, "MqttUsername", configuration_data.mqttUsername);
			cJSON_AddStringToObject(data, "MqttPassword", configuration_data.mqttPassword);
			cJSON_AddStringToObject(data, "MqttClientName", configuration_data.mqttClientName);
			cJSON_AddStringToObject(data, "MqttTopic", configuration_data.mqttTopic);
			cJSON_AddNumberToObject(data, "LogType", configuration_data.logType);
			cJSON_AddStringToObject(data, "LogHost", configuration_data.logHost);
			cJSON_AddNumberToObject(data, "LogPort", configuration_data.logPort);
		}
		else
		{
			cJSON_AddNumberToObject(response, "ResponseCode", -1);
		}
		writeResponse = TRUE;
		break;

		// WriteConfiguration
	case 3:
		cJSON_AddNumberToObject(response, "ResponseCode", configuration_parseData(cJSON_GetObjectItem(root, "CommandData")) == TRUE ? 3 : -1);
		writeResponse = TRUE;
		break;

		// SaveConfigurationAndReboot
	case 4:
		// disconnect
		wifi_station_disconnect();
		wifi_set_opmode_current(NULL_MODE);
		// if configuration is valid => save in flash
		if (configuration_data.version == CONFIGURATION_DATA_VERSION)
		{
			// use the protected write to three sectors method
			system_param_save_with_protect(CONFIGURATION_DATA_START_SEC, &configuration_data, sizeof(configuration_data));
		}
		// leave configuration mode and reboot
		powermanagement_leaveConfigurationMode();
		break;
	}

	// should we send a response back now?
	if (writeResponse == TRUE)
	{
		rendered = cJSON_Print(response);
		os_printf("JSON: %s\n", rendered);
		espconn_send(&configuration_socketConnection, (uint8_t *)rendered, os_strlen(rendered));
		os_free(rendered);
	}

	// free memory
	cJSON_Delete(root);
	cJSON_Delete(response);
}

// will be called after data was sent via the tcp server connection
static void ICACHE_FLASH_ATTR configuration_sentCallback(void *arg)
{
	// close the tcp connection immediately 
	espconn_disconnect((espconn*)arg);
}

static void ICACHE_FLASH_ATTR configuration_reconnectCallback(void *arg, sint8 error)
{
	espconn *conn = arg;
	os_printf("Reconnect from %d.%d.%d.%d:%d error %d\n", conn->proto.tcp->remote_ip[0],
		conn->proto.tcp->remote_ip[1], conn->proto.tcp->remote_ip[2],
		conn->proto.tcp->remote_ip[3], conn->proto.tcp->remote_port, error);
}

static void ICACHE_FLASH_ATTR configuration_disconnectCallback(void *arg)
{
	espconn *conn = arg;

	os_printf("Disconnected from %d.%d.%d.%d:%d\n", conn->proto.tcp->remote_ip[0],
		conn->proto.tcp->remote_ip[1], conn->proto.tcp->remote_ip[2],
		conn->proto.tcp->remote_ip[3], conn->proto.tcp->remote_port);
}

// will be called after a TCp client has connected to the TCP server
static void ICACHE_FLASH_ATTR configuration_connectCallback(void *arg)
{
	// register a callback for data receive
	espconn *conn = arg;
	espconn_regist_recvcb(conn, configuration_receiveCallback);
	espconn_regist_sentcb(conn, configuration_sentCallback);
	espconn_regist_reconcb(conn, configuration_reconnectCallback);
	espconn_regist_disconcb(conn, configuration_disconnectCallback);
	os_printf("Incoming connection ...\n");
}

// called after the single shot ultrasonic measurement is finished
void ICACHE_FLASH_ATTR configuration_sendSingleShotMeasurement()
{
	// let the led blink slowly
	io_ledBlink(500, 500);

	// read and send distance
	int distance = (int)ultrasonicMeter_getSingleShotDistance();
	cJSON *response = cJSON_CreateObject();
	cJSON *data = cJSON_CreateObject();
	cJSON_AddItemToObject(response, "ResponseData", data);
	cJSON_AddNumberToObject(response, "ResponseCode", 1);
	if (distance > 0)
	{
		cJSON_AddTrueToObject(data, "IsValid");
		cJSON_AddNumberToObject(data, "Distance", distance);
	}
	else
	{
		cJSON_AddFalseToObject(data, "IsValid");
		cJSON_AddNumberToObject(data, "Distance", -1);
	}
	char *rendered = cJSON_Print(response);
	os_printf("JSON: %s\n", rendered);
	espconn_send(&configuration_socketConnection, (uint8_t *)rendered, os_strlen(rendered));
	// free memory
	os_free(response);
	os_free(rendered);
}

// start the configuration mode
void ICACHE_FLASH_ATTR configuration_start()
{
	// read from flash and test if data is valid
	os_memset(&configuration_data, 0, sizeof(configuration_data));
	if (system_param_load(CONFIGURATION_DATA_START_SEC, 0, &configuration_data, sizeof(configuration_data)) != TRUE ||
		configuration_data.version != CONFIGURATION_DATA_VERSION)
	{
		// clear configuration if invalid
		os_memset(&configuration_data, 0, sizeof(configuration_data));
	}

	// activate the a Wifi acces point and start a TCP server for listening for configuration data
	os_printf("Activating access point '%s' and listening at port %d ...\n", ACCESS_POINT_SSID, ACCESS_POINT_LISTENING_PORT);
	struct softap_config accessPointConfiguration;
	// set SSID and password
	os_strcpy(accessPointConfiguration.ssid, ACCESS_POINT_SSID);
	os_strcpy(accessPointConfiguration.password, ACCESS_POINT_PASSWORD);
	accessPointConfiguration.ssid_len = 0;
	// set the radio channel an the autorisation mode
	accessPointConfiguration.channel = ACCESS_POINT_CHANNEL;
	accessPointConfiguration.authmode = AUTH_WPA2_PSK;
	accessPointConfiguration.ssid_hidden = 0;
	// only one Wifi station can connect to the access point
	accessPointConfiguration.max_connection = 1;
	// default beacon interval is 100 milliseconds
	accessPointConfiguration.beacon_interval = 100;
	wifi_station_disconnect();
	wifi_set_opmode_current(SOFTAP_MODE);
	wifi_softap_set_config_current(&accessPointConfiguration);

	// configure a TCP server
	configuration_socketConnection.type = ESPCONN_TCP;
	configuration_socketConnection.state = ESPCONN_NONE;
	configuration_tcpConnection.local_port = ACCESS_POINT_LISTENING_PORT;
	configuration_socketConnection.proto.tcp = &configuration_tcpConnection;
	// register a callback for TCP client connections
	espconn_regist_connectcb(&configuration_socketConnection, configuration_connectCallback);
	espconn_accept(&configuration_socketConnection);
	//espconn_tcp_set_max_con_allow(&configuration_socketConnection, 1);

	os_printf("Listening ...\n");
}

// initialize the configuration module; return true if successfully initialized; false if no configuration was found
unsigned char ICACHE_FLASH_ATTR configuration_init()
{
	// read from flash and test if data is valid
	os_memset(&configuration_data, 0, sizeof(configuration_data));
	if (system_param_load(CONFIGURATION_DATA_START_SEC, 0, &configuration_data, sizeof(configuration_data)) == TRUE &&
		configuration_data.version == CONFIGURATION_DATA_VERSION)
	{
		return TRUE;
	}
	else
	{
		// start the configuration mode
		os_printf("\nConfiguration invalid ...\n");
		powermanagement_enterConfigurationMode();
		return FALSE;
	}
}

// WiFi network name (SSID)
char* ICACHE_FLASH_ATTR configuration_getWifiSsid()
{
	return configuration_data.wifiSsid;
}
// WiFi password
char* ICACHE_FLASH_ATTR configuration_getWifiPassword()
{
	return configuration_data.wifiPassword;
}

// the ESP8266 set this hostname after a connection to the access point is established
char* ICACHE_FLASH_ATTR configuration_getHostname()
{
	return configuration_data.hostname;
}

// the deep sleep period in seconds
unsigned short ICACHE_FLASH_ATTR configuration_getDeepSleepPeriod()
{
	return configuration_data.deepSleepPeriod;
}

// if the difference between the last measurement and the current measurement is greater than this value in mm the data should be posted to the internet immediately
unsigned short ICACHE_FLASH_ATTR configuration_getMinDifferenceToPost()
{
	return configuration_data.minDifferenceToPost;
}

// after this max count of deep sleep cycles also an unchanged measurement will be postet
unsigned short ICACHE_FLASH_ATTR configuration_getMaxDataAgeToPost()
{
	return configuration_data.maxDataAgeToPost;
}

// if TRUE the data should be posted to a Thingspeak server
unsigned char ICACHE_FLASH_ATTR configuration_shouldPostToThingspeak()
{
	return configuration_data.shouldPostToThingspeak;
}
// Thingspeak server URL
char* ICACHE_FLASH_ATTR configuration_getThingspeakServerUrl()
{
	return configuration_data.thingspeakServerUrl;
}
// API key for Thingspeak
char* ICACHE_FLASH_ATTR configuration_getThingspeakApiKey()
{
	return configuration_data.thingspeakApiKey;
}

// if TRUE the data should be posted to a MQTT server
unsigned char ICACHE_FLASH_ATTR configuration_shouldPostToMqtt()
{
	return configuration_data.shouldPostToMqtt;
}
// MQTT server name or IP address
char* ICACHE_FLASH_ATTR configuration_getMqttServer()
{
	return configuration_data.mqttServer;
}
// MQTT server port
unsigned short ICACHE_FLASH_ATTR configuration_getMqttPort()
{
	return configuration_data.mqttPort;
}
// MQTT username
char* ICACHE_FLASH_ATTR configuration_getMqttUsername()
{
	return configuration_data.mqttUsername;
}
// MQTT password
char* ICACHE_FLASH_ATTR configuration_getMqttPassword()
{
	return configuration_data.mqttPassword;
}
// MQTT client name
char* ICACHE_FLASH_ATTR configuration_getMqttClientName()
{
	return configuration_data.mqttClientName;
}
// MQTT topic
char* ICACHE_FLASH_ATTR configuration_getMqttTopic()
{
	return configuration_data.mqttTopic;
}

// returns the cistern parameters in the parameters
void ICACHE_FLASH_ATTR configuration_getCisternParameters(unsigned char *cisternType, unsigned int *cisternRadius,
	unsigned int *cisternLength, unsigned int *distanceEmpty, unsigned int *litersFull)
{
	*cisternType = configuration_data.cisternType;
	*cisternRadius = configuration_data.cisternRadius;
	*cisternLength = configuration_data.cisternLength;
	*distanceEmpty = configuration_data.distanceEmpty;
	*litersFull = configuration_data.litersFull;
}

// gets the distance in millimeters water to ultrasonic sensor if the cistern is empty
unsigned int ICACHE_FLASH_ATTR configuration_getDistanceEmpty()
{
	return configuration_data.distanceEmpty;
}

// return the log type: 0 = logging disabled; 1 = logging will be sent using insecure TCP connection; 2 = logging will be sent using secure TCP connection
unsigned char ICACHE_FLASH_ATTR configuration_getLogType()
{
	return configuration_data.logType;
}

// host name or IPv4addres: if we have a wifi connection we send the log to this host
char* ICACHE_FLASH_ATTR configuration_getLogHost()
{
	return configuration_data.logHost;
}

// if we have a wifi connection we send the log to this port
unsigned short ICACHE_FLASH_ATTR configuration_getLogPort()
{
	return configuration_data.logPort;
}