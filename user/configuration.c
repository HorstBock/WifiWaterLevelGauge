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
#include "json.h"
#include "jsonparse.h"
#include <espmissingincludes.h>
#include <io.h>
#include <powermanagement.h>
#include <configuration.h>

// data that will be stored into the RTC memory; this data survive the deep sleep
typedef struct
{
	unsigned char version;	// if not CONFIGURATION_DATA_VERSION then the data in the struct is not valid
	unsigned char cisternType;	// cistern type; 1 = horizontal cylinder; 2 = vertical cylinder
	unsigned int cisternRadius; // cistern radius in millimeters
	unsigned int cisternLength; // cistern length in millimeters only for the type 1 cistern needed
	unsigned int distanceEmpty; // Distance in millimeters water to ultrasonic sensor if the cistern is empty
	unsigned int litersFull; // Liters if the cistern is full and flooding
	char hostname[256];	// the ESP8266 set this hostname after a connection to the access point is established
	unsigned short deepSleepPeriod;	// the deep sleep period in seconds
	unsigned short minDifferenceToPost; // if the difference between the last measurement and the current measurement is greater than this value in mm the data should be posted to the internet immediately
	unsigned short maxDataAgeToPost; // after this max count of deep sleep cycles also an unchanged measurement will be postet
	char thingspeakApiKey[32]; // API key for ThingSpeak
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

// encapsulation to reading the next value as string
static void ICACHE_FLASH_ATTR configuration_getNextString(struct jsonparse_state *parser, char *buf, int buf_size)
{
	jsonparse_next(parser);
	jsonparse_next(parser);
	jsonparse_copy_value(parser, buf, buf_size);
}

// encapsulation to reading the next value as integer
static int ICACHE_FLASH_ATTR configuration_getNextInteger(struct jsonparse_state *parser)
{
	jsonparse_next(parser);
	jsonparse_next(parser);
	return jsonparse_get_value_as_int(parser);
}

// will be called after data was received via the tcp server connection
static void ICACHE_FLASH_ATTR configuration_receiveCallback(void *arg, char *pdata, unsigned short len)
{
	os_printf("Data received ...\n");
	struct jsonparse_state parser;
	int type;
	char ssid[32];
	os_bzero(ssid, sizeof(ssid));
	char password[64];
	os_bzero(password, sizeof(password));
	unsigned char cisternType = 0;
	unsigned int cisternRadius = 0;
	unsigned int cisternLength = 0;
	unsigned int distanceEmpty = 0;
	unsigned int litersFull = 0;
	char hostname[256];
	os_bzero(hostname, sizeof(hostname));
	int deepSleepPeriod = -1;
	int minDifferenceToPost = -1;
	int maxDataAgeToPost = -1;
	char thingspeakApiKey[32];
	os_bzero(thingspeakApiKey, sizeof(thingspeakApiKey));
	unsigned char logType = 0;
	char logHost[256];
	os_bzero(logHost, sizeof(logHost));
	unsigned short logPort = 0;

	// initialize the json parser and parse all data
	jsonparse_setup(&parser, pdata, len);
	while ((type = jsonparse_next(&parser)) != 0)
	{
		if (type == JSON_TYPE_PAIR_NAME)
		{
			if (jsonparse_strcmp_value(&parser, "SSID") == 0)
			{
				configuration_getNextString(&parser, ssid, sizeof(ssid));
				os_printf("SSID = '%s'\n", ssid);
			}
			else if (jsonparse_strcmp_value(&parser, "Password") == 0)
			{
				configuration_getNextString(&parser, password, sizeof(password));
				os_printf("Password = '%s'\n", password);
			}
			else if (jsonparse_strcmp_value(&parser, "CisternType") == 0)
			{
				cisternType = (unsigned char)configuration_getNextInteger(&parser);
				os_printf("CisternType = %d\n", cisternType);
			}
			else if (jsonparse_strcmp_value(&parser, "CisternRadius") == 0)
			{
				cisternRadius = configuration_getNextInteger(&parser);
				os_printf("CisternRadius = %d\n", cisternRadius);
			}
			else if (jsonparse_strcmp_value(&parser, "CisternLength") == 0)
			{
				cisternLength = configuration_getNextInteger(&parser);
				os_printf("CisternLength = %d\n", cisternLength);
			}
			else if (jsonparse_strcmp_value(&parser, "DistanceEmpty") == 0)
			{
				distanceEmpty = configuration_getNextInteger(&parser);
				os_printf("DistanceEmpty = %d\n", distanceEmpty);
			}
			else if (jsonparse_strcmp_value(&parser, "LitersFull") == 0)
			{
				litersFull = configuration_getNextInteger(&parser);
				os_printf("LitersFull = %d\n", litersFull);
			}
			else if (jsonparse_strcmp_value(&parser, "HostName") == 0)
			{
				configuration_getNextString(&parser, hostname, sizeof(hostname));
				os_printf("HostName = '%s'\n", hostname);
			}
			else if (jsonparse_strcmp_value(&parser, "DeepSleepPeriod") == 0)
			{
				deepSleepPeriod = configuration_getNextInteger(&parser);
				os_printf("DeepSleepPeriod = %d\n", deepSleepPeriod);
			}
			else if (jsonparse_strcmp_value(&parser, "MinDifferenceToPost") == 0)
			{
				minDifferenceToPost = configuration_getNextInteger(&parser);
				os_printf("MinDifferenceToPost = %d\n", minDifferenceToPost);
			}
			else if (jsonparse_strcmp_value(&parser, "MaxDataAgeToPost") == 0)
			{
				maxDataAgeToPost = configuration_getNextInteger(&parser);
				os_printf("MaxDataAgeToPost = %d\n", maxDataAgeToPost);
			}
			else if (jsonparse_strcmp_value(&parser, "ThingspeakApiKey") == 0)
			{
				configuration_getNextString(&parser, thingspeakApiKey, sizeof(thingspeakApiKey));
				os_printf("ThingspeakApiKey = '%s'\n", thingspeakApiKey);
			}
			else if (jsonparse_strcmp_value(&parser, "LogType") == 0)
			{
				logType = configuration_getNextInteger(&parser);
				os_printf("LogType = %d\n", logType);
			}
			else if (jsonparse_strcmp_value(&parser, "LogHost") == 0)
			{
				configuration_getNextString(&parser, logHost, sizeof(logHost));
				os_printf("LogHost = '%s'\n", logHost);
			}
			else if (jsonparse_strcmp_value(&parser, "LogPort") == 0)
			{
				logPort = configuration_getNextInteger(&parser);
				os_printf("LogPort = %d\n", logPort);
			}
		}
	}

	// all data found in the received json data?
	if (strlen(ssid) > 0 && strlen(password) > 0 &&
		((cisternType == 1 && cisternLength > 0) || cisternType == 2) &&
		cisternRadius > 0 && distanceEmpty > 0 && litersFull > 0 &&
		strlen(hostname) > 0 && deepSleepPeriod > 0 &&
		minDifferenceToPost > 0 && maxDataAgeToPost > 0 &&strlen(thingspeakApiKey) > 0)
	{
		// set the access point configuration
		os_printf("Found valid configuration!\n");
		struct station_config stationConfiguration;
		os_strcpy(stationConfiguration.ssid, ssid);
		os_strcpy(stationConfiguration.password, password);
		stationConfiguration.bssid_set = 0;
		memset(stationConfiguration.bssid, 0, 6);
		// store the configuration
		wifi_station_disconnect();
		wifi_set_opmode_current(STATION_MODE);
		wifi_station_set_config(&stationConfiguration);
		// diabel the staton mode
		wifi_set_opmode(NULL_MODE);

		// store the configuration data in flash
		configuration_data.version = CONFIGURATION_DATA_VERSION;
		configuration_data.cisternType = cisternType;
		configuration_data.cisternRadius = cisternRadius;
		configuration_data.cisternLength = cisternLength;
		configuration_data.distanceEmpty = distanceEmpty;
		configuration_data.litersFull = litersFull;
		os_strcpy(configuration_data.hostname, hostname);
		configuration_data.deepSleepPeriod = deepSleepPeriod;
		configuration_data.minDifferenceToPost = minDifferenceToPost;
		configuration_data.maxDataAgeToPost = maxDataAgeToPost;
		os_strcpy(configuration_data.thingspeakApiKey, thingspeakApiKey);
		configuration_data.logType = logType;
		os_strcpy(configuration_data.logHost, logHost);
		configuration_data.logPort = logPort;

		// use the protected write to three sectors method
		system_param_save_with_protect(CONFIGURATION_DATA_START_SEC, &configuration_data, sizeof(configuration_data));
		
		powermanagement_leaveConfigurationMode();
	}
	else
	{
		os_printf("Data incomplete!\n");
	}
}

// will be called after a TCp client has connected to the TCP server
static void ICACHE_FLASH_ATTR configuration_connectCallback(void *arg)
{
	// register a callback for data receive
	espconn *conn = arg;
	espconn_regist_recvcb(conn, configuration_receiveCallback);
	os_printf("Incoming connection ...\n");
}

// start the configuration mode
void ICACHE_FLASH_ATTR configuration_start()
{
	// activate the a Wifi acces point and start a TCp server for listening for configuration data
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
	espconn_tcp_set_max_con_allow(&configuration_socketConnection, 1);

	os_printf("Listening ...\n");
}

// initialize the configuration module; return true if successfully initialized; false if no configuration was found
unsigned char ICACHE_FLASH_ATTR configuration_init()
{
	// read from flash and test if data is valid
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

// API key for ThingSpeak
char* ICACHE_FLASH_ATTR configuration_getThingspeakApiKey()
{
	return configuration_data.thingspeakApiKey;
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